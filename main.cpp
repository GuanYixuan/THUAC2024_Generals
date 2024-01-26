#include "include/ai_template.hpp"

std::vector<Operation> simple_ai(int my_seat, const GameState &gamestate) {
    std::vector<Operation> ops;
    if(gamestate.round==4)ops.push_back(move_army_op(gamestate.generals[my_seat].position,Direction::DOWN,1));
    return ops;
}

int main() {
    run_ai(simple_ai);
}
