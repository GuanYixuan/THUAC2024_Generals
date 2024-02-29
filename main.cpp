#include "include/controller.hpp"

#include "include/assess.hpp"
#include "include/logger.hpp"

#include <cmath>
#include <queue>
#include <cstdlib>
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

struct Danger {
    int eff_dist;
    const Generals* enemy;
    Critical_tactic tactic;

    Danger(int eff_dist, const Generals* enemy, const Critical_tactic& tactic) noexcept : eff_dist(eff_dist), enemy(enemy), tactic(tactic) {}

    bool operator>(const Danger& other) const noexcept {
        if (eff_dist != other.eff_dist) return eff_dist < other.eff_dist;
        return tactic.required_oil < other.tactic.required_oil;
    }
};

class Strategy_target {
public:
    Coord coord;
    const Generals* general;
    std::optional<Danger> danger;

    // 构造函数：占领
    Strategy_target(const Coord& coord) noexcept : coord(coord), general(nullptr) {}
    // 构造函数：进攻
    Strategy_target(const Generals* general) noexcept : coord(general->position), general(general) {}
    // 构造函数：防御
    Strategy_target(const Coord& coord, const Danger& danger) noexcept : coord(coord), general(danger.enemy), danger(danger) {}
    // 构造函数：撤退
    Strategy_target(const Danger& danger) noexcept : coord(danger.enemy->position), general(danger.enemy), danger(danger) {}
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

    int oil_savings;
    int oil_after_op;
    int remain_move_count;

    std::optional<Deterrence_analyzer> deterrence_analyzer;

    // 站在敌方立场进行路径搜索时的额外开销，体现了我方的威慑范围
    int enemy_pathfind_cost[Constant::col][Constant::row];

    void main_process() {
        // 初始操作
        if (game_state.round == 1) {
            logger.log(LOG_LEVEL_INFO, "Seat %d\n", my_seat);

            add_operation(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));

            std::vector<Oil_cluster> clusters{identify_oil_clusters()};
            if (!clusters.empty()) {
                cluster = clusters[0];
                logger.log(LOG_LEVEL_INFO, "Selected oil cluster: %s", cluster->str().c_str());
            }
            return;
        }

        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);

        // 参数更新
        oil_after_op = game_state.coin[my_seat];
        remain_move_count = game_state.rest_move_step[my_seat];
        int oil_production = game_state.calc_oil_production(my_seat);
        deterrence_analyzer.emplace(main_general, enemy_general, oil_after_op, game_state);

        // 计算我方威慑范围
        if (deterrence_analyzer->rush_tactic || deterrence_analyzer->non_rush_tactic) { // 有威慑
            bool has_rush = deterrence_analyzer->rush_tactic.has_value();
            for (int x = 0; x < Constant::col; ++x)
                for (int y = 0; y < Constant::row; ++y)
                    enemy_pathfind_cost[x][y] = Dist_map::effect_dist(Coord{x, y}, main_general->position, has_rush, game_state.get_mobility(my_seat)) < 0 ? 3 : 0;
        } else memset(enemy_pathfind_cost, 0, sizeof(enemy_pathfind_cost)); // 无威慑

        // 计算油量目标值
        if (game_state.round > 15 || game_state[main_general->position].army < game_state[enemy_general->position].army)
            oil_savings = 35;
        else oil_savings = 20;
        if (game_state.round > 20) {
            if (oil_after_op + oil_production * 9 >= deterrence_analyzer->min_oil)
                oil_savings = std::max(oil_savings, deterrence_analyzer->min_oil);
        }
        logger.log(LOG_LEVEL_INFO, "Oil %d(+%d) vs %d(+%d), savings %d",
                   oil_after_op, oil_production, game_state.coin[1 - my_seat], game_state.calc_oil_production(1 - my_seat), oil_savings);

        // 进攻搜索
        Attack_searcher searcher(my_seat, game_state);
        std::optional<std::vector<Operation>> ret = searcher.search();
        if (ret) {
            logger.log(LOG_LEVEL_INFO, "Critical tactic found");
            for (const Operation& op : *ret) {
                logger.log(LOG_LEVEL_INFO, "\t Op: %s", op.str().c_str());
                add_operation(op);
            }
            return;
        }

        // 考虑可能的升级
        assess_upgrades();

        // 向各个将领分配策略
        update_strategy();

        // 根据策略执行操作
        execute_strategy();

        // 民兵任务分配与移动
        militia_move();
    }

    [[noreturn]] void run() {
        init();
        while (true) {
            // 先手
            if (my_seat == 0) {
                // 给出操作
                main_process();
                // 向judger发送操作
                send_ops();
                // 读取并应用敌方操作
                read_and_apply_enemy_ops();
                // 更新回合
                game_state.update_round();
                logger.round = game_state.round;
            }
            // 后手
            else {
                // 读取并应用敌方操作
                read_and_apply_enemy_ops();
                // 给出操作
                main_process();
                // 向judger发送操作
                send_ops();
                // 更新回合
                game_state.update_round();
                logger.round = game_state.round;
            }
        }
    }

private:
    std::vector<Oil_cluster> identify_oil_clusters() const {
        static constexpr double MIN_ENEMY_DIST = 7.0;
        static constexpr double MAX_DIST = 5.0;

        static constexpr int MIN_CLUSTER_SIZE = 3;

        std::vector<Oil_cluster> clusters;

        // 计算双方距离
        Dist_map my_dist(game_state, game_state.generals[my_seat]->position, {2.0}); // 沙漠视为2格
        Dist_map enemy_dist(game_state, game_state.generals[1 - my_seat]->position, {2.0});

        // 考虑各个油井作为中心的可能性
        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const OilWell* center_well = dynamic_cast<const OilWell*>(game_state.generals[i]);
            if (center_well == nullptr || game_state[center_well->position].type == CellType::SWAMP) continue;

            // 计算距离
            int my_dist_to_center = my_dist[center_well->position];
            int enemy_dist_to_center = enemy_dist[center_well->position];
            Dist_map dist_map(game_state, center_well->position, {2.0});

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
                logger.log(LOG_LEVEL_INFO, "[Cluster finding] Oil cluster too far (%d vs %d) %s", my_dist_to_center, enemy_dist_to_center, cluster.str().c_str());
                continue;
            }

            clusters.push_back(cluster);
            cluster.sort_wells(enemy_dist);
            logger.log(LOG_LEVEL_INFO, "[Cluster finding] %s", cluster.str().c_str());
        }

        // 按数量和总距离排序
        std::sort(clusters.begin(), clusters.end());
        return clusters;
    }

    void assess_upgrades() {
        // 计算“相遇时间”（仅考虑主将）
        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);
        Dist_map my_dist(game_state, main_general->position, Path_find_config{1.0});
        int approach_time = (my_dist[enemy_general->position] - 5 - enemy_general->mobility_level) / (main_general->mobility_level + enemy_general->mobility_level);
        int oil_on_approach = oil_after_op + game_state.calc_oil_production(my_seat) * std::max(0, approach_time);
        logger.log(LOG_LEVEL_INFO, "[Assess] Approach time: %d, oil on approach: %d", approach_time, oil_on_approach);

        // 考虑油井升级
        Path_find_config enemy_dist_cfg(1.0, game_state.has_swamp_tech(1-my_seat));
        enemy_dist_cfg.custom_dist = enemy_pathfind_cost;
        bool unlock_upgrade_3 = main_general->produce_level >= Constant::GENERAL_PRODUCTION_VALUES[2];
        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const OilWell* well = dynamic_cast<const OilWell*>(game_state.generals[i]);
            if (well == nullptr || well->player != my_seat) continue;

            int tire = well->production_tire();
            int cost = well->production_upgrade_cost();
            if (tire >= 2 && !unlock_upgrade_3) continue;
            if (oil_after_op < oil_savings + cost ||
                oil_on_approach + (Constant::OILWELL_PRODUCTION_VALUES[tire + 1] - Constant::OILWELL_PRODUCTION_VALUES[tire]) * approach_time < oil_savings + cost) continue;

            // 以油井为中心计算到敌方的距离（考虑我方威慑）
            Dist_map dist_map(game_state, well->position, enemy_dist_cfg);
            double min_dist = std::numeric_limits<double>::max();
            for (int j = 0; j < siz; ++j) {
                const Generals* enemy = game_state.generals[j];
                if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;
                min_dist = std::min(min_dist, dist_map[enemy->position]);
            }
            if (min_dist >= 6 + 3 * tire) {
                logger.log(LOG_LEVEL_INFO, "[Upgrade] Well %s upgrade to tire %d (min dist %.1f)", well->position.str().c_str(), tire + 1, min_dist);
                add_operation(Operation::upgrade_generals(well->id, QualityType::PRODUCTION));
                oil_after_op -= Constant::OILWELL_PRODUCTION_COST[tire];
                oil_on_approach -= Constant::OILWELL_PRODUCTION_COST[tire];
                break;
            }
        }

        // 注意：以下升级每回合至多进行一个
        int mob_tire = game_state.get_mobility_tire(my_seat);
        // 主将产量升级
        if (oil_after_op >= main_general->production_upgrade_cost() &&
            oil_on_approach >= oil_savings + main_general->production_upgrade_cost()) {

            oil_after_op -= main_general->production_upgrade_cost();
            add_operation(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));
        }
        // 主将防御升级
        else if (oil_after_op >= main_general->defence_upgrade_cost() &&
                 oil_on_approach >= oil_savings + main_general->defence_upgrade_cost() &&
                 game_state[main_general->position].army > (main_general->defence_tire() == 0 ? 40 : 80)) {

            oil_after_op -= main_general->defence_upgrade_cost();
            add_operation(Operation::upgrade_generals(my_seat, QualityType::DEFENCE));
        }
        // 油足够多，则考虑升级行动力
        else if (oil_after_op >= PLAYER_MOVEMENT_COST[mob_tire] &&
                 oil_on_approach >= oil_savings + 100 + PLAYER_MOVEMENT_COST[mob_tire]) {

            oil_after_op -= PLAYER_MOVEMENT_COST[mob_tire];
            add_operation(Operation::upgrade_tech(TechType::MOBILITY));
        }
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
            int enemy_lookahead_oil = game_state.coin[1 - my_seat] + game_state.calc_oil_production(1 - my_seat) * 2; // 取两回合后的油量
            logger.log(LOG_LEVEL_INFO, "[Assess] General %s with army %d, defence mult %.2f, enemy lookahead oil %d",
                       general->position.str().c_str(), curr_army, defence_mult, enemy_lookahead_oil);

            // 考虑是否在危险范围内
            Danger most_danger{100000, nullptr, Critical_tactic(false, BASE_TACTICS[0])};
            for (int j = 0; j < siz; ++j) {
                const Generals* enemy = game_state.generals[j];
                int enemy_army = game_state[enemy->position].army;
                if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;

                for (int k = sizeof(BASE_TACTICS)/sizeof(BASE_TACTICS[0])-1; k >= 0; --k) {
                    const Base_tactic& base_tactic = BASE_TACTICS[k];
                    if (enemy_lookahead_oil < base_tactic.required_oil) continue;

                    // 判定是否需要rush（必要条件）
                    Critical_tactic tactic(Dist_map::effect_dist(general->position, enemy->position, false, game_state.get_mobility(1-my_seat)) >= 0, base_tactic);

                    // 初步计算能否攻下（不计路程补充/损耗）
                    double attack_multiplier = pow(Constant::GENERAL_SKILL_EFFECT[SkillType::COMMAND], base_tactic.command_count);
                    attack_multiplier *= pow(Constant::GENERAL_SKILL_EFFECT[SkillType::WEAKEN], -base_tactic.weaken_count); // 注意是负数次幂

                    int effective_army = std::max(0, curr_army - (base_tactic.strike_count * Constant::STRIKE_DAMAGE));
                    if ((enemy_army * attack_multiplier < effective_army * defence_mult) &&
                        (enemy_army + enemy->produce_level) * attack_multiplier < (effective_army + general->produce_level) * defence_mult) continue; // 1回合 lookahead

                    // 假如敌方因为rush而不够油，则不在tactic中记录rush（但要知道往前送是危险的）
                    tactic.can_rush &= (enemy_lookahead_oil >= tactic.required_oil);
                    int effect_dist = Dist_map::effect_dist(general->position, enemy->position, tactic.can_rush, game_state.get_mobility(1-my_seat));
                    Danger new_danger{effect_dist, enemy, tactic};
                    if (new_danger > most_danger) {
                        most_danger = new_danger;
                        if (effect_dist < 0)
                            logger.log(LOG_LEVEL_INFO, "[Assess] General %s threaten by [%s], eff dist %d", general->position.str().c_str(), tactic.str().c_str(), effect_dist);
                    }
                }
            }
            logger.log(LOG_LEVEL_INFO, "[Assess] General %s most dangerous [%s], eff dist %d", general->position.str().c_str(), most_danger.tactic.str().c_str(), most_danger.eff_dist);
            // 撤退：在攻击范围内
            if (most_danger.eff_dist < 0) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::RETREAT, Strategy_target(most_danger)});
                logger.log(LOG_LEVEL_INFO, "[Allocate] General %s retreat %s, eff dist %d", general->position.str().c_str(), most_danger.enemy->position.str().c_str(), most_danger.eff_dist);
                continue;
            }

            // 占领：4格内油田考虑使用民兵占领
            Dist_map near_map(game_state, general->position, Path_find_config{1.0, game_state.has_swamp_tech(my_seat)});
            int best_well = -1;
            for (int j = 0; j < siz; ++j) {
                const Generals* oil_well = game_state.generals[j];
                if (dynamic_cast<const OilWell*>(oil_well) == nullptr || oil_well->player == my_seat) continue;
                if (best_well == -1 || near_map[oil_well->position] < near_map[game_state.generals[best_well]->position]) best_well = j;
            }
            const OilWell* best_well_obj = best_well >= 0 ? dynamic_cast<const OilWell*>(game_state.generals[best_well]) : nullptr;
            if (best_well_obj && near_map[best_well_obj->position] <= 4.0 && (!militia_plan || militia_plan->target->position != best_well_obj->position)) {
                // 如果民兵能占领就找民兵了
                Militia_analyzer analyzer(game_state);
                auto plan = analyzer.search_plan_from_provider(best_well_obj, game_state.generals[my_seat]);
                // 要么仍然能保持威慑，要么是占领中立油井
                if (plan && plan->army_used <= curr_army - 1 && (best_well_obj->player == -1 || curr_army - plan->army_used >= deterrence_analyzer->min_army)) {
                    // 但是不能被别人威胁
                    bool safe = true;
                    if (most_danger.enemy) {
                        Deterrence_analyzer enemy_deter(most_danger.enemy, general, game_state.coin[1-my_seat], game_state);
                        if (curr_army - plan->army_used < enemy_deter.target_max_army && Dist_map::effect_dist(general->position, most_danger.enemy->position, true, game_state.get_mobility(1-my_seat)) < 0)
                            safe = false;
                    }
                    if (safe) {
                        logger.log(LOG_LEVEL_INFO, "\t[Militia] Directly calling for militia to occupy %s, plan size %d", best_well_obj->position.str().c_str(), plan->plan.size());
                        militia_plan.emplace(*plan);
                        next_action_index = 0;
                        continue; // 将领等待1回合
                    }
                }
            }

            // 防御：优先防御cluster中的油田
            bool defence_triggered = false;
            Path_find_config enemy_dist_cfg(1.0, game_state.has_swamp_tech(1-my_seat));
            enemy_dist_cfg.custom_dist = enemy_pathfind_cost;
            if (cluster) for (const OilWell* well : cluster->wells) {
                if (well->player != my_seat) continue;

                // 若敌方的到达时间小于等于我方，则转入防御
                Dist_map oil_dist(game_state, well->position, Path_find_config(1.0));
                double my_arrival_time = oil_dist[general->position] / general->mobility_level;
                if (oil_dist[general->position] >= Dist_map::MAX_DIST) continue; // 无法防御走不到的油井

                Dist_map enemy_dist(game_state, well->position, enemy_dist_cfg);
                for (int j = 0; j < siz; ++j) {
                    const Generals* enemy = game_state.generals[j];
                    if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;

                    if (enemy_dist[enemy->position] / enemy->mobility_level <= my_arrival_time) {
                        strategies.emplace_back(General_strategy{i, General_strategy_type::DEFEND, Strategy_target(well->position, most_danger)});
                        logger.log(LOG_LEVEL_INFO, "[Allocate] General %s defend oil well %s under [%s]",
                                   general->position.str().c_str(), well->position.str().c_str(), most_danger.tactic.str().c_str());

                        defence_triggered = true;
                        break;
                    }
                }
                if (defence_triggered) break;
            }
            if (defence_triggered) continue;

            // 占领：cluster中的油田
            Dist_map dist_map(game_state, general->position, Path_find_config{curr_army <= 20 ? 3.0 : 2.0, game_state.has_swamp_tech(my_seat)});
            if (cluster) {
                bool found = false;
                for (const OilWell* well : cluster->wells) {
                    if (game_state[well->position].player != my_seat && dist_map[well->position] < Dist_map::MAX_DIST) {
                        strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{well->position, most_danger}});
                        logger.log(LOG_LEVEL_INFO, "[Allocate] General %s -> well %s (cluster)", general->position.str().c_str(), well->position.str().c_str());
                        found = true;
                        break;
                    }
                }
                if (found) continue;
            }

            // 否则取最近油田
            if (best_well == -1 || dist_map[game_state.generals[best_well]->position] > Dist_map::MAX_DIST)
                logger.log(LOG_LEVEL_WARN, "[Allocate] No oil well found for general at %s", general->position.str().c_str());
            else {
                strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position, most_danger}});
                logger.log(LOG_LEVEL_INFO, "[Allocate] General %s -> well %s", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
            }
        }
    }

    void execute_strategy() {
        for (const General_strategy& strategy : strategies) {
            const Generals* general = game_state.generals[strategy.general_id];
            int curr_army = game_state[general->position].army;

            if (strategy.type == General_strategy_type::DEFEND) {
                const Coord& target = strategy.target.coord;
                Dist_map dist_map(game_state, target, Path_find_config{2.0, game_state.has_swamp_tech(my_seat)});
                Direction dir = dist_map.direction_to_origin(general->position);
                Coord next_pos = general->position + DIRECTION_ARR[dir];

                const Cell& next_cell = game_state[next_pos];
                int next_cell_army = ceil(next_cell.army * game_state.defence_multiplier(next_pos));

                // 不踏入危险区，可能会跟direction_to_origin冲突
                const Generals* enemy = strategy.target.general;
                const Critical_tactic& tactic = strategy.target.danger->tactic;
                if (enemy && Dist_map::effect_dist(next_pos, enemy->position, tactic.can_rush, game_state.get_mobility(1-my_seat)) < 0) {
                    logger.log(LOG_LEVEL_INFO, "\tGeneral %s avoiding danger zone", general->position.str().c_str());
                    continue;
                }

                if (next_pos == target || curr_army <= 1) logger.log(LOG_LEVEL_INFO, "\t[Defend] General stay at %s", general->position.str().c_str());
                else if (curr_army - 1 > (next_cell.player == my_seat ? 0 : next_cell_army)) {
                    logger.log(LOG_LEVEL_INFO, "\t[Defend] General at %s -> %s", general->position.str().c_str(), next_pos.str().c_str());
                    add_operation(Operation::move_army(general->position, dir, curr_army - 1));
                    add_operation(Operation::move_generals(strategy.general_id, next_pos));
                    remain_move_count -= 1;
                }
            } else if (strategy.type == General_strategy_type::OCCUPY) {
                const Coord& target = strategy.target.coord;
                Dist_map dist_map(game_state, target, Path_find_config{2.0, game_state.has_swamp_tech(my_seat)});
                Direction dir = dist_map.direction_to_origin(general->position);
                Coord next_pos = general->position + DIRECTION_ARR[dir];

                const Cell& next_cell = game_state[next_pos];
                int next_cell_army = ceil(next_cell.army * game_state.defence_multiplier(next_pos));

                const Generals* enemy = strategy.target.general;
                const Critical_tactic& tactic = strategy.target.danger->tactic;

                // 已经到旁边了就直接占领
                if (next_pos == target) {
                    bool safe = true;
                    if (enemy) {
                        Deterrence_analyzer enemy_deter(enemy, general, game_state.coin[1-my_seat], game_state);
                        if (curr_army - (next_cell_army + 1) < enemy_deter.target_max_army && Dist_map::effect_dist(general->position, enemy->position, true, game_state.get_mobility(1-my_seat)) < 0)
                            safe = false;
                    }
                    // 能占就占
                    if (safe && curr_army - 1 > next_cell_army) {
                        add_operation(Operation::move_army(general->position, dir, next_cell_army + 1));
                        remain_move_count -= 1;
                    } else logger.log(LOG_LEVEL_INFO, "\t[Occupy] General at %s -> %s, but not safe", general->position.str().c_str(), next_pos.str().c_str());
                    continue;
                }

                // 如果民兵正在占，则不再行动
                if (militia_plan && militia_plan->target->position == target) {
                    logger.log(LOG_LEVEL_INFO, "\t[Occupy] Waiting for militia action");
                    continue;
                }

                // 不踏入危险区，可能会跟direction_to_origin冲突
                if (enemy && Dist_map::effect_dist(next_pos, enemy->position, tactic.can_rush, game_state.get_mobility(1-my_seat)) < 0) {
                    // 如果民兵能占领就找民兵
                    if (strategy.type == General_strategy_type::OCCUPY && (!militia_plan || militia_plan->target->position != target)) {
                        Militia_analyzer analyzer(game_state);
                        auto plan = analyzer.search_plan_from_provider(game_state[target].generals, game_state.generals[my_seat]);
                        if (plan && plan->army_used <= curr_army - 1 && (curr_army - plan->army_used >= deterrence_analyzer->min_army || plan->plan.size() <= 3)) {
                            logger.log(LOG_LEVEL_INFO, "\t[Occupy] Calling for militia to occupy %s, plan size %d", target.str().c_str(), plan->plan.size());
                            militia_plan.emplace(*plan);
                            next_action_index = 0;
                            continue;
                        }
                    }

                    logger.log(LOG_LEVEL_INFO, "\tGeneral %s avoiding danger zone", general->position.str().c_str());
                    continue;
                }

                // 否则再往前走一步
                logger.log(LOG_LEVEL_INFO, "\t[Occupy] General at %s -> %s", general->position.str().c_str(), next_pos.str().c_str());
                if (curr_army > 1 && (curr_army - 1 > (next_cell.player == my_seat ? 0 : next_cell.army))) {
                    add_operation(Operation::move_army(general->position, dir, curr_army - 1));
                    add_operation(Operation::move_generals(strategy.general_id, next_pos));
                    remain_move_count -= 1;
                }
            } else if (strategy.type == General_strategy_type::ATTACK) {

            } else if (strategy.type == General_strategy_type::RETREAT) {
                const Generals* enemy = strategy.target.general;
                const Critical_tactic& tactic = strategy.target.danger->tactic;

                // 寻找能走到的最大有效距离
                Coord best_pos{-1, -1};
                int max_eff_dist = Dist_map::effect_dist(general->position, enemy->position, tactic.can_rush, game_state.get_mobility(1-my_seat));
                for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
                    Coord next_pos = general->position + DIRECTION_ARR[dir];
                    if (!next_pos.in_map() || !game_state.can_general_step_on(next_pos, my_seat)) continue;
                    if (curr_army - 1 <= -game_state.eff_army(next_pos, my_seat)) continue;

                    int eff_dist = Dist_map::effect_dist(next_pos, enemy->position, tactic.can_rush, game_state.get_mobility(1-my_seat));
                    logger.log(LOG_LEVEL_INFO, "[Retreat] \tNext pos %s, eff dist %d", next_pos.str().c_str(), eff_dist);
                    if (eff_dist > max_eff_dist) {
                        best_pos = next_pos;
                        max_eff_dist = eff_dist;
                    }
                }

                // 尝试移动（没有检查兵数的问题）
                if (best_pos.in_map()) {
                    if (curr_army > 1) {
                        add_operation(Operation::move_army(general->position, from_coord(general->position, best_pos), curr_army - 1));
                        remain_move_count -= 1;
                    }
                    add_operation(Operation::move_generals(strategy.general_id, best_pos));
                    logger.log(LOG_LEVEL_INFO, "[Retreat] General at %s -> %s, dist -> %d", general->position.str().c_str(), best_pos.str().c_str(), max_eff_dist);
                }
                // 找不到解，尝试升级防御
                if (max_eff_dist < 0) {
                    Deterrence_analyzer analyzer(enemy, general, game_state.coin[1-my_seat], game_state);

                    // 假如是真的威胁，则升级防御
                    if (analyzer.rush_tactic && oil_after_op >= general->defence_upgrade_cost()) {
                        oil_after_op -= general->defence_upgrade_cost();
                        add_operation(Operation::upgrade_generals(general->id, QualityType::DEFENCE));
                        logger.log(LOG_LEVEL_INFO, "[Retreat] General at %s upgrade defence due to danger", general->position.str().c_str());
                    }
                }

            } else assert(!"Invalid strategy type");
        }
    }

    int next_action_index = 0;
    std::optional<Militia_plan> militia_plan;
    void militia_move() {
        if (militia_plan && next_action_index >= (int)militia_plan->plan.size()) militia_plan.reset();

        // 10回合分析一次，不允许打断主将分配的任务
        if ((game_state.round % 10 == 1 || !militia_plan) && (militia_plan ? militia_plan->area != nullptr : true)) {
            Militia_analyzer analyzer(game_state);

            // 寻找总时长尽量小的方案，且要求集合用时不超过7步
            std::optional<Militia_plan> best_plan;
            for (int i = PLAYER_COUNT, siz = game_state.generals.size(); i < siz; ++i) {
                const Generals* target = game_state.generals[i];
                if (target->player == my_seat || !game_state.can_soldier_step_on(target->position, my_seat)) continue;

                std::optional<Militia_plan> plan = analyzer.search_plan_from_militia(target);
                if (!plan || plan->gather_steps > 7) continue;
                if (dynamic_cast<const SubGenerals*>(target) && plan->gather_steps > 5) continue; // 副将不能超过5步

                if (!best_plan) {
                    best_plan.emplace(plan.value());
                    continue;
                }

                // 比较两种方案
                bool better = false;
                better |= (plan->plan.size() < best_plan->plan.size());
                better |= (plan->plan.size() == best_plan->plan.size() && plan->army_used < best_plan->army_used);
                if (better) best_plan.emplace(plan.value());
            }

            if (best_plan && best_plan->plan.size() <= 16) {
                militia_plan.emplace(best_plan.value());
                next_action_index = 0;

                logger.log(LOG_LEVEL_INFO, "[Militia] Militia plan size %d, gather %d, found for target %s:", militia_plan->plan.size(), militia_plan->gather_steps, militia_plan->target->position.str().c_str());
                for (const auto& op : militia_plan->plan) logger.log(LOG_LEVEL_INFO, "\t%s->%s", op.first.str().c_str(), (op.first + DIRECTION_ARR[op.second]).str().c_str());
            }
        }

        if (!remain_move_count) return;
        // 无任务则随机扩展
        if (!militia_plan) {
            // 寻找可扩展的格子
            for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
                Coord pos{x, y};
                const Cell& cell = game_state[pos];
                if (cell.player != my_seat || cell.army <= 1) continue;
                if (cell.generals && dynamic_cast<const OilWell*>(cell.generals) == nullptr) continue; // 排除主副将格

                // 尝试扩展
                for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
                    Coord next_pos = pos + DIRECTION_ARR[dir];
                    if (!next_pos.in_map() || !game_state.can_soldier_step_on(next_pos, my_seat)) continue; // 排除无法走上的沼泽

                    const Cell& next_cell = game_state[next_pos];
                    if (next_cell.player == my_seat || next_cell.type == CellType::DESERT) continue; // 排除自己的格子和沙漠格
                    if (next_cell.player == 1-my_seat && next_cell.army > cell.army - 1) continue; // 排除攻不下（无法中立化）的对方格
                    if (next_cell.player == -1 && next_cell.army >= cell.army - 1) continue; // 中立格弄成中立也没用

                    // 进行移动
                    logger.log(LOG_LEVEL_INFO, "[Militia] Expanding to %s", next_pos.str().c_str());
                    add_operation(Operation::move_army(pos, static_cast<Direction>(dir), cell.army - 1));
                    remain_move_count -= 1;
                    break;
                }
                if (!remain_move_count) return;
            }
        }
        // 否则执行计划
        else while (remain_move_count && next_action_index < (int)militia_plan->plan.size()) {
            // 一些初步检查
            const Coord& pos = militia_plan->plan[next_action_index].first;
            const Cell& cell = game_state[pos];
            const OilWell* well = dynamic_cast<const OilWell*>(cell.generals);

            // 是否是从主将上提取兵力的第一步操作
            bool take_army_from_general = (cell.generals && cell.generals->id == my_seat && next_action_index == 0);
            if (take_army_from_general)
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, take %d army from general", next_action_index+1, militia_plan->army_used);

            // 暂时把副将当作工厂
            if ((cell.player != my_seat || cell.army <= 1 || (cell.generals && cell.generals->id == my_seat)) && !take_army_from_general) {
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, invalid position", next_action_index+1);
                militia_plan.reset();
                break;
            }
            if (take_army_from_general && cell.army - 1 < militia_plan->army_used) {
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, army not enough to take %d from general", next_action_index+1, militia_plan->army_used);
                militia_plan.reset();
                break;
            }

            logger.log(LOG_LEVEL_INFO, "[Militia] Executing plan step %d, %s->%s",
                        next_action_index+1, pos.str().c_str(), (pos + DIRECTION_ARR[militia_plan->plan[next_action_index].second]).str().c_str());
            add_operation(Operation::move_army(pos, militia_plan->plan[next_action_index].second, take_army_from_general ? militia_plan->army_used : cell.army - 1));
            remain_move_count -= 1;
            next_action_index += 1;
        }
    }
};

int main() {
    // 设置随机种子
    std::srand(std::time(nullptr));

    myAI ai;
    ai.run();
    return 0;
}
