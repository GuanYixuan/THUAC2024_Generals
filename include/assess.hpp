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

// 寻路算法类
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
};
// 不考虑rush情形下 耗油量不超过250且召唤副将数不超过2 的固定连招，按`required_oil`排序
constexpr Base_tactic BASE_TACTICS[] = {
    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 1, 1},
    {1, 1, 1}, {2, 0, 0}, {0, 2, 0}, {2, 1, 0}, {1, 2, 0},
    {0, 2, 1}, {2, 2, 0}, {3, 0, 0}, {1, 2, 1}, {0, 2, 2},
    {2, 2, 1}, {3, 1, 0}, {1, 2, 2}, {0, 3, 0}, {2, 2, 2},
    {1, 3, 0}, {3, 2, 0}, {0, 3, 1}, {2, 3, 0}, {1, 3, 1},
    {3, 3, 0}, {0, 3, 2}, {2, 3, 1},
};
// 单将一步杀的不同类型（rush及其耗油在此体现）
struct Critical_tactic : public Base_tactic {
    bool can_rush;

    Critical_tactic(bool can_rush, const Base_tactic& base) noexcept :
        Base_tactic(base.strike_count, base.command_count, base.weaken_count), can_rush(can_rush) {

        required_oil += can_rush * GENERAL_SKILL_COST[SkillType::RUSH];
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

    // 构造并进行分析，对`attacker`与`target`同阵营的情况结果可能不正确
    Deterrence_analyzer(const Generals* attacker, const Generals* target, int attacker_oil, const GameState& state) noexcept :
        attacker(attacker), target(target), attacker_oil(attacker_oil) {

        min_oil = std::numeric_limits<int>::max();
        min_army = std::numeric_limits<int>::max();

        int attacker_army = state[attacker->position].army;
        int target_army = state[target->position].army;
        double def_mult = state.defence_multiplier(target->position);
        for (const Base_tactic& base : BASE_TACTICS) {
            double atk_mult = pow(GENERAL_SKILL_EFFECT[SkillType::COMMAND], base.command_count) / def_mult;
            atk_mult *= pow(GENERAL_SKILL_EFFECT[SkillType::WEAKEN], -base.weaken_count);

            // 能够俘获
            if (attacker_army * atk_mult > target_army) {
                min_oil = std::min(min_oil, base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH]);
                // 检查油量并更新策略
                if (!non_rush_tactic && attacker_oil >= base.required_oil) non_rush_tactic.emplace(false, base);
                if (!rush_tactic && attacker_oil >= base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH]) rush_tactic.emplace(true, base);
            }
            // 能够负担得起
            if (attacker_oil >= base.required_oil + GENERAL_SKILL_COST[SkillType::RUSH])
                min_army = std::min(min_army, (int)std::ceil(target_army / atk_mult));
        }
    }

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
    // 发起行动的根据地，如果是从主将上分兵则为nullptr
    const Militia_area* area;
    // 行动方案，每一个元素均为一个“从A出发，向dir方向移动”的操作
    std::vector<std::pair<Coord, Direction>> plan;

    // 需要的军队数量
    int army_used;
    // 军队汇集至集合点需要的步数
    int gather_steps;

    // 构造函数：通过`calc_gather_plan`计算出的`plan`构造
    Militia_plan(const Generals* target, const Militia_area* area, std::vector<std::pair<Coord, Direction>>&& plan, int army_used) noexcept :
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

    // 针对一个目标（油井或中立副将）搜索可行的占领方案，其兵力来自民兵
    std::optional<Militia_plan> search_plan_from_militia(const Generals* target) const noexcept;

    // 针对一个目标（油井或中立副将）搜索可行的占领方案，其兵力从指定`provider`中获取
    std::optional<Militia_plan> search_plan_from_provider(const Generals* target, const Generals* provider) const noexcept;

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

std::optional<std::vector<Operation>> Attack_searcher::search() const noexcept {
    static std::vector<int> army_left{};
    static std::vector<Coord> landing_points{};
    static std::vector<Operation> attack_ops{};

    int oil = state.coin[attacker_seat];
    int my_mobility = state.tech_level[attacker_seat][static_cast<int>(TechType::MOBILITY)];
    const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(state.generals[1 - attacker_seat]);
    assert(enemy_general);
    if (!state.can_soldier_step_on(enemy_general->position, attacker_seat)) return std::nullopt; // 排除敌方主将在沼泽而走不进的情况

    Dist_map enemy_dist(state, enemy_general->position, Path_find_config(1.0, state.has_swamp_tech(attacker_seat), false));

    // 对每个将领
    for (int i = 0, siz = state.generals.size(); i < siz; ++i) {
        const Generals* general = state.generals[i];
        if (general->player != attacker_seat || dynamic_cast<const OilWell*>(general) != nullptr) continue;

        // 对每种连招
        for (const Base_tactic& base_tactic : BASE_TACTICS) {
            // 基本油量检查（不考虑rush）
            if (oil < base_tactic.required_oil) break;

            // 根据精细距离决定是否需要rush
            Critical_tactic tactic(enemy_dist[general->position] > my_mobility, base_tactic);
            if (oil < tactic.required_oil) continue; // 根据rush信息再次检查油量

            // 距离检查
            int eff_dist = Dist_map::effect_dist(general->position, enemy_general->position, tactic.can_rush, my_mobility);
            if (eff_dist > 0) continue;

            // 检查技能冷却
            if (tactic.can_rush && general->cd(SkillType::RUSH)) continue;
            if (tactic.strike_count && general->cd(SkillType::STRIKE)) continue;
            if (tactic.command_count && general->cd(SkillType::COMMAND)) continue;
            if (tactic.weaken_count && general->cd(SkillType::WEAKEN)) continue;

            // 检查非rush状态下技能的覆盖范围（暂且认为将领不移动）
            if (!tactic.can_rush && (tactic.strike_count || tactic.command_count || tactic.weaken_count) &&
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
            double main_atk_mult = (tactic.command_count ? Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::COMMAND)] : 1.0); // 由主将带来的攻击倍率
            double main_def_mult = (tactic.weaken_count ? Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::WEAKEN)] : 1.0);   // 由主将带来的弱化倍率
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
                    bool final_cell = (j == sjz - 1);

                    if (dest.player == attacker_seat) army_left.push_back(army_left.back() - 1 + dest.army);
                    else {
                        double local_attack_mult = state.attack_multiplier(from, attacker_seat);
                        double local_defence_mult = state.defence_multiplier(to);

                        // 假定副将带来的效果仅对敌方主将格有效（这样可以放宽副将位置的限制）
                        if (final_cell && tactic.command_count > 1) local_attack_mult *= pow(Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::COMMAND)], tactic.command_count - 1);
                        if (final_cell && tactic.weaken_count > 1) local_defence_mult *= pow(Constant::GENERAL_SKILL_EFFECT[static_cast<int>(SkillType::WEAKEN)], tactic.weaken_count - 1);

                        bool can_strike = final_cell && to.in_attack_range(landing_point); // 理论上不应该出现空袭不到的情况
                        int local_army = std::max(0, dest.army - (tactic.strike_count * Constant::STRIKE_DAMAGE * can_strike)); // 多次空袭全部针对敌方主将格
                        if (to.in_attack_range(landing_point)) {
                            local_attack_mult *= main_atk_mult;
                            local_defence_mult *= main_def_mult;
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

                // 最后需要确定副将能够放得下
                int spawn_count = tactic.spawn_count();
                std::vector<Coord> spawn_points;
                if (spawn_count) {
                    bool subgeneral_covers_enemy = (spawn_count + 1 == tactic.strike_count) || (spawn_count + 1 == tactic.weaken_count);
                    Coord search_core = path[path.size() - 2]; // 最后一步的前一个格子（即进攻敌方主将前，我方士兵所在格）
                    for (int x = std::max(search_core.x - Constant::GENERAL_ATTACK_RADIUS, 0); x <= std::min(search_core.x + Constant::GENERAL_ATTACK_RADIUS, Constant::col - 1); ++x) {
                        for (int y = std::max(search_core.y - Constant::GENERAL_ATTACK_RADIUS, 0); y <= std::min(search_core.y + Constant::GENERAL_ATTACK_RADIUS, Constant::row - 1); ++y) {
                            Coord pos(x, y);
                            if (subgeneral_covers_enemy && !pos.in_attack_range(enemy_general->position)) continue;

                            bool walk_pass = false;
                            for (int k = 0, skz = path.size(); k < skz; ++k) if (path[k] == pos) walk_pass = true;

                            bool can_place = (walk_pass && state[pos].generals == nullptr && pos != landing_point); // 途中经过的格子（包括主将原有占领格）
                            can_place |= (state[pos].player == attacker_seat && state[pos].generals == nullptr && pos != landing_point); // 原本就是我方占领格

                            if (can_place) spawn_points.push_back(pos);
                            if ((int)spawn_points.size() >= spawn_count) break;
                        }
                        if ((int)spawn_points.size() >= spawn_count) break;
                    }
                }
                if ((int)spawn_points.size() < spawn_count) continue;

                // 可攻击，准备导出行动
                logger.log(LOG_LEVEL_INFO, "[%s] Army left %d, path size %d", tactic.str().c_str(), army_left.back(), path.size()-1);

                // 此时才放入各种技能
                if (tactic.command_count) attack_ops.insert(attack_ops.begin(), Operation::generals_skill(general->id, SkillType::COMMAND));
                if (tactic.weaken_count) attack_ops.insert(attack_ops.begin(), Operation::generals_skill(general->id, SkillType::WEAKEN));

                // 在最后一步前加入空袭和副将操作
                if (tactic.strike_count) attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(general->id, SkillType::STRIKE, enemy_general->position));

                Base_tactic remain_skills = base_tactic;
                for (int spawn_index = 0; spawn_index < spawn_count; ++spawn_index) {
                    attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::recruit_generals(spawn_points[spawn_index]));
                    if (remain_skills.strike_count > 1) {
                        attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(state.next_generals_id + spawn_index, SkillType::STRIKE, enemy_general->position));
                        remain_skills.strike_count--;
                    }
                    if (remain_skills.command_count > 1) {
                        attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(state.next_generals_id + spawn_index, SkillType::COMMAND));
                        remain_skills.command_count--;
                    }
                    if (remain_skills.weaken_count > 1) {
                        attack_ops.insert(attack_ops.begin() + (attack_ops.size() - 1), Operation::generals_skill(state.next_generals_id + spawn_index, SkillType::WEAKEN));
                        remain_skills.weaken_count--;
                    }
                }

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
    }
}

std::optional<Militia_plan> Militia_analyzer::search_plan_from_militia(const Generals* target) const noexcept {
    assert(target);

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
    int enemy_army = state[target->position].army;
    if (target->player != -1) enemy_army += 3; // 额外余量

    for (const Militia_dist_info& info : dist_info) {
        const Militia_area& area = *info.area;
        int army_required = enemy_army + info.dist;

        if (area.max_army < army_required) continue; // 兵力不足

        // 计算方案：兵力汇集到最近点处
        Militia_plan plan(target, info.area, calc_gather_plan(info, army_required), army_required);

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
    assert(total_army >= required_army);

    std::reverse(plan.begin(), plan.end());
    return plan;
}
