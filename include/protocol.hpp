#pragma once

#include <vector>
#include <tuple>
#include <iostream>

#include "json.hpp"
#include "operation.hpp"
#include "gamestate.hpp"

/**
 * @brief 读取初始地图及先后手信息
 * @return std::tuple<int, Gamestate> - 即先后手编号和游戏状态
 */
std::tuple<int, GameState> read_init_map() {
    GameState gamestate;
    std::string s;
    getline(std::cin, s);

    auto d = nlohmann::json::parse(s);
    int my_seat = d["Player"];
    auto map = d["Cells"], generals = d["Generals"], coins = d["Coins"];
    std::string types = d["Cell_type"].dump();
    gamestate.coin[0] = coins[0], gamestate.coin[1] = coins[1];
    for (int i = 0, siz = map.size(); i < siz; ++i) {
        int x = int(map[i][0][0]);
        int y = int(map[i][0][1]);
        Cell& cell = gamestate.board[x][y];

        cell.type = CellType(int(types[i + 1]) - '0');
        cell.player = int(map[i][1]);
        cell.army = int(map[i][2]);
        cell.position = Coord(x, y);
    }
    for (int i = 0, siz = generals.size(); i < siz; ++i) {
        int id = int(generals[i]["Id"]);
        int player = int(generals[i]["Player"]);
        gamestate.next_generals_id++;

        Coord position{int(generals[i]["Position"][0]), int(generals[i]["Position"][1])};
        Cell& cell = gamestate[position];

        switch (int(generals[i]["Type"])) {
        case 1:
            cell.generals = new MainGenerals(id, player, position);
            break;
        case 2:
            cell.generals = new SubGenerals(id, player, position);
            break;
        case 3:
            cell.generals = new OilWell(id, player, position);
            break;
        default:
            assert(false);
        }
        gamestate.generals.push_back(cell.generals);
    }
    return std::tuple<int, GameState>(my_seat, gamestate);
}
/**
 * @brief 读取敌方操作列表
 * @return a vector of operations
 */
std::vector<Operation> read_enemy_operations() {
    static std::vector<Operation> operations;
    static std::vector<int> params;
    int param, op_type;

    operations.clear();
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        std::stringstream ss(line);

        // 读取操作类型
        ss >> op_type;
        // 操作结束
        if (op_type == 8) break;

        // 读取参数
        while (true) {
            ss >> param;
            if (ss.fail()) break;
            params.push_back(param);
        }
        operations.emplace_back(static_cast<OperationType>(op_type), params);
        params.clear(); // 原本的代码并未进行此重置
    }
    return operations;
}
inline void convert_to_big_endian(const void *src, std::size_t size, void *dest)
{
    for (std::size_t i = 0; i < size; ++i)
        static_cast<char *>(dest)[size - i - 1] = static_cast<const char *>(src)[i];
}
inline void print_header(int size)
{
    // 按照judger要求发送消息长度
    char buf[4] = {};
    convert_to_big_endian(&size, sizeof(size), buf);
    for (int i = 0; i < 4; ++i)
        std::cout << buf[i];
}

/**
 * @brief 向评测机发送一个字符串
 * @param msg string
 */
void write_to_judger(const std::string &msg)
{
    // 先发送消息长度，再发送消息本身
    print_header(msg.length());
    std::cout << msg;
}
