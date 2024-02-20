#pragma once

#include <queue>
#include <vector>
#include <cstring>
#include <iostream>

#include "gamestate.hpp"
#include "controller.hpp"

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
    static constexpr double MAX_DIST = 1000 * Constant::col * Constant::row;
    static constexpr double INF = 1e9;

    // 搜索的原点
    const Coord origin;

    // 搜索设置
    const Path_find_config cfg;

    // 距离矩阵
    double dist[Constant::col][Constant::row];

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
    static int effect_dist(const Coord& pos, const Coord& general_pos, bool can_rush, int movement_val = Constant::PLAYER_MOVEMENT_VALUES[0]) noexcept;

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
        if (can_rush) ret += ", can rush";
        if (can_strike) ret += ", can strike";
        if (can_command) ret += ", can command";
        if (can_command_and_weaken) ret += " + weaken";
        return ret;
    }
};
// 固定的几种连招，按`required_oil`排序
constexpr Critical_tactic CRITICAL_TACTICS[] = {
    {0, false, false, false, false},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE], false, true, false, false},
    {Constant::GENERAL_SKILL_COST[SkillType::RUSH], true, false, false, false},
    {Constant::GENERAL_SKILL_COST[SkillType::COMMAND], false, false, true, false},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE] + Constant::GENERAL_SKILL_COST[SkillType::RUSH], true, true, false, false},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND], false, true, true, false},
    {Constant::GENERAL_SKILL_COST[SkillType::RUSH] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND], true, false, true, false},
    {Constant::GENERAL_SKILL_COST[SkillType::COMMAND] + Constant::GENERAL_SKILL_COST[SkillType::WEAKEN], false, false, true, true},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE] + Constant::GENERAL_SKILL_COST[SkillType::RUSH] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND], true, true, true, false},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND] + Constant::GENERAL_SKILL_COST[SkillType::WEAKEN], false, true, true, true},
    {Constant::GENERAL_SKILL_COST[SkillType::RUSH] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND] + Constant::GENERAL_SKILL_COST[SkillType::WEAKEN], true, false, true, true},
    {Constant::GENERAL_SKILL_COST[SkillType::STRIKE] + Constant::GENERAL_SKILL_COST[SkillType::RUSH] + Constant::GENERAL_SKILL_COST[SkillType::COMMAND] + Constant::GENERAL_SKILL_COST[SkillType::WEAKEN], true, true, true, true},
};

// ******************** 寻路部分实现 ********************

Dist_map::Dist_map(const GameState& board, const Coord& origin, const Path_find_config& cfg) noexcept : origin(origin), cfg(cfg), board(board) {
    // 初始化
    bool vis[Constant::col][Constant::row];
    double cell_dist[static_cast<int>(CellType::Type_count)] = {1.0, cfg.desert_dist, cfg.swamp_dist};
    std::memset(vis, 0, sizeof(vis));
    std::fill_n(reinterpret_cast<double*>(dist), Constant::col * Constant::row, INF);

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
    assert(min_dist <= Constant::col * Constant::row);
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
    int dx = std::max(std::abs(pos.x - general_pos.x) - Constant::GENERAL_ATTACK_RADIUS, 0);
    int dy = std::max(std::abs(pos.y - general_pos.y) - Constant::GENERAL_ATTACK_RADIUS, 0);
    if (dx == 0 && dy == 0) return std::min(std::abs(pos.x - general_pos.x), std::abs(pos.y - general_pos.y)) - movement_val - 3;
    return dx + dy - movement_val - 1;
}
