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

    Path_find_config(double desert_dist = 2, double swamp_dist = 1e9, bool general_path = true, double max_dist = 1e9) noexcept :
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
