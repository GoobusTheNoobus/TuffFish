#include "evaluate.hpp"
#include "position.hpp"

namespace TuffFish {
namespace Evaluate {


namespace {

// =========================== Mobility ===========================

constexpr int MOBILITY_NORMALIZATION = 2;

constexpr Score KNIGHT_MOBILITY[] = {
//    0    1    2    3    4    5    6    7    8
    -25, -15, -10,  -5,  10,  15,  20,  25,  35  
};

constexpr Score BISHOP_MOBILITY[] = {
//    0    1    2    3    4    5    6    7    8    9   10   11   12   13
    -30, -20, -10,   0,   5,  10,  15,  20,  25,  30,  35,  40,  45,  50
};

constexpr Score ROOK_MOBILITY[] = {
//    0    1    2    3    4    5    6    7    8    9   10   11   12   13   14
    -30, -30, -30, -15, -10,  -5,   0,   5,  10,  15,  20,  25,  25,  25,  25
};

void compute_mobility(const Position* pos, ScorePair& sp) {
    Score score = 0;
    
    Bitboard white_knights = pos->bitboard(W_KNIGHT);
    Bitboard black_knights = pos->bitboard(B_KNIGHT);
    Bitboard white_bishops = pos->bitboard(W_BISHOP);
    Bitboard black_bishops = pos->bitboard(B_BISHOP);
    Bitboard white_rooks   = pos->bitboard(W_ROOK);
    Bitboard black_rooks   = pos->bitboard(B_ROOK);

    while(white_knights) {
        Square squ = Square(pop_lsb(white_knights));
        score += KNIGHT_MOBILITY[cnt(Bitboards::knight_attack(squ) & ~pos->bitboard())];
    }
    while(black_knights) {
        Square squ = Square(pop_lsb(black_knights));
        score -= KNIGHT_MOBILITY[cnt(Bitboards::knight_attack(squ) & ~pos->bitboard())];
    }
    while(white_bishops) {
        Square squ = Square(pop_lsb(white_bishops));
        score += BISHOP_MOBILITY[cnt(Bitboards::bishop_attack(squ, pos->bitboard()))];
    }
    while(black_bishops) {
        Square squ = Square(pop_lsb(black_bishops));
        score -= BISHOP_MOBILITY[cnt(Bitboards::bishop_attack(squ, pos->bitboard()))];
    }
    while(white_rooks) {
        Square squ = Square(pop_lsb(white_rooks));
        score += ROOK_MOBILITY[cnt(Bitboards::rook_attack(squ, pos->bitboard()))];
    }
    while(black_rooks) {
        Square squ = Square(pop_lsb(black_rooks));
        score -= ROOK_MOBILITY[cnt(Bitboards::rook_attack(squ, pos->bitboard()))];
    }

    score /= MOBILITY_NORMALIZATION;

    sp.mg += score;
    sp.eg += score;
}

// =========================== Pawn Structures ===========================

constexpr int PASSED_PAWN_NORMALIZATION = 3;

constexpr Score PASSED_PAWN_BONUS_MG[RANK_NB] = {
//  1    2    3    4    5    6    7
    0,   5,   8,  12,  25,  40,  60
};

constexpr Score PASSED_PAWN_BONUS_EG[RANK_NB] = {
//  1    2    3    4    5    6    7
    0,   5,  15,  30,  45,  60, 100
};

void compute_passed_pawns(const Position* pos, ScorePair& sp) {
    Bitboard white_pawns = pos->bitboard(W_PAWN);
    Bitboard black_pawns = pos->bitboard(B_PAWN);

    while (white_pawns) {
        int lsb = pop_lsb(white_pawns);
        Bitboard required = Bitboards::PASSED_PAWN[WHITE][lsb];
        if (!(required & pos->bitboard(B_PAWN))) {
            sp.mg += PASSED_PAWN_BONUS_MG[rank_of(Square(lsb))] / PASSED_PAWN_NORMALIZATION;
            sp.eg += PASSED_PAWN_BONUS_EG[rank_of(Square(lsb))] / PASSED_PAWN_NORMALIZATION;
        }
    }

    while (black_pawns) {
        int lsb = pop_lsb(black_pawns);
        Bitboard required = Bitboards::PASSED_PAWN[BLACK][lsb];
        if (!(required & pos->bitboard(W_PAWN))) {
            sp.mg -= PASSED_PAWN_BONUS_MG[7 - rank_of(Square(lsb))] / PASSED_PAWN_NORMALIZATION ;
            sp.eg -= PASSED_PAWN_BONUS_EG[7 - rank_of(Square(lsb))] / PASSED_PAWN_NORMALIZATION;
        }
    }
}

} // namespace anonymous

// Pointer because the argument is passed as 'this'
int game_phase(const Position* pos) {
    static constexpr const int phase_inc[PIECE_NB] = {0, 1, 1, 2, 4, 0, 0, 1, 1, 2, 4, 0};

    int phase = 0;
    phase += phase_inc[W_KNIGHT] * cnt(pos->bitboard(W_KNIGHT));
    phase += phase_inc[B_KNIGHT] * cnt(pos->bitboard(B_KNIGHT));
    phase += phase_inc[W_BISHOP] * cnt(pos->bitboard(W_BISHOP));
    phase += phase_inc[B_BISHOP] * cnt(pos->bitboard(B_BISHOP));
    phase += phase_inc[W_ROOK]   * cnt(pos->bitboard(W_ROOK));
    phase += phase_inc[B_ROOK]   * cnt(pos->bitboard(B_ROOK));
    phase += phase_inc[W_QUEEN]  * cnt(pos->bitboard(W_QUEEN));
    phase += phase_inc[B_QUEEN]  * cnt(pos->bitboard(B_QUEEN));

    return phase;
}

// Includes other factors of evaluating a position statically
Score positional(const Position* pos, int phase) {
    ScorePair sp;

    // compute_mobility(pos, sp);

    // HASN'T PASSED SPRT
    // compute_passed_pawns(pos, sp);

    return sp.compute(phase);
}
}
}