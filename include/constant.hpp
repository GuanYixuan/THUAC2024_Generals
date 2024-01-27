#pragma once

class Constant {
public:
    // 玩家数量
    static constexpr int PLAYER_COUNT = 2;

    static constexpr int col = 15; // 定义了地图的列数。
    static constexpr int row = 15; // 定义了地图的行数。

    static constexpr int OilWell_production_T1 = 10;
    static constexpr int OilWell_production_T2 = 25;
    static constexpr int OilWell_production_T3 = 35;
    static constexpr int OilWell_defense_T1 = 10;
    static constexpr int OilWell_defense_T2 = 15;
    static constexpr int OilWell_defense_T3 = 30;

    static constexpr int lieutenant_new_recruit = 50;
    static constexpr int lieutenant_production_T1 = 40;
    static constexpr int lieutenant_production_T2 = 80;
    static constexpr int lieutenant_defense_T1 = 40;
    static constexpr int lieutenant_defense_T2 = 100;

    static constexpr int general_movement_T1 = 20;
    static constexpr int general_movement_T2 = 40;

    // 召唤副将的开销
    static constexpr int SPAWN_GENERAL_COST = 50;

    // 将领的攻击半径（闭区间）
    static constexpr int GENERAL_ATTACK_RADIUS = 2;


    // 将领的技能数量
    static constexpr int GENERAL_SKILL_COUNT = 5;

    // 技能冷却时间
    static constexpr int GENERAL_SKILL_CD[GENERAL_SKILL_COUNT] = {5, 10, 10, 10, 10};

    // 技能开销
    static constexpr int GENERAL_SKILL_COST[GENERAL_SKILL_COUNT] = {20, 15, 30, 30, 30};

    // 技能持续时间，注意下标为0和1的技能无此属性
    static constexpr int GENERAL_SKILL_DURATION[GENERAL_SKILL_COUNT] = {0, 0, 10, 10, 10};

    // 技能效果，注意下标为0和1的技能无此属性
    static constexpr double GENERAL_SKILL_EFFECT[GENERAL_SKILL_COUNT] = {0, 0, 1.5, 1.5, 0.75};

    static constexpr int STRIKE_DAMAGE = 20;

    static constexpr int army_movement_T1 = 80;
    static constexpr int army_movement_T2 = 150;
    static constexpr int swamp_immunity = 100;
    static constexpr int sand_immunity = 75;
    static constexpr int unlock_super_weapon = 250;

    // 超级武器的冷却时间
    static constexpr int SUPER_WEAPON_CD = 50;
    // 超级武器的作用半径（闭区间）
    static constexpr int SUPER_WEAPON_RADIUS = 1;

    // 辐射效果每回合的伤害
    static constexpr int NUCLEAR_BOMB_DAMAGE = 3;

};
