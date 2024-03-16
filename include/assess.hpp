#pragma once

#include <queue>
#include <vector>
#include <cstring>
#include <iostream>
#include <optional>

#include "gamestate.hpp"
#include "controller.hpp"

using namespace Constant;

// 寻路配置
struct Path_find_config {
    // 沙漠格子的相对距离
    double desert_dist;

    // 是否允许经过沼泽
    bool can_walk_swamp;

    // 是否只搜索将领可通过的路径
    bool general_path;

    // 最大搜索距离
    double max_dist;

    // 自定义的额外距离
    int (*custom_dist)[Constant::row];

    Path_find_config(double desert_dist, bool can_walk_swamp = false, bool general_path = true, double max_dist = 1e9) noexcept :
        desert_dist(desert_dist), can_walk_swamp(can_walk_swamp), general_path(general_path), max_dist(max_dist), custom_dist(nullptr) {}

};

// 距离计算器
class Dist_map {
public:
    static constexpr double MAX_DIST = 1000 * col * row;

    // 搜索的原点
    const Coord origin;

    // 搜索设置
    const Path_find_config cfg;

    // 距离矩阵
    double dist[col][row];

    // 取距离矩阵值
    double operator[] (const Coord& coord) const noexcept {
        assert(coord.in_map());
        return dist[coord.x][coord.y];
    }

    // 计算从`pos`走向`origin`的下一步最佳方向，返回的是`DIRECTION_ARR`中的下标
    Direction direction_to_origin(const Coord& pos) const noexcept;

    /**
     * @brief 计算从`pos`走向`origin`的完整路径，包括`pos`和`origin`本身
     * @note 本方法断言能够从`pos`走到`origin`
     */
    std::vector<Coord> path_to_origin(const Coord& pos) const noexcept;

    // 不考虑地形地计算`pos`到某个将领坐标`general_pos`的有效距离，以0为恰好安全
    static int effect_dist(const Coord& pos, const Coord& general_pos, bool can_rush, int movement_val) noexcept;

    // 构造函数，暂且把计算也写在这里
    Dist_map(const GameState& board, const Coord& origin, const Path_find_config& cfg) noexcept;
private:
    const GameState& board;

    struct __Queue_Node {
        Coord coord;
        double dist;

        __Queue_Node(const Coord& coord, double dist) noexcept : coord(coord), dist(dist) {}
        bool operator> (const __Queue_Node& other) const noexcept { return dist > other.dist; }
    };
};

// **************************************** 攻击搜索相关声明 ****************************************

// 单将一步杀的不同类型（rush由距离直接决定，故不在此体现）
struct Base_tactic {
    // 空袭次数
    int strike_count;
    // “统率”次数
    int command_count;
    // 弱化次数
    int weaken_count;

    // 所需油量
    int required_oil;

    constexpr Base_tactic(int strike_count, int command_count, int weaken_count) noexcept :
        strike_count(strike_count), command_count(command_count), weaken_count(weaken_count),
        required_oil(strike_count * GENERAL_SKILL_COST[SkillType::STRIKE] +
                     command_count * GENERAL_SKILL_COST[SkillType::COMMAND] + weaken_count * GENERAL_SKILL_COST[SkillType::WEAKEN] +
                     spawn_count() * SPAWN_GENERAL_COST) {}

    // 需要召唤的副将数量
    constexpr int spawn_count() const noexcept {
        return std::max(std::max(strike_count, std::max(command_count, weaken_count)), 1) - 1;
    }

    // 最少需要使用的将领数量
    constexpr int general_count() const noexcept {
        return std::max(strike_count, std::max(command_count, weaken_count));
    }

    // 是否已经将技能释放完毕
    constexpr bool skill_discharged() const noexcept {
        return strike_count == 0 && command_count == 0 && weaken_count == 0;
    }

    // 单纯使用技能的开销
    constexpr int skill_cost() const noexcept {
        return strike_count * GENERAL_SKILL_COST[SkillType::STRIKE] +
               command_count * GENERAL_SKILL_COST[SkillType::COMMAND] +
               weaken_count * GENERAL_SKILL_COST[SkillType::WEAKEN];
    }
};
// 不考虑rush情形下 耗油量不超过300 的固定连招，按`required_oil`排序
constexpr Base_tactic BASE_TACTICS[] = {
    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 1, 1},
    {1, 1, 1}, {2, 0, 0}, {0, 2, 0}, {2, 1, 0}, {1, 2, 0},
    {0, 2, 1}, {2, 2, 0}, {3, 0, 0}, {1, 2, 1}, {0, 2, 2},
    {2, 2, 1}, {3, 1, 0}, {1, 2, 2}, {0, 3, 0}, {2, 2, 2},
    {1, 3, 0}, {3, 2, 0}, {4, 0, 0}, {0, 3, 1}, {2, 3, 0},
    {1, 3, 1}, {3, 3, 0}, {4, 1, 0}, {0, 3, 2}, {2, 3, 1},
    {1, 3, 2}, {3, 3, 1}, {0, 4, 0}, {4, 2, 0}, {5, 0, 0},
    {0, 3, 3}, {2, 3, 2}, {1, 4, 0}, {1, 3, 3}, {3, 3, 2},
    {0, 4, 1}, {2, 4, 0}, {4, 3, 0},
};
// 单将一步杀的不同类型（rush及其耗油在此体现）
struct Critical_tactic : public Base_tactic {
    bool can_rush;

    Critical_tactic(bool can_rush, const Base_tactic& base) noexcept :
        Base_tactic(base.strike_count, base.command_count, base.weaken_count), can_rush(can_rush) {

        required_oil += can_rush * GENERAL_SKILL_COST[SkillType::RUSH];
    }

    // 单纯使用技能的开销
    constexpr int skill_cost() const noexcept {
        return strike_count * GENERAL_SKILL_COST[SkillType::STRIKE] +
               command_count * GENERAL_SKILL_COST[SkillType::COMMAND] +
               weaken_count * GENERAL_SKILL_COST[SkillType::WEAKEN] + (can_rush ? GENERAL_SKILL_COST[SkillType::RUSH] : 0);
    }

    std::string str() const noexcept {
        std::string ret(wrap("Tactic requiring oil %d", required_oil));
        if (can_rush) ret += ", rush";
        if (strike_count) ret += ", strike";
        if (strike_count > 1) ret += wrap(" x %d", strike_count);
        if (command_count) ret += ", command";
        if (command_count > 1) ret += wrap(" x %d", command_count);
        if (weaken_count) ret += " + weaken";
        if (weaken_count > 1) ret += wrap(" x %d", weaken_count);
        return ret;
    }
};

// 威慑分析器：在不考虑地形、距离与统率等防御效果的前提下分析是否能一回合内俘获某个将领
class Deterrence_analyzer {
public:
    // 威慑者
    const Generals* attacker;
    // 被威慑者
    const Generals* target;
    // 威慑方的油量
    int attacker_oil;

    // 在当前兵数下能够建立rush威慑的最小油量
    int min_oil;
    // 在当前油量下能够建立rush威慑的最小兵力
    int min_army;
    // 当前情况下的最小耗油非rush威慑方案，可能不存在
    std::optional<Critical_tactic> non_rush_tactic;
    // 当前情况下的最小耗油rush威慑方案，可能不存在
    std::optional<Critical_tactic> rush_tactic;

    // 能对对方建立rush威慑的“对方最大兵力”
    int target_max_army;

    // 构造并进行分析，对`attacker`与`target`同阵营的情况结果可能不正确
    Deterrence_analyzer(const Generals* attacker, const Generals* target, int attacker_oil, const GameState& state) noexcept :
        attacker(attacker), target(target), attacker_oil(attacker_oil) {

        min_oil = std::numeric_limits<int>::max();
        min_army = std::numeric_limits<int>::max();
        target_max_army = 0;

        int attacker_army = state[attacker->position].army;
        int target_army = state[target->position].army;
        double def_mult = state.defence_multiplier(target->position);
        for (const Base_tactic& base : BASE_TACTICS) {
            int rael_target_army = std::max(0, target_army - STRIKE_DAMAGE * base.strike_count);
            double atk_mult = pow(GENERAL_SKILL_EFFECT[SkillType::COMMAND], base.command_count) / def_mult;
            atk_mult *= pow(GENERAL_SKILL_EFFECT[SkillType::WEAKEN], -base.weaken_count);

            // 能够俘获
            if (attacker_army * atk_mult > rael_target_army) {
                min_oil = std::min(min_oil, base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH]);
                // 检查油量并更新策略
                if (!non_rush_tactic && attacker_oil >= base.required_oil) non_rush_tactic.emplace(false, base);
                if (!rush_tactic && attacker_oil >= base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH]) rush_tactic.emplace(true, base);
            }
            // 能够负担得起
            if (attacker_oil >= base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH]) {
                min_army = std::min(min_army, (int)std::ceil(rael_target_army / atk_mult));
                target_max_army = std::max(target_max_army, (int)(attacker_army * atk_mult) + STRIKE_DAMAGE * base.strike_count);
            }
        }
    }

};

// 攻击搜索器
class Attack_searcher {
public:
    // 指定攻击搜索器的阵营和基于的状态
    Attack_searcher(int attacker_seat, const GameState& state) noexcept : attacker_seat(attacker_seat), state(state) {}
    // 利用攻击搜索器进行一次完整的单将攻击搜索，仅返回一个结果
    std::optional<std::vector<Operation>> search(int extra_oil = 0) const noexcept;

private:
    const int attacker_seat;
    const GameState& state;

    // 技能释放表中的元素
    struct Skill_discharger {
        // 释放技能的位置
        Coord pos;
        // 可借用的将领
        const Generals* general_ptr;
        // 该位置是否能够进行统率（若有将领则要求冷却已完成）
        bool can_command;
        // 该位置是否能够进行弱化或空袭（若有将领则要求冷却已完成）
        bool can_cover_enemy;

        Skill_discharger(const Coord& pos, const Generals* general_ptr, bool can_command, bool can_cover_enemy) noexcept :
            pos(pos), general_ptr(general_ptr), can_command(can_command), can_cover_enemy(can_cover_enemy) {}

        constexpr bool general_available() const noexcept { return (bool)general_ptr; }

        // 获取该位置的优先级，用于排序
        constexpr int score() const noexcept {
            return (int)can_cover_enemy + (int)can_command * 2 + (int)((bool)general_ptr) * 4;
        }
        bool operator> (const Skill_discharger& other) const noexcept { return score() > other.score(); }
    };
    // 技能释放表
    static std::vector<Skill_discharger> skill_table;

    // 汇合方式
    struct Gather_point {
        // 汇合位置
        Coord pos;
        // 走至集合点消耗的【军队】行动力
        int army_steps;

        Gather_point(const Coord& pos, int army_steps) noexcept : pos(pos), army_steps(army_steps) {}
    };
};

// **************************************** 移动搜索相关声明 ****************************************

// 移动代价配置
struct Move_cost_cfg {
    double step_cost;
    double desert_cost;
    double target_distance_cost;

    Move_cost_cfg(double step_cost, double desert_cost, double target_distance_cost) noexcept :
        step_cost(step_cost), desert_cost(desert_cost), target_distance_cost(target_distance_cost) {}
};
struct Move_plan {
    Coord destination;
    Operation_list ops;

    int step_count;
    double step_cost;
    double desert_cost;
    double target_distance_cost;

    Move_plan(Coord destination, int player) noexcept :
        destination(destination), ops(player), step_count(0), step_cost(0), desert_cost(0), target_distance_cost(0) {}

    bool operator< (const Move_plan& other) const noexcept { return ops.score < other.ops.score; }
    bool operator> (const Move_plan& other) const noexcept { return ops.score > other.ops.score; }

    const char* c_str() const noexcept {
        static char buf[256];
        std::snprintf(buf, sizeof(buf), "%s, steps=%d, step_cost=%.0f, desert_cost=%.0f, target_distance_cost=%.0f",
                      destination.str().c_str(), step_count, step_cost, desert_cost, target_distance_cost);
        return buf;
    }
};

// 主将移动搜索算法
class Maingeneral_mover {
public:
    // 最大步数
    int steps_available;
    // 移动目的地
    std::optional<Coord> target_pos;

    // 移动代价配置
    Move_cost_cfg cost_cfg;
    // 寻路配置
    Path_find_config path_cfg;

    // 构造函数，目前默认立场是我方
    Maingeneral_mover(const GameState& state, int steps_available, std::optional<Coord> target_pos) noexcept :
        state(state), steps_available(steps_available), target_pos(target_pos),
        cost_cfg(1, 2, 4),
        path_cfg(3.0, state.has_swamp_tech(my_seat), true) {}

    // 参数设置函数
    Maingeneral_mover& operator<< (const Move_cost_cfg& cfg) noexcept { cost_cfg = cfg; return *this; }

    // 进行寻路搜索
    std::vector<Move_plan> search() const noexcept;

private:
    const GameState& state;
};

// **************************************** 民兵分析器相关声明 ****************************************

// 民兵“根据地”：一块连起来的有兵区域
class Militia_area {
public:
    // 根据地的面积
    int area;
    // 根据地能够汇集的最大军队数量
    int max_army;
    // 区域遮罩
    bool mask[Constant::col][Constant::row];

    // 取值函数
    bool& operator[] (const Coord& coord) noexcept {
        assert(coord.in_map());
        return mask[coord.x][coord.y];
    }
    bool operator[] (const Coord& coord) const noexcept {
        assert(coord.in_map());
        return mask[coord.x][coord.y];
    }
    bool* operator[] (int x) noexcept { return mask[x]; }

    Militia_area() noexcept : area(0), max_army(0), mask{0} {}
};

struct Militia_dist_info {
    // 最小距离
    int dist;
    // 最近点（集合点）
    Coord clostest_point;
    // 对应的根据地
    const Militia_area* area;

    Militia_dist_info(int dist, const Coord& clostest_point, const Militia_area* area) noexcept :
        dist(dist), clostest_point(clostest_point), area(area) {}

    bool operator< (const Militia_dist_info& other) const noexcept {
        if (dist != other.dist) return dist < other.dist;
        return area->max_army < other.area->max_army; // 优先选择军队数量最小的根据地
    }
};

// 一次民兵行动
class Militia_plan {
public:
    // 目标
    const Generals* target;
    // 目标位置
    Coord target_pos;

    // 发起行动的根据地，如果是从主将上分兵则为nullptr
    const Militia_area* area;
    // 行动方案，每一个元素均为一个“从A出发，向dir方向移动”的操作
    std::vector<std::pair<Coord, Direction>> plan;

    // 需要的军队数量
    int army_used;
    // 军队汇集至集合点需要的步数
    int gather_steps;

    // 构造函数：通过`calc_gather_plan`计算出结果构造
    Militia_plan(const Generals* target, const Militia_area* area, const std::vector<std::pair<Coord, Direction>>& plan, int army_used) noexcept :
        target(target), target_pos(target->position), area(area), plan(plan), army_used(army_used), gather_steps(this->plan.size()) {}
};

// 民兵分析器
class Militia_analyzer {
public:
    // 根据地列表
    std::vector<Militia_area> areas;

    // 指定当前状态并进行分析
    Militia_analyzer(const GameState& state) noexcept;

    /**
     * @brief 针对一个目标搜索可行的占领方案或支援方案，其兵力来自民兵
     * @param target 需要占领或支援的目标
     * @param max_support_steps 支援的最大步数，仅在`target`为己方将领时有效
     * @return std::optional<Militia_plan> 搜索出的方案，如果不存在则返回空
     */
    std::optional<Militia_plan> search_plan_from_militia(const Generals* target, int max_support_steps = -1) const noexcept;

    // 针对一个目标（油井或中立副将）搜索可行的占领方案，其兵力从指定`provider`中获取
    std::optional<Militia_plan> search_plan_from_provider(const Generals* target, const Generals* provider) const noexcept;

private:
    const GameState& state;

    mutable bool vis[Constant::col][Constant::row];
    // 对`areas`中最后一个根据地进行DFS
    void analyzer_dfs(const Coord& coord) noexcept;

    /**
     * @brief 计算应当如何从`info`中的根据地汇集军队至`info.clostest_point`
     * @param info 指定的根据地及其距离信息
     * @param required_army 需要的军队数量
     * @param max_steps 最大步数，若填写此参数则忽略`required_army`
     * @return std::pair<int, std::vector<std::pair<Coord, Direction>>> 第一个元素为实际使用士兵数，第二个元素为行动方案
     */
    std::pair<int, std::vector<std::pair<Coord, Direction>>> calc_gather_plan(const Militia_dist_info& info, int required_army, int max_steps = -1) const noexcept;

    struct __Queue_Node {
        Coord coord;
        int army;
        // 扩展至此格时的方向
        Direction dir;

        __Queue_Node(const Coord& coord, int army, Direction dir) noexcept : coord(coord), army(army), dir(dir) {}
        bool operator< (const __Queue_Node& other) const noexcept { return army < other.army; }
    };
};

// **************************************** 距离计算器实现 ****************************************

Dist_map::Dist_map(const GameState& board, const Coord& origin, const Path_find_config& cfg) noexcept : origin(origin), cfg(cfg), board(board) {
    // 初始化
    bool vis[col][row];
    double cell_dist[static_cast<int>(CellType::Type_count)] = {1.0, cfg.desert_dist, cfg.can_walk_swamp ? 1.0 : 1e9};
    std::memset(vis, 0, sizeof(vis));
    std::fill_n(reinterpret_cast<double*>(dist), col * row, MAX_DIST + 1);

    // 单源最短路
    std::priority_queue<__Queue_Node, std::vector<__Queue_Node>, std::greater<__Queue_Node>> queue;
    queue.emplace(origin, 0);
    while (!queue.empty()) {
        __Queue_Node node = queue.top();
        const Coord& curr_pos = node.coord;
        queue.pop();

        if (node.dist > cfg.max_dist) break;
        if (vis[curr_pos.x][curr_pos.y]) continue;
        vis[curr_pos.x][curr_pos.y] = true;
        dist[curr_pos.x][curr_pos.y] = node.dist;

        // 不能走的格子不允许扩展
        if (cfg.general_path && board[curr_pos].generals != nullptr && curr_pos != origin) continue; // 将领所在格

        // 扩展
        for (const Coord& dir : DIRECTION_ARR) {
            Coord next_pos = curr_pos + dir;
            if (!next_pos.in_map() || vis[next_pos.x][next_pos.y]) continue;

            double next_dist = node.dist + cell_dist[static_cast<int>(board[next_pos].type)];
            if (cfg.custom_dist) next_dist += cfg.custom_dist[next_pos.x][next_pos.y];
            if (next_dist < dist[next_pos.x][next_pos.y]) queue.emplace(next_pos, next_dist);
        }
    }
}

Direction Dist_map::direction_to_origin(const Coord& pos) const noexcept {
    assert(pos.in_map());

    int min_dir = -1;
    double min_dist = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 4; ++i) {
        Coord next_pos = pos + DIRECTION_ARR[i];
        if (!next_pos.in_map()) continue;
        if (cfg.general_path && next_pos != origin && board[next_pos].generals != nullptr) continue;

        double next_dist = dist[next_pos.x][next_pos.y] + board[next_pos].army * 1e-6 * ((board[next_pos].player != my_seat) ? 1 : -1);
        if (next_dist < min_dist) {
            min_dist = next_dist;
            min_dir = i;
        }
    }
    assert(min_dist <= MAX_DIST);
    return static_cast<Direction>(min_dir);
}
std::vector<Coord> Dist_map::path_to_origin(const Coord& pos) const noexcept {
    assert(pos.in_map());
    assert(dist[pos.x][pos.y] <= MAX_DIST);

    std::vector<Coord> path{pos};
    for (Coord curr_pos = pos; curr_pos != origin; ) {
        Direction dir = direction_to_origin(curr_pos);
        curr_pos += DIRECTION_ARR[dir];
        path.push_back(curr_pos);

        if (path.size() >= 50) {
            logger.log(LOG_LEVEL_ERROR, "path_to_origin: path too long");
            for (const Coord& coord : path) logger.log(LOG_LEVEL_ERROR, "\t%s", coord.str().c_str());
            assert(false);
        }
    }
    return path;
}

int Dist_map::effect_dist(const Coord& pos, const Coord& general_pos, bool can_rush, int movement_val) noexcept {
    assert(pos.in_map() && general_pos.in_map());

    // 若不考虑技能，则行动力为movement_val
    if (!can_rush) return pos.dist_to(general_pos) - movement_val - 1;

    // 考虑技能
    int dx = std::max(std::abs(pos.x - general_pos.x) - GENERAL_ATTACK_RADIUS, 0);
    int dy = std::max(std::abs(pos.y - general_pos.y) - GENERAL_ATTACK_RADIUS, 0);
    if (dx == 0 && dy == 0) return std::min(std::abs(pos.x - general_pos.x), std::abs(pos.y - general_pos.y)) - movement_val - 3;
    return dx + dy - movement_val - 1;
}

// **************************************** 攻击搜索器实现 ****************************************

std::vector<Attack_searcher::Skill_discharger> Attack_searcher::skill_table = {};

std::optional<std::vector<Operation>> Attack_searcher::search(int extra_oil) const noexcept {
    static std::vector<int> army_left{};
    static std::vector<Coord> landing_points{};
    static std::vector<Operation> attack_ops{};

    // 参数初始化
    int oil = state.coin[attacker_seat] + extra_oil;
    bool enemy_extra_army = (attacker_seat != my_seat && attacker_seat == 0);
    int attacker_mobility = state.tech_level[attacker_seat][static_cast<int>(TechType::MOBILITY)];
    const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(state.generals[1 - attacker_seat]);
    if (!state.can_soldier_step_on(enemy_general->position, attacker_seat)) return std::nullopt; // 排除敌方主将在沼泽而走不进的情况

    Dist_map enemy_dist(state, enemy_general->position, Path_find_config(1.0, state.has_swamp_tech(attacker_seat), false));

    // 初筛技能范围
    static std::vector<Base_tactic> avail_base_tactics{};
    avail_base_tactics.clear();
    for (const Base_tactic& base_tactic : BASE_TACTICS) {
        // 基本油量检查（不考虑rush和将领召唤开销）
        if (oil < base_tactic.skill_cost()) continue;
        avail_base_tactics.push_back(base_tactic);
    }

    // 统计信息
    int atks_till_landing = 0;
    int atks_till_check_discharger = 0;

    // 对每个将领
    for (int i = 0, siz = state.generals.size(); i < siz; ++i) {
        const Generals* general = state.generals[i];
        if (general->player != attacker_seat || dynamic_cast<const OilWell*>(general) != nullptr) continue;
        if (state[general->position].army <= 1) continue;

        // 搜索汇合点
        static std::vector<Gather_point> gather_points{};
        Dist_map attacker_dist(state, general->position, Path_find_config(1.0, state.has_swamp_tech(attacker_seat)));
        gather_points.clear();

        // 不携带军队的汇合点
        for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
            Coord pos{x, y};
            if (pos != general->position &&
                (attacker_dist[pos] > general->mobility_level || state[pos].player != attacker_seat || !state.can_general_step_on(pos, attacker_seat))) continue;
            // 其实需要检查整条路径
            gather_points.emplace_back(pos, 0);
        }
        // 带军队的汇合点，目前限制在主将附近4格
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
            Coord pos = general->position + DIRECTION_ARR[dir];
            if (!pos.in_map() || !state.can_general_step_on(pos, attacker_seat)) continue;
            gather_points.emplace_back(pos, 1);
        }

        // 对每个汇合点以及每种连招
        for (const Gather_point& gather : gather_points)
        for (const Base_tactic& base_tactic : avail_base_tactics) {
            Coord gather_point = gather.pos;
            int gather_point_army = state[gather_point].army;
            int remain_move = attacker_mobility - gather.army_steps;
            if (gather.army_steps) gather_point_army += state[general->position].army - 1; // 加上主将的军队数量

            // 基本油量检查已经提前完成
            // 根据精细距离决定是否需要rush
            Critical_tactic tactic(enemy_dist[gather_point] > remain_move, base_tactic);
            int skill_cost = tactic.skill_cost();
            if (oil < skill_cost) continue; // 根据rush信息再次检查油量
            if (tactic.can_rush && gather_point_army <= 1) continue; // gather_point我方军队数量不足，无法rush

            // 距离检查
            int eff_dist = Dist_map::effect_dist(gather_point, enemy_general->position, tactic.can_rush, remain_move);
            if (eff_dist >= 0) continue;

            atks_till_landing++;
            // 生成落地点（无rush时落地点即我方位置）
            landing_points.clear();
            if (!tactic.can_rush) landing_points.push_back(gather_point);
            else for (int x = std::max(gather_point.x - Constant::GENERAL_ATTACK_RADIUS, 0); x <= std::min(gather_point.x + Constant::GENERAL_ATTACK_RADIUS, Constant::col - 1); ++x)
                for (int y = std::max(gather_point.y - Constant::GENERAL_ATTACK_RADIUS, 0); y <= std::min(gather_point.y + Constant::GENERAL_ATTACK_RADIUS, Constant::row - 1); ++y) {
                    Coord pos(x, y);
                    if (enemy_dist[pos] > remain_move || !state.can_general_step_on(pos, attacker_seat)) continue;
                    landing_points.push_back(pos);
                }

            // 对每个落地点计算能否攻下
            for (const Coord& landing_point : landing_points) {
                std::vector<Coord> path{enemy_dist.path_to_origin(landing_point)};
                if (tactic.can_rush) path.insert(path.begin(), gather_point);
                if (gather.army_steps) path.insert(path.begin(), general->position); // 需要将军队移动到汇合点

                army_left.clear();
                attack_ops.clear();
                army_left.push_back(state[path.front()].army + enemy_extra_army * general->produce_level);

                // 模拟计算途径路径每一格时的军队数（落地点也需要算）
                bool calc_pass = true;
                for (int j = 1, sjz = path.size(); j < sjz; ++j) {
                    const Coord& from = path[j - 1], to = path[j];
                    const Cell& dest = state[to];
                    bool final_cell = (j == sjz - 1);

                    if (dest.player == attacker_seat) army_left.push_back(army_left.back() - 1 + dest.army);
                    else {
                        double local_attack_mult = state.attack_multiplier(from, attacker_seat);
                        double local_defence_mult = state.defence_multiplier(to);

                        // 假定所有技能仅对敌方主将格有效（这样可以放宽各个将领位置的限制）
                        int local_army = dest.army;
                        if (final_cell) {
                            local_attack_mult *= pow(Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::COMMAND)], tactic.command_count);
                            local_defence_mult *= pow(Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::WEAKEN)], tactic.weaken_count);
                            local_army -= tactic.strike_count * Constant::STRIKE_DAMAGE;
                            local_army = std::max(0, local_army);
                        }

                        double vs = (army_left.back() - 1) * local_attack_mult - local_army * local_defence_mult;
                        if (vs <= 0) {
                            calc_pass = false;
                            break;
                        }

                        army_left.push_back(std::ceil(vs / local_attack_mult));
                    }
                    // 创建操作对象（包括rush技能）
                    if (to == landing_point && tactic.can_rush) attack_ops.push_back(Operation::generals_skill(general->id, SkillType::RUSH, to));
                    else if (army_left[j-1] - 1 > 0) attack_ops.push_back(Operation::move_army(from, from_coord(from, to), army_left[j-1] - 1)); // 有兵才移动
                }
                if (!calc_pass) continue;
                atks_till_check_discharger++;

                // 补充移动到汇合点的操作
                if (gather_point != general->position) {
                    if (!gather.army_steps) attack_ops.insert(attack_ops.begin(), Operation::move_generals(general->id, gather_point));
                    else {
                        auto gather_point_it = std::find_if(attack_ops.begin(), attack_ops.end(),
                                                            [&](const Operation& op) {
                                                                return op.opcode == OperationType::MOVE_ARMY && Coord(op.operand[0], op.operand[1]) + DIRECTION_ARR[op.operand[2] - 1] == gather_point;
                                                            }) + 1;
                        assert(gather_point_it != attack_ops.end());
                        attack_ops.insert(gather_point_it, Operation::move_generals(general->id, gather_point));
                    }
                }

                // 最后需要确定能够找到用于释放技能的将领，并重新核算费用
                if (attacker_seat == my_seat || true) {
                    logger.log(LOG_LEVEL_DEBUG, "\t[%s] skill_cost = %d", tactic.str().c_str(), skill_cost);
                    logger.log(LOG_LEVEL_DEBUG, "\t\tGather at %s (army_steps = %d), Landing at %s, army_left = %d",
                               gather_point.str().c_str(), gather.army_steps, landing_point.str().c_str(), army_left.back());
                }

                // 重新计算技能释放表（考虑将领位置）
                skill_table.clear();
                Coord atk_pos = path[path.size() - 2]; // 统率位置
                int x_min = std::max(atk_pos.x - (GENERAL_ATTACK_RADIUS + 1), 0);
                int x_max = std::min(atk_pos.x + (GENERAL_ATTACK_RADIUS + 1), Constant::col - 1);
                int y_min = std::max(atk_pos.y - (GENERAL_ATTACK_RADIUS + 1), 0);
                int y_max = std::min(atk_pos.y + (GENERAL_ATTACK_RADIUS + 1), Constant::row - 1);
                for (int x = x_min; x <= x_max; ++x) for (int y = y_min; y <= y_max; ++y) {
                    Coord pos{x, y};
                    bool can_command = pos.in_attack_range(atk_pos);
                    bool can_cover_enemy = pos.in_attack_range(enemy_general->position);
                    if (!can_command && !can_cover_enemy) continue; // 啥都不能干的格子

                    // 确认格子上的将领
                    // landing_point处会出现general，而general原位置的general需要忽略
                    const Generals* cell_general = state[pos].generals;
                    if (pos == landing_point) cell_general = general;
                    else if (pos == general->position) cell_general = nullptr;

                    if (cell_general && dynamic_cast<const OilWell*>(cell_general)) continue; // 油井无法利用
                    if (cell_general) { // 有将领（主将或副将）
                        // 阵营检查
                        if (cell_general->player != attacker_seat) continue;
                        // 冷却检查
                        if (can_command && cell_general->cd(SkillType::COMMAND)) continue;
                        if (can_cover_enemy && (cell_general->cd(SkillType::WEAKEN) || cell_general->cd(SkillType::STRIKE))) continue;
                        skill_table.emplace_back(pos, cell_general, can_command, can_cover_enemy);
                    }
                    // 无将领
                    else {
                        // 在(landing_point, attack_pos]范围中的格子可跳过阵营检查
                        bool bypass_team = std::find(path.begin() + (tactic.can_rush ? 2 : 1), path.end() - 1, pos) != (path.end() - 1);
                        // 阵营检查（隐含了地形）
                        if (state[pos].player != attacker_seat && !bypass_team) continue;
                        skill_table.emplace_back(pos, nullptr, can_command, can_cover_enemy);
                    }
                }

                // 排序技能释放表
                std::sort(skill_table.begin(), skill_table.end(), std::greater<Skill_discharger>());
                if (attacker_seat == my_seat) {
                    logger.log(LOG_LEVEL_DEBUG, "\t\tDischargers:");
                    for (const Skill_discharger& discharger : skill_table) {
                        logger.log(LOG_LEVEL_DEBUG, "\t\t\t%s, general_available = %d, can_command = %d, can_cover_enemy = %d",
                                   discharger.pos.str().c_str(), discharger.general_available(), discharger.can_command, discharger.can_cover_enemy);
                    }
                }

                // 暂时先不考虑路径上的格子（以及落点处不能召唤副将）
                int spawn_count = 0;
                int next_general_id = state.next_generals_id;
                Base_tactic remain_skills = base_tactic;
                for (const Skill_discharger& discharger : skill_table) {
                    int skill_count = 0;
                    int general_id = discharger.general_available() ? discharger.general_ptr->id : next_general_id;

                    // 尝试匹配
                    if (discharger.can_command && remain_skills.command_count) {
                        skill_count++;
                        remain_skills.command_count--;
                        // 插入到倒数第二个位置
                        attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(general_id, SkillType::COMMAND));
                    }
                    if (discharger.can_cover_enemy) {
                        if (remain_skills.weaken_count) {
                            skill_count++;
                            remain_skills.weaken_count--;
                            attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(general_id, SkillType::WEAKEN));
                        }
                        if (remain_skills.strike_count) {
                            skill_count++;
                            remain_skills.strike_count--;
                            attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(general_id, SkillType::STRIKE, enemy_general->position));
                        }
                    }
                    // 假如要召唤将领
                    if (skill_count && general_id == next_general_id) {
                        spawn_count++;
                        next_general_id++;
                        attack_ops.insert(attack_ops.begin() + (attack_ops.size() - skill_count - 1), Operation::recruit_generals(discharger.pos));
                    }
                    if (remain_skills.skill_discharged()) break; // 所有技能都已释放完毕
                }
                // 检查技能是否全部释放完毕以及费用是否足够
                if (!remain_skills.skill_discharged() || skill_cost + spawn_count * SPAWN_GENERAL_COST > oil) continue;

                // 可攻击，导出行动
                if (attacker_seat == my_seat)
                    logger.log(LOG_LEVEL_INFO, "\t\t\tComfirmed:[%s] Army left %d, path size %d", tactic.str().c_str(), army_left.back(), path.size()-1);
                return attack_ops;
            }
        }
    }
    if (atks_till_landing > 100)
        logger.log(LOG_LEVEL_INFO, "No attack plan found, till_landing = %d, till_check_discharger = %d", atks_till_landing, atks_till_check_discharger);
    return std::nullopt;
}

// **************************************** 移动搜索实现 ****************************************

std::vector<Move_plan> Maingeneral_mover::search() const noexcept {
    static std::vector<int> army_left{};

    // 参数初始化
    std::vector<Move_plan> ret;
    int extra_oil = state.calc_oil_production(1-my_seat) * 2; // 额外的油量
    const MainGenerals* my_general = dynamic_cast<const MainGenerals*>(state.generals[my_seat]);
    std::optional<Dist_map> target_dist = target_pos ? std::make_optional<Dist_map>(state, *target_pos, path_cfg) : std::nullopt;

    // 计算主将的可行走范围
    static std::vector<Coord> avail_terminals{};
    Dist_map general_dist(state, my_general->position, Path_find_config(1.0, state.has_swamp_tech(my_seat)));

    avail_terminals.clear();
    for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
        Coord pos{x, y};
        if (pos != my_general->position && (general_dist[pos] > steps_available || !state.can_general_step_on(pos, my_seat))) continue;
        avail_terminals.push_back(pos);
    }

    // 对范围内的每个格子单独考虑
    for (const Coord& terminal : avail_terminals) {
        std::vector<Coord> path{general_dist.path_to_origin(terminal)};
        std::reverse(path.begin(), path.end());

        army_left.clear();
        army_left.push_back(state[my_general->position].army);

        Move_plan move_plan(terminal, my_seat);

        // 模拟计算途径路径每一格时的军队数（落地点也需要算）
        bool calc_pass = true;
        for (int j = 1, sjz = path.size(); j < sjz; ++j) {
            const Coord& from = path[j - 1], to = path[j];
            const Cell& dest = state[to];

            if (dest.player == my_seat) army_left.push_back(army_left.back() - 1 + dest.army);
            else {
                double local_attack_mult = state.attack_multiplier(from, my_seat);
                double local_defence_mult = state.defence_multiplier(to);

                // 假定所有技能仅对敌方主将格有效（这样可以放宽各个将领位置的限制）
                int local_army = dest.army;
                double vs = (army_left.back() - 1) * local_attack_mult - local_army * local_defence_mult;
                if (vs <= 0) {
                    calc_pass = false;
                    break;
                }

                army_left.push_back(std::ceil(vs / local_attack_mult));
            }
            // 创建操作对象
            if (army_left[j-1] - 1 > 0) move_plan.ops += Operation::move_army(from, from_coord(from, to), army_left[j-1] - 1); // 有兵才移动
        }
        if (!calc_pass) continue;

        // 把主将移动也加上
        if (terminal != my_general->position)
            move_plan.ops += Operation::move_generals(my_general->id, terminal);

        // 然后开始计算安全性
        GameState temp_state;
        temp_state.copy_as(state);
        bool exec_pass = execute_operations(temp_state, move_plan.ops);
        if (!exec_pass) {
            logger.log(LOG_LEVEL_ERROR, "\t\tMove plan execution failed, ops:");
            for (const Operation& op : move_plan.ops) logger.log(LOG_LEVEL_ERROR, "\t\t\t%s", op.str().c_str());
            continue;
        }

        Attack_searcher searcher(1-my_seat, temp_state);
        if (searcher.search(extra_oil)) continue;// 会被攻击则舍弃

        // 否则计算各类cost
        move_plan.step_count = path.size() - 1;
        move_plan.desert_cost = state[terminal].type == CellType::DESERT ? cost_cfg.desert_cost : 0;
        move_plan.step_cost = (path.size() - 1) * cost_cfg.step_cost;
        if (target_dist) move_plan.target_distance_cost = (*target_dist)[terminal] * cost_cfg.target_distance_cost;
        move_plan.ops.score = - (move_plan.desert_cost + move_plan.step_cost + move_plan.target_distance_cost);

        logger.log(LOG_LEVEL_DEBUG, "\t\t[Search] New move plan: %s", move_plan.c_str());
        ret.push_back(move_plan);
    }

    // 排序并输出
    std::sort(ret.begin(), ret.end(), std::greater<Move_plan>());
    return ret;
}

// **************************************** 民兵分析器实现 ****************************************

Militia_analyzer::Militia_analyzer(const GameState& state) noexcept : state(state) {
    // 寻找所有根据地
    memset(vis, 0, sizeof(vis));
    for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
        Coord coord(x, y);
        if (vis[x][y] || state[coord].player != my_seat) continue;

        const Generals* generals = state[coord].generals;
        if (generals != nullptr && dynamic_cast<const MainGenerals*>(generals)) continue; // 不动主将

        areas.emplace_back();
        analyzer_dfs(coord);
    }
}

std::optional<Militia_plan> Militia_analyzer::search_plan_from_militia(const Generals* target, int max_support_steps) const noexcept {
    assert(target);

    bool support_mode = target->player == my_seat;
    assert(!support_mode || max_support_steps > 0);

    // 将敌军数考虑到距离中
    static int extra_dist[Constant::col][Constant::row];
    for (int x = 0; x < Constant::col; ++x)
        for (int y = 0; y < Constant::row; ++y)
            extra_dist[x][y] = std::max(0, -state.eff_army(Coord(x, y), my_seat));

    Path_find_config dist_cfg(2.0);
    dist_cfg.custom_dist = extra_dist;
    Dist_map target_dist(state, target->position, dist_cfg); // 以沙漠为2格计算余量

    // 将根据地从近到远排序
    std::vector<Militia_dist_info> dist_info;
    for (const Militia_area& area : areas) {
        // 对每一个根据地计算最近点
        Coord clostest_point;
        int min_dist = std::numeric_limits<int>::max();
        for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
            Coord coord(x, y);
            if (area[coord] && target_dist[coord] < min_dist) {
                clostest_point = coord;
                min_dist = target_dist[coord];
            }
        }

        if (min_dist >= Dist_map::MAX_DIST) continue;
        dist_info.emplace_back(min_dist, clostest_point, &area);
    }
    std::sort(dist_info.begin(), dist_info.end());

    // 开始寻找方案
    int enemy_army = support_mode ? 0 : state[target->position].army;
    if (target->player == 1-my_seat) enemy_army += 3; // 额外余量

    for (const Militia_dist_info& info : dist_info) {
        const Militia_area& area = *info.area;
        int army_required = enemy_army + info.dist;

        if (area.max_army < army_required) continue; // 兵力不足

        // 计算方案：兵力汇集到最近点处
        const auto& gather_plan = calc_gather_plan(info, army_required, support_mode ? max_support_steps : -1);
        Militia_plan plan(target, info.area, gather_plan.second, gather_plan.first);

        // 再从集合点处走到目标点
        std::vector<Coord> path = target_dist.path_to_origin(info.clostest_point);
        for (int i = 1, siz = path.size(); i < siz; ++i) plan.plan.emplace_back(path[i-1], from_coord(path[i-1], path[i]));

        return plan;
    }
    return std::nullopt;
}

std::optional<Militia_plan> Militia_analyzer::search_plan_from_provider(const Generals* target, const Generals* provider) const noexcept {
    assert(target && provider);

    // 将敌军数考虑到距离中
    static int extra_dist[Constant::col][Constant::row];
    for (int x = 0; x < Constant::col; ++x)
        for (int y = 0; y < Constant::row; ++y)
            extra_dist[x][y] = std::max(0, -state.eff_army(Coord(x, y), my_seat));

    Path_find_config dist_cfg(2.0, state.has_swamp_tech(provider->player));
    dist_cfg.custom_dist = extra_dist;
    Dist_map target_dist(state, target->position, dist_cfg); // 以沙漠为2格计算余量

    // 开始寻找方案，集合点就是`provider`的位置
    int enemy_army = state[target->position].army;
    if (target->player != -1) enemy_army += 3; // 额外余量

    int army_required = enemy_army + target_dist[provider->position];
    if (state[provider->position].army - 1 < army_required) return std::nullopt; // 兵力不足

    // 此时不需要集合
    Militia_plan plan(target, nullptr, {}, army_required);

    // 从`provider`处走到目标点
    std::vector<Coord> path = target_dist.path_to_origin(provider->position);
    for (int i = 1, siz = path.size(); i < siz; ++i) plan.plan.emplace_back(path[i-1], from_coord(path[i-1], path[i]));

    return plan;
}

void Militia_analyzer::analyzer_dfs(const Coord& coord) noexcept {
    vis[coord.x][coord.y] = true;
    areas.back().area++;
    areas.back()[coord] = true;
    areas.back().max_army += state[coord].army - 1;

    for (const Coord& dir : DIRECTION_ARR) {
        Coord next_pos = coord + dir;
        if (!next_pos.in_map() || vis[next_pos.x][next_pos.y]) continue;
        if (state[next_pos].player != my_seat) continue;

        const Generals* generals = state[next_pos].generals;
        if (generals != nullptr && dynamic_cast<const MainGenerals*>(generals)) continue; // 不动主将

        analyzer_dfs(next_pos);
    }
}

std::pair<int, std::vector<std::pair<Coord, Direction>>> Militia_analyzer::calc_gather_plan(const Militia_dist_info& info, int required_army, int max_steps) const noexcept {
    static std::vector<std::pair<Coord, Direction>> plan;
    const Militia_area& area = *info.area;
    bool step_mode = max_steps >= 0;

    // 优先对士兵多的格子进行BFS直至满足要求
    int total_army = 0;
    plan.clear();
    memset(vis, 0, sizeof(vis));
    std::priority_queue<__Queue_Node> queue;
    queue.emplace(info.clostest_point, state[info.clostest_point].army - 1, static_cast<Direction>(-1));

    while (!queue.empty() && (!step_mode || (int)plan.size() < max_steps)) {
        __Queue_Node node = queue.top();
        const Coord& coord = node.coord;
        queue.pop();

        // 记录访问信息
        if (vis[coord.x][coord.y]) continue;
        vis[coord.x][coord.y] = true;

        // 更新统计信息并生成行动
        total_army += node.army;
        if (node.dir >= 0) plan.emplace_back(coord, dir_reverse(node.dir));

        if (!step_mode && total_army >= required_army) break;

        // 扩展周围格子
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
            Coord next_pos = coord + DIRECTION_ARR[dir];
            if (!next_pos.in_map() || !area[next_pos] || vis[next_pos.x][next_pos.y]) continue;
            queue.emplace(next_pos, state[next_pos].army - 1, static_cast<Direction>(dir));
        }
    }
    assert(step_mode || total_army >= required_army);

    std::reverse(plan.begin(), plan.end());
    return std::make_pair(total_army, plan);
}
