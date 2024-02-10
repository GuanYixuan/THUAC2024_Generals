#include "include/controller.hpp"
#include "include/test_sync.hpp"

#include "include/assess.hpp"
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


class myAI : public GameController {
public:
    std::vector<General_strategy> strategies;

    void main_process() {
        // 初始操作
        if (game_state.round == 1) {
            my_operation_list.push_back(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));
            return;
        }


        // show_map(game_state, std::cerr);
        update_strategy();

        for (const General_strategy& strategy : strategies) {
            const Generals* general = game_state.generals[strategy.general_id];
            const Coord& target = strategy.target.coord;
            int curr_army = game_state[general->position].army;

            Dist_map dist_map(game_state, target, {});
            Direction dir = dist_map.direction_to_origin(general->position);
            Coord next_pos = general->position + DIRECTION_ARR[dir];
            const Cell& next_cell = game_state[next_pos];

            logger.log(LOG_LEVEL_DEBUG, "General at %s(%d) -> %s(%d)", general->position.str().c_str(), curr_army, next_pos.str().c_str(), next_cell.army);
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

            Dist_map dist_map(game_state, general->position, {});

            int best_well = -1;
            for (int j = 0; j < game_state.generals.size(); ++j) {
                const Generals* oil_well = game_state.generals[j];
                if (dynamic_cast<const OilWell*>(oil_well) == nullptr || oil_well->player == my_seat) continue;
                if (best_well == -1 || dist_map[oil_well->position] < dist_map[game_state.generals[best_well]->position]) best_well = j;
            }
            if (best_well == -1 || dist_map[game_state.generals[best_well]->position] > 1e8)
                logger.log(LOG_LEVEL_WARN, "No oil well found for general at %s", general->position.str().c_str());
            else {
                strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position}});
                logger.log(LOG_LEVEL_INFO, "General at %s -> oil well %s", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
            }
        }
    }
};

int main() {
    myAI ai;
    ai.run();
    return 0;
}
