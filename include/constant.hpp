#pragma once

class Constant {
public:
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

    static constexpr int tactical_strike = 20;
    static constexpr int breakthrough = 15;
    static constexpr int leadership = 30;
    static constexpr int fortification = 30;
    static constexpr int weakening = 30;

    static constexpr int army_movement_T1 = 80;
    static constexpr int army_movement_T2 = 150;
    static constexpr int swamp_immunity = 100;
    static constexpr int sand_immunity = 75;
    static constexpr int unlock_super_weapon = 250;

    static constexpr double sand_percent=0.15;
    static constexpr double swamp_percent=0.05;
};
