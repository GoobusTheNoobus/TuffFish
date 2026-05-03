#include "engine.hpp"
#include "uci.hpp"

#include <atomic>
#include <chrono>

using namespace std::chrono;

namespace TuffFish {
    
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

// =========================== Search Settings ===========================
// Barely any
constexpr int QS_MAX_DEPTH        = 32;
constexpr bool USE_QSEARCH        = true;
constexpr int DELTA_MARGIN        = MG_VALUES[PAWN] * 2;

// =========================== Perft ===========================

int perft(Position& pos, int depth) {
    if (depth <= 0) {
        return 1;
    }

    StoredGameState gs(pos);

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    int nodes = 0;

    for (Move move: list) {
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, gs);
            continue;
        }

        nodes += perft(pos, depth - 1);

        pos.undo_move(move, gs);
    }

    return nodes;
}

void perft_divide(Position& pos, int depth) {
    StoredGameState gs(pos);

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    uint64_t total = 0;

    auto start = steady_clock::now();

    for (Move move: list) {
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, gs);
            continue;
        }

        int nodes = perft(pos, depth - 1);

        total += nodes;

        std::cout << algebraic(move) << ": " << nodes << std::endl;

        pos.undo_move(move, gs);
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
        if constexpr (USE_QSEARCH) {
            return qnegamax(info, pos, QS_MAX_DEPTH, alpha, beta);
        } else {
            return pos.psqt_eval();
        }

        
    }
    StoredGameState gs(pos);

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();
    Score best_score = -INF_CP;

    int legal_moves = 0;
    for (Move* p = list.begin(); p != list.end(); ++p) {
        Move move = *p;
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, gs);
            continue;
        }
        ++legal_moves;

        Score score = -negamax(info, pos, depth - 1, -beta, -alpha);

        if (score == -TIMEOUT_CP) {
            pos.undo_move(move, gs);
            info.plies_from_root--;
            return TIMEOUT_CP;
        }

        pos.undo_move(move, gs);

        if (should_stop()) {
            info.plies_from_root--;
            return TIMEOUT_CP;
        }
            

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

Score qnegamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta) {
    info.nodes++;

    if (depth == 0) return pos.psqt_eval();

    info.plies_from_root++;

    Score stand_pat = pos.psqt_eval();

    if (stand_pat >= beta) {
        info.plies_from_root--;
        return beta;
    }
    
    alpha = std::max(stand_pat, alpha);

    StoredGameState gs(pos);

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    for (Move* p = list.begin(); p != list.end(); ++p) {
        Move move = *p;

        bool is_noisy = pos.piece_on(dest(move)) != NO_PIECE || flag(move) >= EN_PASSANT;

        if (!is_noisy) continue;

        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, gs);
            continue;
        }

        Score score = -qnegamax(info, pos, depth - 1, -beta, -alpha);

        pos.undo_move(move, gs);

        if (score >= beta) {
            info.plies_from_root--;
            return beta;
        }

        alpha = std::max(score, alpha);
        
    }

    info.plies_from_root--;
    return alpha;
}

void search(Position& pos, int depth_max, int movetime) {
    UCI::info_string("starting search with hce eval");
    UCI::info_string("depth = " + std::to_string(depth_max));
    UCI::info_string(" time = " + std::to_string(movetime) + "ms");
    StoredGameState gs(pos);
    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    Score prev_score = 0;
    Move  prev_move  = 0;

    // Keep a legal fallback move in case we time out before finishing depth 1.
    for (Move move : list) {
        pos.make_move(move);
        if (!pos.is_in_check(moving_color)) {
            prev_move = move;
            pos.undo_move(move, gs);
            break;
        }
        pos.undo_move(move, gs);
    }

    // Set timing information
    start = steady_clock::now();
    time_limit = movetime;
    stop_.store(false);

    SearchInfo info;

    int depth;
    for (depth = 1; depth <= depth_max; ++depth) {
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
                pos.undo_move(move, gs);
                continue;
            }  
            Score score = -negamax(info, pos, depth - 1, -beta, -alpha);
            // std::cout << "Move: " << algebraic(move) << ", Score " << score << std::endl;

            pos.undo_move(move, gs);

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
                UCI::info_string(pos.is_in_check(moving_color)
                    ? "No legal moves (checkmate)"
                    : "No legal moves (stalemate)");
                std::cout << "bestmove 0000" << std::endl;
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

    if (prev_move == 0) {
        UCI::info_string("No legal move selected");
        std::cout << "bestmove 0000" << std::endl;
        return;
    }

    std::cout << "bestmove " << algebraic(prev_move) << std::endl;
    UCI::info_string("search stopped");
}
}
