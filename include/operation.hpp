#pragma once

#include <array>
#include <vector>

#include "gamestate.hpp"

class OperationType {
public:
    enum __Inner_type {
        DEFAULT_OP,
        MOVE_ARMY,
        MOVE_GENERALS,
        UPDATE_GENERALS,
        USE_GENERAL_SKILLS,
        UPDATE_TECH,
        USE_SUPERWEAPON,
        CALL_GENERAL
    } __val;

    constexpr OperationType() noexcept : __val(DEFAULT_OP) {}
    constexpr OperationType(__Inner_type val) noexcept : __val(val) {}
    constexpr explicit OperationType(int val) noexcept : __val(__Inner_type(val)) {}

    operator __Inner_type() const noexcept { return __val; }
    explicit operator bool() const noexcept = delete;

    // 获取描述字符串
    const char* str() const noexcept { return __str_table[__val]; }

private:
    static constexpr const char* __str_table[] = {
        "DEFAULT_OP", "MOVE_ARMY", "MOVE_GENERALS", "UPDATE_GENERALS", "USE_GENERAL_SKILLS", "UPDATE_TECH", "USE_SUPERWEAPON", "CALL_GENERAL"
    };
};

// 单个操作，不包含玩家信息
class Operation {
public:
    OperationType opcode;
    int operand_count;
    int operand[5]; // 至多五个参数

    Operation() noexcept : opcode(OperationType::DEFAULT_OP), operand_count(0), operand{0} {}
    Operation(OperationType opcode, const std::vector<int>& _operand) noexcept : opcode(opcode), operand_count(_operand.size()) {
        assert(_operand.size() <= 5);
        for (int i = 0; i < operand_count; ++i) operand[i] = _operand[i];
    }

private:
    // 构造函数私有，应该采用便捷构造函数进行构造
    template<std::size_t _operand_count>
    Operation(OperationType opcode, const std::array<int, _operand_count>& _operand) noexcept : opcode(opcode), operand_count(_operand.size()) {
        assert(_operand.size() <= 5);
        for (int i = 0; i < operand_count; ++i) operand[i] = _operand[i];
    }

public:

    int operator[](int index) const noexcept {
        assert(index < operand_count);
        return operand[index];
    }

    // 获取描述字符串
    std::string str() const noexcept {
        std::string result{opcode.str()};
        result += ' ';
        for (int i = 0; i < operand_count; ++i) result += std::to_string(operand[i]) + ' ';
        return result;
    }

    std::string stringize() const noexcept {
        std::string result = std::to_string(int(opcode)) + " ";
        for (int i = 0; i < operand_count; ++i) result += std::to_string(operand[i]) + " ";
        result.push_back('\n');
        return result;
    }

    // 便捷构造：移动军队
    static Operation move_army(const Coord& position, Direction direction, int num) noexcept {
        return Operation(OperationType::MOVE_ARMY, std::array<int, 4>{position.x, position.y, static_cast<int>(direction) + 1, num});
    }
    // 便捷构造：移动将军
    static Operation move_generals(int generals_id, const Coord& position) noexcept {
        return Operation(OperationType::MOVE_GENERALS, std::array<int, 3>{generals_id, position.x, position.y});
    }
    // 便捷构造：升级将军属性
    static Operation upgrade_generals(int generals_id, QualityType type) noexcept {
        return Operation(OperationType::UPDATE_GENERALS, std::array<int, 2>{generals_id, static_cast<int>(type) + 1});
    }
    // 便捷构造：使用将军技能
    static Operation generals_skill(int generals_id, SkillType type, const Coord& position = {-1, -1}) noexcept {
        return Operation(OperationType::USE_GENERAL_SKILLS, std::array<int, 4>{generals_id, static_cast<int>(type) + 1, position.x, position.y});
    }
    // 便捷构造：升级科技
    static Operation upgrade_tech(TechType type) noexcept {
        return Operation(OperationType::UPDATE_TECH, std::array<int, 1>{static_cast<int>(type) + 1});
    }
    // 便捷构造：使用超级武器
    static Operation use_superweapon(WeaponType type, const Coord& destination, const Coord& origin = {-1, -1}) noexcept {
        return Operation(OperationType::USE_SUPERWEAPON, std::array<int, 5>{static_cast<int>(type) + 1, destination.x, destination.y, origin.x, origin.y});
    }
    // 便捷构造：召唤副将
    static Operation recruit_generals(const Coord& position) noexcept {
        return Operation(OperationType::CALL_GENERAL, std::array<int, 2>{position.x, position.y});
    }
};

// 一系列操作
class Operation_list {
public:
    // 执行操作的玩家
    int player;
    // 抽象的分数，可用于排序等
    double score;
    // 操作列表
    std::vector<Operation> ops;

    Operation_list(int player) noexcept : player(player) {}
    Operation_list(int player, const std::vector<Operation>& ops) noexcept : player(player), ops(ops) {}
    Operation_list(int player, std::vector<Operation>&& ops) noexcept : player(player), ops(std::move(ops)) {}

    // 取值函数与迭代函数
    Operation& operator[](int index) noexcept { return ops[index]; }
    const Operation& operator[](int index) const noexcept { return ops[index]; }
    std::vector<Operation>::iterator begin() noexcept { return ops.begin(); }
    std::vector<Operation>::iterator end() noexcept { return ops.end(); }
    std::vector<Operation>::const_iterator begin() const noexcept { return ops.begin(); }
    std::vector<Operation>::const_iterator end() const noexcept { return ops.end(); }

    bool operator> (const Operation_list& other) const noexcept { return score > other.score; }

    // 连接运算
    Operation_list& operator+=(const Operation& op) noexcept {
        ops.push_back(op);
        return *this;
    }
    Operation_list& operator+=(const Operation_list& other) noexcept {
        assert(player == other.player);
        ops.insert(ops.end(), other.ops.begin(), other.ops.end());
        return *this;
    }
};
