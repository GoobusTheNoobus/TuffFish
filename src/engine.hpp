#pragma once

#include "types.hpp"
#include "position.hpp"

namespace TuffFish {
    struct SearchInfo {
        int plies_from_root = 0;
        uint64_t nodes = 0;
    };
    

    int perft(Position& pos, int depth);
    void perft_divide(Position& pos, int depth);

    Score negamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta);
    Score qnegamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta);
    void search(Position& pos, int depth, int movetime);

    void request_stop();

    
}