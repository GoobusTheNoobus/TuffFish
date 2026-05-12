#pragma once

#include "types.hpp"
#include "bitboards.hpp"
#include "evaluate.hpp"
#include "engine.hpp"

#include <thread>

namespace TuffFish {

const std::string VERSION = "v1.2.1";

namespace UCI {

    inline void info_string(const std::string& msg) {
        std::cout << "info string " << msg << std::endl;
    }
    inline void info_depth(int depth, Score score, uint64_t nodes, uint64_t elapsed, MoveList& pv) {
        std::string score_string;
        const int abs_score = std::abs(score);

        if (abs_score >= MAX_CP && abs_score <= MATE_CP) {
            int mate_dist = (MATE_CP - abs_score + 1) / 2;

            if (score < 0) 
                mate_dist = -mate_dist;

            score_string = "mate " + std::to_string(mate_dist);
        }
        else
            score_string = "cp " + std::to_string(score);

        elapsed = std::max<uint64_t>(elapsed, 1);

        std::cout << "info depth " << depth << " score " << score_string << " nodes " << nodes << " nps " << (nodes * 1000 / elapsed) << " time " << elapsed << " pv ";

        for (Move m: pv) {
            std::cout << algebraic(m) << " ";
        }

        std::cout << std::endl;
    }

    void loop();
    
}
}


