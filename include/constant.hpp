#pragma once

#include <cstdio>
#include <string>
#include <cstdarg>

// 输出至`std::string`版本的`printf`
[[nodiscard]] std::string wrap(const char *format, ...) {
    static char buffer[1024];

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    return std::string(buffer);
}

class Constant {
public:
    // 玩家数量
    static constexpr int PLAYER_COUNT = 2;

    static constexpr int col = 15; // 定义了地图的列数。
    static constexpr int row = 15; // 定义了地图的行数。

    // 油井产量升级相关常数
    static constexpr int OILWELL_PRODUCTION_LEVELS = 4;
    static constexpr int OILWELL_PRODUCTION_VALUES[OILWELL_PRODUCTION_LEVELS] = {1, 2, 4, 6};
    static constexpr int OILWELL_PRODUCTION_COST[OILWELL_PRODUCTION_LEVELS] = {10, 25, 35, 1 << 30};

    // 油井防御升级相关常数
    static constexpr int OILWELL_DEFENCE_LEVELS = 4;
    static constexpr double OILWELL_DEFENCE_VALUES[OILWELL_DEFENCE_LEVELS] = {1, 1.5, 2, 3};
    static constexpr int OILWELL_DEFENCE_COST[OILWELL_DEFENCE_LEVELS] = {10, 15, 30, 1 << 30};

    // 召唤副将的开销
    static constexpr int SPAWN_GENERAL_COST = 50;

    // 主将升级折扣常数
    static constexpr int MAIN_GENERAL_DISCOUNT = 2;

    // 将领产量升级相关常数
    static constexpr int GENERAL_PRODUCTION_LEVELS = 3;
    static constexpr int GENERAL_PRODUCTION_VALUES[GENERAL_PRODUCTION_LEVELS] = {1, 2, 4};
    static constexpr int GENERAL_PRODUCTION_COST[GENERAL_PRODUCTION_LEVELS] = {40, 80, 1 << 30};

    // 将领防御升级相关常数
    static constexpr int GENERAL_DEFENCE_LEVELS = 3;
    static constexpr int GENERAL_DEFENCE_VALUES[GENERAL_DEFENCE_LEVELS] = {1, 2, 3};
    static constexpr int GENERAL_DEFENCE_COST[GENERAL_DEFENCE_LEVELS] = {40, 100, 1 << 30};

    // 将领行动力升级相关常数
    static constexpr int GENERAL_MOVEMENT_LEVELS = 3;
    static constexpr int GENERAL_MOVEMENT_VALUES[GENERAL_MOVEMENT_LEVELS] = {1, 2, 4};
    static constexpr int GENERAL_MOVEMENT_COST[GENERAL_MOVEMENT_LEVELS] = {20, 40, 1 << 30};

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

    // 玩家行动力升级相关常数
    static constexpr int PLAYER_MOVEMENT_LEVELS = 3;
    static constexpr int PLAYER_MOVEMENT_VALUES[PLAYER_MOVEMENT_LEVELS] = {2, 3, 5};
    static constexpr int PLAYER_MOVEMENT_COST[PLAYER_MOVEMENT_LEVELS] = {80, 150, 1 << 30};

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
