#pragma once

#include <vector>
#include <iostream>

#include "operation.hpp"
#include "gamestate.hpp"
#include "protocol.hpp"
#include "util.hpp"
#include "logger.hpp"

#include "test_sync.hpp"

// 标识玩家的先后手：0先手，1后手
int my_seat = 0;


/* ## `class GameController`

游戏控制器。可以帮助选手处理游戏流程有关的繁琐操作。

### 成员变量

#### `int my_seat`

* 描述：标识玩家的先后手。
* 可取值：0 或 1，其中 0 代表先手，1 代表后手。

#### `GameState game_state`

* 描述：游戏状态对象。
* 类型：`GameState`。

#### `std::vector<Operation> my_operation_list`

* 描述：存储我方未发送的操作列表。
* 类型：`std::vector<Operation>`。 */
class GameController
{
public:
    GameState game_state;                     // 游戏状态，一个 GameState 类的对象。
    std::vector<Operation> my_operation_list; // 我方未发送的操作列表。

    void init();
    bool execute_single_command(int player, const Operation &op);
    /**
     * @brief 为敌方应用动作序列`ops`
     * @note 此方法断言所有敌方操作合法
     */
    void apply_enemy_ops(const std::vector<Operation> &ops) {
        logger.log(LOG_LEVEL_INFO, "Applying enemy ops:");
        for (const auto &op : ops) {
            bool valid = execute_single_command(1 - my_seat, op);
            logger.log(LOG_LEVEL_INFO, "\tOp: %s", op.str().c_str());

            if (!valid) {
                show_map(game_state, std::cerr);
                throw std::runtime_error("Invalid enemy operation: " + op.str());
            }
        }
    }
    /**
     * @brief 从`stdin`读取并应用敌方操作
     * @note 此方法断言所有敌方操作合法
     */
    void read_and_apply_enemy_ops() { apply_enemy_ops(read_enemy_operations()); }
    // 结束我方操作回合，将操作列表打包发送并清空
    void finish_and_send_our_ops();
};

/* #### `bool execute_single_command(int player, const Operation &op)`

* 描述：执行单个操作。
* 参数：
  * `int player`：操作所属玩家，取值为 0 或 1。
  * `const Operation &op`：待执行的操作。
* 返回值：操作是否成功执行，成功返回 true，否则返回 false */
bool GameController::execute_single_command(int player, const Operation &op)
{
    // 获取操作码和操作数
    OperationType command = op.opcode;
    std::vector<int> params = op.operand;

    // 根据操作码执行相应的操作
    switch (command) {
        // 移动军队
        case OperationType::MOVE_ARMY:
            if (params[3] > game_state[{params[0], params[1]}].army - 1) {
                logger.log(LOG_LEVEL_ERROR, "Invalid army count for enemy op MOVE_ARMY: %s %d %d, truncated",
                           Coord(params[0], params[1]).str().c_str(), params[2], params[3]);
                params[3] = game_state[{params[0], params[1]}].army - 1;
            }
            return army_move({params[0], params[1]}, game_state, player, static_cast<Direction>(params[2] - 1), params[3]);
        // 移动将军
        case OperationType::MOVE_GENERALS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;
            return general_move(pos, game_state, player, {params[1], params[2]});
        }
        // 更新将军
        case OperationType::UPDATE_GENERALS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;

            switch (params[1]) {
                case 1:
                    // 提升生产力
                    return production_up(pos, game_state, player);
                case 2:
                    // 提升防御力
                    return defence_up(pos, game_state, player);
                case 3:
                    // 提升移动力
                    return movement_up(pos, game_state, player);
                default:
                    assert(false);
            }
        }
        // 使用将军技能
        case OperationType::USE_GENERAL_SKILLS: {
            int id = params[0];
            auto pos = game_state.find_general_position_by_id(id);
            if (pos.x == -1) return false;
            if (params[1] == 1 || params[1] == 2)
                return skill_activate(player, pos, {params[2], params[3]}, game_state, static_cast<SkillType>(params[1] - 1));
            else return skill_activate(player, pos, {-1, -1}, game_state, static_cast<SkillType>(params[1] - 1));
        }
        // 更新科技
        case OperationType::UPDATE_TECH:
            return tech_update(static_cast<TechType>(params[0] - 1), game_state, player);
        // 使用超级武器
        case OperationType::USE_SUPERWEAPON:
            switch (params[0]) {
                case 1:
                    // 炸弹
                    return bomb(game_state, {params[1], params[2]}, player);
                case 2:
                    // 强化
                    return strengthen(game_state, {params[1], params[2]}, player);
                case 3:
                    // 传送
                    return tp(game_state, {params[3], params[4]}, {params[1], params[2]}, player);
                case 4:
                    // 停止时间
                    return timestop(game_state, {params[1], params[2]}, player);
                default:
                    assert(false);
            }
        // 召唤将军
        case OperationType::CALL_GENERAL:
            return call_generals(game_state, player, {params[0], params[1]});
        default:
            assert(false);
    }
    assert(!"Should not reach here");
    return false;
}

/* #### `void init()`

* 描述：初始化游戏。
* 参数：无。
* 返回值：无。 */
void GameController::init()
{
    // 初始化游戏。
    std::tie(my_seat, game_state) = read_init_map();
}

void GameController::finish_and_send_our_ops() {
    // 应用我方操作
    logger.log(LOG_LEVEL_INFO, "Applying our ops:");
    for (const Operation &op : my_operation_list) {
        bool valid = execute_single_command(my_seat, op);

        logger.log(LOG_LEVEL_INFO, "\tOp: %s", op.str().c_str());
        if (!valid) throw std::runtime_error("Sending invalid operation: " + op.str());
    }

    // 结束我方操作回合，将操作列表打包发送并清空。
    std::string msg = "";
    for (const auto &op : my_operation_list) msg += op.stringize();
    msg += "8\n";
    write_to_judger(msg);

    my_operation_list.clear();
}
