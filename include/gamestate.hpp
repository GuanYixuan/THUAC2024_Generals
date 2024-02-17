#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <iostream>
#include <algorithm>
#include "constant.hpp"

#ifndef M_PI
    #define M_PI (3.14159265358979323846)
#endif

// 坐标类
class Coord {
public:
    int x;
    int y;

    Coord() noexcept = default;
    constexpr Coord(int x, int y) noexcept : x(x), y(y) {};
    // 无缝衔接`std::pair<int, int>`
    constexpr Coord(const std::pair<int, int>& pos) noexcept : x(pos.first), y(pos.second) {};
    operator std::pair<int, int>() const noexcept { return std::make_pair(x, y); }

    // 比较与算术运算
    constexpr bool operator==(const Coord& other) const noexcept { return x == other.x && y == other.y; }
    constexpr bool operator!=(const Coord& other) const noexcept { return x != other.x || y != other.y; }
    constexpr Coord operator+(const Coord& other) const noexcept { return Coord(x + other.x, y + other.y); }
    constexpr Coord operator-(const Coord& other) const noexcept { return Coord(x - other.x, y - other.y); }
    constexpr Coord& operator+=(const Coord& other) noexcept { x += other.x; y += other.y; return *this; }
    constexpr Coord& operator-=(const Coord& other) noexcept { x -= other.x; y -= other.y; return *this; }
    constexpr int operator*(const Coord& other) const noexcept { return x * other.x + y * other.y; }

    // 获取描述字符串
    std::string str() const noexcept { return wrap("(%2d, %2d)", x, y); }

    // 判断此坐标是否在地图范围内
    constexpr bool in_map() const noexcept {
        return 0 <= x && x < Constant::row && 0 <= y && y < Constant::col;
    }
    // 判断`target`是否在此处将领的攻击范围内
    constexpr bool in_attack_range(const Coord& target) const noexcept {
        return std::abs(x - target.x) <= Constant::GENERAL_ATTACK_RADIUS && std::abs(y - target.y) <= Constant::GENERAL_ATTACK_RADIUS;
    }
    // 判断`target`是否在此处超级武器的作用范围内
    constexpr bool in_super_weapon_range(const Coord& target) const noexcept {
        return std::abs(x - target.x) <= Constant::SUPER_WEAPON_RADIUS && std::abs(y - target.y) <= Constant::SUPER_WEAPON_RADIUS;
    }

    // 计算此位置到另一个位置的曼哈顿距离
    constexpr int dist_to(const Coord& other) const noexcept {
        return std::abs(x - other.x) + std::abs(y - other.y);
    }
    // 计算此位置到另一个位置的欧几里得距离
    constexpr double euclidean_dist(const Coord& other) const noexcept {
        return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
    }

    // 将此`Coord`对象视为二维向量，计算其与另一个`Coord`对象的夹角
    double angle_to(const Coord& other) const noexcept {
        return std::acos((double)(*this * other) / (x * x + y * y) / (other.x * other.x + other.y * other.y));
    }

};

// 将军技能类型
class SkillType {
public:
    enum __Inner_type : int8_t {
        RUSH = 0,
        STRIKE = 1,
        COMMAND = 2,
        DEFENCE = 3,
        WEAKEN = 4
    } __val;

    constexpr SkillType(__Inner_type val) noexcept : __val(val) {}
    constexpr SkillType(int val) noexcept : __val(static_cast<__Inner_type>(val)) {}
    // 允许直接比较
    constexpr operator __Inner_type() const noexcept { return __val; }
    // 不允许隐式转换为bool
    explicit operator bool() const noexcept = delete;


    // 技能冷却回合数
    constexpr int cd() const noexcept { return Constant::GENERAL_SKILL_CD[static_cast<int>(__val)]; }

    // 技能开销
    constexpr int cost() const noexcept { return Constant::GENERAL_SKILL_COST[static_cast<int>(__val)]; }

    // 技能持续回合数
    constexpr int duration() const noexcept { return Constant::GENERAL_SKILL_DURATION[static_cast<int>(__val)]; }

    // 获取描述字符串
    constexpr const char* str() const noexcept { return __str[static_cast<int>(__val)]; }

private:
    constexpr static const char* __str[5] = {"RUSH", "STRIKE", "COMMAND", "DEFENCE", "WEAKEN"};
};

// 将军属性类型
enum class QualityType {
    PRODUCTION = 0,
    DEFENCE = 1,
    MOBILITY = 2
};

// 超级武器类型
enum class WeaponType {
    NUCLEAR_BOOM = 0,
    ATTACK_ENHANCE = 1,
    TRANSMISSION = 2,
    TIME_STOP = 3
};

// 格子类型
enum class CellType {
    PLAIN = 0,
    DESERT = 1,
    SWAMP = 2,
    Type_count = 3
};

// 科技类型
enum class TechType {
    MOBILITY = 0,
    IMMUNE_SWAMP = 1,
    IMMUNE_SAND = 2,
    UNLOCK = 3
};

// 方向类型
enum Direction {
    LEFT = 0,
    RIGHT = 1,
    DOWN = 2,
    UP = 3
};
constexpr int DIRECTION_COUNT = 4;
constexpr Coord DIRECTION_ARR[DIRECTION_COUNT] = {Coord(-1, 0), Coord(1, 0), Coord(0, -1), Coord(0, 1)}; // 方向数组
Direction from_coord(const Coord& from, const Coord& to) noexcept {
    assert(to.dist_to(from) == 1);
    if (from.x < to.x) return RIGHT;
    if (from.x > to.x) return LEFT;
    if (from.y < to.y) return UP;
    if (from.y > to.y) return DOWN;
    assert(!"from == to");
    return LEFT;
}

// 将军技能结构体
struct Skill {
    SkillType type = SkillType::RUSH; // 技能类型
    int cd = 0; //冷却回合数
};

// 超级武器结构体
struct SuperWeapon {
    WeaponType type; // 武器类型
    int player; // 所属玩家
    int cd; // 冷却回合数
    int rest; // 效果剩余回合数
    Coord position; // 位置坐标
    SuperWeapon(WeaponType type, int player, int cd, int rest, Coord position) noexcept :
        type(type), player(player), cd(cd), rest(rest), position(position) {};
};

class GameState;

// 将军基类
class Generals {
public:
    int id; // 将军编号
    int player; //所属玩家
    Coord position; // 位置坐标

    int produce_level; // 生产力等级
    double defence_level; // 防御力等级
    int mobility_level; //移动力等级

    // 技能冷却回合数
    int skills_cd[Constant::GENERAL_SKILL_COUNT];

    // 技能持续回合数，注意下标0,1的元素无意义
    int skill_duration[Constant::GENERAL_SKILL_COUNT];

    // 剩余移动步数
    int rest_move;

    // 获取升级开销
    virtual int production_upgrade_cost() const noexcept = 0;
    virtual int defence_upgrade_cost() const noexcept = 0;
    virtual int movement_upgrade_cost() const noexcept = 0;

    // 提升生产力
    virtual bool production_up(GameState &gamestate, int player) noexcept = 0;
    // 提升防御力
    virtual bool defence_up(GameState &gamestate, int player) noexcept = 0;
    // 提升移动力
    virtual bool movement_up(GameState &gamestate, int player) noexcept = 0;
    Generals(int id, int player, Coord position) noexcept:
        id(id), player(player), position(position),
        produce_level(1), defence_level(1), mobility_level(1),
        skills_cd{0}, skill_duration{0}, rest_move(1) {};

    // 是否有归属
    bool is_occupied() const noexcept { return player >= 0 && player < Constant::PLAYER_COUNT; }
};

// 油井类，继承自将军基类
class OilWell final: public Generals {
public:
    int production_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::OILWELL_PRODUCTION_LEVELS; ++i)
            if (produce_level == Constant::OILWELL_PRODUCTION_VALUES[i])
                return Constant::OILWELL_PRODUCTION_COST[i];
        assert(false);
        return 0;
    }
    int defence_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::OILWELL_DEFENCE_LEVELS; ++i)
            if (defence_level == Constant::OILWELL_DEFENCE_VALUES[i])
                return Constant::OILWELL_DEFENCE_COST[i];
        assert(false);
        return 0;
    }
    int movement_upgrade_cost() const noexcept override {
        assert(false);
        return 0;
    }

    bool production_up(GameState &gamestate, int player) noexcept override;
    bool defence_up(GameState &gamestate, int player) noexcept override;
    bool movement_up(GameState &gamestate, int player) noexcept override {
        assert(false);
        return false;
    }
    OilWell(int id, int player, Coord position) noexcept : Generals(id, player, position) {
        mobility_level = 0;
    };
};

// 主将类，继承自将军基类
class MainGenerals final: public Generals {
public:
    int production_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_PRODUCTION_LEVELS; ++i)
            if (produce_level == Constant::GENERAL_PRODUCTION_VALUES[i])
                return Constant::GENERAL_PRODUCTION_COST[i] / 2;
        assert(false);
        return 0;
    }
    int defence_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_DEFENCE_LEVELS; ++i)
            if (defence_level == Constant::GENERAL_DEFENCE_VALUES[i])
                return Constant::GENERAL_DEFENCE_COST[i] / 2;
        assert(false);
        return 0;
    }
    int movement_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_MOVEMENT_LEVELS; ++i)
            if (mobility_level == Constant::GENERAL_MOVEMENT_VALUES[i])
                return Constant::GENERAL_MOVEMENT_COST[i] / 2;
        assert(false);
        return 0;
    }

    bool production_up(GameState &gamestate, int player) noexcept override;
    bool defence_up(GameState &gamestate, int player) noexcept override;
    bool movement_up(GameState &gamestate, int player) noexcept override;
    MainGenerals(int id, int player, Coord position) noexcept : Generals(id, player, position) {};
};

// 副将类，继承自将军基类
class SubGenerals final: public Generals {
public:
    int production_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_PRODUCTION_LEVELS; ++i)
            if (produce_level == Constant::GENERAL_PRODUCTION_VALUES[i])
                return Constant::GENERAL_PRODUCTION_COST[i];
        assert(false);
        return 0;
    }
    int defence_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_DEFENCE_LEVELS; ++i)
            if (defence_level == Constant::GENERAL_DEFENCE_VALUES[i])
                return Constant::GENERAL_DEFENCE_COST[i];
        assert(false);
        return 0;
    }
    int movement_upgrade_cost() const noexcept override {
        for (int i = 0; i < Constant::GENERAL_MOVEMENT_LEVELS; ++i)
            if (mobility_level == Constant::GENERAL_MOVEMENT_VALUES[i])
                return Constant::GENERAL_MOVEMENT_COST[i];
        assert(false);
        return 0;
    }

    bool production_up(GameState &gamestate, int player) noexcept override;
    bool defence_up(GameState &gamestate, int player) noexcept override;
    bool movement_up(GameState &gamestate, int player) noexcept override;
    SubGenerals(int id, int player, Coord position) noexcept : Generals(id, player, position) {};
};

// 格子类
class Cell {
public:
    // 格子的类型
    CellType type;

    // 控制格子的玩家编号
    int player;

    // 格子的位置坐标
    Coord position;

    // 格子上的将军对象指针，这与`GameState::generals`是连通的
    Generals* generals;

    // 格子里的军队数量
    int army;

    // 已激活的超级武器列表
    std::vector<SuperWeapon> weapon_activate;

    Cell() noexcept : type(CellType::PLAIN), player(-1), generals(nullptr), army(0) {};

    // 格子上是否有将领
    bool has_general() const noexcept { return generals != nullptr; }

    // 格子是否有归属
    bool is_occupied() const noexcept { return player >= 0 && player < Constant::PLAYER_COUNT; }

};

// 游戏状态类
class GameState {
public:
    int round; // 当前游戏回合数
    std::vector<Generals*> generals;
    int coin[Constant::PLAYER_COUNT]; // 每个玩家的金币数量列表，分别对应玩家1，玩家2
    std::vector<SuperWeapon> active_super_weapon;
    bool super_weapon_unlocked[Constant::PLAYER_COUNT]; // 超级武器是否解锁的列表，解锁了是true，分别对应玩家1，玩家2
    int super_weapon_cd[Constant::PLAYER_COUNT]; // 超级武器的冷却回合数列表，分别对应玩家1，玩家2
    int tech_level[Constant::PLAYER_COUNT][4]; // 科技等级列表，第一层对应玩家一，玩家二，第二层分别对应行动力，免疫沼泽，免疫流沙，超级武器
    int rest_move_step[Constant::PLAYER_COUNT];

    int next_generals_id;

    // 游戏棋盘的二维列表，每个元素是一个Cell对象，下标为[x][y]
    Cell board[Constant::col][Constant::row];

    GameState() noexcept :
        round(1), coin{0, 0},
        super_weapon_unlocked{false, false}, super_weapon_cd{-1, -1},
        tech_level{{2, 0, 0, 0}, {2, 0, 0, 0}}, rest_move_step{2, 2},
        next_generals_id(0), board{} {}

    // 便捷的取Cell方法
    Cell& operator[](const Coord& pos) noexcept {
        assert(pos.in_map());
        return board[pos.x][pos.y];
    }
    const Cell& operator[](const Coord& pos) const noexcept {
        assert(pos.in_map());
        return board[pos.x][pos.y];
    }

    // 计算某玩家的军队【从`pos`出发时】获得的攻击力加成，玩家默认为拥有此格的玩家
    double attack_multiplier(const Coord& pos, int player = std::numeric_limits<int>::min()) const noexcept;
    // 计算某玩家防御某格时获得的防御力加成，玩家默认为拥有此格的玩家
    double defence_multiplier(const Coord& pos, int player = std::numeric_limits<int>::min()) const noexcept;
    // 返回某格上某玩家的有效士兵数，负数表示敌军
    int eff_army(const Coord& pos, int player) const noexcept {
        assert(pos.in_map());
        const Cell& cell = board[pos.x][pos.y];
        if (cell.player == player) return cell.army;
        return -cell.army;
    }

    // 寻找将军id对应的格子，找不到返回`(-1,-1)`
    Coord find_general_position_by_id(int general_id) const noexcept {
        for (const Generals* gen : generals) if (gen->id == general_id) return gen->position;
        return Coord(-1, -1);
    }
    // 考虑沼泽科技，指定玩家的【军队】是否可以移动到指定位置
    bool can_soldier_step_on(const Coord& pos, int player) const noexcept {
        assert(pos.in_map());
        const Cell& cell = board[pos.x][pos.y];
        return cell.type != CellType::SWAMP || tech_level[player][static_cast<int>(TechType::IMMUNE_SWAMP)] > 0;
    }
    // 考虑沼泽科技，指定玩家的【将领】是否可以移动到指定位置
    bool can_general_step_on(const Coord& pos, int player) const noexcept {
        assert(pos.in_map());
        const Cell& cell = board[pos.x][pos.y];
        if (cell.generals != nullptr) return false;
        return cell.type != CellType::SWAMP || tech_level[player][static_cast<int>(TechType::IMMUNE_SWAMP)] > 0;
    }

    // 更新游戏回合信息
    void update_round() noexcept;
};

// ******************** GameState ********************

double GameState::attack_multiplier(const Coord& pos, int player) const noexcept {
    assert(pos.in_map());

    double attack = 1.0;
    int cell_x = pos.x, cell_y = pos.y;
    const Cell& cell = board[cell_x][cell_y];
    if (player == std::numeric_limits<int>::min()) player = cell.player;

    // 遍历cell周围至少5*5的区域，寻找里面是否有将军，他们是否使用了增益或减益技能
    for (int i = -Constant::GENERAL_ATTACK_RADIUS; i <= Constant::GENERAL_ATTACK_RADIUS; ++i) {
        for (int j = -Constant::GENERAL_ATTACK_RADIUS; j <= Constant::GENERAL_ATTACK_RADIUS; ++j) {
            int x = cell_x + i;
            int y = cell_y + j;
            if (0 <= x && x < Constant::row && 0 <= y && y < Constant::col) {
                const Cell &neighbor_cell = board[x][y];
                if (neighbor_cell.generals == nullptr) continue;

                if (neighbor_cell.player == player && neighbor_cell.generals->skill_duration[SkillType::COMMAND] > 0)
                    attack *= Constant::GENERAL_SKILL_EFFECT[SkillType::COMMAND];
                if (neighbor_cell.player != player && neighbor_cell.generals->skill_duration[SkillType::WEAKEN] > 0)
                    attack *= Constant::GENERAL_SKILL_EFFECT[SkillType::WEAKEN];
            }
        }
    }
    // 考虑gamestate中的超级武器是否被激活，（可以获取到激活的位置）该位置的军队是否会被影响
    for (const SuperWeapon &weapon : active_super_weapon) {
        if (weapon.type == WeaponType::ATTACK_ENHANCE && pos.in_super_weapon_range(weapon.position) && weapon.player == player) {
            attack *= Constant::ATTACK_ENHANCE_EFFECT;
            break;
        }
    }

    return attack;
}
double GameState::defence_multiplier(const Coord& pos, int player) const noexcept {
    assert(pos.in_map());

    double defence = 1.0;
    int cell_x = pos.x, cell_y = pos.y;
    const Cell& cell = board[cell_x][cell_y];
    if (player == std::numeric_limits<int>::min()) player = cell.player;

    // 遍历cell周围至少5*5的区域，寻找里面是否有将军，他们是否使用了增益或减益技能
    for (int i = -Constant::GENERAL_ATTACK_RADIUS; i <= Constant::GENERAL_ATTACK_RADIUS; ++i) {
        for (int j = -Constant::GENERAL_ATTACK_RADIUS; j <= Constant::GENERAL_ATTACK_RADIUS; ++j) {
            int x = cell_x + i;
            int y = cell_y + j;
            if (0 <= x && x < Constant::row && 0 <= y && y < Constant::col) {
                const Cell &neighbor_cell = board[x][y];
                if (neighbor_cell.generals == nullptr) continue;

                if (neighbor_cell.player == player && neighbor_cell.generals->skill_duration[SkillType::DEFENCE] > 0)
                    defence *= Constant::GENERAL_SKILL_EFFECT[SkillType::DEFENCE];
                if (neighbor_cell.player != player && neighbor_cell.generals->skill_duration[SkillType::WEAKEN] > 0)
                    defence *= Constant::GENERAL_SKILL_EFFECT[SkillType::WEAKEN];
            }
        }
    }
    // 考虑cell上是否有general，它的防御力是否被升级
    if (cell.generals != nullptr) defence *= cell.generals->defence_level;
    // 考虑gamestate中的超级武器是否被激活，（可以获取到激活的位置）该位置的军队是否会被影响
    for (const SuperWeapon &weapon : active_super_weapon) {
        if (weapon.type == WeaponType::ATTACK_ENHANCE && pos.in_super_weapon_range(weapon.position) && weapon.player == player) {
            defence *= Constant::ATTACK_ENHANCE_EFFECT;
            break;
        }
    }

    return defence;
}
void GameState::update_round() noexcept {
    for (int i = 0; i < Constant::row; ++i) {
        for (int j = 0; j < Constant::col; ++j) {
            Cell& cell = this->board[i][j];
            // 将军
            Generals* general = cell.generals;
            if (general != nullptr) general->rest_move = general->mobility_level;
            if (dynamic_cast<MainGenerals*>(general)) cell.army += general->produce_level;
            // 曾经的错误：没有检查副将是否有归属
            else if (dynamic_cast<SubGenerals*>(general) && general->is_occupied()) cell.army += general->produce_level;
            else if (dynamic_cast<OilWell*>(general) && general->is_occupied()) coin[general->player] += general->produce_level;

            // 流沙减兵
            if (cell.type == CellType::DESERT && cell.player != -1 && cell.army > 0) {
                if (this->tech_level[cell.player][static_cast<int>(TechType::IMMUNE_SAND)] == 0) {
                    cell.army -= 1;
                    if (cell.army == 0 && cell.generals == nullptr) cell.player = -1;
                }
            }
        }
    }
    // 每10回合增兵
    if (round % 10 == 0) for (int i = 0; i < Constant::row; ++i) for (int j = 0; j < Constant::col; ++j)
        if (board[i][j].player != -1) board[i][j].army += 1;

    // 超级武器判定
    for (auto &weapon : this->active_super_weapon) {
        if (weapon.type == WeaponType::NUCLEAR_BOOM) {
            for (int _i = std::max(0, weapon.position.x - Constant::SUPER_WEAPON_RADIUS); _i < std::min(Constant::row, weapon.position.x + Constant::SUPER_WEAPON_RADIUS); ++_i) {
                for (int _j = std::max(0, weapon.position.y - Constant::SUPER_WEAPON_RADIUS); _j < std::min(Constant::col, weapon.position.y + Constant::SUPER_WEAPON_RADIUS); ++_j) {
                    Cell& cell = board[_i][_j];
                    if (cell.army > 0) {
                        cell.army = std::max(0, cell.army - Constant::NUCLEAR_BOMB_DAMAGE);
                        if (cell.army == 0 && cell.generals == nullptr) cell.player = -1;
                    }
                }
            }
        }
    }

    for (auto &i : this->super_weapon_cd) if (i > 0) --i;

    for (auto &weapon : this->active_super_weapon) --weapon.rest;

    // cd和duration 减少
    for (Generals* gen : this->generals) {
        for (auto &i : gen->skills_cd) if (i > 0) --i;
        for (auto &i : gen->skill_duration) if (i > 0) --i;
    }

    // 移动步数恢复
    this->rest_move_step[0] = this->tech_level[0][0];
    this->rest_move_step[1] = this->tech_level[1][0];

    std::vector<SuperWeapon> filtered_super_weapon;
    for (const auto& weapon : this->active_super_weapon) {
        if (weapon.rest > 0) filtered_super_weapon.push_back(weapon);
    }
    this->active_super_weapon = filtered_super_weapon;

    ++this->round;
}

// ******************* MainGenerals、SubGenerals以及OilWell ********************

bool MainGenerals::production_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_PRODUCTION_LEVELS; ++i) {
        if (produce_level == Constant::GENERAL_PRODUCTION_VALUES[i]) {
            int cost = Constant::GENERAL_PRODUCTION_COST[i] / Constant::MAIN_GENERAL_DISCOUNT;
            if (gamestate.coin[player] < cost) return false;

            gamestate.coin[player] -= cost;
            produce_level = Constant::GENERAL_PRODUCTION_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
bool MainGenerals::defence_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_DEFENCE_LEVELS; ++i) {
        if (defence_level == Constant::GENERAL_DEFENCE_VALUES[i]) {
            int cost = Constant::GENERAL_DEFENCE_COST[i] / Constant::MAIN_GENERAL_DISCOUNT;
            if (gamestate.coin[player] < cost) return false;

            gamestate.coin[player] -= cost;
            defence_level = Constant::GENERAL_DEFENCE_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
bool MainGenerals::movement_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_MOVEMENT_LEVELS; ++i) {
        if (mobility_level == Constant::GENERAL_MOVEMENT_VALUES[i]) {
            int cost = Constant::GENERAL_MOVEMENT_COST[i] / Constant::MAIN_GENERAL_DISCOUNT;
            if (gamestate.coin[player] < cost) return false;

            gamestate.coin[player] -= cost;
            mobility_level = Constant::GENERAL_MOVEMENT_VALUES[i + 1];
            rest_move = mobility_level; // 【立即恢复移动步数（未说明的feature）】
            return true;
        }
    }
    return false;
}
bool SubGenerals::production_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_PRODUCTION_LEVELS; ++i) {
        if (produce_level == Constant::GENERAL_PRODUCTION_VALUES[i]) {
            if (gamestate.coin[player] < Constant::GENERAL_PRODUCTION_COST[i]) return false;

            gamestate.coin[player] -= Constant::GENERAL_PRODUCTION_COST[i];
            produce_level = Constant::GENERAL_PRODUCTION_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
bool SubGenerals::defence_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_DEFENCE_LEVELS; ++i) {
        if (defence_level == Constant::GENERAL_DEFENCE_VALUES[i]) {
            if (gamestate.coin[player] < Constant::GENERAL_DEFENCE_COST[i]) return false;

            gamestate.coin[player] -= Constant::GENERAL_DEFENCE_COST[i];
            defence_level = Constant::GENERAL_DEFENCE_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
bool SubGenerals::movement_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::GENERAL_MOVEMENT_LEVELS; ++i) {
        if (mobility_level == Constant::GENERAL_MOVEMENT_VALUES[i]) {
            if (gamestate.coin[player] < Constant::GENERAL_MOVEMENT_COST[i]) return false;

            gamestate.coin[player] -= Constant::GENERAL_MOVEMENT_COST[i];
            mobility_level = Constant::GENERAL_MOVEMENT_VALUES[i + 1];
            rest_move = mobility_level; // 【立即恢复移动步数（未说明的feature）】
            return true;
        }
    }
    return false;
}
bool OilWell::production_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::OILWELL_PRODUCTION_LEVELS; ++i) {
        if (produce_level == Constant::OILWELL_PRODUCTION_VALUES[i]) {
            if (gamestate.coin[player] < Constant::OILWELL_PRODUCTION_COST[i]) return false;

            gamestate.coin[player] -= Constant::OILWELL_PRODUCTION_COST[i];
            produce_level = Constant::OILWELL_PRODUCTION_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
bool OilWell::defence_up(GameState &gamestate, int player) noexcept {
    for (int i = 0; i < Constant::OILWELL_DEFENCE_LEVELS; ++i) {
        if (defence_level == Constant::OILWELL_DEFENCE_VALUES[i]) {
            if (gamestate.coin[player] < Constant::OILWELL_DEFENCE_COST[i]) return false;

            gamestate.coin[player] -= Constant::OILWELL_DEFENCE_COST[i];
            defence_level = Constant::OILWELL_DEFENCE_VALUES[i + 1];
            return true;
        }
    }
    return false;
}
