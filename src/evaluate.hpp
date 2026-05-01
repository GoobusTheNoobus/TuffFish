#pragma once

#include "types.hpp"
#include "bitboards.hpp"
#include "position.hpp"

namespace TuffChess {
    // These are some very tuned precise accurate piece values:
    // Source: my ah
    constexpr Score PAWN_VALUE_MG   = 96;
    constexpr Score KNIGHT_VALUE_MG = 292;
    constexpr Score BISHOP_VALUE_MG = 326;
    constexpr Score ROOK_VALUE_MG   = 430;
    constexpr Score QUEEN_VALUE_MG  = 938;

    constexpr Score PAWN_VALUE_EG   = 125;
    constexpr Score KNIGHT_VALUE_EG = 275;
    constexpr Score BISHOP_VALUE_EG = 314;
    constexpr Score ROOK_VALUE_EG   = 534;
    constexpr Score QUEEN_VALUE_EG  = 969;

    constexpr Score MG_VALUES[PT_NB] = {PAWN_VALUE_MG, KNIGHT_VALUE_MG, BISHOP_VALUE_MG, ROOK_VALUE_MG, QUEEN_VALUE_MG, INF_CP};
    constexpr Score EG_VALUES[PT_NB] = {PAWN_VALUE_EG, KNIGHT_VALUE_EG, BISHOP_VALUE_EG, ROOK_VALUE_EG, QUEEN_VALUE_EG, INF_CP};

    namespace Evaluate {
        namespace {

        // Basic PSQTs
        // I will replace soon
        constexpr Score PSQT_MG[PT_NB][SQUARE_NB] = {
            {
                0,  0,  0,  0,  0,  0,  0,  0,
                50, 50, 50, 50, 50, 50, 50, 50,
                10, 10, 20, 30, 30, 20, 10, 10,
                5,  5, 10, 25, 25, 10,  5,  5,
                0,  0,  0, 20, 20,  0,  0,  0,
                5, -5,-10,  0,  0,-10, -5,  5,
                5, 10, 10,-20,-20, 10, 10,  5,
                0,  0,  0,  0,  0,  0,  0,  0,
            },
            {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50,
            },
            {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -20,-10,-10,-10,-10,-10,-10,-20,
            },
            {
                0,  0,  0,  0,  0,  0,  0,  0,
                5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                0,  0,  0,  5,  5,  0,  0,  0
            },
            {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            },
            {
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -20,-30,-30,-40,-40,-30,-30,-20,
                -10,-20,-20,-20,-20,-20,-20,-10,
                20, 20,  0,  0,  0,  0, 20, 20,
                20, 30, 10,  0,  0, 10, 30, 20
            },
        };

        constexpr Score PSQT_EG[PT_NB][SQUARE_NB] = {
            {
                0,  0,  0,  0,  0,  0,  0,  0,
                50, 50, 50, 50, 50, 50, 50, 50,
                10, 10, 20, 30, 30, 20, 10, 10,
                5,  5, 10, 25, 25, 10,  5,  5,
                0,  0,  0, 20, 20,  0,  0,  0,
                5, -5,-10,  0,  0,-10, -5,  5,
                5, 10, 10,-20,-20, 10, 10,  5,
                0,  0,  0,  0,  0,  0,  0,  0,
            },
            {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50,
            },
            {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -20,-10,-10,-10,-10,-10,-10,-20,
            },
            {
                0,  0,  0,  0,  0,  0,  0,  0,
                5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                0,  0,  0,  5,  5,  0,  0,  0
            },
            {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                -5,  0,  5,  5,  5,  5,  0, -5,
                0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            },
            {
                -50,-40,-30,-20,-20,-30,-40,-50,
                -30,-20,-10,  0,  0,-10,-20,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-30,  0,  0,  0,  0,-30,-30,
                -50,-30,-30,-30,-30,-30,-30,-50

            },
        };
        }

        inline Score PSQT_VALUE_MG[PT_NB][SQUARE_NB];
        inline Score PSQT_VALUE_EG[PT_NB][SQUARE_NB];

        inline void initialize() {
            for (PieceType pt = PAWN; pt < PT_NB; ++pt) {
                for (Square squ = A1; squ < SQUARE_NB; ++squ) {
                    PSQT_VALUE_MG[pt][squ] = MG_VALUES[pt] + PSQT_MG[pt][squ];
                    PSQT_VALUE_EG[pt][squ] = EG_VALUES[pt] + PSQT_MG[pt][squ];
                }
            }
        }

        // Pointer because the argument is passed as 'this'
        inline int game_phase(const Position* pos) {
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
    }
}