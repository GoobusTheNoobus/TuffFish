#include "engine.hpp"
#include "tt.hpp"
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

constexpr int QS_MAX_DEPTH        = 32;
constexpr bool USE_QSEARCH        = true;
constexpr bool USE_NMP            = true;
constexpr int DELTA_MARGIN        = MG_VALUES[PAWN] * 2;
constexpr int NMP_MIN_DEPTH       = 3;
constexpr int NMP_BASE_REDUCTION  = 3;
constexpr int NMP_MIN_PHASE       = 10;

inline Score score_to_tt(Score score, int plies_from_root) {
    if (score >= MAX_CP) return score + plies_from_root;
    if (score <= -MAX_CP) return score - plies_from_root;
    return score;
}

inline Score score_from_tt(Score score, int plies_from_root) {
    if (score >= MAX_CP) return score - plies_from_root;
    if (score <= -MAX_CP) return score + plies_from_root;
    return score;
}

// =========================== Engine & Search Implementation ===========================

namespace {

using ScoreList = std::array<int, 256>;

// The bonus for promoting a piece
// You trade a pawn for whatever piece you promote to, ergo
// the following values
static constexpr Score PROMO_SCORE_BONUS[] = {
    KNIGHT_VALUE_MG - PAWN_VALUE_MG,
    BISHOP_VALUE_MG - PAWN_VALUE_MG,
    ROOK_VALUE_MG   - PAWN_VALUE_MG,
    QUEEN_VALUE_MG  - PAWN_VALUE_MG
};


// Stockfish does this btw
// Instead of a raw move list and looping thru every single move,
// we have a container with a list of "scores" based on how GOOD the 
// move looks. For example, a capture is more appealing than a normal 
// move. By trying the most promising moves first, we can cause more 
// cutoffs
struct MovePick {
    MoveList  list;
    ScoreList scores;
    Move*     current;

    // Score based on how promising it looks
    int score_move(const Position& pos, Move m, Move tt_move) {
        if (tt_move != 0 && m == tt_move) return 2'000'000;

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

    MovePick(const Position& pos, Move tt_move = 0) {
        current = list.begin();
  
        pos.generate_moves(list);
        // Cache scores into seperate array
        for (int i = 0; i < list.size(); ++i) {
            Move m = list[i];

            scores[i] = score_move(pos, m, tt_move);
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
Score negamax(SearchInfo& info, Position& pos, int depth, Score alpha, Score beta, bool null_allowed) {
    if (pos.is_repetition()) return DRAW_CP;

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

    // TT probe (before generating moves)
    //
    // We first check whether this exact position was already searched.
    //
    // If stored depth is deep enough:
    // - EXACT: return immediately (no need to search pos again)
    // - LOWER: raise alpha (position is at least this good)
    // - UPPER: lower beta  (position is at most this good)

    // If alpha >= beta after applying bounds, this node is resolved and we can
    // cut off without exploring moves.

    const Score alpha_orig = alpha;
    const Score beta_orig = beta;
    const HashKey key = pos.zobrist();
    Move tt_move = 0;

    if (HashEntry* entry = TranspositionTable::get(key)) {
        tt_move = entry->best_move;
        if (entry->depth >= depth) {
            Score tt_score = score_from_tt(entry->score, info.plies_from_root);

            if (entry->flag == VALUE_EXACT) {
                info.plies_from_root--;
                return tt_score;
            }
            if (entry->flag == VALUE_LOWER) {
                alpha = std::max(alpha, tt_score);
            } else if (entry->flag == VALUE_UPPER) {
                beta = std::min(beta, tt_score);
            }

            if (alpha >= beta) {
                info.plies_from_root--;
                return entry->flag == VALUE_LOWER ? alpha : beta;
            }
        }
    }

    Color moving_color = pos.get_side();
    bool in_check = pos.is_in_check(moving_color);

    // Null Move Pruning
    if constexpr (USE_NMP)
    if (null_allowed
        && depth >= NMP_MIN_DEPTH
        && !in_check
        && Evaluate::game_phase(&pos) >= NMP_MIN_PHASE
        && beta < MAX_CP) {

        const int reduction = NMP_BASE_REDUCTION + (depth >= 6 ? 1 : 0);
        const int null_depth = depth - reduction - 1;

        if (null_depth > 0) {
            StoredGameState null_gs(pos);
            pos.make_move(0);

            Score null_score = -negamax(info, pos, null_depth, -beta, -beta + 1, false);

            pos.undo_move(0, null_gs);

            if (null_score == -TIMEOUT_CP) {
                info.plies_from_root--;
                return TIMEOUT_CP;
            }

            if (null_score >= beta) {
                info.plies_from_root--;
                return beta;
            }
        }
    }

    StoredGameState gs(pos);
    
    MovePick moves(pos, tt_move);

    Score best_score = -INF_CP;
    Move best_move = 0;

    int legal_moves = 0;
    Move* move_ptr;
    while ((move_ptr = moves.next())) {

        Move move = *move_ptr;
        pos.make_move(move);

        if (pos.is_in_check(moving_color)) {
            pos.undo_move(move, gs);
            continue;
        }
        ++legal_moves;

        Score score = -negamax(info, pos, depth - 1, -beta, -alpha, true);

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

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        alpha = std::max(alpha, score);

        if (alpha >= beta)
            break;
    }

    // No legal moves
    if (legal_moves == 0) {
        Score terminal_score = DRAW_CP;
        if (in_check)
            terminal_score = -(MATE_CP - info.plies_from_root);

        TranspositionTable::put(key, score_to_tt(terminal_score, info.plies_from_root), 0, depth, VALUE_EXACT);

        info.plies_from_root--;
        return terminal_score;
    }

    // TT store
    // We store what this node proved;
    // - VALUE_UPPER: score <= original_alpha
    // - VALUE_LOWER: score >= original_beta
    // - VALUE_EXACT: original_alpha < score < beta

    TTFlag flag = VALUE_EXACT;

    if (best_score <= alpha_orig) flag = VALUE_UPPER;
    else if (best_score >= beta_orig) flag = VALUE_LOWER;

    TranspositionTable::put(key, score_to_tt(best_score, info.plies_from_root), best_move, depth, flag);

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
    Move fallback_move = 0;

    // Precompute one legal root move so we never emit an illegal "bestmove 0000"
    // when timing expires before finishing a full iteration.
    {
        MoveList root_moves;
        pos.generate_moves(root_moves);
        for (Move m: root_moves) {
            pos.make_move(m);
            bool legal = !pos.is_in_check(moving_color);
            pos.undo_move(m, gs);

            if (legal) {
                fallback_move = m;
                break;
            }
        }
    }

    Score prev_score = 0;
    Move  prev_move  = 0;

    // Set timing information
    start = steady_clock::now();
    time_limit = movetime;
    stop_.store(false);

    SearchInfo info;

    // Iterative Deepening container for DFS search
    int depth;
    for (depth = 1; depth <= depth_max; ++depth) {
        if (should_stop()) break;

        std::vector<std::pair<Move, Score>> map;

        Score best_score = -INF_CP;
        Move best_move = 0;

        Move root_tt_move = 0;
        if (HashEntry* entry = TranspositionTable::get(pos.zobrist()))
            root_tt_move = entry->best_move;

        MovePick list(pos, root_tt_move);
        const Score alpha = -INF_CP;
        const Score beta  = INF_CP;

        int legal_moves = 0;
        Move* p;
        while ((p = list.next())) {
            Move move = *p;
            pos.make_move(move);

            if (pos.is_in_check(moving_color)) {
                pos.undo_move(move, gs);
                continue;
            }  
            Score score = -negamax(info, pos, depth - 1, -beta, -alpha);
            // std::cout << "Move: " << algebraic(move) << ", Score " << score << std::endl;
            map.push_back(std::pair{move, score});

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
        /*for (auto pair: map) {
            UCI::info_string(algebraic(pair.first) + ": " + std::to_string(pair.second));
        }*/
        
        
    }

    if (prev_move == 0) {
        if (fallback_move != 0) {
            UCI::info_string("Using fallback legal move after early stop");
            prev_move = fallback_move;
        } else {
            UCI::info_string("No legal move selected");
            std::cout << "bestmove 0000" << std::endl;
            return;
        }
    }

    std::cout << "bestmove " << algebraic(prev_move) << std::endl;
}

// =========================== Perft ===========================
// Used to determine the number of nodes in a certain position to 
// a certain depth. Good for verifying that the engine's move 
// generation is correct

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

// =========================== GUI ===========================

// This is a function where instead of printing the results, it returns and writes to 
// an info struct
void console_ui_search(ConsoleUISearchOutput* output, Position pos, int movetime) {
    StoredGameState gs(pos);
    
    Color moving_color = pos.get_side();

    Score prev_score = 0;
    Move  prev_move  = 0;

    // Set timing information
    start = steady_clock::now();
    time_limit = movetime;
    stop_.store(false);

    SearchInfo info;

    // Iterative Deepening container for DFS search
    int depth;
    for (depth = 1; depth <= MAX_PLY; ++depth) {
        if (should_stop()) break;

        Score best_score = -INF_CP;
        Move best_move = 0;

        Move root_tt_move = 0;
        if (HashEntry* entry = TranspositionTable::get(pos.zobrist()))
            root_tt_move = entry->best_move;

        MovePick list(pos, root_tt_move);
        const Score alpha = -INF_CP;
        const Score beta  = INF_CP;

        int legal_moves = 0;
        Move* p;
        while ((p = list.next())) {
            Move move = *p;
            pos.make_move(move);

            if (pos.is_in_check(moving_color)) {
                pos.undo_move(move, gs);
                continue;
            }  
            Score score = -negamax(info, pos, depth - 1, -beta, -alpha);

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

        output->depth.store(depth, std::memory_order_relaxed);
        output->movetime.store(duration_cast<milliseconds>(steady_clock::now() - start).count(), std::memory_order_relaxed);
        output->nodes.store(info.nodes, std::memory_order_relaxed);
        output->score.store(prev_score, std::memory_order_relaxed);
        
    }

    output->move.store(prev_move, std::memory_order_relaxed);
}
} // namespace TuffFish
