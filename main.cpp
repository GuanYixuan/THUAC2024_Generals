#include "include/controller.hpp"
#include "include/test_sync.hpp"

#include "include/assess.hpp"
#include "include/logger.hpp"

#include <queue>
#include <optional>

class Oil_cluster {
public:
    double total_dist;
    const OilWell* center_well;
    std::vector<const OilWell*> wells;

    Oil_cluster(const OilWell* center_well) noexcept : total_dist(0.0), center_well(center_well) {}
    bool operator<(const Oil_cluster& other) const noexcept {
        if (wells.size() == other.wells.size()) return total_dist < other.total_dist;
        return wells.size() < other.wells.size();
    }

    // 将距离敌方近的油井排在前面
    void sort_wells(const Dist_map& enemy_dist) noexcept {
        std::sort(wells.begin(), wells.end(), [&enemy_dist](const OilWell* a, const OilWell* b) {
            return enemy_dist[a->position] < enemy_dist[b->position];
        });
    }

    // 获取描述字符串
    std::string str() const noexcept {
        std::string ret{wrap("Cluster size %d with center %s, total distance %.0f:", wells.size(), center_well->position.str().c_str(), total_dist)};
        for (const OilWell* well : wells) ret += wrap(" %s", well->position.str().c_str());
        return ret;
    }
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
    const Generals* general;

    Strategy_target(const Coord& coord) noexcept : coord(coord), general(nullptr) {}
    Strategy_target(const Generals* general) noexcept : coord(general->position), general(general) {}
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

    std::optional<Oil_cluster> cluster;

    void main_process() {
        // 初始操作
        if (game_state.round == 1) {
            my_operation_list.push_back(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));

            std::vector<Oil_cluster> clusters{identify_oil_clusters()};
            if (!clusters.empty()) cluster = clusters[0];
            return;
        }

        // show_map(game_state, std::cerr);
        update_strategy();

        bool can_rush = (game_state.coin[1 - my_seat] >= Constant::GENERAL_SKILL_COST[SkillType::RUSH]);
        for (const General_strategy& strategy : strategies) {
            const Generals* general = game_state.generals[strategy.general_id];
            int curr_army = game_state[general->position].army;

            if (strategy.type == General_strategy_type::DEFEND) {

            } else if (strategy.type == General_strategy_type::ATTACK) {

            } else if (strategy.type == General_strategy_type::RETREAT) {
                logger.log(LOG_LEVEL_INFO, "General %d retreating", strategy.general_id);
                const Generals* enemy = strategy.target.general;

                // 寻找能走到的最大有效距离
                Coord best_pos{-1, -1};
                int max_eff_dist = Dist_map::effect_dist(general->position, enemy->position, can_rush);
                for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
                    Coord next_pos = general->position + DIRECTION_ARR[dir];
                    if (!next_pos.in_map() || !game_state.can_general_step_on(next_pos, my_seat)) continue;
                    if (curr_army - 1 <= -game_state.eff_army(next_pos, my_seat)) continue;

                    int eff_dist = Dist_map::effect_dist(next_pos, enemy->position, can_rush);
                    logger.log(LOG_LEVEL_DEBUG, "Next pos %s, eff dist %d", next_pos.str().c_str(), eff_dist);
                    if (eff_dist > max_eff_dist) {
                        best_pos = next_pos;
                        max_eff_dist = eff_dist;
                    }
                }

                // 尝试移动（没有检查兵数的问题）
                if (best_pos.in_map()) {
                    if (curr_army > 1)
                        my_operation_list.push_back(Operation::move_army(general->position, from_coord(general->position, best_pos), curr_army - 1));
                    my_operation_list.push_back(Operation::move_generals(strategy.general_id, best_pos));
                    logger.log(LOG_LEVEL_INFO, "General at %s -> %s, dist -> %d", general->position.str().c_str(), best_pos.str().c_str(), max_eff_dist);
                }
            } else if (strategy.type == General_strategy_type::OCCUPY) {
                const Coord& target = strategy.target.coord;
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
    std::vector<Oil_cluster> identify_oil_clusters() const {
        static constexpr double MIN_ENEMY_DIST = 7.0;
        static constexpr double MAX_DIST = 7.0;

        static constexpr int MIN_CLUSTER_SIZE = 3;

        std::vector<Oil_cluster> clusters;

        // 计算敌方距离
        Dist_map enemy_dist(game_state, game_state.generals[1 - my_seat]->position, {});

        // 考虑各个油井作为中心的可能性
        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const OilWell* center_well = dynamic_cast<const OilWell*>(game_state.generals[i]);
            if (center_well == nullptr || game_state[center_well->position].type == CellType::SWAMP) continue;

            Dist_map dist_map(game_state, center_well->position, {});

            // 搜索其它油井
            Oil_cluster cluster(center_well);
            cluster.wells.push_back(center_well);
            for (int j = 0; j < siz; ++j) {
                const OilWell* well = dynamic_cast<const OilWell*>(game_state.generals[j]);
                if (well == nullptr || j == i) continue;

                if (dist_map[well->position] <= MAX_DIST && enemy_dist[well->position] >= MIN_ENEMY_DIST) {
                    cluster.wells.push_back(well);
                    cluster.total_dist += dist_map[well->position];
                }
            }
            if (cluster.wells.size() >= MIN_CLUSTER_SIZE) {
                clusters.push_back(cluster);
                cluster.sort_wells(enemy_dist);
                logger.log(LOG_LEVEL_INFO, "Oil cluster: %s", cluster.str().c_str());
            }
        }

        // 按数量和总距离排序
        std::sort(clusters.begin(), clusters.end());
        return clusters;
    }

    void update_strategy() {
        strategies.clear();

        bool cluster_occupied = true;
        if (cluster) for (const OilWell* well : cluster->wells)
            if (game_state[well->position].player != my_seat) cluster_occupied = false;

        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const Generals* general = game_state.generals[i];
            // 暂时只给主将分配策略
            if (general->player != my_seat || dynamic_cast<const MainGenerals*>(general) == nullptr) continue;

            int min_effect_dist = 1e8;
            const Generals* nearest_enemy = nullptr;
            for (int j = 0; j < siz; ++j) {
                const Generals* enemy = game_state.generals[j];
                if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;

                if (game_state[enemy->position].army >= game_state[general->position].army - 3) {
                    int effect_dist = Dist_map::effect_dist(general->position, enemy->position, game_state.coin[1 - my_seat] >= Constant::GENERAL_SKILL_COST[SkillType::RUSH]);
                    if (effect_dist < min_effect_dist) {
                        min_effect_dist = effect_dist;
                        nearest_enemy = enemy;
                    }
                }
            }
            if (min_effect_dist <= 0) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::RETREAT, Strategy_target(nearest_enemy)});
                logger.log(LOG_LEVEL_INFO, "General %s retreat %s, eff dist %d", general->position.str().c_str(), nearest_enemy->position.str().c_str(), min_effect_dist);
                continue;
            }

            // 找最近的油田
            Dist_map dist_map(game_state, general->position, {});
            int best_well = -1;
            for (int j = 0; j < siz; ++j) {
                const Generals* oil_well = game_state.generals[j];
                if (dynamic_cast<const OilWell*>(oil_well) == nullptr || oil_well->player == my_seat) continue;
                if (best_well == -1 || dist_map[oil_well->position] < dist_map[game_state.generals[best_well]->position]) best_well = j;
            }

            // 3格内油田优先
            if (best_well != -1 && dist_map[game_state.generals[best_well]->position] <= 3.0) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position}});
                logger.log(LOG_LEVEL_INFO, "General %s -> oil well %s (near)", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
                continue;
            }

            // 优先采用cluster中的油田
            if (cluster) {
                bool found = false;
                for (const OilWell* well : cluster->wells) {
                    if (game_state[well->position].player != my_seat && dist_map[well->position] < 1e8) {
                        strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{well->position}});
                        logger.log(LOG_LEVEL_INFO, "General %s -> oil well %s (cluster)", general->position.str().c_str(), well->position.str().c_str());
                        found = true;
                        break;
                    }
                }
                if (found) continue;
            }

            // 否则取最近油田
            if (best_well == -1 || dist_map[game_state.generals[best_well]->position] > 1e8)
                logger.log(LOG_LEVEL_WARN, "No oil well found for general at %s", general->position.str().c_str());
            else {
                strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position}});
                logger.log(LOG_LEVEL_INFO, "General %s -> oil well %s", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
            }
        }
    }
};

int main() {
    myAI ai;
    ai.run();
    return 0;
}
