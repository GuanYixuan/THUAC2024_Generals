#include "include/ai_template.hpp"
#include "include/test_sync.hpp"

std::vector<Operation> simple_ai(int my_seat, const GameState &gamestate) {
    std::vector<Operation> ops;

    show_map(gamestate, std::cerr);

    return ops;
}

int main() {
    run_ai(simple_ai);
}
