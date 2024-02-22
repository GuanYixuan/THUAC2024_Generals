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

    // 沼泽格子的相对距离（默认沼泽不通）
    double swamp_dist;

    // 是否只搜索将领可通过的路径
    bool general_path;

    // 最大搜索距离
    double max_dist;

    Path_find_config(double desert_dist, double swamp_dist = 1e9, bool general_path = true, double max_dist = 1e9) noexcept :
        desert_dist(desert_dist), swamp_dist(swamp_dist), general_path(general_path), max_dist(max_dist) {}

};

// 寻路算法类
class Dist_map {
public:
    static constexpr double MAX_DIST = 1000 * col * row;
    static constexpr double INF = 1e9;

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
    static int effect_dist(const Coord& pos, const Coord& general_pos, bool can_rush, int movement_val = PLAYER_MOVEMENT_VALUES[0]) noexcept;

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

// 单将一步杀的不同类型
struct Critical_tactic {
    int required_oil;

    bool can_rush;
    bool can_strike;
    bool can_command;
    bool can_command_and_weaken;

    std::string str() const noexcept {
        std::string ret(wrap("Tactic requiring oil %d", required_oil));
        if (can_rush) ret += ", rush";
        if (can_strike) ret += ", strike";
        if (can_command) ret += ", command";
        if (can_command_and_weaken) ret += " + weaken";
        return ret;
    }
};
// 固定的几种连招，按`required_oil`排序
constexpr Critical_tactic CRITICAL_TACTICS[] = {
    {0, false, false, false, false},
    {GENERAL_SKILL_COST[SkillType::STRIKE], false, true, false, false},
    {GENERAL_SKILL_COST[SkillType::RUSH], true, false, false, false},
    {GENERAL_SKILL_COST[SkillType::COMMAND], false, false, true, false},
    {GENERAL_SKILL_COST[SkillType::STRIKE] + GENERAL_SKILL_COST[SkillType::RUSH], true, true, false, false},
    {GENERAL_SKILL_COST[SkillType::STRIKE] + GENERAL_SKILL_COST[SkillType::COMMAND], false, true, true, false},
    {GENERAL_SKILL_COST[SkillType::RUSH] + GENERAL_SKILL_COST[SkillType::COMMAND], true, false, true, false},
    {GENERAL_SKILL_COST[SkillType::COMMAND] + GENERAL_SKILL_COST[SkillType::WEAKEN], false, false, true, true},
    {GENERAL_SKILL_COST[SkillType::STRIKE] + GENERAL_SKILL_COST[SkillType::RUSH] + GENERAL_SKILL_COST[SkillType::COMMAND], true, true, true, false},
    {GENERAL_SKILL_COST[SkillType::STRIKE] + GENERAL_SKILL_COST[SkillType::COMMAND] + GENERAL_SKILL_COST[SkillType::WEAKEN], false, true, true, true},
    {GENERAL_SKILL_COST[SkillType::RUSH] + GENERAL_SKILL_COST[SkillType::COMMAND] + GENERAL_SKILL_COST[SkillType::WEAKEN], true, false, true, true},
    {GENERAL_SKILL_COST[SkillType::STRIKE] + GENERAL_SKILL_COST[SkillType::RUSH] + GENERAL_SKILL_COST[SkillType::COMMAND] + GENERAL_SKILL_COST[SkillType::WEAKEN], true, true, true, true},
};

// 攻击搜索器
class Attack_searcher {
public:
    // 指定攻击搜索器的阵营和基于的状态
    Attack_searcher(int attacker_seat, const GameState& state) noexcept : attacker_seat(attacker_seat), state(state) {}
    // 利用攻击搜索器进行一次完整的单将攻击搜索，仅返回一个结果
    std::optional<std::vector<Operation>> search() const noexcept;
private:
    int attacker_seat;
    const GameState& state;
};

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
    // 发起行动的根据地
    const Militia_area& area;
    // 行动方案，每一个元素均为一个“从A出发，向dir方向移动”的操作
    std::vector<std::pair<Coord, Direction>> plan;

    // 需要的军队数量
    int army_used;
    // 军队汇集至集合点需要的步数
    int gather_steps;

    // 构造函数：通过`calc_gather_plan`计算出的`plan`构造
    Militia_plan(const Generals* target, const Militia_area& area, std::vector<std::pair<Coord, Direction>>&& plan, int army_used) noexcept :
        target(target), area(area), plan(std::move(plan)), army_used(army_used), gather_steps(this->plan.size()) {}
    Militia_plan(const Militia_plan&) noexcept = default;

};

// 民兵分析器
class Militia_analyzer {
public:
    // 根据地列表
    std::vector<Militia_area> areas;

    // 指定当前状态并进行分析
    Militia_analyzer(const GameState& state) noexcept;

    // 针对一个目标（油井或中立副将）搜索可行的占领方案
    std::optional<Militia_plan> search_plan(const Generals* target) const noexcept;

private:
    const GameState& state;

    mutable bool vis[Constant::col][Constant::row];
    // 对`areas`中最后一个根据地进行DFS
    void analyzer_dfs(const Coord& coord) noexcept;

    // 计算应当如何从`info`中的根据地汇集`required_army`规模的军队至`info.clostest_point`
    std::vector<std::pair<Coord, Direction>> calc_gather_plan(const Militia_dist_info& info, int required_army) const noexcept;

    struct __Queue_Node {
        Coord coord;
        int army;
        // 扩展至此格时的方向
        Direction dir;

        __Queue_Node(const Coord& coord, int army, Direction dir) noexcept : coord(coord), army(army), dir(dir) {}
        bool operator< (const __Queue_Node& other) const noexcept { return army < other.army; }
    };
};

// **************************************** 寻路部分实现 ****************************************

Dist_map::Dist_map(const GameState& board, const Coord& origin, const Path_find_config& cfg) noexcept : origin(origin), cfg(cfg), board(board) {
    // 初始化
    bool vis[col][row];
    double cell_dist[static_cast<int>(CellType::Type_count)] = {1.0, cfg.desert_dist, cfg.swamp_dist};
    std::memset(vis, 0, sizeof(vis));
    std::fill_n(reinterpret_cast<double*>(dist), col * row, INF);

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

        if (path.size() >= Constant::col * Constant::row) {
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

std::optional<std::vector<Operation>> Attack_searcher::search() const noexcept {
    static std::vector<int> army_left{};
    static std::vector<Coord> landing_points{};
    static std::vector<Operation> attack_ops{};

    int oil = state.coin[attacker_seat];
    logger.log(LOG_LEVEL_DEBUG, "Searching for attack with %d oil", oil);
    int my_mobility = state.tech_level[attacker_seat][static_cast<int>(TechType::MOBILITY)];
    const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(state.generals[1 - attacker_seat]);
    assert(enemy_general);

    Dist_map enemy_dist(state, enemy_general->position, Path_find_config(1.0, 1e9, false));

    // 对每个将领
    if (state.can_soldier_step_on(enemy_general->position, attacker_seat)) // 排除敌方主将在沼泽而走不进的情况
        for (int i = 0, siz = state.generals.size(); i < siz; ++i) {
            const Generals* general = state.generals[i];
            if (general->player != attacker_seat || dynamic_cast<const OilWell*>(general) != nullptr) continue;

            // 对每种连招
            for (const Critical_tactic& tactic : CRITICAL_TACTICS) {
                // 剩余油量检查
                if (oil < tactic.required_oil) break;

                // 基本距离检查
                int eff_dist = Dist_map::effect_dist(general->position, enemy_general->position, tactic.can_rush, my_mobility);
                if (eff_dist > 0) continue;

                // 精细距离检查
                if (tactic.can_rush && enemy_dist[general->position] <= my_mobility) continue; // 无需rush
                if (!tactic.can_rush && enemy_dist[general->position] > my_mobility) continue; // 需要rush才能到达

                // 检查技能冷却
                if (tactic.can_rush && general->cd(SkillType::RUSH)) continue;
                if (tactic.can_strike && general->cd(SkillType::STRIKE)) continue;
                if (tactic.can_command && general->cd(SkillType::COMMAND)) continue;
                if (tactic.can_command_and_weaken && general->cd(SkillType::WEAKEN)) continue;

                // 检查非rush状态下技能的覆盖范围（暂且认为将领不移动）
                if (!tactic.can_rush && (tactic.can_strike || tactic.can_command || tactic.can_command_and_weaken) &&
                    !general->position.in_attack_range(enemy_general->position)) continue;

                // 生成落地点（无rush时落地点即我方位置）
                landing_points.clear();
                if (!tactic.can_rush) landing_points.push_back(general->position);
                else for (int x = std::max(general->position.x - Constant::GENERAL_ATTACK_RADIUS, 0); x <= std::min(general->position.x + Constant::GENERAL_ATTACK_RADIUS, Constant::col - 1); ++x)
                    for (int y = std::max(general->position.y - Constant::GENERAL_ATTACK_RADIUS, 0); y <= std::min(general->position.y + Constant::GENERAL_ATTACK_RADIUS, Constant::row - 1); ++y) {
                        Coord pos(x, y);
                        if (enemy_dist[pos] > my_mobility || !state.can_general_step_on(pos, attacker_seat)) continue;
                        landing_points.push_back(pos);
                    }

                // 对每个落地点计算能否攻下
                double attack_skill_mult = (tactic.can_command ? Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::COMMAND)] : 1.0);
                double defence_skill_mult = (tactic.can_command_and_weaken ? Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::WEAKEN)] : 1.0);
                for (const Coord& landing_point : landing_points) {
                    std::vector<Coord> path{enemy_dist.path_to_origin(landing_point)};
                    if (tactic.can_rush) path.insert(path.begin(), general->position);

                    army_left.clear();
                    attack_ops.clear();
                    army_left.push_back(state.eff_army(general->position, attacker_seat));

                    bool calc_pass = true;
                    for (int j = 1, sjz = path.size(); j < sjz; ++j) { // 模拟计算途径路径每一格时的军队数（落地点也需要算）
                        const Coord& from = path[j - 1], to = path[j];
                        const Cell& dest = state[to];

                        if (dest.player == attacker_seat) army_left.push_back(army_left.back() - 1 + dest.army);
                        else {
                            double local_attack_mult = 1.0;
                            double local_defence_mult = (dest.generals ? dest.generals->defence_level : 1.0);

                            bool can_strike = tactic.can_strike && to == enemy_general->position && enemy_general->position.in_attack_range(landing_point);
                            int local_army = std::max(0, dest.army - (can_strike ? Constant::STRIKE_DAMAGE : 0));
                            if (to.in_attack_range(general->position)) {
                                local_attack_mult *= attack_skill_mult;
                                local_defence_mult *= defence_skill_mult;
                            }

                            double vs = (army_left.back() - 1) * local_attack_mult - local_army * local_defence_mult;
                            if (vs <= 0) {
                                calc_pass = false;
                                break;
                            }

                            army_left.push_back(std::ceil(vs / local_attack_mult));
                        }
                        if (j == 1 && tactic.can_rush) attack_ops.push_back(Operation::generals_skill(general->id, SkillType::RUSH, to));
                        else attack_ops.push_back(Operation::move_army(from, from_coord(from, to), army_left[j-1] - 1));
                    }
                    if (!calc_pass) continue;

                    // 可攻击，准备导出行动
                    logger.log(LOG_LEVEL_INFO, "[%s] Army left %d, path size %d", tactic.str().c_str(), army_left.back(), path.size()-1);

                    // 此时才放入各种技能
                    if (tactic.can_command) attack_ops.insert(attack_ops.begin(), Operation::generals_skill(general->id, SkillType::COMMAND));
                    if (tactic.can_command_and_weaken) attack_ops.insert(attack_ops.begin(), Operation::generals_skill(general->id, SkillType::WEAKEN));

                    // 在最后一步前加入空袭
                    if (tactic.can_strike) attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(general->id, SkillType::STRIKE, enemy_general->position));

                    return attack_ops;
                }
            }
        }
    return std::nullopt;
}

// *************************************** 民兵分析器实现 ***************************************

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

        logger.log(LOG_LEVEL_DEBUG, "Militia area at %s with area %d and max army %d", coord.str().c_str(), areas.back().area, areas.back().max_army);
    }
}

std::optional<Militia_plan> Militia_analyzer::search_plan(const Generals* target) const noexcept {
    logger.log(LOG_LEVEL_DEBUG, "Searching for plan for %s", target->position.str().c_str());
    Dist_map target_dist(state, target->position, Path_find_config(2.0)); // 以沙漠为2格计算余量

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
    int enemy_army = state[target->position].army;
    for (const Militia_dist_info& info : dist_info) {
        const Militia_area& area = *info.area;
        int army_required = enemy_army + info.dist;

        logger.log(LOG_LEVEL_DEBUG, "Searching for plan at %s with area %d and max army %d, required army %d", info.clostest_point.str().c_str(), area.area, area.max_army, army_required);
        if (area.max_army < army_required) continue; // 兵力不足

        // 计算方案：兵力汇集到最近点处
        Militia_plan plan(target, *info.area, calc_gather_plan(info, army_required), army_required);
        assert(!plan.plan.empty());

        // 再从集合点处走到目标点
        std::vector<Coord> path = target_dist.path_to_origin(info.clostest_point);
        for (int i = 1, siz = path.size(); i < siz; ++i) plan.plan.emplace_back(path[i-1], from_coord(path[i-1], path[i]));

        logger.log(LOG_LEVEL_INFO, "Plan found with %d steps: %d gathers, %d moves", plan.plan.size(), plan.gather_steps, plan.plan.size() - plan.gather_steps);
        for (const auto& op : plan.plan) logger.log(LOG_LEVEL_INFO, "\t%s->%s", op.first.str().c_str(), (op.first + DIRECTION_ARR[op.second]).str().c_str());

        return plan;
    }
    return std::nullopt;
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

std::vector<std::pair<Coord, Direction>> Militia_analyzer::calc_gather_plan(const Militia_dist_info& info, int required_army) const noexcept {
    static std::vector<std::pair<Coord, Direction>> plan;
    const Militia_area& area = *info.area;

    // 优先对士兵多的格子进行BFS直至满足要求
    int total_army = 0;
    plan.clear();
    memset(vis, 0, sizeof(vis));
    std::priority_queue<__Queue_Node> queue;
    queue.emplace(info.clostest_point, state[info.clostest_point].army - 1, static_cast<Direction>(-1));

    while (!queue.empty()) {
        __Queue_Node node = queue.top();
        const Coord& coord = node.coord;
        queue.pop();

        // 记录访问信息
        if (vis[coord.x][coord.y]) continue;
        vis[coord.x][coord.y] = true;

        // 更新统计信息并生成行动
        total_army += node.army;
        if (node.dir >= 0) plan.emplace_back(coord, dir_reverse(node.dir));

        if (total_army >= required_army) break;

        // 扩展周围格子
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
            Coord next_pos = coord + DIRECTION_ARR[dir];
            if (!next_pos.in_map() || !area[next_pos] || vis[next_pos.x][next_pos.y]) continue;
            queue.emplace(next_pos, state[next_pos].army - 1, static_cast<Direction>(dir));
        }
    }
    if (total_army < required_army) return {}; // 兵力不足

    std::reverse(plan.begin(), plan.end());
    return plan;
}
