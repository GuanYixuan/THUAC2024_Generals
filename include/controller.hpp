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
    std::vector<Operation> last_enemy_ops;    // 记录上一次收到的敌方操作列表。


    void init();
    bool execute_single_command(int player, const Operation &op) { return execute_operation(game_state, player, op); }
    /**
     * @brief 为敌方应用动作序列`ops`
     * @note 此方法断言所有敌方操作合法
     */
    void apply_enemy_ops() {
        logger.log(LOG_LEVEL_INFO, "Applying enemy ops:");
        for (const auto &op : last_enemy_ops) {
            bool valid = execute_single_command(1 - my_seat, op);
            logger.log(LOG_LEVEL_INFO, "\t%s", op.str().c_str());

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
    void read_and_apply_enemy_ops() {
        last_enemy_ops = read_enemy_operations();
        apply_enemy_ops();
    }

    // 向动作列表中添加并立即执行一个操作
    void add_operation(const Operation &op) {
        my_operation_list.push_back(op);

        // 立即应用操作
        bool valid = execute_single_command(my_seat, op);
        if (!valid) {
            show_map(game_state, std::cerr);
            throw std::runtime_error("Applied invalid operation: " + op.str());
        }
    }
    // 向动作列表中添加并立即执行一组操作
    void add_operations(const Operation_list &ops) {
        assert(ops.player == my_seat);
        for (const auto &op : ops) add_operation(op);
    }


    // 我方等待发送的操作列表
    std::vector<Operation> my_operation_list;
    // 结束我方操作回合，将操作列表打包发送并清空
    void send_ops();
};

/* #### `void init()`

* 描述：初始化游戏。
* 参数：无。
* 返回值：无。 */
void GameController::init() {
    // 初始化游戏。
    my_seat = read_init_map(game_state);
}

void GameController::send_ops() {
    logger.log(LOG_LEVEL_INFO, "Sending ops:");
    for (const Operation &op : my_operation_list) logger.log(LOG_LEVEL_INFO, "\t%s", op.str().c_str());

    // 结束我方操作回合，将操作列表打包发送并清空。
    std::string msg = "";
    for (const auto &op : my_operation_list) msg += op.stringize();
    msg += "8\n";
    write_to_judger(msg);

    my_operation_list.clear();
}
