#pragma once
#include <string>
#include <fstream>
#include <typeindex>
#include <typeinfo>
#include "gamestate.hpp"

// This function shows the map of the game state and writes it to a file
void show_map(const GameState& state, std::ostream& f) {
    // write the state information to the file
    f << state.round << "\n";
    f << "[" << state.coin[0] << ", " << state.coin[1] << "]"
      << "\n";
    f << "[[";
    f << state.tech_level[0][0] << ", " << state.tech_level[0][1] << ", " << state.tech_level[0][2] << ", " << state.tech_level[0][3];
    f << "], [";
    f << state.tech_level[1][0] << ", " << state.tech_level[1][1] << ", " << state.tech_level[1][2] << ", " << state.tech_level[1][3];
    f << "]]"
      << "\n";
    for (int i = Constant::row - 1; i >= 0; --i) {
        for (int j = 0; j < Constant::col; ++j) {
            const Cell& element = state.board[j][i];
            if (element.player == 1) f << wrap("-%2d ", element.army);
            else f << wrap(" %2d ", element.army);
        }
        f << "\n";
    }
    for (const Generals* element : state.generals) {
        char typenow = '?';
        if (dynamic_cast<const MainGenerals*>(element)) typenow = 'M';
        else if (dynamic_cast<const SubGenerals*>(element)) typenow = 'S';
        else if (dynamic_cast<const OilWell*>(element)) typenow = 'O';
        else assert(false);

        f << "id: " << element->id << " ";
        f << "type: " << typenow << " ";
        f << "player: " << element->player << " ";
        f << "position: (" << element->position.x << ", " << element->position.y << ")\n";
    }
    f << '\n';
}
