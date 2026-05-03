#include "engine.hpp"
#include "uci.hpp"

#include <atomic>
#include <chrono>

using namespace std::chrono;

namespace TuffFish {
    
// =========================== Timing & Search control ===========================

namespace {

// Used for search timing
steady_clock::time_point start;
// Measured in ms
int time_limit;
// Stop flag, when active, search will exit as soon as possible
std::atomic_bool stop_;

inline bool should_stop() {
    return stop_.load() || (time_limit > 0 && duration_cast<milliseconds>(steady_clock::now() - start).count() > time_limit);
}

} // namespace anonymous

void request_stop() { stop_.store(true); }

// =========================== Search Settings ===========================
// Barely any
constexpr int QS_MAX_DEPTH        = 32;
constexpr bool USE_QSEARCH        = true;
constexpr int DELTA_MARGIN        = MG_VALUES[PAWN] * 2;

// =========================== Engine & Search Implementation ===========================

namespace {

using ScoreList = std::array<int, 256>;

static constexpr Score PROMO_SCORE_BONUS[] = {
    KNIGHT_VALUE_MG - PAWN_VALUE_MG,
    BISHOP_VALUE_MG - PAWN_VALUE_MG,
    ROOK_VALUE_MG   - PAWN_VALUE_MG,
    QUEEN_VALUE_MG  - PAWN_VALUE_MG
};

// Stockfish does this btw
struct MovePick {
    MoveList  list;
    ScoreList scores;
    Move*     current;

    

    int score_move(const Position& pos, Move m) {
        Piece captured = pos.piece_on(dest(m));
        Piece moved    = pos.piece_on(from(m));
        MoveFlag flag_ = flag(m);

        if (captured != NO_PIECE) {
            Score victim = flag_ == EN_PASSANT ? make_piece(PAWN, opposite(color_of(moved))) : MG_VALUES[type_of(captured)];
            Score attacker = MG_VALUES[type_of(moved)];

            return 1'000'000 + (10'000 * victim) - (attacker * 1000);
        } else if (flag_ >= NPROMO) {
            return PROMO_SCORE_BONUS[flag_ - NPROMO] + 900'000;
        }
        return 0;
    }

    MovePick(const Position& pos) {
        current = list.begin();
  
        pos.generate_moves(list);
        // Cache scores into seperate array
        for (int i = 0; i < list.size(); ++i) {
            Move m = list[i];

            scores[i] = score_move(pos, m);
        }

    }

    Move* next() {
        if (current >= list.end()) return nullptr;

        Move* best = current;
        for (Move* m = current + 1; m != list.end(); ++m) {
            if (scores[m - list.begin()] > scores[best - list.begin()])
                best = m;
        }

        auto ci = current - list.begin();
        auto bi = best - list.begin();
        std::swap(scores[ci], scores[bi]);
        std::swap(*current, *best);

        Move* selected = current;
        ++current;
        return selected;
    }

};

} // namespace anonymous

// Main search recursive function
// Uses a negamax framework 
// score = -negamax(-b, -a)
Score negamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta) {
    info.nodes++;
    
    if (should_stop())
        return TIMEOUT_CP;
    
    info.plies_from_root++;
    
    if (depth == 0) {
        info.plies_from_root--;
        if constexpr (USE_QSEARCH) {
            // Queiscence search to avoid horizon effect
            return qnegamax(info, pos, QS_MAX_DEPTH, alpha, beta);
        } else {
            return pos.evaluate();
        }
    }

    // We store the game infos of this position before modifying
    // to be able to restore it
    StoredGameState gs(pos);

    MovePick moves(pos);

    Color moving_color = pos.get_side();
    Score best_score = -INF_CP;

    int legal_moves = 0;
    Move* move_ptr;
    while (move_ptr = moves.next()) {

        Move move = *move_ptr;
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

        best_score = std::max(score, best_score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) 
            break;
    }

    // No legal moves
    if (legal_moves == 0) {
        if (pos.is_in_check(moving_color)) 
            return -(MATE_CP - info.plies_from_root--);
        info.plies_from_root--;
        return DRAW_CP;
    }
    
    info.plies_from_root--;
    return best_score;
}

Score qnegamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta) {
    info.nodes++;

    if (depth == 0) return pos.evaluate();

    info.plies_from_root++;

    Score stand_pat = pos.evaluate();

    if (stand_pat >= beta) {
        info.plies_from_root--;
        return beta;
    }
    
    alpha = std::max(stand_pat, alpha);

    StoredGameState gs(pos);

    MoveList list;
    pos.generate_moves(list);

    Color moving_color = pos.get_side();

    bool in_check = pos.is_in_check(pos.get_side());

    for (Move* p = list.begin(); p != list.end(); ++p) {
        Move move = *p;

        // Only search noisy moves
        bool is_noisy = pos.piece_on(dest(move)) != NO_PIECE || flag(move) >= EN_PASSANT;

        if (!is_noisy) continue;

        MoveFlag flag_ = flag(move);

        Piece attacker = pos.piece_on(from(move));
        Piece victim = flag_ == EN_PASSANT ? make_piece(PAWN, opposite(color_of(attacker))) : pos.piece_on(dest(move));
        
        Score gain = 0;

        if (victim != NO_PIECE) {
            gain += MG_VALUES[type_of(victim)];
        } if (attacker != NO_PIECE) {
            gain -= MG_VALUES[type_of(attacker)];
        } if (flag_ >= NPROMO) {
            gain += PROMO_SCORE_BONUS[flag_ - NPROMO];
        }

        if (!in_check && gain + stand_pat + DELTA_MARGIN < alpha) continue;

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

// Root search negamax function, this is the function called
// upon recieving the "go" UCI command
void search(Position& pos, int depth_max, int movetime) {
    
    StoredGameState gs(pos);
    

    Color moving_color = pos.get_side();

    Score prev_score = 0;
    Move  prev_move  = 0;


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

        MovePick list(pos);

        int legal_moves = 0;
        Move* p;
        while (p = list.next()) {

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

        // No legal moves
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
}

// =========================== Perft ===========================

int perft(Position& pos, int depth) {
    if (depth <= 0) 
        return 1;
    
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
} // namespace TuffFish
