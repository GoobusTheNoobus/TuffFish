#include "engine.hpp"
#include "uci.hpp"

#include <atomic>
#include <chrono>

using namespace std::chrono;

namespace TuffChess {

// =========================== Timing & Search control ===========================
namespace {

steady_clock::time_point start;
int time_limit;
std::atomic_bool stop_;

bool should_stop() {
    if (stop_.load()) return true;

    if (time_limit > 0 && duration_cast<milliseconds>(steady_clock::now() - start).count() > time_limit) return true;

    return false;
}
}

void request_stop() {
    stop_.store(true);
}

// =========================== Perft ===========================

int perft(Position& pos, int depth) {
    if (depth <= 0) {
        return 1;
    }

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    int nodes = 0;

    for (Move move: list) {
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, list);
            continue;
        }

        nodes += perft(pos, depth - 1);

        pos.undo_move(move, list);
    }

    return nodes;
}

void perft_divide(Position& pos, int depth) {
    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    uint64_t total = 0;

    auto start = steady_clock::now();

    for (Move move: list) {
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, list);
            continue;
        }

        int nodes = perft(pos, depth - 1);

        total += nodes;

        std::cout << algebraic(move) << ": " << nodes << std::endl;

        pos.undo_move(move, list);
    }
    auto end = steady_clock::now();

    std::cout << "Total Nodes: " << total << std::endl;
    std::cout << "Nodes Per Second: " << (uint64_t)(total / duration_cast<milliseconds>(end - start).count() * 1000) << std::endl;
}

// =========================== Engine & Search Implementation ===========================

Score negamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta) {
    info.nodes++;
    info.plies_from_root++;

    if (should_stop())  {
        info.plies_from_root--;
        return TIMEOUT_CP;
    }
        
    
    if (depth == 0) {
        info.plies_from_root--;
        return pos.psqt_eval();
    }

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();
    Score best_score = -INF_CP;

    int legal_moves = 0;
    for (Move* p = list.begin(); p != list.end(); ++p) {
        Move move = *p;
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, list);
            continue;
        }
        ++legal_moves;

        Score score = -negamax(info, pos, depth - 1, -beta, -alpha);

        pos.undo_move(move, list);

        if (should_stop()) 
            return TIMEOUT_CP;

        if (score > best_score) 
            best_score = score;
        
        alpha = std::max(alpha, score);

        if (alpha >= beta) 
            break;
    }

    // No legal moves
    if (legal_moves == 0) {
        
        if (pos.is_in_check(moving_color)) {
            return -(MATE_CP - info.plies_from_root--);
        }

        info.plies_from_root--;
        return DRAW_CP;
    }
    
    info.plies_from_root--;
    return best_score;
}



void search(Position& pos, int depth_max, int movetime) {
    UCI::info_string("starting search with hce eval");
    UCI::info_string("depth=" + std::to_string(depth_max));
    UCI::info_string(" time=" + std::to_string(movetime) + "ms");
    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    Score prev_score = 0;
    Move  prev_move  = 0;

    // Set timing information
    start = steady_clock::now();
    time_limit = movetime;
    stop_.store(false);

    SearchInfo info;

    for (int depth = 1; depth <= depth_max; ++depth) {
        if (should_stop()) break;

        Score best_score = -INF_CP;
        Move best_move = 0;

        int legal_moves = 0;
        for (Move* p = list.begin(); p != list.end(); ++p) {

            int alpha = -INF_CP;
            int beta  = INF_CP;

            Move move = *p;
            pos.make_move(move);

            if (pos.is_in_check(moving_color)) {
                pos.undo_move(move, list);
                continue;
            }  
            Score score = -negamax(info, pos, depth - 1, -beta, -alpha);
            // std::cout << "Move: " << algebraic(move) << ", Score " << score << std::endl;

            pos.undo_move(move, list);

            if (should_stop()) {
                best_move = 0;
                best_score = TIMEOUT_CP;

                break;
            }

            if (score > best_score) {
                best_move = move;
                best_score = score;
            }
            ++legal_moves;
        }

        if (depth == 1) {
            if (legal_moves == 0) {
            // No legal moves
            if (pos.is_in_check(moving_color)) {
                std::cout << "Invalid Position: Checkmate" << std::endl;
                return;
            }
            std::cout << "Invalid Position: No Legal Moves" << std::endl;
            return;
        }
        }

        if (best_score != TIMEOUT_CP && best_move != 0) {
            prev_score = best_score;
            prev_move = best_move;
        } else break;
        
        

        MoveList pv;
        pv.add(prev_move);

        UCI::info_depth(depth, prev_score, info.nodes, duration_cast<milliseconds>(steady_clock::now() - start).count(), pv);
    }

    std::cout << "bestmove " << algebraic(prev_move) << std::endl;
}
}