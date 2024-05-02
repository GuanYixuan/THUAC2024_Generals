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
    OCCUPY,
    RETREAT,
    DEFEND,
    ATTACK,
};
class Strategy_target {
public:
    Coord coord;
    const Generals* general;
    std::optional<Attack_info> attack_info;

    // 构造函数：占领或防御
    Strategy_target(const Coord& coord) noexcept : coord(coord), general(nullptr) {}
    // 构造函数：进攻
    Strategy_target(const Generals* general) noexcept : coord(general->position), general(general) {}
    // 构造函数：撤退
    Strategy_target(const Attack_info& _attack_info) noexcept : coord(), general(nullptr), attack_info(_attack_info) {}
};
class General_strategy {
public:
    int general_id;

    General_strategy_type type;

    Strategy_target target;
};

enum class Militia_action_type {
    OCCUPY_FREE,
    OCCUPY_NEAR,
    OCCUPY_MAINGENERAL,
    SUPPORT
};
struct Militia_move_task {
    Militia_action_type type;
    Militia_plan plan;
    int next_action;

    int start_time;

    Militia_move_task(Militia_action_type type, const Militia_plan& plan, int start_time) noexcept :
        type(type), plan(plan), next_action(0), start_time(start_time) {}
    std::pair<Coord, Direction>& operator[] (int index) noexcept { return plan.plan[index]; }

    int step_count() const noexcept { return plan.plan.size(); }
};

class myAI : public GameController {
public:
    std::vector<General_strategy> strategies;

    std::optional<Oil_cluster> cluster;

    bool first_oil = true;
    bool late_game = false;

    int oil_savings;
    int oil_after_op;
    int remain_move_count;

    bool army_disadvantage;
    bool oil_prod_advantage;

    std::optional<Deterrence_analyzer> deterrence_analyzer;

    // 站在敌方立场进行路径搜索时的额外开销，体现了我方的威慑范围
    int enemy_pathfind_cost[Constant::col][Constant::row];

    void main_process() {
        // 初始操作
        if (game_state.round == 1) {
            logger.log(LOG_LEVEL_INFO, "Seat %d\n", my_seat);

            add_operation(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));
            return;
        } else if (game_state.round == 2) {
            std::vector<Oil_cluster> clusters{identify_oil_clusters()};
            if (!clusters.empty()) {
                cluster = clusters[0];
                logger.log(LOG_LEVEL_INFO, "Selected oil cluster: %s", cluster->str().c_str());
            }
        }

        // 识别“初始时聚兵”的策略
        // identify_aggregate_strategy();

        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);
        int my_army = game_state[main_general->position].army;
        int enemy_army = game_state[enemy_general->position].army;
        int army_around_enemy = 0; // 敌方主将旁边的军队数量
        for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
            Coord new_pos = main_general->position + DIRECTION_ARR[dir];
            if (new_pos.in_map() && game_state[new_pos].player == 1 - my_seat)
                army_around_enemy = std::max(army_around_enemy, game_state[new_pos].army);
        }
        int max_single_army = 0;
        for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
            const Cell& cell = game_state[Coord{x, y}];
            if (cell.player != 1 - my_seat) continue;
            if (cell.generals && !dynamic_cast<const OilWell*>(cell.generals)) continue;
            max_single_army = std::max(max_single_army, cell.army);
        }
        enemy_army += enemy_aggregate ? max_single_army : army_around_enemy;

        // 参数更新
        oil_after_op = game_state.coin[my_seat];
        remain_move_count = game_state.rest_move_step[my_seat];
        int oil_production = game_state.calc_oil_production(my_seat);
        // 判断主将的兵是否处于劣势
        army_disadvantage = my_army * 1.5 < enemy_army ||
                            (my_army + 15 * enemy_general->produce_level) * 1.5 < enemy_army + 15 * main_general->produce_level;
        if (army_disadvantage) logger.log(LOG_LEVEL_INFO, "[Assess] Army disadvantage (%d vs %d)", my_army, enemy_army);
        deterrence_analyzer.emplace(main_general, enemy_general, oil_after_op, game_state, army_around_enemy);
        // 判断产量是否有优势
        oil_prod_advantage = oil_production >= game_state.calc_oil_production(1 - my_seat) + 4;
        if (oil_prod_advantage) logger.log(LOG_LEVEL_INFO, "[Assess] Oil production advantage");
        // 判断是否是“后期”
        late_game = game_state.coin[my_seat] >= 180;

        // 计算我方威慑范围
        if (deterrence_analyzer->rush_tactic || deterrence_analyzer->non_rush_tactic) { // 有威慑
            bool has_rush = deterrence_analyzer->rush_tactic.has_value();
            for (int x = 0; x < Constant::col; ++x)
                for (int y = 0; y < Constant::row; ++y)
                    enemy_pathfind_cost[x][y] = Dist_map::effect_dist(Coord{x, y}, main_general->position, has_rush, game_state.get_mobility(my_seat)) < 0 ? 3 : 0;
        } else memset(enemy_pathfind_cost, 0, sizeof(enemy_pathfind_cost)); // 无威慑

        // 计算油量目标值
        if (game_state.round > 15 || my_army < enemy_army)
            oil_savings = 35;
        else oil_savings = 20;
        if (game_state.round > 20) {
            if (oil_after_op + oil_production * 10 >= deterrence_analyzer->min_oil)
                oil_savings = std::max(oil_savings, deterrence_analyzer->min_oil);
            else oil_savings = 70;
        }
        logger.log(LOG_LEVEL_INFO, "Oil %d(+%d) vs %d(+%d), savings %d%s",
                   oil_after_op, oil_production, game_state.coin[1 - my_seat], game_state.calc_oil_production(1 - my_seat), oil_savings,
                   oil_prod_advantage ? " [Prod advantage]" : "");
        logger.log(LOG_LEVEL_INFO, "Army %d(+%d) vs %d(+%d)%s%s",
                   my_army, main_general->produce_level, enemy_army, enemy_general->produce_level,
                   enemy_aggregate ? " [Aggregate]" : "",
                   army_disadvantage ? " [Disadvantage]" : "");

        // 进攻搜索
        Attack_searcher searcher(my_seat, game_state);
        std::optional<Attack_info> ret = searcher.search();
        if (ret) {
            logger.log(LOG_LEVEL_INFO, "Critical tactic found");
            for (const Operation& op : ret->ops) {
                logger.log(LOG_LEVEL_INFO, "\t Op: %s", op.str().c_str());
                add_operation(op);
            }
            return;
        }

        // 对着敌方主将放核弹
        if (game_state.super_weapon_unlocked[my_seat] && game_state.super_weapon_cd[my_seat] == 0) {
            add_operation(Operation::use_superweapon(WeaponType::NUCLEAR_BOOM, enemy_general->position));
            logger.log(LOG_LEVEL_INFO, "Use superweapon!");
        }

        // 检查当前的支援计划
        if (militia_task && militia_task->type == Militia_action_type::SUPPORT && militia_task->plan.target_pos != main_general->position) {
            logger.log(LOG_LEVEL_INFO, "[Support] Militia support plan expired");
            militia_task.reset();
        }

        // 考虑进行支援
        if (!militia_task || militia_task->type == Militia_action_type::OCCUPY_FREE)
        if (game_state.round >= 11 && (army_disadvantage || my_army < 20 || game_state.round <= 20)) {
            logger.log(LOG_LEVEL_INFO, "[Support] Consider support");

            Militia_analyzer m_analyzer(game_state);
            std::optional<Militia_plan> best_plan;
            for (int step = 2; step <= 12; step += 2) {
                std::optional<Militia_plan> plan = m_analyzer.search_plan_from_militia(main_general, step);
                if (!plan) continue;
                if (plan->army_used < step) continue; // 性价比太低

                if (!best_plan || plan->army_used > best_plan->army_used) best_plan = plan;
            }
            if (best_plan) {
                militia_task.emplace(Militia_action_type::SUPPORT, *best_plan, game_state.round);
                logger.log(LOG_LEVEL_INFO, "[Support] Militia support plan size %d, army %d",
                           militia_task->step_count(), best_plan->army_used);
                for (const auto& op : best_plan->plan)
                    logger.log(LOG_LEVEL_INFO, "\t%s->%s", op.first.str().c_str(), (op.first + DIRECTION_ARR[op.second]).str().c_str());
            } else logger.log(LOG_LEVEL_INFO, "[Support] No support plan found");
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
        static constexpr double MIN_ENEMY_DIST = 8.0;
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
            int enemy_dist_to_center = enemy_dist[center_well->position] / game_state.generals[1 - my_seat]->mobility_level;
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

    bool enemy_aggregate = false;
    void identify_aggregate_strategy() {
        static double feature_score = 0;
        static int prev_single_army = 0;

        // 计算敌方单兵最大军队数量
        int max_single_army = 0;
        for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
            const Cell& cell = game_state[Coord{x, y}];
            if (cell.player != 1 - my_seat) continue;
            if (cell.generals && dynamic_cast<const OilWell*>(cell.generals) == nullptr) continue;
            max_single_army = std::max(max_single_army, cell.army);
        }

        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);

        // 特征：对方主将不移动，全部行动力用于移兵
        bool enemy_general_stay = !std::any_of(last_enemy_ops.begin(), last_enemy_ops.end(),
                                               [](const Operation& op) { return op.opcode == OperationType::MOVE_GENERALS && op[0] == 1 - my_seat; });
        enemy_general_stay &= std::count_if(last_enemy_ops.begin(), last_enemy_ops.end(),
                                            [](const Operation& op) { return op.opcode == OperationType::MOVE_ARMY; }) == game_state.get_mobility(1 - my_seat);

        // 特征：敌方单兵最大军队量越来越大
        bool enemy_single_army_grow = max_single_army > prev_single_army;
        prev_single_army = max_single_army;

        // 特征：敌方主将与我方距离较远
        bool enemy_general_far = Dist_map::effect_dist(main_general->position, enemy_general->position, true, PLAYER_MOVEMENT_VALUES[0]) > 0;

        if (!enemy_general_stay) feature_score = 0;
        else {
            if (enemy_general_far) feature_score += 1;
            if (!enemy_single_army_grow) feature_score -= 0.5;
        }

        if (feature_score >= 4 && !enemy_aggregate) {
            enemy_aggregate = true;
            logger.log(LOG_LEVEL_INFO, "[Assess] Enemy aggregate strategy detected!");
        }
        if (!enemy_aggregate)
            logger.log(LOG_LEVEL_INFO, "[Assess] Aggregate detection score: %.1f, stay = %d, far = %d, grow = %d",
                feature_score, enemy_general_stay, enemy_general_far, enemy_single_army_grow);
    }

    void assess_upgrades() {
        // 计算“相遇时间”（仅考虑主将）
        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);
        Dist_map my_dist(game_state, main_general->position, Path_find_config{1.0, game_state.has_swamp_tech(my_seat)});
        Dist_map enemy_dist(game_state, enemy_general->position, Path_find_config{1.0, game_state.has_swamp_tech(1-my_seat)});
        int approach_time = (std::min(my_dist[enemy_general->position], enemy_dist[main_general->position]) - 5 - enemy_general->mobility_level) /
                            (main_general->mobility_level + enemy_general->mobility_level) * 1.5;
        approach_time = std::max(0, approach_time);
        int oil_on_approach = oil_after_op + game_state.calc_oil_production(my_seat) * approach_time;
        if (approach_time > 100) oil_on_approach = oil_after_op; // 假如无法碰在一起，认为储油量就是当前量
        logger.log(LOG_LEVEL_INFO, "[Assess] Approach time: %d, oil on approach: %d, first = %d", approach_time, oil_on_approach, first_oil);

        // 考虑油井升级
        Path_find_config enemy_dist_cfg(1.0, game_state.has_swamp_tech(1-my_seat));
        enemy_dist_cfg.custom_dist = enemy_pathfind_cost;
        bool unlock_upgrade_3 = main_general->produce_level >= Constant::GENERAL_PRODUCTION_VALUES[2];
        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const OilWell* well = dynamic_cast<const OilWell*>(game_state.generals[i]);
            if (well == nullptr || well->player != my_seat) continue;

            // 主将未升到产量为4时且石油有优势时，不再升级油井
            if (main_general->produce_level < Constant::GENERAL_PRODUCTION_VALUES[2] && oil_prod_advantage) break;

            int tire = well->production_tire();
            int cost = well->production_upgrade_cost();
            if (oil_after_op < cost) continue;
            if (tire >= 2 && !unlock_upgrade_3) continue;

            // 检查油量
            bool enough_oil = oil_after_op >= oil_savings + cost;
            enough_oil |= oil_on_approach + (Constant::OILWELL_PRODUCTION_VALUES[tire+1] - Constant::OILWELL_PRODUCTION_VALUES[tire]) * approach_time >= oil_savings + cost;
            if (!enough_oil && !first_oil) continue;

            // 以油井为中心计算到敌方的距离（考虑我方威慑）
            Dist_map dist_map(game_state, well->position, enemy_dist_cfg);
            double min_dist = std::numeric_limits<double>::max();
            for (int j = 0; j < siz; ++j) {
                const Generals* enemy = game_state.generals[j];
                if (enemy->player != 1 - my_seat || dynamic_cast<const OilWell*>(enemy) != nullptr) continue;
                min_dist = std::min(min_dist, dist_map[enemy->position]);
            }
            if (min_dist >= 6 + 3 * tire || (first_oil && game_state.round <= 15)) {
                first_oil = false;

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
        int enemy_eff_dist = Dist_map::effect_dist(main_general->position, enemy_general->position, true, PLAYER_MOVEMENT_VALUES[0]);
        bool aggregate_handle = enemy_aggregate && enemy_general->production_tire() == 2 &&
                                oil_on_approach >= 20 - std::min(20, 5*Dist_map::effect_dist(main_general->position, enemy_general->position, true, PLAYER_MOVEMENT_VALUES[0])) + main_general->production_upgrade_cost();
        if (oil_after_op >= main_general->production_upgrade_cost() &&
            (oil_on_approach >= oil_savings + main_general->production_upgrade_cost() || aggregate_handle)) {

            oil_after_op -= main_general->production_upgrade_cost();
            add_operation(Operation::upgrade_generals(my_seat, QualityType::PRODUCTION));
            logger.log(LOG_LEVEL_INFO, "[Upgrade%s] Main general upgrade production to %d",
                       aggregate_handle ? ", aggregate handle" : "", main_general->produce_level + 1);
        }
        // 主将防御升级
        else if (unlock_upgrade_3 && oil_after_op >= main_general->defence_upgrade_cost() &&
                 oil_on_approach >= oil_savings + main_general->defence_upgrade_cost() &&
                 game_state[main_general->position].army > (main_general->defence_tire() == 0 ? 40 : 80)) {

            oil_after_op -= main_general->defence_upgrade_cost();
            add_operation(Operation::upgrade_generals(my_seat, QualityType::DEFENCE));
            logger.log(LOG_LEVEL_INFO, "[Upgrade] Main general upgrade defence to %.0f", main_general->defence_tire());
        }
        // 油足够多，则考虑升级行动力
        // 若敌方已升级此科技，则将延迟量50取消
        else if (oil_after_op >= PLAYER_MOVEMENT_COST[mob_tire] &&
                 oil_on_approach >= oil_savings + PLAYER_MOVEMENT_COST[mob_tire] + (game_state.get_mobility(1 - my_seat) > game_state.get_mobility(my_seat) ? 0 : 50)) {

            oil_after_op -= PLAYER_MOVEMENT_COST[mob_tire];
            add_operation(Operation::upgrade_tech(TechType::MOBILITY));
            logger.log(LOG_LEVEL_INFO, "[Upgrade] Upgrade mobility to %d", game_state.get_mobility(my_seat));
        }
        // 油更加多，则考虑升级沼泽科技
        // 若敌方已升级此科技，则将延迟量100取消
        else if (!game_state.has_swamp_tech(my_seat) && oil_after_op >= swamp_immunity &&
                 oil_on_approach >= oil_savings + swamp_immunity + (game_state.has_swamp_tech(1-my_seat) ? 0 : 100)) {

            oil_after_op -= swamp_immunity;
            add_operation(Operation::upgrade_tech(TechType::IMMUNE_SWAMP));
            logger.log(LOG_LEVEL_INFO, "[Upgrade] Upgrade swamp immunity");
        }
        // 最后考虑升级超级武器
        else if (!game_state.super_weapon_unlocked[my_seat] && oil_after_op >= unlock_super_weapon &&
                 oil_on_approach >= oil_savings + unlock_super_weapon + (game_state.super_weapon_unlocked[1-my_seat] ? 0 : 100)) {

            oil_after_op -= unlock_super_weapon;
            add_operation(Operation::upgrade_tech(TechType::UNLOCK));
        }
    }

    void update_strategy() {
        strategies.clear();

        const MainGenerals* main_general = dynamic_cast<const MainGenerals*>(game_state.generals[my_seat]);
        const MainGenerals* enemy_general = dynamic_cast<const MainGenerals*>(game_state.generals[1 - my_seat]);
        int enemy_lookahead_oil = game_state.coin[1 - my_seat] + game_state.calc_oil_production(1 - my_seat) * 2; // 取两回合后的油量

        int my_prod = game_state.calc_oil_production(my_seat);

        for (int i = 0, siz = game_state.generals.size(); i < siz; ++i) {
            const Generals* general = game_state.generals[i];
            bool is_subgeneral = dynamic_cast<const SubGenerals*>(general) != nullptr;
            if (general->player != my_seat || dynamic_cast<const OilWell*>(general)) continue;

            // 感觉不对就炸
            if (army_disadvantage && oil_after_op >= std::max(oil_savings - 15, GENERAL_SKILL_COST[SkillType::STRIKE]) &&
                enemy_general->position.in_attack_range(general->position) && !general->cd(SkillType::STRIKE)) {

                add_operation(Operation::generals_skill(general->id, SkillType::STRIKE, enemy_general->position));
                oil_after_op -= GENERAL_SKILL_COST[SkillType::STRIKE];
                logger.log(LOG_LEVEL_INFO, "[Skill] General %s strike!", general->position.str().c_str());
            }


            // 副将炸主将，只允许“油较多”时使用
            if (oil_after_op >= 120 && !general->cd(SkillType::STRIKE) && is_subgeneral) {
                if (enemy_general->position.in_attack_range(general->position)) {
                    add_operation(Operation::generals_skill(general->id, SkillType::STRIKE, enemy_general->position));
                    oil_after_op -= GENERAL_SKILL_COST[SkillType::STRIKE];
                    logger.log(LOG_LEVEL_INFO, "[Skill] Sub general %s strike!", general->position.str().c_str());
                }
            }

            int curr_army = game_state[general->position].army;
            double defence_mult = game_state.defence_multiplier(general->position);
            if (curr_army <= 1) continue;
            logger.log(LOG_LEVEL_INFO, "[Assess] General %s with army %d, defence mult %.2f, enemy lookahead oil %d",
                       general->position.str().c_str(), curr_army, defence_mult, enemy_lookahead_oil);

            // 考虑是否在危险范围内（新方法）
            GameState temp_state;
            temp_state.copy_as(game_state);
            Attack_searcher searcher(1-my_seat, temp_state);
            std::optional<Attack_info> atk_search_result = searcher.search(enemy_lookahead_oil - game_state.coin[1 - my_seat]);
            int threat_eff_dist = atk_search_result ?
                Dist_map::effect_dist(atk_search_result->origin, general->position, atk_search_result->tactic.can_rush, game_state.get_mobility(1-my_seat)) : 1000;

            // 当主将在攻击范围内时撤退
            if (atk_search_result && !is_subgeneral) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::RETREAT, Strategy_target(*atk_search_result)});
                logger.log(LOG_LEVEL_INFO, "[Allocate:retreat] General %s retreat %s, eff dist %d",
                    general->position.str().c_str(), atk_search_result->origin.str().c_str(), threat_eff_dist);
                continue;
            }

            // 假如能够威慑敌方，但敌方无法威慑我，则主动贴近
            int enemy_eff_dist = Dist_map::effect_dist(general->position, enemy_general->position, true, game_state.get_mobility(my_seat));
            Deterrence_analyzer sub_general_det(general, enemy_general, game_state.coin[my_seat], game_state);
            bool atk_cond = (oil_after_op >= oil_savings || (is_subgeneral && oil_after_op >= sub_general_det.min_oil)) && (my_prod > 0 && enemy_eff_dist <= 1 && threat_eff_dist > enemy_eff_dist);
            atk_cond |= oil_after_op >= 300;
            if (atk_cond) { // threat_eff_dist在找不到威胁时还是有问题的
                strategies.emplace_back(General_strategy{i, General_strategy_type::ATTACK, Strategy_target(enemy_general)});
                logger.log(LOG_LEVEL_INFO, "[Allocate:attack] General %s attack enemy general %s", general->position.str().c_str(), enemy_general->position.str().c_str());
                continue;
            }

            // （主将）假如不进攻也不撤退，则等待支援完成
            if (!is_subgeneral && militia_task && militia_task->type == Militia_action_type::SUPPORT) {
                logger.log(LOG_LEVEL_INFO, "[Allocate:wait] General %s wait for militia support", general->position.str().c_str());
                continue;
            }

            // 占领：4格内油田或副将考虑使用民兵占领（打副将仅在后期有效）
            Dist_map near_map(game_state, general->position, Path_find_config{1.0, game_state.has_swamp_tech(my_seat)});
            int best_well = -1;
            for (int j = PLAYER_COUNT; j < siz; ++j) {
                const Generals* oil_well = game_state.generals[j];
                if (oil_well->player == my_seat) continue;
                if (dynamic_cast<const SubGenerals*>(oil_well) && !late_game) continue;
                if (best_well == -1 || near_map[oil_well->position] < near_map[game_state.generals[best_well]->position]) best_well = j;
            }
            const OilWell* best_well_obj = best_well >= 0 ? dynamic_cast<const OilWell*>(game_state.generals[best_well]) : nullptr;
            if (best_well_obj && near_map[best_well_obj->position] <= 4.0 &&
                (!militia_task || (militia_task->type != Militia_action_type::SUPPORT && militia_task->plan.target->position != best_well_obj->position))) {
                // 如果民兵能占领就找民兵了
                Militia_analyzer analyzer(game_state);
                std::optional<Militia_plan> plan = analyzer.search_plan_from_provider(best_well_obj, general);
                // 要么仍然能保持威慑，要么是占领中立油井
                if (plan && plan->army_used <= curr_army - 1 &&
                    ((best_well_obj->player == -1 && !army_disadvantage) || curr_army - plan->army_used >= deterrence_analyzer->min_army * 1.2)) {
                    // 不能被别人威胁
                    GameState temp_state;
                    temp_state.copy_as(game_state);
                    execute_operation(temp_state, my_seat, Operation::move_army(general->position, plan->plan[0].second, plan->army_used));

                    Attack_searcher searcher(1-my_seat, temp_state);
                    if (!searcher.search(enemy_lookahead_oil - game_state.coin[1 - my_seat])) {
                        logger.log(LOG_LEVEL_INFO, "\t[Militia] Directly calling for militia to occupy %s, plan size %d", best_well_obj->position.str().c_str(), plan->plan.size());
                        militia_task.emplace(Militia_action_type::OCCUPY_NEAR, *plan, game_state.round);
                        continue; // 将领等待1回合
                    } else logger.log(LOG_LEVEL_INFO, "\t[Militia] Cannot directly occupy %s due to enemy threat", best_well_obj->position.str().c_str());
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
                        strategies.emplace_back(General_strategy{i, General_strategy_type::DEFEND, Strategy_target(well->position)});
                        logger.log(LOG_LEVEL_INFO, "[Allocate:defend] General %s defend oil well %s under [%s]",
                                   general->position.str().c_str(), well->position.str().c_str(), atk_search_result ? atk_search_result->tactic.str().c_str() : "");

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
                        strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{well->position}});
                        logger.log(LOG_LEVEL_INFO, "[Allocate:occupy] General %s -> well %s (cluster)", general->position.str().c_str(), well->position.str().c_str());
                        found = true;
                        break;
                    }
                }
                if (found) continue;
            }

            // 否则取最近油田
            bool no_wandering = army_disadvantage &&
                                ((game_state.coin[my_seat] >= game_state.coin[1-my_seat] * 1.5 && game_state.coin[my_seat] >= 40) || oil_prod_advantage);
            no_wandering |= (enemy_aggregate && game_state.count_oil_wells(my_seat) > game_state.count_oil_wells(1-my_seat));
            no_wandering &= (!is_subgeneral); // 副将不受此约束
            if (best_well >= 0 && dist_map[game_state.generals[best_well]->position] < Dist_map::MAX_DIST && !no_wandering) {
                strategies.emplace_back(General_strategy{i, General_strategy_type::OCCUPY, Strategy_target{game_state.generals[best_well]->position}});
                logger.log(LOG_LEVEL_INFO, "[Allocate:occupy] General %s -> well %s", general->position.str().c_str(), game_state.generals[best_well]->position.str().c_str());
                continue;
            }
            logger.log(LOG_LEVEL_WARN, "[Allocate:no_action] No oil well found for general at %s", general->position.str().c_str());
        }
    }

    void execute_strategy() {
        for (const General_strategy& strategy : strategies) {
            const Generals* general = game_state.generals[strategy.general_id];
            int curr_army = game_state[general->position].army;

            if (strategy.type == General_strategy_type::DEFEND) {
                const Coord& target = strategy.target.coord;

                // 进行移动搜索
                logger.log(LOG_LEVEL_DEBUG, "\t[Defend] Move search:");
                General_mover mover(game_state, general, std::min(general->mobility_level, remain_move_count), target);
                auto move_plans = mover.search();
                for (const auto& move_plan : move_plans) {
                    logger.log(LOG_LEVEL_DEBUG, "\t\t[Defend] Move plan: %s", move_plan.c_str());
                }

                // 如果最优行动存在，则执行
                if (move_plans.size() && move_plans[0].step_count) {
                    const Move_plan& plan = move_plans[0];
                    for (const Operation& op : plan.ops) {
                        logger.log(LOG_LEVEL_INFO, "\t[Defend] Plan step: %s", op.str().c_str());
                        add_operation(op);
                    }
                    remain_move_count -= plan.step_count;
                } else logger.log(LOG_LEVEL_INFO, "\t[Defend] General %s has no plan to defend %s", general->position.str().c_str(), target.str().c_str());
            } else if (strategy.type == General_strategy_type::OCCUPY) {
                const Coord& target = strategy.target.coord;
                const Generals* enemy = strategy.target.general;

                // 已经到旁边了就直接占领
                if (general->position.dist_to(target) == 1 && remain_move_count) {
                    const Cell& next_cell = game_state[target];
                    int next_cell_army = ceil(next_cell.army * game_state.defence_multiplier(target));
                    bool safe = true;
                    if (enemy) {
                        Deterrence_analyzer enemy_deter(enemy, general, game_state.coin[1-my_seat], game_state);
                        if (curr_army - (next_cell_army + 1) < enemy_deter.target_max_army && Dist_map::effect_dist(general->position, enemy->position, true, game_state.get_mobility(1-my_seat)) < 0)
                            safe = false;
                    }
                    // 能占就占
                    if (safe && curr_army - 1 > next_cell_army) {
                        add_operation(Operation::move_army(general->position, from_coord(general->position, target), next_cell_army + 1));
                        remain_move_count -= 1;
                    } else logger.log(LOG_LEVEL_INFO, "\t[Occupy] General at %s -> %s, but not safe", general->position.str().c_str(), target.str().c_str());
                    continue;
                }

                // 如果民兵正在占，则不再行动
                if (militia_task && militia_task->plan.target->position == target) {
                    logger.log(LOG_LEVEL_INFO, "\t[Occupy] Waiting for militia action");
                    continue;
                }

                // 否则进行移动搜索
                logger.log(LOG_LEVEL_DEBUG, "\t[Occupy] Move search:");
                General_mover mover(game_state, general, std::min(general->mobility_level, remain_move_count), target);
                auto move_plans = mover.search();
                for (const auto& move_plan : move_plans) {
                    logger.log(LOG_LEVEL_DEBUG, "\t\t[Occupy] Move plan: %s", move_plan.c_str());
                }

                // 如果最优行动存在
                if (move_plans.size() && move_plans[0].step_count) {
                    const Move_plan& plan = move_plans[0];
                    for (const Operation& op : plan.ops) {
                        logger.log(LOG_LEVEL_INFO, "\t[Occupy] Plan step: %s", op.str().c_str());
                        add_operation(op);
                    }
                    remain_move_count -= plan.step_count;

                    // 已经到旁边了就直接占领
                    if (general->position.dist_to(target) == 1 && remain_move_count) {
                        const Cell& next_cell = game_state[target];
                        int next_cell_army = ceil(next_cell.army * game_state.defence_multiplier(target));
                        bool safe = true;
                        if (enemy) {
                            Deterrence_analyzer enemy_deter(enemy, general, game_state.coin[1-my_seat], game_state);
                            if (curr_army - (next_cell_army + 1) < enemy_deter.target_max_army && Dist_map::effect_dist(general->position, enemy->position, true, game_state.get_mobility(1-my_seat)) < 0)
                                safe = false;
                        }
                        // 能占就占
                        if (safe && curr_army - 1 > next_cell_army) {
                            add_operation(Operation::move_army(general->position, from_coord(general->position, target), next_cell_army + 1));
                            remain_move_count -= 1;
                        } else logger.log(LOG_LEVEL_INFO, "\t[Occupy] General at %s -> %s, but not safe", general->position.str().c_str(), target.str().c_str());
                        continue;
                    }
                }
                // 反之尝试分兵占领
                else if (!militia_task || militia_task->plan.target->position != target) {
                    Militia_analyzer analyzer(game_state);
                    auto plan = analyzer.search_plan_from_provider(game_state[target].generals, game_state.generals[my_seat]);
                    // 首先兵要足够，其次不能太远
                    if (plan && plan->army_used <= curr_army - 1 &&
                        plan->plan.size() <= 8 && plan->army_used <= 0.4 * curr_army &&
                        ((curr_army - plan->army_used >= deterrence_analyzer->min_army * 1.2 && !enemy_aggregate) || plan->plan.size() <= 3)) {
                        logger.log(LOG_LEVEL_INFO, "\t[Occupy] Calling for militia to occupy %s, plan size %d", target.str().c_str(), plan->plan.size());
                        militia_task.emplace(Militia_action_type::OCCUPY_MAINGENERAL, *plan, game_state.round);
                        continue;
                    }
                }
                // 否则无事可做
                else logger.log(LOG_LEVEL_INFO, "\t[Occupy] General at %s has no valid move to well %s", general->position.str().c_str(), target.str().c_str());
            } else if (strategy.type == General_strategy_type::ATTACK) {
                const Generals* enemy = strategy.target.general;
                Dist_map dist_map(game_state, enemy->position, Path_find_config{1.0, game_state.has_swamp_tech(my_seat)});

                if (dist_map[general->position] >= Dist_map::MAX_DIST) {
                    logger.log(LOG_LEVEL_INFO, "\t[Attack] General at %s cannot reach enemy %s", general->position.str().c_str(), enemy->position.str().c_str());
                    continue;
                }

                // 进行移动搜索
                logger.log(LOG_LEVEL_DEBUG, "\t[Attack] Move search:");
                General_mover mover(game_state, general, std::min(general->mobility_level, remain_move_count), enemy->position);
                auto move_plans = mover.search();
                for (const auto& move_plan : move_plans) {
                    logger.log(LOG_LEVEL_DEBUG, "\t\t[Attack] Move plan: %s", move_plan.c_str());
                }

                // 没问题就往前走一步
                if (move_plans.size() && move_plans[0].step_count) {
                    const Move_plan& plan = move_plans[0];
                    for (const Operation& op : plan.ops) {
                        logger.log(LOG_LEVEL_INFO, "\t[Attack] Plan step: %s", op.str().c_str());
                        add_operation(op);
                    }
                    remain_move_count -= plan.step_count;
                } else logger.log(LOG_LEVEL_INFO, "\t[Attack] General at %s cannot reach enemy %s", general->position.str().c_str(), enemy->position.str().c_str());
            } else if (strategy.type == General_strategy_type::RETREAT) {
                Coord enemy_pos = strategy.target.attack_info->origin;
                Move_cost_cfg move_cfg(0.1, 0, -1); // 距离敌人越远越好

                // 进行移动搜索
                logger.log(LOG_LEVEL_DEBUG, "\t[Retreat] Move search:");
                General_mover mover(game_state, general, std::min(general->mobility_level, remain_move_count), enemy_pos);
                mover << move_cfg;
                auto move_plans = mover.search();
                for (const auto& move_plan : move_plans) {
                    logger.log(LOG_LEVEL_DEBUG, "\t\t[Retreat] Move plan: %s", move_plan.c_str());
                }

                // 如果最优行动存在，则执行
                if (move_plans.size() && move_plans[0].step_count) {
                    const Move_plan& plan = move_plans[0];
                    for (const Operation& op : plan.ops) {
                        logger.log(LOG_LEVEL_INFO, "\t[Retreat] Plan step: %s", op.str().c_str());
                        add_operation(op);
                    }
                    remain_move_count -= plan.step_count;
                    continue;
                }

                // 否则尝试空袭一次
                if (!strategy.target.attack_info->pure_army_attack)
                if (oil_after_op >= GENERAL_SKILL_COST[SkillType::STRIKE] && !general->cd(SkillType::STRIKE) &&
                    enemy_pos.in_attack_range(general->position)) {

                    logger.log(LOG_LEVEL_DEBUG, "\t[Retreat] Move search (+strike):");
                    GameState temp_state;
                    temp_state.copy_as(game_state);
                    execute_operation(temp_state, my_seat, Operation::generals_skill(general->id, SkillType::STRIKE, enemy_pos));

                    General_mover mover(temp_state, general, std::min(general->mobility_level, remain_move_count), enemy_pos);
                    mover << move_cfg; // 距离敌人越远越好
                    auto move_plans = mover.search();
                    for (const auto& move_plan : move_plans) {
                        logger.log(LOG_LEVEL_DEBUG, "\t\t[Retreat] Move plan: %s", move_plan.c_str());
                    }

                    // 如果最优行动存在，则执行
                    if (move_plans.size() && move_plans[0].step_count) {
                        const Move_plan& plan = move_plans[0];
                        oil_after_op -= GENERAL_SKILL_COST[SkillType::STRIKE];
                        add_operation(Operation::generals_skill(general->id, SkillType::STRIKE, enemy_pos));
                        for (const Operation& op : plan.ops) {
                            logger.log(LOG_LEVEL_INFO, "\t[Retreat] Plan step: %s", op.str().c_str());
                            add_operation(op);
                        }
                        remain_move_count -= plan.step_count;
                        continue;
                    }
                }

                // 否则尝试升级防御
                if (oil_after_op >= general->defence_upgrade_cost()) {
                    logger.log(LOG_LEVEL_DEBUG, "\t[Retreat] Move search (+defence):");
                    GameState temp_state;
                    temp_state.copy_as(game_state);
                    execute_operation(temp_state, my_seat, Operation::upgrade_generals(general->id, QualityType::DEFENCE));

                    General_mover mover(temp_state, general, std::min(general->mobility_level, remain_move_count), enemy_pos);
                    mover << move_cfg;
                    auto move_plans = mover.search();
                    for (const auto& move_plan : move_plans) {
                        logger.log(LOG_LEVEL_DEBUG, "\t\t[Retreat] Move plan: %s", move_plan.c_str());
                    }

                    // 如果最优行动存在，则执行
                    if (move_plans.size() && move_plans[0].step_count) {
                        const Move_plan& plan = move_plans[0];
                        oil_after_op -= general->defence_upgrade_cost();
                        add_operation(Operation::upgrade_generals(general->id, QualityType::DEFENCE));
                        for (const Operation& op : plan.ops) {
                            logger.log(LOG_LEVEL_INFO, "\t[Retreat] Plan step: %s", op.str().c_str());
                            add_operation(op);
                        }
                        remain_move_count -= plan.step_count;
                        continue;
                    }
                }

                // 否则尝试升级行动力
                if (general->movement_tire() == 0 && oil_after_op >= general->movement_upgrade_cost()) {
                    logger.log(LOG_LEVEL_DEBUG, "\t[Retreat] Move search (+mobility):");
                    GameState temp_state;
                    temp_state.copy_as(game_state);
                    execute_operation(temp_state, my_seat, Operation::upgrade_generals(general->id, QualityType::MOBILITY));

                    General_mover mover(temp_state, general, std::min(GENERAL_MOVEMENT_VALUES[general->movement_tire()+1], remain_move_count), enemy_pos);
                    mover << move_cfg; // 距离敌人越远越好
                    auto move_plans = mover.search();
                    for (const auto& move_plan : move_plans) {
                        logger.log(LOG_LEVEL_DEBUG, "\t\t[Retreat] Move plan: %s", move_plan.c_str());
                    }

                    // 如果最优行动存在，则执行
                    if (move_plans.size() && move_plans[0].step_count) {
                        const Move_plan& plan = move_plans[0];
                        oil_after_op -= general->movement_upgrade_cost();
                        add_operation(Operation::upgrade_generals(general->id, QualityType::MOBILITY));
                        for (const Operation& op : plan.ops) {
                            logger.log(LOG_LEVEL_INFO, "\t[Retreat] Plan step: %s", op.str().c_str());
                            add_operation(op);
                        }
                        remain_move_count -= plan.step_count;
                        continue;
                    }
                }
                logger.log(LOG_LEVEL_INFO, "\t[Retreat] General at %s cannot retreat", general->position.str().c_str());
            } else assert(!"Invalid strategy type");
        }
    }

    std::optional<Militia_move_task> militia_task;
    void militia_move() {
        // 任务完成
        if (militia_task && militia_task->next_action >= militia_task->step_count()) militia_task.reset();

        // 10回合分析一次，不允许打断非“自由占领”型的任务
        if ((game_state.round % 10 == 1 || !militia_task) && (militia_task ? militia_task->type == Militia_action_type::OCCUPY_FREE : true)) {
            Militia_analyzer analyzer(game_state);

            // 寻找总时长尽量小的方案，且要求集合用时不超过7步
            std::optional<Militia_plan> best_plan;
            for (int i = PLAYER_COUNT, siz = game_state.generals.size(); i < siz; ++i) {
                const Generals* target = game_state.generals[i];
                if (target->player == my_seat || !game_state.can_soldier_step_on(target->position, my_seat)) continue;

                std::optional<Militia_plan> plan = analyzer.search_plan_from_militia(target);
                if (!plan || plan->gather_steps > 7) continue;

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

            if (best_plan && (int)best_plan->plan.size() <= 8 * game_state.get_mobility(my_seat)) {
                militia_task.emplace(Militia_action_type::OCCUPY_FREE, *best_plan, game_state.round);

                logger.log(LOG_LEVEL_INFO, "[Militia] Militia plan size %d, gather %d, found for target %s:",
                           militia_task->step_count(), militia_task->plan.gather_steps, militia_task->plan.target->position.str().c_str());
                for (const auto& op : militia_task->plan.plan)
                    logger.log(LOG_LEVEL_INFO, "\t%s->%s", op.first.str().c_str(), (op.first + DIRECTION_ARR[op.second]).str().c_str());
            }
        }

        if (!remain_move_count) return;
        // 无任务则扩展
        if (!militia_task) {
            // 按照到敌方的距离升序
            Dist_map enemy_dist(game_state, game_state.generals[1-my_seat]->position, Path_find_config{1.0, game_state.has_swamp_tech(my_seat)});
            static std::vector<std::pair<int, Operation>> potential_ops;
            potential_ops.clear();

            // 寻找可扩展的格子
            for (int x = 0; x < Constant::col; ++x) for (int y = 0; y < Constant::row; ++y) {
                Coord pos{x, y};
                const Cell& cell = game_state[pos];
                if (cell.player != my_seat || cell.army <= 1) continue;
                if (cell.generals && dynamic_cast<const OilWell*>(cell.generals) == nullptr) continue; // 排除主副将格

                // 油田仅在周围无敌军时允许扩展
                if (cell.generals && dynamic_cast<const OilWell*>(cell.generals)) {
                    bool has_enemy = false;
                    for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
                        Coord next_pos = pos + DIRECTION_ARR[dir];
                        if (!next_pos.in_map()) continue;
                        const Cell& next_cell = game_state[next_pos];
                        if (next_cell.player == 1-my_seat && next_cell.army > 0) {
                            has_enemy = true;
                            break;
                        }
                    }
                    if (has_enemy) continue;
                }

                // 尝试扩展
                for (int dir = 0; dir < DIRECTION_COUNT; ++dir) {
                    Coord next_pos = pos + DIRECTION_ARR[dir];
                    if (!next_pos.in_map() || !game_state.can_soldier_step_on(next_pos, my_seat)) continue; // 排除无法走上的沼泽

                    const Cell& next_cell = game_state[next_pos];
                    if (next_cell.player == my_seat || next_cell.type == CellType::DESERT) continue; // 排除自己的格子和沙漠格
                    if (next_cell.player == 1-my_seat && next_cell.army > cell.army - 1) continue; // 排除攻不下（无法中立化）的对方格
                    if (next_cell.player == -1 && next_cell.army >= cell.army - 1) continue; // 中立格弄成中立也没用

                    // 创建移动操作
                    potential_ops.emplace_back(enemy_dist[next_pos], Operation::move_army(pos, static_cast<Direction>(dir), cell.army - 1));
                    break;
                }
            }

            // 将候选动作排序并执行
            std::sort(potential_ops.begin(), potential_ops.end(),
                      [](const std::pair<int, Operation>& a, const std::pair<int, Operation>& b) { return a.first < b.first; });
            for (const auto& op : potential_ops) {
                logger.log(LOG_LEVEL_INFO, "[Militia] Expanding (dist %d): %s", op.first, op.second.str().c_str());
                add_operation(op.second);
                remain_move_count -= 1;
                if (!remain_move_count) return;
            }
        }
        // 否则执行计划
        else while (remain_move_count && militia_task->next_action < militia_task->step_count()) {
            // 一些初步检查
            int next_action_index = militia_task->next_action;
            Direction move_dir = (*militia_task)[next_action_index].second;
            const Coord& pos = (*militia_task)[next_action_index].first;
            const Cell& cell = game_state[pos];

            // 是否是从主将上提取兵力的第一步操作
            bool take_army_from_general = (cell.generals && cell.generals->id == my_seat && next_action_index == 0);
            if (take_army_from_general)
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, take %d army from general", next_action_index+1, militia_task->plan.army_used);

            // 需要移动的格子不属于自己，或未经授权从主将取兵
            if (cell.player != my_seat || (cell.generals && cell.generals->id == my_seat && !take_army_from_general)) {
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, invalid position %s (player %d, army %d)",
                           next_action_index+1, pos.str().c_str(), cell.player, cell.army);
                militia_task.reset();
                break;
            }
            // 需要移动的格子没有兵
            if (cell.army <= 1) {
                if (militia_task->type == Militia_action_type::SUPPORT) {
                    militia_task->next_action += 1;
                    continue;
                }
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, invalid position %s (player %d, army %d)",
                           next_action_index+1, pos.str().c_str(), cell.player, cell.army);
                militia_task.reset();
                break;
            }
            // 需要从主将上提取兵力，但兵力不足
            if (take_army_from_general && cell.army - 1 < militia_task->plan.army_used) {
                logger.log(LOG_LEVEL_INFO, "[Militia] Plan step %d, army not enough to take %d from general", next_action_index+1, militia_task->plan.army_used);
                militia_task.reset();
                break;
            }

            logger.log(LOG_LEVEL_INFO, "[Militia] Executing plan step %d, %s->%s",
                        next_action_index+1, pos.str().c_str(), (pos + DIRECTION_ARR[move_dir]).str().c_str());
            add_operation(Operation::move_army(pos, move_dir, take_army_from_general ? militia_task->plan.army_used : cell.army - 1));
            remain_move_count -= 1;
            militia_task->next_action += 1;
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
