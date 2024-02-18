#include "include/controller.hpp"

#include "include/assess.hpp"
#include "include/logger.hpp"

#include <cmath>
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

    int oil_savings = 20;

    void main_process() {
        // 初始操作
        if (game_state.round == 1) {
            my_operation_list.push_back(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));

            std::vector<Oil_cluster> clusters{identify_oil_clusters()};
            if (!clusters.empty()) cluster = clusters[0];
            return;
        }

        // 简单的无Strike一步杀搜索
        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);
        assert(main_general && enemy_general);
        int effective_oil = game_state.coin[my_seat];
        if (true || effective_oil < Constant::GENERAL_SKILL_COST[SkillType::RUSH]) { // 无Rush
            if (Dist_map::effect_dist(main_general->position, enemy_general->position, false,
                game_state.tech_level[my_seat][static_cast<int>(TechType::MOBILITY)]) < 0) {
                Dist_map dist_map(game_state, enemy_general->position, Path_find_config(1.0, 1e9, false));
                logger.log(LOG_LEVEL_INFO, "Enemy enter attack range, dist %.0f", dist_map[main_general->position]);

                if (dist_map[main_general->position] <= game_state.tech_level[my_seat][static_cast<int>(TechType::MOBILITY)]) {
                    std::vector<Coord> path{dist_map.path_to_origin(main_general->position)};
                    int army_left = -((int)path.size() - 1);
                    for (const Coord& pos : path)
                        army_left += game_state.eff_army(pos, my_seat) * (pos == enemy_general->position ? game_state.defence_multiplier(enemy_general->position) : 1);

                    logger.log(LOG_LEVEL_INFO, "Army left %d, path size %d", army_left, path.size()-1);
                    if (army_left > 0) { // 可攻击
                        int army_to_move = game_state.eff_army(main_general->position, my_seat);
                        for (int i = 1, siz = path.size(); i < siz; ++i) {
                            my_operation_list.push_back(Operation::move_army(path[i - 1], from_coord(path[i - 1], path[i]), army_to_move - 1));
                            army_to_move += game_state.eff_army(path[i], my_seat) - 1;
                        }
                        return;
                    }
                }
            }
        } else { // 有Rush

        }

        // 随便写点升级
        // 主将产量升级
        if (effective_oil >= oil_savings + main_general->production_upgrade_cost()) {
            my_operation_list.push_back(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));
            effective_oil -= main_general->production_upgrade_cost();
        }
        // 产量升级完毕后考虑升级防御
        if (effective_oil >= oil_savings + main_general->defence_upgrade_cost() &&
            main_general->defence_level < 2 && main_general->produce_level >= Constant::GENERAL_PRODUCTION_VALUES[2]) {
            my_operation_list.push_back(Operation::upgrade_generals(my_seat, QualityType::DEFENCE));
            effective_oil -= main_general->defence_upgrade_cost();
        }

        // 升级距离敌方足够远的油井
        if (effective_oil >= oil_savings + 25 + Constant::OILWELL_PRODUCTION_COST[0]) {
            for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
                const OilWell* well = dynamic_cast<const OilWell*>(game_state.generals[i]);
                if (well == nullptr || well->player != my_seat) continue;
                if (well->produce_level > Constant::OILWELL_PRODUCTION_VALUES[0]) continue;

                Dist_map dist_map(game_state, well->position, {});
                double min_dist = std::numeric_limits<double>::max();
                for (int j = 0; j < siz; ++j) {
                    const Generals* enemy = game_state.generals[j];
                    if (dynamic_cast<const OilWell*>(enemy) || enemy->player != 1 - my_seat) continue;
                    min_dist = std::min(min_dist, dist_map[enemy->position]);
                }

                if (min_dist >= 17) {
                    my_operation_list.push_back(Operation::upgrade_generals(i, QualityType::PRODUCTION));
                    effective_oil -= Constant::OILWELL_PRODUCTION_COST[0];
                    break;
                }
            }
        }

        // 足够有钱则升级移动力
        if (game_state.tech_level[my_seat][static_cast<int>(TechType::MOBILITY)] == Constant::PLAYER_MOVEMENT_VALUES[0] && effective_oil >= oil_savings + Constant::PLAYER_MOVEMENT_COST[0]) {
            my_operation_list.push_back(Operation::upgrade_tech(TechType::MOBILITY));
            effective_oil -= Constant::PLAYER_MOVEMENT_COST[0];
        }
        if (game_state.tech_level[my_seat][static_cast<int>(TechType::MOBILITY)] == Constant::PLAYER_MOVEMENT_VALUES[1] && effective_oil >= oil_savings + Constant::PLAYER_MOVEMENT_COST[1]) {
            my_operation_list.push_back(Operation::upgrade_tech(TechType::MOBILITY));
            effective_oil -= Constant::PLAYER_MOVEMENT_COST[1];
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

        // 计算双方距离
        Dist_map my_dist(game_state, game_state.generals[my_seat]->position, {});
        Dist_map enemy_dist(game_state, game_state.generals[1 - my_seat]->position, {});

        // 考虑各个油井作为中心的可能性
        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const OilWell* center_well = dynamic_cast<const OilWell*>(game_state.generals[i]);
            if (center_well == nullptr || game_state[center_well->position].type == CellType::SWAMP) continue;

            // 计算距离
            int my_dist_to_center = my_dist[center_well->position];
            int enemy_dist_to_center = enemy_dist[center_well->position];
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

            // 去掉太小的
            if (cluster.wells.size() < MIN_CLUSTER_SIZE) continue;

            // 过滤距离太过悬殊的
            if (my_dist_to_center >= 5 && my_dist_to_center >= 2 * enemy_dist_to_center) {
                logger.log(LOG_LEVEL_INFO, "Oil cluster too far (%d vs %d) %s", my_dist_to_center, enemy_dist_to_center, cluster.str().c_str());
                continue;
            }

            clusters.push_back(cluster);
            cluster.sort_wells(enemy_dist);
            logger.log(LOG_LEVEL_INFO, cluster.str().c_str());
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

            int curr_army = game_state[general->position].army;
            double defence_mult = game_state.defence_multiplier(general->position);

            // 考虑是否在危险范围内
            int min_effect_dist = 1e8;
            const Generals* nearest_enemy = nullptr;
            for (int j = 0; j < siz; ++j) {
                const Generals* enemy = game_state.generals[j];
                int enemy_army = game_state[enemy->position].army;
                if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;

                for (const Critical_tactic& tactic : CRITICAL_TACTICS) {
                    if (game_state.coin[1 - my_seat] < tactic.required_oil) break;

                    // 初步计算能否攻下（不计路程补充/损耗）
                    double attack_multiplier = tactic.can_command ? Constant::GENERAL_SKILL_EFFECT[SkillType::COMMAND] : 1.0;
                    if (enemy_army * attack_multiplier <= std::max(0, curr_army - (tactic.can_strike ? Constant::STRIKE_DAMAGE : 0)) * defence_mult) continue;

                    int effect_dist = Dist_map::effect_dist(general->position, enemy->position, tactic.can_rush,
                                                            game_state.tech_level[1-my_seat][static_cast<int>(TechType::MOBILITY)]);
                    if (effect_dist < min_effect_dist) {
                        if (effect_dist < 0) logger.log(LOG_LEVEL_INFO, "General %s threaten by [%s], eff dist %d", general->position.str().c_str(), tactic.str().c_str(), effect_dist);
                        min_effect_dist = effect_dist;
                        nearest_enemy = enemy;
                    }
                }
            }
            // 在范围内则撤退
            if (min_effect_dist < 0) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::RETREAT, Strategy_target(nearest_enemy)});
                logger.log(LOG_LEVEL_INFO, "General %s retreat %s, eff dist %d", general->position.str().c_str(), nearest_enemy->position.str().c_str(), min_effect_dist);
                continue;
            }
            // 在边缘处则考虑相持
            else if (min_effect_dist == 0) {
                bool found = false;
                // 选择在自己前方的油田
                Dist_map dist_map(game_state, general->position, {});
                for (int j = 0; j < siz; ++j) {
                    const OilWell* oil_well = dynamic_cast<const OilWell*>(game_state.generals[j]);
                    if (oil_well == nullptr || oil_well->player == my_seat) continue;
                    if (dist_map[oil_well->position] <= 4 && (oil_well->position - general->position).angle_to(nearest_enemy->position - general->position) <= M_PI / 2) {
                        strategies.emplace_back(General_strategy{i, General_strategy_type::DEFEND, Strategy_target{oil_well->position}});
                        logger.log(LOG_LEVEL_INFO, "General %s try to occupy oil well %s", general->position.str().c_str(), oil_well->position.str().c_str());
                        found = true;
                        break;
                    }
                }
                if (found) continue;

                // 选择最近的己方油田
                const OilWell* best_well = nullptr;
                for (int j = 0; j < siz; ++j) {
                    const OilWell* oil_well = dynamic_cast<const OilWell*>(game_state.generals[j]);
                    if (oil_well == nullptr || oil_well->player != my_seat) continue;
                    if (best_well == nullptr || dist_map[oil_well->position] < dist_map[best_well->position]) best_well = oil_well;
                }
                if (best_well) {
                    strategies.emplace_back(General_strategy{i, General_strategy_type::DEFEND, Strategy_target{best_well->position}});
                    logger.log(LOG_LEVEL_INFO, "General %s defend oil well %s", general->position.str().c_str(), best_well->position.str().c_str());
                } else logger.log(LOG_LEVEL_INFO, "General %s has no target to defend", general->position.str().c_str());
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
