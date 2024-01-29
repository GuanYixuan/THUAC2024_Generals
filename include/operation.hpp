#pragma once
#include "gamestate.hpp"
#include <vector>

enum class OperationType {
    DEFAULT_OP,
    MOVE_ARMY,
    MOVE_GENERALS,
    UPDATE_GENERALS,
    USE_GENERAL_SKILLS,
    UPDATE_TECH,
    USE_SUPERWEAPON,
    CALL_GENERAL
};

class Operation {
public:
    OperationType opcode;
    std::vector<int> operand;

    Operation() noexcept : opcode(OperationType::DEFAULT_OP) {}
    Operation(OperationType opcode, const std::vector<int>& operand) noexcept : opcode(opcode), operand(operand) {}
    Operation(OperationType opcode, std::vector<int>&& operand) noexcept : opcode(opcode), operand(std::move(operand)) {}

    std::string stringize() const noexcept {
        std::string result = std::to_string(int(opcode)) + " ";
        for (const int& param : operand) result += std::to_string(param) + " ";
        result.push_back('\n');
        return result;
    }

    // 便捷构造：移动军队
    static Operation move_army(const Coord& position, Direction direction, int num) noexcept {
        return Operation(OperationType::MOVE_ARMY, {position.x, position.y, static_cast<int>(direction) + 1, num});
    }
    // 便捷构造：移动将军
    static Operation move_generals(int generals_id, const Coord& position) noexcept {
        return Operation(OperationType::MOVE_GENERALS, {generals_id, position.x, position.y});
    }
    // 便捷构造：升级将军属性
    static Operation update_generals(int generals_id, QualityType type) noexcept {
        return Operation(OperationType::UPDATE_GENERALS, {generals_id, static_cast<int>(type) + 1});
    }
    // 便捷构造：使用将军技能
    static Operation generals_skill(int generals_id, SkillType type, const Coord& position) noexcept {
        return Operation(OperationType::USE_GENERAL_SKILLS, {generals_id, static_cast<int>(type) + 1, position.x, position.y});
    }
    // 便捷构造：升级科技
    static Operation update_tech(TechType type) noexcept {
        return Operation(OperationType::UPDATE_TECH, {static_cast<int>(type) + 1});
    }
    // 便捷构造：使用超级武器
    static Operation use_superweapon(WeaponType type, const Coord& destination, const Coord& origin = {-1, -1}) noexcept {
        return Operation(OperationType::USE_SUPERWEAPON, {static_cast<int>(type) + 1, destination.x, destination.y, origin.x, origin.y});
    }
    // 便捷构造：召唤副将
    static Operation call_generals(const Coord& position) noexcept {
        return Operation(OperationType::CALL_GENERAL, {position.x, position.y});
    }
};
