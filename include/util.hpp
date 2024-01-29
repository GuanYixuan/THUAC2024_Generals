#pragma once
#include <vector>
#include <cassert>
#include <cmath>
#include <set>
#include "gamestate.hpp"

/* ### `bool call_generals(GameState &gamestate, int player, const Coord& location)`

* 描述：在指定位置召唤副将。
* 参数：
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：召唤副将的玩家编号。
  * `const Coord& location`：召唤副将的位置。
* 返回值：如果召唤成功，返回 `true`；否则返回 `false`。
*/
bool call_generals(GameState &gamestate, int player, const Coord& location) {
    Cell &cell = gamestate.board[location.x][location.y];

    // 如果玩家的硬币少于50，返回false
    if (gamestate.coin[player] < Constant::SPAWN_GENERAL_COST) return false;
    // 如果棋盘上的位置不属于玩家，返回false
    if (cell.player != player) return false;
    // 如果棋盘上的位置已经有将军，返回false
    else if (cell.generals != nullptr) return false;

    // 创建一个新的将军
    SubGenerals gen = SubGenerals(gamestate.next_generals_id++, player, location);
    Generals *genPtr = &gen;
    // 将新的将军放置在棋盘上的指定位置
    cell.generals = genPtr;
    // 将新的将军添加到游戏状态的将军列表中
    gamestate.generals.push_back(gen);
    // 玩家的硬币减少50
    gamestate.coin[player] -= Constant::SPAWN_GENERAL_COST;
    return true;
}

/*
### `float compute_attack(Cell cell, GameState gamestate)`

* 描述：计算指定格子的攻击力。
* 参数：
  * `Cell cell`：目标格子。
  * `GameState gamestate`：游戏状态对象。
* 返回值：攻击力值。
 */
float compute_attack(const Cell &cell, const GameState &gamestate) {
    float attack = 1.0;
    int cell_x = cell.position.x;
    int cell_y = cell.position.y;
    // 遍历cell周围至少5*5的区域，寻找里面是否有将军，他们是否使用了增益或减益技能
    for (int i = -Constant::GENERAL_ATTACK_RADIUS; i <= Constant::GENERAL_ATTACK_RADIUS; ++i) {
        for (int j = -Constant::GENERAL_ATTACK_RADIUS; j <= Constant::GENERAL_ATTACK_RADIUS; ++j) {
            int x = cell_x + i;
            int y = cell_y + j;
            if (0 <= x && x < Constant::row && 0 <= y && y < Constant::col) {
                const Cell &neighbor_cell = gamestate.board[x][y];
                if (neighbor_cell.generals != nullptr) {
                    if (neighbor_cell.player == cell.player && neighbor_cell.generals->skill_duration[SkillType::COMMAND] > 0)
                        attack *= Constant::GENERAL_SKILL_EFFECT[SkillType::COMMAND];
                    if (neighbor_cell.player != cell.player && neighbor_cell.generals->skill_duration[SkillType::WEAKEN] > 0)
                        attack *= Constant::GENERAL_SKILL_EFFECT[SkillType::WEAKEN];
                }
            }
        }
    }
    // 考虑gamestate中的超级武器是否被激活，（可以获取到激活的位置）该位置的军队是否会被影响
    for (const SuperWeapon &weapon : gamestate.active_super_weapon) {
        if (weapon.type == WeaponType::ATTACK_ENHANCE &&
            cell_x - Constant::SUPER_WEAPON_RADIUS <= weapon.position.x && weapon.position.x <= cell_x + Constant::SUPER_WEAPON_RADIUS &&
            cell_y - Constant::SUPER_WEAPON_RADIUS <= weapon.position.y && weapon.position.y <= cell_y + Constant::SUPER_WEAPON_RADIUS &&
            weapon.player == cell.player)
        {
            attack = attack * 3;
            break;
        }
    }

    return attack;
}

/* ### `float compute_defence(Cell cell, GameState gamestate)`

* 描述：计算指定格子的防御力。
* 参数：
  * `Cell cell`：目标格子。
  * `GameState gamestate`：游戏状态对象。
* 返回值：防御力值。 */
float compute_defence(const Cell &cell, const GameState &gamestate) {
    float defence = 1.0;
    int cell_x = cell.position.x;
    int cell_y = cell.position.y;
    // 遍历cell周围至少5*5的区域，寻找里面是否有将军，他们是否使用了增益或减益技能
    for (int i = -Constant::GENERAL_ATTACK_RADIUS; i <= Constant::GENERAL_ATTACK_RADIUS; ++i) {
        for (int j = -Constant::GENERAL_ATTACK_RADIUS; j <= Constant::GENERAL_ATTACK_RADIUS; ++j) {
            int x = cell_x + i;
            int y = cell_y + j;
            if (0 <= x && x < Constant::row && 0 <= y && y < Constant::col) {
                const Cell &neighbor_cell = gamestate.board[x][y];
                if (neighbor_cell.generals != nullptr) {
                    if (neighbor_cell.player == cell.player && neighbor_cell.generals->skill_duration[SkillType::DEFENCE] > 0)
                        defence *= Constant::GENERAL_SKILL_EFFECT[SkillType::DEFENCE];
                    if (neighbor_cell.player != cell.player && neighbor_cell.generals->skill_duration[SkillType::WEAKEN] > 0)
                        defence *= Constant::GENERAL_SKILL_EFFECT[SkillType::WEAKEN];
                }
            }
        }
    }
    // 考虑cell上是否有general，它的防御力是否被升级
    if (cell.generals != nullptr) defence *= cell.generals->defence_level;
    // 考虑gamestate中的超级武器是否被激活，（可以获取到激活的位置）该位置的军队是否会被影响
    for (const SuperWeapon &weapon : gamestate.active_super_weapon) {
        if (
            weapon.type == WeaponType::ATTACK_ENHANCE &&
            cell_x - Constant::SUPER_WEAPON_RADIUS <= weapon.position.x && weapon.position.x <= cell_x + Constant::SUPER_WEAPON_RADIUS &&
            cell_y - Constant::SUPER_WEAPON_RADIUS <= weapon.position.y && weapon.position.y <= cell_y + Constant::SUPER_WEAPON_RADIUS &&
            weapon.player == cell.player)
        {
            defence = defence * 3;
            break;
        }
    }

    return defence;
}

/* ### `const Coord& calculate_new_pos(const Coord& location, Direction direction)`

* 描述：根据移动方向计算新的位置。
* 参数：
  * `const Coord& location`：当前位置。
  * `Direction direction`：移动方向。
* 返回值：新的位置坐标。 */
Coord calculate_new_pos(const Coord& location, Direction direction) {
    Coord new_location = location + DIRECTION_ARR[direction];
    if (!new_location.in_map()) return {-1, -1};
    return new_location;
}

/* ### `bool army_move(const const Coord& location, GameState &gamestate, int player, Direction direction, int num)`

* 描述：执行军队移动操作。
* 参数：
  * `const Coord& location`：起始位置。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行移动的玩家编号。
  * `Direction direction`：移动方向。
  * `int num`：移动的兵力数量。
* 返回值：如果移动成功，返回 `true`；否则返回 `false`。 */
bool army_move(const Coord& location, GameState &gamestate, int player, Direction direction, int num) {
    int x = location.x, y = location.y;
    if (!location.in_map()) return false; // 越界
    if (player != 0 && player != 1) return false; // 玩家参数非法
    if (gamestate.board[x][y].player != player) return false; // 操作格子非法
    if (gamestate.rest_move_step[player] == 0) return false;
    if (gamestate.board[x][y].army <= 1) return false;

    if (num <= 0) return false; // 移动数目非法
    if (num > gamestate.board[x][y].army - 1) return false;

    for (SuperWeapon &sw : gamestate.active_super_weapon) { // 超级武器效果
        if (sw.position == location && sw.rest && sw.type == WeaponType::TRANSMISSION) return false; // 超时空传送眩晕
        if (std::abs(sw.position.x - x) <= 1 && std::abs(sw.position.y - y) <= 1 && sw.rest && sw.type == WeaponType::TIME_STOP)
            return false; // 时间暂停效果
    }

    const Coord& new_position = calculate_new_pos(location, direction);
    int newX = new_position.x, newY = new_position.y;
    Cell& new_cell = gamestate.board[newX][newY];

    if (newX < 0) return false; // 越界
    if (new_cell.type == CellType::SWAMP && gamestate.tech_level[player][1] == 0) return false; // 不能经过沼泽

    if (new_cell.player == player) { // 目的地格子己方所有
        new_cell.army += num;
        gamestate.board[x][y].army -= num;
    } else if (new_cell.player == 1 - player || new_cell.player == -1) { // 攻击敌方或无主格子
        float attack = compute_attack(gamestate.board[x][y], gamestate);
        float defence = compute_defence(new_cell, gamestate);
        float vs = num * attack - new_cell.army * defence;
        if (vs > 0) { // 攻下
            new_cell.player = player;
            new_cell.army = (int)(std::ceil(vs / attack));
            gamestate.board[x][y].army -= num;
            if (new_cell.generals != nullptr) new_cell.generals->player = player; // 将军易主
        } else if (vs < 0) { // 防住
            new_cell.army = (int)(std::ceil((-vs) / defence));
            gamestate.board[x][y].army -= num;
        } else if (vs == 0) { // 中立
            if (new_cell.generals == nullptr) new_cell.player = -1;
            new_cell.army = 0;
            gamestate.board[x][y].army -= num;
        }
    }
    gamestate.rest_move_step[player] -= 1;
    return true;
}

/* ### `bool check_general_movement(const Coord& location, GameState &gamestate, int player, const Coord& destination)`

* 描述：检查将军移动的合法性。
* 参数：
  * `const Coord& location`：将军当前位置。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行移动的玩家编号。
  * `const Coord& destination`：目标位置。
* 返回值：如果移动合法，返回 `true`；否则返回 `false`。 */
std::pair<bool, int> check_general_movement(const Coord& location, GameState &gamestate, int player, const Coord& destination) {
    int x = location.x, y = location.y;

    if (!location.in_map() || !destination.in_map()) return std::make_pair(false, -1); // 越界
    if (player != 0 && player != 1) return std::make_pair(false, -1); // 玩家非法
    if (gamestate.board[x][y].player != player || gamestate.board[x][y].generals == nullptr)
        return std::make_pair(false, -1); // 起始格子非法

    // 油井不能移动
    OilWell *oilWellPtr = dynamic_cast<OilWell *>(gamestate.board[x][y].generals);
    if (oilWellPtr) return std::make_pair(false, -1);

    for (SuperWeapon &sw : gamestate.active_super_weapon) {
        if (sw.position == location && sw.rest && sw.type == WeaponType::TRANSMISSION)
            return std::make_pair(false, -1); // 超时空传送眩晕
        if (std::abs(sw.position.x - x) <= 1 && std::abs(sw.position.y - y) <= 1 && sw.rest && sw.type == WeaponType::TIME_STOP)
            return std::make_pair(false, -1); // 时间暂停效果
    }

    int newX = destination.x, newY = destination.y;

    // bfs检查可移动性
    int op = -1, cl = 0;
    std::vector<Coord> queue;
    std::vector<int> steps;
    static bool check[Constant::row][Constant::col];
    memset(check, 0, sizeof(check));

    queue.emplace_back(x, y);
    steps.push_back(0);
    check[x][y] = true;

    while (op < cl) {
        op += 1;
        if (steps[op] > gamestate.board[x][y].generals->rest_move) break; // 步数超限
        if (queue[op] == std::make_pair(newX, newY)) return std::make_pair(true, steps[op]); // 到达目的地

        int p = queue[op].x, q = queue[op].y;
        for (const auto &direction : DIRECTION_ARR) {
            Coord new_coord = Coord(p, q) + direction;
            int newP = p + direction.x, newQ = q + direction.y;

            if (!new_coord.in_map()) continue; // 越界
            if (check[newP][newQ]) continue; // 已入队
            if (gamestate[new_coord].type == CellType::SWAMP && gamestate.tech_level[player][1] == 0)
                continue; // 无法经过沼泽
            if (gamestate[new_coord].player != player || gamestate[new_coord].generals != nullptr)
                continue; // 目的地格子非法
            queue.push_back(new_coord); // 入队
            cl += 1;
            steps.push_back(steps[op] + 1);
            check[newP][newQ] = true;
        }
    }

    // bfs结束，没到达目的地
    return std::make_pair(false, -1);
}

/* ### `bool general_move(const Coord& location, GameState &gamestate, int player, const Coord& destination)`

* 描述：执行将军移动操作（注：此函数不进行合法性检查）。
* 参数：
  * `const Coord& location`：将军当前位置。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行移动的玩家编号。
  * `const Coord& destination`：目标位置。
* 返回值：如果移动成功，返回 `true`；否则返回 `false`。 */
bool general_move(const Coord& location, GameState &gamestate, int player, const Coord& destination) {
    std::pair<bool, int> able = check_general_movement(location, gamestate, player, destination);
    if (!able.first) return false;

    gamestate[destination].generals = gamestate[location].generals;
    gamestate[destination].generals->position = destination;
    gamestate[destination].generals->rest_move -= able.second;
    gamestate[location].generals = nullptr;

    return true;
}

// 用于处理军队突袭
/* ### `bool army_rush(const Coord& location, GameState &gamestate, int player, const Coord& destination)`

* 描述：处理军队突袭技能（注，此函数不检查合法性）。
* 参数：
  * `const Coord& location`：当前位置。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行技能的玩家编号。
  * `const Coord& destination`：目标位置。
* 返回值：如果处理成功，返回 `true`；否则返回 `false`。 */
bool army_rush(const Coord& location, GameState &gamestate, int player, const Coord& destination) {
    int x = location.x, y = location.y;			   // 获取当前位置
    int new_x = destination.x, new_y = destination.y; // 获取目标位置
    int num = gamestate.board[x][y].army - 1;				   // 计算移动的军队数量

    // 如果目标位置没有玩家
    if (gamestate.board[new_x][new_y].player == -1) {
        gamestate.board[new_x][new_y].army += num;
        gamestate.board[x][y].army -= num;
        gamestate.board[new_x][new_y].player = player; // 设置目标位置的玩家
    }
    // 如果目标位置是当前玩家
    else if (gamestate.board[new_x][new_y].player == player) {
        gamestate.board[x][y].army -= num;		   // 减少当前位置的军队数量
        gamestate.board[new_x][new_y].army += num; // 增加目标位置的军队数量
    }
    // 如果目标位置是对手玩家
    else if (gamestate.board[new_x][new_y].player == 1 - player) {
        float attack = compute_attack(gamestate.board[x][y], gamestate);		   // 计算攻击力
        float defence = compute_defence(gamestate.board[new_x][new_y], gamestate); // 计算防御力
        float vs = num * attack - gamestate.board[new_x][new_y].army * defence;	   // 计算战斗结果
        assert(vs > 0);															   // 确保战斗结果为正
        gamestate.board[new_x][new_y].player = player;							   // 设置目标位置的玩家
        gamestate.board[new_x][new_y].army = (int)(std::ceil(vs / attack));		   // 设置目标位置的军队数量
        gamestate.board[x][y].army -= num;										   // 减少当前位置的军队数量
    }
    return true; // 返回成功
}

// 检查军队突袭参数
/* ### `bool check_rush_param(int player, const Coord& destination, const Coord& location, GameState &gamestate)`

* 描述：检查军队突袭技能参数的合法性。
* 参数：
  * `int player`：执行技能的玩家编号。
  * `const Coord& destination`：目标位置。
  * `const Coord& location`：当前位置。
  * `GameState &gamestate`：游戏状态对象的引用。
* 返回值：如果参数合法，返回 `true`；否则返回 `false`。 */
bool check_rush_param(int player, const Coord& destination, const Coord& location, GameState &gamestate) {
    int x = location.x, y = location.y;			   // 获取当前位置
    int x_new = destination.x, y_new = destination.y; // 获取目标位置

    // 检查参数合理性
    if (gamestate[location].generals == nullptr) return false; // 如果当前位置没有将军，返回失败
    if (gamestate[location].army < 2) return false; // 如果当前位置的军队数量小于2，返回失败
    if (gamestate[destination].generals != nullptr) return false; // 如果目标位置有将军，返回失败

    // 如果目标位置是对手玩家
    if (gamestate[destination].player == 1 - player) {
        int num = gamestate.board[x][y].army - 1;								   // 计算移动的军队数量
        float attack = compute_attack(gamestate.board[x][y], gamestate);		   // 计算攻击力
        float defence = compute_defence(gamestate.board[x_new][y_new], gamestate); // 计算防御力
        float vs = num * attack - gamestate[destination].army * defence;	   // 计算战斗结果
        if (vs <= 0) return false; // 如果战斗结果为负，返回失败
    }
    return true; // 返回成功
}

// 处理突破
/* ### `bool handle_breakthrough(const Coord& destination, GameState &gamestate)`

* 描述：处理突破技能（注，此函数不检查合法性）。
* 参数：
  * `const Coord& destination`：目标位置。
  * `GameState &gamestate`：游戏状态对象的引用。
* 返回值：如果处理成功，返回 `true`；否则返回 `false`。 */
bool handle_breakthrough(const Coord& destination, GameState &gamestate) {
    int x = destination.x, y = destination.y; // 获取目标位置

    // 如果目标位置的军队数量大于20
    if (gamestate.board[x][y].army > Constant::STRIKE_DAMAGE)
        gamestate.board[x][y].army -= Constant::STRIKE_DAMAGE; // 减少目标位置的军队数量
    else {
        gamestate.board[x][y].army = 0; // 设置目标位置的军队数量为0
        // 如果目标位置没有将军，则设置目标位置没有玩家
        if (gamestate.board[x][y].generals == nullptr) gamestate.board[x][y].player = -1;
    }
    return true; // 返回成功
}

/* ### `bool skill_activate(int player, const Coord& location, const Coord& destination, GameState &gamestate, SkillType skillType)`

* 描述：激活将军技能。
* 参数：
  * `int player`：执行技能的玩家编号。
  * `const Coord& location`：将军当前位置。
  * `const Coord& destination`：目标位置。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `SkillType skillType`：要激活的技能类型。
* 返回值：如果激活成功，返回 `true`；否则返回 `false`。 */
bool skill_activate(int player, const Coord& location, const Coord& destination, GameState &gamestate, SkillType skillType) {
    // 检查参数范围
    if (player != 0 && player != 1) return false; // 如果玩家参数不是0或1，则返回false
    if (!location.in_map()) return false; // 如果位置坐标超出范围，则返回false

    if (!destination.in_map()) return false; // 如果目的地坐标超出范围，则返回false
    if (!destination.in_attack_range(location)) return false; // 如果目的地与当前位置之间的距离超过“攻击范围”，则返回false

    // 检查参数合理性
    if (gamestate[location].player != player) return false; // 如果指定位置上的玩家不是当前玩家，则返回false
    int coin = gamestate.coin[player];
    Generals *general = gamestate[location].generals;
    if (general == nullptr) return false; // 如果指定位置上没有将领，则返回false

    // 超级武器效果
    for (SuperWeapon &sw : gamestate.active_super_weapon) {
        if (sw.position == location && sw.rest && sw.type == WeaponType::TRANSMISSION) return false; // 超时空传送眩晕
        if (sw.position.in_super_weapon_range(location) && sw.rest && sw.type == WeaponType::TIME_STOP)
            return false; // 时间暂停效果
    }

    assert(skillType >= SkillType::SURPRISE_ATTACK && skillType <= SkillType::WEAKEN);
    if (coin < skillType.cost() || general->skills_cd[static_cast<int>(skillType)] > 0)
        return false; // 未冷却好或石油不足

    if (skillType == SkillType::SURPRISE_ATTACK) {
        if (!check_rush_param(player, destination, location, gamestate)) return false; // 如果突袭技能的参数不合法，则返回false
        general_move(location, gamestate, player, destination);
        army_rush(location, gamestate, player, destination);
    } else if (skillType == SkillType::ROUT) {
        if (!handle_breakthrough(destination, gamestate)) return false;
    }
    gamestate.coin[player] -= skillType.cost();
    general->skills_cd[static_cast<int>(skillType)] = skillType.cd();
    general->skill_duration[static_cast<int>(skillType)] = skillType.duration();
    return true;
}

// 处理炸弹单元格的函数
/* ### `bool handle_bomb_cell(GameState &gamestate, int x, int y)`

* 描述：处理炸弹对单元格的影响。
* 参数：
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int x`：单元格的 x 坐标。
  * `int y`：单元格的 y 坐标。
* 返回值：如果处理成功，返回 `true`；否则返回 `false`。 */
bool handle_bomb_cell(GameState &gamestate, int x, int y) {
    Cell &cell = gamestate.board[x][y];
    // 如果单元格中有主将军，将军队数量减半
    if (dynamic_cast<MainGenerals *>(cell.generals)) cell.army = (int)(cell.army / 2);
    else {
        // 否则，清空单元格
        cell.army = 0;
        cell.player = -1;
        cell.generals = nullptr;
        auto it = gamestate.generals.begin();
        // 从将军列表中移除该将军
        while (it != gamestate.generals.end()) {
            if (it->position == Coord{x, y}) {
                it = gamestate.generals.erase(it);
                break;
            }
            it++;
        }
    }
    return true;
}
// 处理炸弹的函数
/* ### `bool handle_bomb(GameState &gamestate, const Coord &location)`

* 描述：处理使用炸弹的操作，触发炸弹效果。
* 参数：
  * `GameState &gamestate`：游戏状态对象的引用。
  * `const Coord& location`：炸弹爆炸的位置。
* 返回值：如果处理成功，返回 `true`；否则返回 `false`。 */
bool handle_bomb(GameState &gamestate, const Coord& location) {
    // 遍历目标位置周围的单元格
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int x = location.x + i;
            int y = location.y + j;
            // 如果单元格在棋盘内，处理该单元格
            if (x >= 0 && x < Constant::row && y >= 0 && y < Constant::col) handle_bomb_cell(gamestate, x, y);
        }
    }
    return true;
}

// 使用炸弹的函数
/* ### `bool bomb(GameState &gamestate, const Coord& location, int player)`

* 描述：激活炸弹超级武器。
* 参数：
  * `GameState &gamestate`：游戏状态对象的引用。
  * `const Coord& location`：炸弹爆炸的位置。
  * `int player`：执行操作的玩家编号。
* 返回值：如果激活成功，返回 `true`；否则返回 `false`。 */
bool bomb(GameState &gamestate, const Coord& location, int player) {
    // 检查玩家和位置的有效性
    if (player != 0 && player != 1) return false;
    if (location.x < 0 || location.x > Constant::row) return false;
    if (location.y < 0 || location.y > Constant::col) return false;

    // 检查超级武器是否解锁并且冷却时间为0
    bool is_super_weapon_unlocked = gamestate.super_weapon_unlocked[player];
    int cd = gamestate.super_weapon_cd[player];
    if (is_super_weapon_unlocked && cd == 0) {
        // 激活超级武器
        gamestate.active_super_weapon.push_back(SuperWeapon(WeaponType::NUCLEAR_BOOM, player, 0, 5, location));
        // 设置超级武器的冷却时间
        gamestate.super_weapon_cd[player] = Constant::SUPER_WEAPON_CD;
        // 处理炸弹
        handle_bomb(gamestate, location);
        return true;
    }
    return false;
}

// 强化的函数
/* ### `bool strengthen(GameState &gamestate, const Coord& location, int player)`

* 描述：激活强化超级武器。
* 参数：
  * `GameState &gamestate`：游戏状态对象的引用。
  * `const Coord& location`：超级武器激活的位置。
  * `int player`：执行操作的玩家编号。
* 返回值：如果激活成功，返回 `true`；否则返回 `false`。 */
bool strengthen(GameState &gamestate, const Coord& location, int player) {
    // 检查玩家和位置的有效性
    if (player != 0 && player != 1) return false;
    if (location.x < 0 || location.x > Constant::row) return false;
    if (location.y < 0 || location.y > Constant::col) return false;

    // 检查超级武器是否解锁并且冷却时间为0
    bool is_super_weapon_unlocked = gamestate.super_weapon_unlocked[player];
    int cd = gamestate.super_weapon_cd[player];
    if (is_super_weapon_unlocked && cd == 0) {
        // 激活超级武器
        gamestate.active_super_weapon.push_back(SuperWeapon(WeaponType::ATTACK_ENHANCE, player, 5, 5, location));
        // 设置超级武器的冷却时间
        gamestate.super_weapon_cd[player] = Constant::SUPER_WEAPON_CD;
        return true;
    }
    return false;
}

/**
 * 在游戏状态中执行传送操作，将指定位置的军队传送到目标位置。
 *
 * @param gamestate 游戏状态对象
 * @param start 起始位置的坐标（x，y）
 * @param to 目标位置的坐标（x，y）
 * @param player 玩家编号
 * @return 如果传送操作成功，则返回true；否则返回false。
 */
bool tp(GameState &gamestate, const Coord& start, const Coord& to, int player) {
    // 检查玩家编号是否有效
    if (player != 0 && player != 1) return false;

    // 检查起始和目标位置的坐标是否有效
    if (!start.in_map() || !to.in_map()) return false;

    // 检查是否已解锁超级武器并且冷却时间为0
    bool is_super_weapon_unlocked = gamestate.super_weapon_unlocked[player];
    int cd = gamestate.super_weapon_cd[player];
    if (is_super_weapon_unlocked && cd == 0) {
        Cell& cell_st = gamestate[start];
        Cell& cell_to = gamestate[to];

        // 检查起始位置的cell是否属于当前玩家
        if (cell_st.player != player) return false;

        // 检查目标位置是否已被占据
        if (cell_to.generals != nullptr) return false;

        int num = 0;
        if (cell_st.army == 0 || cell_st.army == 1) return false;
        else {
            num = cell_st.army - 1;
            cell_st.army = 1;
        }

        cell_to.army = num;
        cell_to.player = player;
        gamestate.super_weapon_cd[player] = Constant::SUPER_WEAPON_CD;
        gamestate.active_super_weapon.push_back(SuperWeapon(WeaponType::TRANSMISSION, player, 2, 2, to));
        return true;
    }
    return false;
}

/**
 * 在游戏状态中执行时间停止操作，冻结指定位置。
 *
 * @param gamestate 游戏状态对象
 * @param location 冻结位置的坐标（x，y）
 * @param player 玩家编号
 * @return 如果时间停止操作成功，则返回true；否则返回false。
 */
bool timestop(GameState &gamestate, const Coord& location, int player) {
    // 检查玩家编号是否有效
    if (player != 0 && player != 1) return false;

    // 检查冻结位置的坐标是否有效
    if (!location.in_map()) return false;

    // 检查是否已解锁超级武器并且冷却时间为0
    bool is_super_weapon_unlocked = gamestate.super_weapon_unlocked[player];
    int cd = gamestate.super_weapon_cd[player];
    if (is_super_weapon_unlocked && cd == 0) {
        gamestate.active_super_weapon.push_back(SuperWeapon(WeaponType::TIME_STOP, player, 10, 10, location));
        gamestate.super_weapon_cd[player] = Constant::SUPER_WEAPON_CD;
        return true;
    }
    return false;
}

/* ### `bool production_up(const Coord& location, GameState &gamestate, int player)`

* 描述：执行将军生产力升级。
* 参数：
  * `const Coord& location`：升级将军位置的坐标。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行操作的玩家编号。
* 返回值：如果技术升级成功，返回 `true`；否则返回 `false`。 */
bool production_up(const Coord& location, GameState &gamestate, int player) {
    Cell& cell = gamestate[location];
    if (cell.generals == nullptr) return false;
    if (cell.player != player) return false;

    return cell.generals->production_up(gamestate, player);
}

/* ### `bool defence_up(const Coord& location, GameState &gamestate, int player)`

* 描述：执行将军防御力升级。
* 参数：
  * `const Coord& location`：升级将军位置的坐标。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行操作的玩家编号。
* 返回值：如果技术升级成功，返回 `true`；否则返回 `false`。 */
bool defence_up(const Coord& location, GameState &gamestate, int player) {
    Cell& cell = gamestate.board[location.x][location.y];
    if (cell.generals == nullptr) return false;
    if (cell.player != player) return false;

    return cell.generals->defence_up(gamestate, player);
}

/* ### `bool movement_up(const Coord& location, GameState &gamestate, int player)`

* 描述：执行将军移动力升级。
* 参数：
  * `const Coord& location`：升级将军位置的坐标。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行操作的玩家编号。
* 返回值：如果技术升级成功，返回 `true`；否则返回 `false`。 */
bool movement_up(const Coord& location, GameState &gamestate, int player) {
    Cell& cell = gamestate.board[location.x][location.y];
    if (cell.generals == nullptr) return false;
    if (cell.player != player) return false;

    return cell.generals->movement_up(gamestate, player);
}

/* ### `bool tech_update(int tech_type, GameState &gamestate, int player)`

* 描述：科技升级。
* 参数：
  * `int tech_type`：升级的科技类型。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行操作的玩家编号。
* 返回值：如果技术升级成功，返回 `true`；否则返回 `false`。
 */
bool tech_update(int tech_type, GameState &gamestate, int player) {
    switch (tech_type) {
        case 0:
            if (gamestate.tech_level[player][0] == 2) {
                if (gamestate.coin[player] < Constant::army_movement_T1) return false;
                gamestate.tech_level[player][0] = 3;
                gamestate.coin[player] -= Constant::army_movement_T1;
                return true;
            } else if (gamestate.tech_level[player][0] == 3) {
                if (gamestate.coin[player] < Constant::army_movement_T2) return false;
                gamestate.tech_level[player][0] = 5;
                gamestate.coin[player] -= Constant::army_movement_T2;
                return true;
            }
            return false;
            break;
        case 1:
            if (gamestate.tech_level[player][1] == 0) {
                if (gamestate.coin[player] < Constant::swamp_immunity) return false;
                gamestate.tech_level[player][1] = 1;
                gamestate.coin[player] -= Constant::swamp_immunity;
                return true;
            }
            return false;
            break;
        case 2:
            if (gamestate.tech_level[player][2] == 0) {
                if (gamestate.coin[player] < Constant::sand_immunity) return false;
                gamestate.tech_level[player][2] = 1;
                gamestate.coin[player] -= Constant::sand_immunity;
                return true;
            }
            return false;
            break;
        case 3:
            if (gamestate.tech_level[player][3] == 0) {
                if (gamestate.coin[player] < Constant::unlock_super_weapon) return false;
                gamestate.tech_level[player][3] = 1;
                gamestate.super_weapon_cd[player] = 10;
                gamestate.super_weapon_unlocked[player] = true;
                gamestate.coin[player] -= Constant::unlock_super_weapon;
                return true;
            }
            return false;
            break;
    }
    return false;
}
