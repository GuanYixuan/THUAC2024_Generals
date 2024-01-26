#pragma once
#include <string>
#include <fstream>
#include <typeindex>
#include <typeinfo>
#include "gamestate.hpp"
using namespace std;
// This function shows the map of the game state and writes it to a file
void show_map(GameState state, string file_name)
{
    // open the file for appending
    ofstream f;
    f.open(file_name, ios::app);
    // write the state information to the file
    f << state.round << "\n";
    f << "[" << state.coin[0] << ", " << state.coin[1] << "]"
      << "\n";
    cout << "[[";
    cout << state.tech_level[0][0] << ", " << state.tech_level[0][1] << ", " << state.tech_level[0][2] << ", " << state.tech_level[0][3];
    cout << "], [";
    cout << state.tech_level[1][0] << ", " << state.tech_level[1][1] << ", " << state.tech_level[1][2] << ", " << state.tech_level[1][3];
    cout << "]]"
         << "\n";
    for (int i = 0; i < Constant::row; ++i)
    {
        for (int j = 0; j < Constant::col; ++j)
        {
            auto element = state.board[i][j];
            f << (int)element.type << " ";
            f << element.player << " ";
            f << element.army << " ";
        }
        f << "\n";
    }
    for (auto element : state.generals)
    {
        int typenow;
        if (typeid(element) == typeid(MainGenerals))
        {
            typenow = 0;
        }
        else if (typeid(element) == typeid(SubGenerals))
        {
            typenow = 1;
        }
        else
        {
            typenow = 2;
        }
        f << element.id << " ";
        f << typenow << " ";
        f << element.player << " ";
        f << element.position.first << " ";
        f << element.position.second << "\n";
    }
    // close the file
    f.close();
}
