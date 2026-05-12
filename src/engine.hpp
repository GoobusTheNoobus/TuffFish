#pragma once

#include "types.hpp"
#include "position.hpp"

#include <atomic>

namespace TuffFish {
    struct SearchInfo {
        int plies_from_root = 0;
        uint64_t nodes = 0;
    };
    

    int perft(Position& pos, int depth);
    void perft_divide(Position& pos, int depth);

    Score negamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta, bool null_allowed = true);
    Score qnegamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta);
    void search(Position& pos, int depth, int movetime);

    void request_stop();


    // ConsoleGUI thingy

    struct ConsoleUISearchOutput {
        std::atomic<uint64_t> nodes = 0;
        std::atomic<Score> score = 0;
        std::atomic<int> depth = 0;
        std::atomic<int> movetime = 1;

        uint64_t nps() {
            return nodes.load() * 1000 / movetime.load();
        }

        std::atomic<Move> move{0};
    };

    void console_ui_search(ConsoleUISearchOutput* output, Position pos, int movetime);

    

    
} // namespace TuffFish
