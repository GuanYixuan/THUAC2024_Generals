#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include "constant.hpp"

class GameState;

// 坐标类
class Coord {
public:
    int x;
    int y;

    Coord() noexcept = default;
    Coord(int x, int y) noexcept : x(x), y(y) {};
    Coord(const std::pair<int, int>& pos) noexcept : x(pos.first), y(pos.second) {};
    operator std::pair<int, int>() const noexcept { return std::make_pair(x, y); }

    // 比较与算术运算
    bool operator==(const Coord& other) const noexcept { return x == other.x && y == other.y; }
    bool operator!=(const Coord& other) const noexcept { return x != other.x || y != other.y; }
    Coord operator+(const Coord& other) const noexcept { return Coord(x + other.x, y + other.y); }
    Coord operator-(const Coord& other) const noexcept { return Coord(x - other.x, y - other.y); }
};

// 将军技能类型
enum class SkillType {
    SURPRISE_ATTACK = 0,
    ROUT = 1,
    COMMAND = 2,
    DEFENCE = 3,
    WEAKEN = 4
};

// 将军属性类型
enum class QualityType {
    PRODUCTION = 0,
    DEFENCE = 1,
    MOBILITY = 2
};

// 超级武器类型
enum class WeaponType {
    NUCLEAR_BOOM = 0,
    ATTACK_ENHANCE = 1,
    TRANSMISSION = 2,
    TIME_STOP = 3
};

// 格子类型
enum class CellType {
    PLAIN = 0,
    SAND = 1,
    SWAMP = 2
};

// 科技类型
enum class TechType {
    MOBILITY = 0,
    IMMUNE_SWAMP = 1,
    IMMUNE_SAND = 2,
    UNLOCK = 3
};

// 方向类型
enum class Direction {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3
};

// 将军技能结构体
struct Skill {
    SkillType type = SkillType::SURPRISE_ATTACK; // 技能类型
    int cd = 0; //冷却回合数
};

// 超级武器结构体
struct SuperWeapon {
    WeaponType type = WeaponType::NUCLEAR_BOOM; // 武器类型
    int player = -1; // 所属玩家
    int cd = 0; // 冷却回合数
    int rest = 0;// 效果剩余回合数
    Coord position = {0, 0}; // 位置坐标
    SuperWeapon(WeaponType type, int player, int cd, int rest, Coord position):type(type), player(player), cd(cd), rest(rest), position(position){};
};

// 将军基类
class Generals {
    public:
    int id = 0; // 将军编号
    int player = -1; //所属玩家
    int produce_level = 1; // 生产力等级
    int defence_level = 1; // 防御力等级
    int mobility_level = 1; //移动力等级
    Coord position = {0,0}; // 位置坐标
    std::vector<int> skills_cd = {0, 0, 0, 0, 0}; // 技能冷却回合数列表
    std::vector<int> skill_duration = {0, 0, 0}; // 技能持续回合数列表
    int rest_move = 1; // 剩余移动步数
    virtual bool production_up(Coord location,GameState &gamestate, int player){return false;} // 提升生产力
    virtual bool defence_up(Coord location,GameState &gamestate, int player) {return false;} // 提升防御力
    virtual bool movement_up(Coord location, GameState &gamestate, int player) { return false;} // 提升移动力
    Generals(int id,int player,Coord position):
        id(id),player(player),position(position){};
};

// 油井类，继承自将军基类
class OilWell :public Generals {
    public:
        int mobility_level = 0;
        float defence_level=1.0f;
        virtual bool production_up(Coord location,GameState &gamestate, int player);
        virtual bool defence_up(Coord location,GameState &gamestate, int player);
        virtual bool movement_up(Coord location,GameState &gamestate, int player);
        OilWell(int id,int player,Coord position):Generals(id,player,position){};
};

// 主将类，继承自将军基类
class MainGenerals :public Generals {
    public:
    std::vector<Skill> skills = {
        {SkillType::SURPRISE_ATTACK, 5},
        {SkillType::ROUT, 10},
        {SkillType::COMMAND, 10},
        {SkillType::DEFENCE, 10},
        {SkillType::WEAKEN, 10}
    };
    virtual bool production_up(Coord location,GameState &gamestate, int player);
    virtual bool defence_up(Coord location,GameState &gamestate, int player);
    virtual bool movement_up(Coord location,GameState &gamestate, int player);
    MainGenerals(int id,int player,Coord position):Generals(id,player,position){};
};

// 副将类，继承自将军基类
class SubGenerals :public Generals {
    public:

    std::vector<Skill> skills = {
        {SkillType::SURPRISE_ATTACK, 5},
        {SkillType::ROUT, 10},
        {SkillType::COMMAND, 10},
        {SkillType::DEFENCE, 10},
        {SkillType::WEAKEN, 10}
    };
    virtual bool production_up(Coord location,GameState &gamestate, int player);
    virtual bool defence_up(Coord location,GameState &gamestate, int player);
    virtual bool movement_up(Coord location,GameState &gamestate, int player);
    SubGenerals(int id,int player,Coord position):
        Generals(id,player,position){};
};

// 格子类
class Cell {
    public:
    Coord position = {0,0};  // 格子的位置坐标
    CellType type=CellType::PLAIN; // 格子的类型
    int player = -1;  // 控制格子的玩家编号
    Generals* generals = nullptr;  // 格子上的将军对象指针
    std::vector<SuperWeapon> weapon_activate;  // 已激活的超级武器列表
    int army = 0;  // 格子里的军队数量

    Cell(){}  // 默认构造函数，初始化格子对象
};

// 游戏状态类
class GameState {
public:
    int round = 1; // 当前游戏回合数
    std::vector<Generals> generals;
    int coin[2] = {0,0};//每个玩家的金币数量列表，分别对应玩家1，玩家2
    std::vector<SuperWeapon> active_super_weapon;
    bool super_weapon_unlocked[2] = {false, false};// 超级武器是否解锁的列表，解锁了是true，分别对应玩家1，玩家2
    int super_weapon_cd[2] = {-1, -1};//超级武器的冷却回合数列表，分别对应玩家1，玩家2
    int tech_level[2][4] = {{2, 0, 0, 0}, {2, 0, 0, 0}};//科技等级列表，第一层对应玩家一，玩家二，第二层分别对应行动力，免疫沼泽，免疫流沙，超级武器
    int rest_move_step[2] = {2, 2};
    Cell board[Constant::row][Constant::col];
    //游戏棋盘的二维列表，每个元素是一个Cell对象
    int next_generals_id = 0;
    int winner = -1;

    Coord find_general_position_by_id(int general_id) {
        for (Generals& gen : generals) {
            if (gen.id == general_id) {
                return gen.position;
            }
        }
        return Coord(-1,-1);
    }//寻找将军id对应的格子，找不到返回(-1,-1)
    void update_round();
};

/* 更新游戏回合信息。 */
void GameState::update_round() 
{
    for (int i = 0; i < Constant::row; ++i) {
        for (int j = 0; j < Constant::col; ++j) {
            // 将军
            if (this->board[i][j].generals!=nullptr){
                this->board[i][j].generals->rest_move = this->board[i][j].generals->mobility_level;
            }
            if (dynamic_cast<MainGenerals*>(this->board[i][j].generals)) {
                this->board[i][j].army += this->board[i][j].generals->produce_level;
            } else if (dynamic_cast<SubGenerals*>(this->board[i][j].generals)) {
                this->board[i][j].army += this->board[i][j].generals->produce_level;
            } else if (dynamic_cast<OilWell*>(this->board[i][j].generals)) {
                if (this->board[i][j].generals->player != -1) {
                    this->coin[this->board[i][j].generals->player] += this->board[i][j].generals->produce_level;
                }
            }
            // 每10回合增兵
            if (this->round % 10 == 0) {
                if (this->board[i][j].player != -1) {
                    this->board[i][j].army += 1;
                }
            }
            // 流沙减兵
            if (this->board[i][j].type == CellType(1) && this->board[i][j].player != -1 && this->board[i][j].army > 0) {
                if (this->tech_level[this->board[i][j].player][2] == 0) {
                    this->board[i][j].army -= 1;
                    if (this->board[i][j].army == 0 && this->board[i][j].generals == nullptr) {
                        this->board[i][j].player = -1;
                    }
                }
            }
        }
    }

    // 超级武器判定
    for (auto &weapon : this->active_super_weapon) {
        if (weapon.type == WeaponType(0)) {
            for (int _i = std::max(0, weapon.position.x - 1); _i < std::min(Constant::row, weapon.position.x + 1); ++_i) {
                for (int _j = std::max(0, weapon.position.y - 1); _j < std::min(Constant::col, weapon.position.y + 1); ++_j) {
                    if (this->board[_i][_j].army > 0) {
                        this->board[_i][_j].army = std::max(0, this->board[_i][_j].army - 3);
                        if (this->board[_i][_j].army == 0 && this->board[_i][_j].generals == nullptr) {
                            this->board[_i][_j].player = -1;
                        }
                    }
                }
            }
        }
    }

    for (auto &i : this->super_weapon_cd) {
        if (i > 0) {
            --i;
        }
    }

    for (auto &weapon : this->active_super_weapon) {
        --weapon.rest;
    }

    // cd和duration 减少
    for (auto &gen : this->generals) {
        for (auto &i : gen.skills_cd) {
            if (i > 0) {
                --i;
            }
        }
        for (auto &i : gen.skill_duration) {
            if (i > 0) {
                --i;
            }
        }
    }

    // 移动步数恢复
    this->rest_move_step[0] = this->tech_level[0][0];
    this->rest_move_step[1]=this->tech_level[1][0];

    std::vector<SuperWeapon> filtered_super_weapon;
    for (const auto& weapon : this->active_super_weapon) {
        if (weapon.rest > 0) {
            filtered_super_weapon.push_back(weapon);
        }
    }
    
    this->active_super_weapon = filtered_super_weapon;

    ++this->round;
}

bool MainGenerals::production_up(Coord location, GameState &gamestate, int player)
{
  // 获取将军的生产等级
  int level = gamestate.board[location.x][location.y].generals->produce_level;
  // 根据生产等级选择不同的操作
  switch (level)
  {
  case 1: // 如果生产等级为1
    // 检查玩家是否有足够的金币
    if (gamestate.coin[player] < Constant::lieutenant_production_T1 / 2)
    {
      return false; // 金币不足，返回false
    }
    else
    {
      // 金币足够，提升生产等级为2
      gamestate.board[location.x][location.y].generals->produce_level = 2;
      // 扣除相应的金币
      gamestate.coin[player] -= Constant::lieutenant_production_T1 / 2;
      return true; // 返回true
    }
    break; // 跳出switch语句
  case 2:  // 如果生产等级为2
    // 检查玩家是否有足够的金币
    if (gamestate.coin[player] < Constant::lieutenant_production_T2 / 2)
    {
      return false; // 金币不足，返回false
    }
    else
    {
      // 金币足够，提升生产等级为4
      gamestate.board[location.x][location.y].generals->produce_level = 4;
      // 扣除相应的金币
      gamestate.coin[player] -= Constant::lieutenant_production_T2 / 2;
      return true; // 返回true
    }
    break;        // 跳出switch语句
  default:        // 如果生产等级不是1或2
    return false; // 返回false
  }
}

bool MainGenerals::defence_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->defence_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::general_movement_T1 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 2;
      gamestate.coin[player] -= Constant::general_movement_T1 / 2;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::lieutenant_defense_T2 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 3;
      gamestate.coin[player] -= Constant::lieutenant_defense_T2 / 2;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool MainGenerals::movement_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->mobility_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::general_movement_T1 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->mobility_level = 2;
      gamestate.coin[player] -= Constant::general_movement_T1 / 2;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::general_movement_T2 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->mobility_level = 4;
      gamestate.coin[player] -= Constant::general_movement_T2 / 2;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool SubGenerals::production_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->produce_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::lieutenant_production_T1)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->produce_level = 2;
      gamestate.coin[player] -= Constant::lieutenant_production_T1;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::lieutenant_production_T2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->produce_level = 4;
      gamestate.coin[player] -= Constant::lieutenant_production_T2;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool SubGenerals::defence_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->defence_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::general_movement_T1 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 2;
      gamestate.coin[player] -= Constant::general_movement_T1 / 2;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::lieutenant_defense_T2 / 2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 3;
      gamestate.coin[player] -= Constant::lieutenant_defense_T2 / 2;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool SubGenerals::movement_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->mobility_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::general_movement_T1)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->mobility_level = 2;
      gamestate.coin[player] -= Constant::general_movement_T1;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::general_movement_T2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->mobility_level = 4;
      gamestate.coin[player] -= Constant::general_movement_T2;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool OilWell::production_up(Coord location, GameState &gamestate, int player)
{
  switch (gamestate.board[location.x][location.y].generals->produce_level)
  {
  case 1:
    if (gamestate.coin[player] < Constant::OilWell_production_T1)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->produce_level = 2;
      gamestate.coin[player] -= Constant::OilWell_production_T1;
    }
    break;
  case 2:
    if (gamestate.coin[player] < Constant::OilWell_production_T2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->produce_level = 4;
      gamestate.coin[player] -= Constant::OilWell_production_T2;
    }
    break;
  case 4:
    if (gamestate.coin[player] < Constant::OilWell_production_T3)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->produce_level = 6;
      gamestate.coin[player] -= Constant::OilWell_production_T3;
    }
    break;
  default:
    return false;
  }
  return true;
}

bool OilWell::defence_up(Coord location, GameState &gamestate, int player)
{
  if (gamestate.board[location.x][location.y].generals->defence_level == 1)
  {
    if (gamestate.coin[player] < Constant::OilWell_defense_T1)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 1.5;
      gamestate.coin[player] -= Constant::OilWell_defense_T1;
    }
  }
  else if (gamestate.board[location.x][location.y].generals->defence_level == 1.5)
  {
    if (gamestate.coin[player] < Constant::OilWell_defense_T2)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 2;
      gamestate.coin[player] -= Constant::OilWell_defense_T2;
    }
  }
  else if (gamestate.board[location.x][location.y].generals->defence_level == 2)
  {
    if (gamestate.coin[player] < Constant::OilWell_defense_T3)
    {
      return false;
    }
    else
    {
      gamestate.board[location.x][location.y].generals->defence_level = 3;
      gamestate.coin[player] -= Constant::OilWell_defense_T3;
    }
  }
  else
  {
    return false;
  }
  return true;
}
bool OilWell::movement_up(Coord location, GameState &gamestate, int player)
{
  return false;
}
