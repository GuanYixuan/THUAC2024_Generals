#include "include/controller.hpp"
#include "include/test_sync.hpp"

#include "include/logger.hpp"

#include <queue>

class Oil_cluster {
public:
    int size;
    std::vector<OilWell*> wells;
};

enum class General_strategy_type {
    DEFEND,
    ATTACK,
    RETREAT,
    OCCUPY
};

class Strategy_target {
public:
    Coord coord;
};

class General_strategy {
public:
    int general_id;

    General_strategy_type type;

    Strategy_target target;
};

class Dist_map {
public:
    // 搜索的原点
    const Coord origin;

    // 距离矩阵
    double dist[Constant::col][Constant::row];

    // 取距离矩阵值
    double operator[] (const Coord& coord) const noexcept {
        assert(coord.in_map());
        return dist[coord.x][coord.y];
    }

    // 计算从`pos`走向`origin`的下一步最佳方向，返回的是`DIRECTION_ARR`中的下标
    Direction direction_to_origin(const Coord& pos) const noexcept {
        assert(pos.in_map());

        int min_dir = -1;
        double min_dist = std::numeric_limits<double>::infinity();
        for (int i = 0; i < 4; ++i) {
            Coord next_pos = pos + DIRECTION_ARR[i];
            if (!next_pos.in_map()) continue;

            if (dist[next_pos.x][next_pos.y] < min_dist) {
                min_dist = dist[next_pos.x][next_pos.y];
                min_dir = i;
            }
        }
        assert(min_dist <= Constant::col * Constant::row);
        return static_cast<Direction>(min_dir);
    }

    // 构造函数，暂且把计算也写在这里
    Dist_map(const GameState& board, const Coord& origin, int desert_dist = 2) noexcept : origin(origin), board(board) {
        // 初始化
        bool vis[Constant::col][Constant::row];
        std::memset(vis, 0, sizeof(vis));
        std::fill_n(reinterpret_cast<double*>(dist), Constant::col * Constant::row, std::numeric_limits<double>::infinity());

        // 单源最短路
        std::priority_queue<__Queue_Node, std::vector<__Queue_Node>, std::greater<__Queue_Node>> queue;
        queue.emplace(origin, 0);
        while (!queue.empty()) {
            __Queue_Node node = queue.top();
            const Coord& curr_pos = node.coord;
            queue.pop();

            if (vis[curr_pos.x][curr_pos.y]) continue;
            vis[curr_pos.x][curr_pos.y] = true;
            dist[curr_pos.x][curr_pos.y] = node.dist;

            // 不能走的格子不允许扩展
            if (board[curr_pos].type == CellType::SWAMP) continue; // 假定沼泽不能走
            if (board[curr_pos].generals != nullptr && curr_pos != origin) continue; // 假定将领所在格都不能走

            // 扩展
            for (const Coord& dir : DIRECTION_ARR) {
                Coord next_pos = curr_pos + dir;
                if (!next_pos.in_map() || vis[next_pos.x][next_pos.y]) continue;

                double next_dist = node.dist + (board[next_pos].type == CellType::DESERT ? desert_dist : 1);
                if (next_dist < dist[next_pos.x][next_pos.y]) queue.emplace(next_pos, next_dist);
            }
        }
    }
private:
    const GameState& board;

    struct __Queue_Node {
        Coord coord;
        double dist;

        __Queue_Node(const Coord& coord, double dist) noexcept : coord(coord), dist(dist) {}
        bool operator> (const __Queue_Node& other) const noexcept { return dist > other.dist; }
    };
};

class myAI : public GameController {
public:
    std::vector<General_strategy> strategies;

    void main_process() {
        // show_map(game_state, std::cerr);
        update_strategy();

        for (const General_strategy& strategy : strategies) {
            const Generals* general = game_state.generals[strategy.general_id];
            const Coord& target = strategy.target.coord;
            int curr_army = game_state[general->position].army;

            Dist_map dist_map(game_state, target);
            Direction dir = dist_map.direction_to_origin(general->position);
            Coord next_pos = general->position + DIRECTION_ARR[dir];
            const Cell& next_cell = game_state[next_pos];

            if (next_pos == target) {
                if (curr_army - 1 > next_cell.army)
                    my_operation_list.push_back(Operation::move_army(general->position, dir, next_cell.army + 1));
            } else {
                if (curr_army > 1 && (curr_army - 1 > (next_cell.player == my_seat ? 0 : next_cell.army))) {
                    my_operation_list.push_back(Operation::move_army(general->position, dir, curr_army - 1));
                    my_operation_list.push_back(Operation::move_generals(strategy.general_id, next_pos));
                }
            }
        }
    }

    [[noreturn]] void run() {
        init();
        while (true) {
            // 先手
            if (my_seat == 0) {
                // 给出操作
                logger.round = game_state.round;
                main_process();
                // 向judger发送操作
                finish_and_send_our_ops();
                // 读取并应用敌方操作
                read_and_apply_enemy_ops();
                //更新回合
                game_state.update_round();
            }
            // 后手
            else {
                // 读取并应用敌方操作
                read_and_apply_enemy_ops();
                // 给出操作
                logger.round = game_state.round;
                main_process();
                // 向judger发送操作
                finish_and_send_our_ops();
                //更新回合
                game_state.update_round();
            }
        }
    }

private:
    std::vector<Oil_cluster> identify_oil_clusters() const;

    void update_strategy() {
        strategies.clear();

        // 临时策略：以最近的油田为目标
        for (int i = 0; i < game_state.generals.size(); ++i) {
            const Generals* general = game_state.generals[i];
            if (general->player != my_seat || dynamic_cast<const MainGenerals*>(general) == nullptr) continue;

            Dist_map dist_map(game_state, general->position);

            int best_well = -1;
            for (int j = 0; j < game_state.generals.size(); ++j) {
                const Generals* oil_well = game_state.generals[j];
                if (dynamic_cast<const OilWell*>(oil_well) == nullptr || oil_well->player == my_seat) continue;
                if (best_well == -1 || dist_map[oil_well->position] < dist_map[game_state.generals[best_well]->position]) best_well = j;
            }
            if (best_well == -1) logger.log(2, "No oil well found for general at %s", general->position.str().c_str());
            else strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position}});

            logger.log(1, "General at %s -> oil well %s", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
        }
    }
};

int main() {
    myAI ai;
    ai.run();
    return 0;
}
