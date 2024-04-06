#pragma once
#include <vector>
#include <cassert>
#include <cmath>

#include "logger.hpp"
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
    Generals *genPtr = new SubGenerals(gamestate.next_generals_id++, player, location);
    // 将新的将军放置在棋盘上的指定位置
    cell.generals = genPtr;
    // 将新的将军添加到游戏状态的将军列表中
    gamestate.generals.push_back(genPtr);
    // 玩家的硬币减少50
    gamestate.coin[player] -= Constant::SPAWN_GENERAL_COST;
    return true;
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
    assert(location.in_map() && (player == 0 || player == 1));
    if (num == 0) return false;
    assert(num > 0);

    Cell& cell = gamestate[location];

    if (cell.player != player) return false; // 操作格子非法
    if (gamestate.rest_move_step[player] == 0) return false;
    if (num > cell.army - 1) return false;

    for (SuperWeapon &sw : gamestate.active_super_weapon) { // 超级武器效果
        if (sw.position == location && sw.rest &&
            sw.type == WeaponType::TRANSMISSION && sw.player == player) return false; // 超时空传送眩晕
        if (location.in_super_weapon_range(sw.position) && sw.rest && sw.type == WeaponType::TIME_STOP)
            return false; // 时间暂停效果
    }

    const Coord& new_position = location + DIRECTION_ARR[direction];
    Cell& new_cell = gamestate[new_position];

    if (!new_position.in_map()) return false; // 越界
    if (new_cell.type == CellType::SWAMP && gamestate.tech_level[player][1] == 0) return false; // 不能经过沼泽

    if (new_cell.player == player) { // 目的地格子己方所有
        new_cell.army += num;
        cell.army -= num;
    } else if (new_cell.player == 1 - player || new_cell.player == -1) { // 攻击敌方或无主格子
        float attack = gamestate.attack_multiplier(location);
        float defence = gamestate.defence_multiplier(new_position);
        float vs = num * attack - new_cell.army * defence;
        if (vs > 0) { // 攻下
            new_cell.player = player;
            new_cell.army = (int)(std::ceil(vs / attack));
            cell.army -= num;
            if (new_cell.generals != nullptr) new_cell.generals->player = player; // 将军易主
        } else if (vs < 0) { // 防住
            new_cell.army = (int)(std::ceil((-vs) / defence));
            cell.army -= num;
        } else if (vs == 0) { // 中立
            if (new_cell.generals == nullptr) new_cell.player = -1;
            new_cell.army = 0;
            cell.army -= num;
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
    if (!location.in_map() || !destination.in_map()) return std::make_pair(false, -1); // 越界

    Cell& cell = gamestate[location];
    if (player != 0 && player != 1) return std::make_pair(false, -1); // 玩家非法
    if (cell.player != player || cell.generals == nullptr) return std::make_pair(false, -1); // 起始格子非法

    // 油井不能移动
    OilWell *oilWellPtr = dynamic_cast<OilWell *>(cell.generals);
    if (oilWellPtr) return std::make_pair(false, -1);

    for (SuperWeapon &sw : gamestate.active_super_weapon) {
        if (sw.position == location && sw.rest &&
            sw.type == WeaponType::TRANSMISSION && sw.player == player) return std::make_pair(false, -1); // 超时空传送眩晕
        if (location.in_super_weapon_range(sw.position) && sw.rest && sw.type == WeaponType::TIME_STOP)
            return std::make_pair(false, -1); // 时间暂停效果
    }

    // bfs检查可移动性
    int op = -1, cl = 0;
    std::vector<Coord> queue;
    std::vector<int> steps;
    static bool check[Constant::row][Constant::col];
    memset(check, 0, sizeof(check));

    queue.emplace_back(location);
    steps.push_back(0);
    check[location.x][location.y] = true;

    int rest_move = cell.generals->rest_move;
    while (op < cl) {
        op += 1;
        if (steps[op] > rest_move) break; // 步数超限
        if (queue[op] == destination) return std::make_pair(true, steps[op]); // 到达目的地

        int p = queue[op].x, q = queue[op].y;
        for (const auto &direction : DIRECTION_ARR) {
            Coord new_coord = Coord(p, q) + direction;
            int newP = p + direction.x, newQ = q + direction.y;

            if (!new_coord.in_map()) continue; // 越界
            if (check[newP][newQ]) continue; // 已入队
            if (!gamestate.can_general_step_on(new_coord, player)) continue; // 无法经过沼泽
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
    assert(location.in_map() && destination.in_map());
    Cell& old_cell = gamestate[location];
    Cell& new_cell = gamestate[destination];
    int num = old_cell.army - 1; // 计算移动的军队数量

    // 如果目标位置没有玩家
    if (new_cell.player == -1) {
        new_cell.army += num;
        old_cell.army -= num;
        new_cell.player = player; // 设置目标位置的玩家
    }
    // 如果目标位置是当前玩家
    else if (new_cell.player == player) {
        old_cell.army -= num;		   // 减少当前位置的军队数量
        new_cell.army += num; // 增加目标位置的军队数量
    }
    // 如果目标位置是对手玩家
    else if (new_cell.player == 1 - player) {
        float attack = gamestate.attack_multiplier(location);
        float vs = num * attack - new_cell.army * gamestate.defence_multiplier(destination); // 计算战斗结果
        // 确保战斗结果为正
        if (vs <= 0) {
            logger.log(LOG_LEVEL_ERROR, "vs = %d * %f - %d * %f = %f < 0",
                       num, attack, new_cell.army, gamestate.defence_multiplier(destination), vs);
            assert(!"army_rush error: vs < 0");
        }

        new_cell.player = player;
        new_cell.army = (int)(std::ceil(vs / attack));
        old_cell.army -= num;
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
bool check_rush_param(int player, const Coord& destination, const Coord& location, const GameState &gamestate) {
    if (!location.in_map() || !destination.in_map()) return false;
    const Cell& old_cell = gamestate[location];
    const Cell& new_cell = gamestate[destination];

    // 检查参数合理性
    if (old_cell.generals == nullptr) return false; // 如果当前位置没有将军，返回失败
    if (old_cell.army < 2) return false; // 如果当前位置的军队数量小于2，返回失败
    if (new_cell.generals != nullptr) return false; // 如果目标位置有将军，返回失败
    if (new_cell.type == CellType::SWAMP && gamestate.tech_level[player][static_cast<int>(TechType::IMMUNE_SWAMP)] == 0)
        return false; // 如果目标位置是沼泽且玩家没有免疫沼泽技能，返回失败

    // 如果目标位置是对手玩家
    if (new_cell.player == 1 - player) {
        int num = old_cell.army - 1; // 计算移动的军队数量
        float vs = num * gamestate.attack_multiplier(location) - new_cell.army * gamestate.defence_multiplier(destination);
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

    // 只有前两种技能是有“目标位置”的
    if (skillType == SkillType::RUSH || skillType == SkillType::STRIKE) {
        if (!destination.in_map()) return false; // 如果目的地坐标超出范围，则返回false
        if (!destination.in_attack_range(location)) return false; // 如果目的地与当前位置之间的距离超过“攻击范围”，则返回false
    }

    // 检查参数合理性
    if (gamestate[location].player != player) return false; // 如果指定位置上的玩家不是当前玩家，则返回false
    int coin = gamestate.coin[player];
    Generals *general = gamestate[location].generals;
    if (general == nullptr) return false; // 如果指定位置上没有将领，则返回false

    // 超级武器效果
    for (SuperWeapon &sw : gamestate.active_super_weapon) {
        if (sw.position == location && sw.rest &&
            sw.type == WeaponType::TRANSMISSION && sw.player == player) return false; // 超时空传送眩晕
        if (sw.position.in_super_weapon_range(location) && sw.rest && sw.type == WeaponType::TIME_STOP)
            return false; // 时间暂停效果
    }

    assert(skillType >= SkillType::RUSH && skillType <= SkillType::WEAKEN);
    if (coin < skillType.cost() || general->skills_cd[static_cast<int>(skillType)] > 0)
        return false; // 未冷却好或石油不足

    if (skillType == SkillType::RUSH) {
        if (!check_rush_param(player, destination, location, gamestate)) return false; // 如果突袭技能的参数不合法，则返回false

        general->position = destination;
        gamestate[destination].generals = general;
        gamestate[location].generals = nullptr;
        army_rush(location, gamestate, player, destination);
    } else if (skillType == SkillType::STRIKE) {
        if (!handle_breakthrough(destination, gamestate)) return false;
    }
    gamestate.coin[player] -= skillType.cost();
    general->skills_cd[static_cast<int>(skillType)] = skillType.cd();
    general->skill_duration[static_cast<int>(skillType)] = skillType.duration();
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
    if (!location.in_map()) return false;

    // 检查超级武器是否解锁并且冷却时间为0
    bool is_super_weapon_unlocked = gamestate.super_weapon_unlocked[player];
    int cd = gamestate.super_weapon_cd[player];
    if (is_super_weapon_unlocked && cd == 0) {
        // 激活超级武器
        gamestate.active_super_weapon.push_back(SuperWeapon(WeaponType::NUCLEAR_BOOM, player, 0, 5, location));
        // 设置超级武器的冷却时间
        gamestate.super_weapon_cd[player] = Constant::SUPER_WEAPON_CD;

        // 处理炸弹效果
        // 遍历目标位置周围的单元格
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                // 如果单元格在棋盘内，处理该单元格
                Coord coord{location.x + i, location.y + j};
                if (!coord.in_map())  continue;

                Cell &cell = gamestate[coord];
                // 如果单元格中有主将，军队数量减半
                if (dynamic_cast<MainGenerals *>(cell.generals)) cell.army = (int)(cell.army / 2);
                else { // 否则，清空单元格
                    cell.army = 0;
                    cell.player = -1;
                    cell.generals = nullptr;

                    auto it = gamestate.generals.begin();
                    // 从将军列表中移除该将军
                    while (it != gamestate.generals.end()) {
                        if ((*it)->position == coord) {
                            it = gamestate.generals.erase(it);
                            break;
                        }
                        it++;
                    }
                }
            }
        }
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

        // 检查目标位置是否为沼泽且玩家无沼泽免疫
        if (cell_to.type == CellType::SWAMP && gamestate.tech_level[player][static_cast<int>(TechType::IMMUNE_SWAMP)] == 0)
            return false;

        if (cell_st.army <= 1) return false;

        cell_to.army = cell_st.army - 1;
        cell_st.army = 1;
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

/*
* 描述：科技升级。
* 参数：
  * `int tech_type`：升级的科技类型。
  * `GameState &gamestate`：游戏状态对象的引用。
  * `int player`：执行操作的玩家编号。
* 返回值：如果技术升级成功，返回 `true`；否则返回 `false`。
 */
bool tech_update(TechType tech_type, GameState &gamestate, int player) {
    if (tech_type == TechType::MOBILITY) {
        for (int i = 0; i < Constant::PLAYER_MOVEMENT_LEVELS; ++i) {
                if (gamestate.tech_level[player][static_cast<int>(tech_type)] == Constant::PLAYER_MOVEMENT_VALUES[i]) {
                    if (gamestate.coin[player] < Constant::PLAYER_MOVEMENT_COST[i]) return false;

                    gamestate.coin[player] -= Constant::PLAYER_MOVEMENT_COST[i];
                    gamestate.rest_move_step[player] = Constant::PLAYER_MOVEMENT_VALUES[i + 1]; // 【立即恢复移动步数（未说明的feature）】
                    gamestate.tech_level[player][static_cast<int>(tech_type)] = Constant::PLAYER_MOVEMENT_VALUES[i + 1];
                    return true;
                }
            }
        return false;
    }

    if (gamestate.tech_level[player][static_cast<int>(tech_type)] != 0) return false;
    switch (tech_type) {
        case TechType::IMMUNE_SWAMP:
            if (gamestate.tech_level[player][1] == 0) {
                if (gamestate.coin[player] < Constant::swamp_immunity) return false;
                gamestate.tech_level[player][1] = 1;
                gamestate.coin[player] -= Constant::swamp_immunity;
                return true;
            }
            return false;
            break;
        case TechType::IMMUNE_SAND:
            if (gamestate.tech_level[player][2] == 0) {
                if (gamestate.coin[player] < Constant::sand_immunity) return false;
                gamestate.tech_level[player][2] = 1;
                gamestate.coin[player] -= Constant::sand_immunity;
                return true;
            }
            return false;
            break;
        case TechType::UNLOCK:
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
        default:
            assert(false);
    }
    return false;
}

// 执行单个操作，返回是否成功
bool execute_operation(GameState &game_state, int player, const Operation &op) {
    // 获取操作码和操作数
    OperationType command = op.opcode;
    const int* params = op.operand;

    // 根据操作码执行相应的操作
    switch (command) {
        // 移动军队
        case OperationType::MOVE_ARMY: {
            int real_army = params[3];
            if (real_army > game_state[{params[0], params[1]}].army - 1) {
                logger.log(LOG_LEVEL_ERROR, "\t\tInvalid army count for op MOVE_ARMY: %s %d %d, truncated to %d",
                           Coord(params[0], params[1]).str().c_str(), params[2], real_army, game_state[{params[0], params[1]}].army - 1);
                real_army = game_state[{params[0], params[1]}].army - 1;
            }
            return army_move({params[0], params[1]}, game_state, player, static_cast<Direction>(params[2] - 1), real_army);
        }
        // 移动将军
        case OperationType::MOVE_GENERALS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;
            return general_move(pos, game_state, player, {params[1], params[2]});
        }
        // 更新将军
        case OperationType::UPDATE_GENERALS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;

            switch (params[1]) {
                case 1:
                    // 提升生产力
                    return production_up(pos, game_state, player);
                case 2:
                    // 提升防御力
                    return defence_up(pos, game_state, player);
                case 3:
                    // 提升移动力
                    return movement_up(pos, game_state, player);
                default:
                    assert(false);
            }
        }
        // 使用将军技能
        case OperationType::USE_GENERAL_SKILLS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;
            if (params[1] == 1 || params[1] == 2)
                return skill_activate(player, pos, {params[2], params[3]}, game_state, static_cast<SkillType>(params[1] - 1));
            else return skill_activate(player, pos, {-1, -1}, game_state, static_cast<SkillType>(params[1] - 1));
        }
        // 更新科技
        case OperationType::UPDATE_TECH:
            return tech_update(static_cast<TechType>(params[0] - 1), game_state, player);
        // 使用超级武器
        case OperationType::USE_SUPERWEAPON:
            switch (params[0]) {
                case 1:
                    // 炸弹
                    return bomb(game_state, {params[1], params[2]}, player);
                case 2:
                    // 强化
                    return strengthen(game_state, {params[1], params[2]}, player);
                case 3:
                    // 传送
                    return tp(game_state, {params[3], params[4]}, {params[1], params[2]}, player);
                case 4:
                    // 停止时间
                    return timestop(game_state, {params[1], params[2]}, player);
                default:
                    assert(false);
            }
        // 召唤将军
        case OperationType::CALL_GENERAL:
            return call_generals(game_state, player, {params[0], params[1]});
        default:
            assert(false);
    }
    assert(!"Should not reach here");
    return false;
}

// 执行一系列操作，返回是否成功
bool execute_operations(GameState &game_state, const Operation_list& operations) {
    for (const Operation& op : operations)
        if (!execute_operation(game_state, operations.player, op))
            return false;
    return true;
}
