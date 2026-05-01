#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <sstream>
#include <iostream>

namespace TuffChess {
using Bitboard = uint64_t;


enum Square: uint8_t {
A1, B1, C1, D1, E1, F1, G1, H1,
A2, B2, C2, D2, E2, F2, G2, H2,
A3, B3, C3, D3, E3, F3, G3, H3,
A4, B4, C4, D4, E4, F4, G4, H4,
A5, B5, C5, D5, E5, F5, G5, H5,
A6, B6, C6, D6, E6, F6, G6, H6,
A7, B7, C7, D7, E7, F7, G7, H7,
A8, B8, C8, D8, E8, F8, G8, H8,
NO_SQUARE,

SQUARE_NB = 64
};

constexpr const char* square_str[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
};

enum File: int8_t { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };
enum Rank: int8_t { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

inline Square parse_square(Rank r, File f) { return Square(r << 3 | f); }
inline File   file_of(Square s) { return File(s & 7); }
inline Rank   rank_of(Square s) { return Rank(s >> 3); }

inline Square operator++(Square& s) { return s = Square(s + 1); }
inline Square operator++(Square& s, int) { Square old = s; ++s; return old; }
inline File   operator++(File& f) { return f = File(f + 1); }
inline File   operator++(File& f, int) { File old = f; ++f; return old; }
inline Rank   operator++(Rank& r) { return r = Rank(r + 1); }
inline Rank   operator++(Rank& r, int) { Rank old = r; ++r; return old; }
inline Rank   operator--(Rank& r) { return r = Rank(r - 1); }
inline Rank   operator--(Rank& r, int) { Rank old = r; --r; return old; }

enum Piece: uint8_t {
W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING, 
B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING, 
NO_PIECE,
PIECE_NB = 12
};

enum PieceType: uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PT_NB};
enum Color: uint8_t { WHITE, BLACK, COLOR_NB };

inline Piece     make_piece(PieceType pt, Color c) { return Piece(pt + c * 6); }
inline PieceType type_of(Piece p) { return PieceType(p % 6); }
inline Color     color_of(Piece p) { return Color(p / 6); }

inline Color operator!(Color& c) { return c == WHITE ? BLACK: WHITE; }
inline PieceType operator++(PieceType& pt) { return pt = PieceType(pt + 1); }
inline PieceType operator++(PieceType& pt, int) { PieceType old = pt; ++pt; return old; }

enum CastlingRight: uint8_t {
    NO_CASTLING = 0,

    WHITE_KS = 1,
    WHITE_QS = 2,
    BLACK_KS = 4,
    BLACK_QS = 8,

    WHITE_CASTLING = WHITE_KS | WHITE_QS,
    BLACK_CASTLING = BLACK_KS | BLACK_QS
};

enum Direction: int8_t {
    NORTH = 8,
    SOUTH = -8,
    WEST  = -1,
    EAST  = 1,

    NORTH_WEST = NORTH + WEST,
    NORTH_EAST = NORTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    SOUTH_EAST = SOUTH + EAST
};

inline Square operator+(const Square& square, const Direction& dir) { return Square(square + int(dir)); }
inline Square operator-(const Square& square, const Direction& dir) { return Square(square - int(dir)); }
inline Direction operator*(const Direction& dir, const int n) { return Direction(int(dir) * n); }

// Move representation & storage
using Move = uint16_t;

enum MoveFlag: uint8_t {
    NORMAL,
    CASTLING,
    EN_PASSANT,
    DOUBLE_PUSH,
    NPROMO,
    BPROMO,
    RPROMO,
    QPROMO,
};

inline constexpr const char* const promo_chars = "nbrq";

// Move helpers
// Encoding: 
// 0-5   From
// 6-11  Dest
// 12-15 Flag
inline Square   from(Move move) { return Square(move & 0x3F); }
inline Square   dest(Move move) { return Square((move >> 6) & 0x3F); }
inline MoveFlag flag(Move move) { return MoveFlag((move >> 12) & 0x0F); }
inline std::string algebraic(Move move) {
    std::string from_str = square_str[from(move)];
    std::string dest_str = square_str[dest(move)];

    MoveFlag f = flag(move);

    if (f >= NPROMO) 
        return from_str + dest_str + promo_chars[f - NPROMO];

    return from_str + dest_str;
}

inline Move create_move(Square from, Square dest, MoveFlag flag) { return Move(uint16_t(from) | (uint16_t(dest) << 6) | (uint16_t(flag) << 12)); }
inline Move apply_flag(Move move, MoveFlag flag) { return move | (flag << 12); }

struct MoveList {
    std::array<Move, 256> data;
    int count = 0;

    CastlingRight prev_castling;
    Square prev_ep;
    uint8_t prev_r50;

    inline Move  operator[](int i) { return data[i]; }
    inline int   size() { return count; }
    inline Move* begin() { return data.data(); }
    inline Move* end()  { return data.data() + count; }

    inline void add(Move move) { data[count++] = move; }

};

inline std::ostream& operator<<(std::ostream& os, MoveList& moves) {
    for (Move move: moves) {
        os << algebraic(move) << "\n";
    }

    os << "Castling: " << int(moves.prev_castling) << std::endl;
    os << "EP: " << int(moves.prev_ep) << std::endl;
    os << "R50" << moves.prev_r50 << std::endl;
    return os;

} 

constexpr const int MAX_PLY = 100;

using Score = int;

constexpr Score MATE_CP    = 10000;
constexpr Score MAX_CP     = MATE_CP - MAX_PLY;
constexpr Score DRAW_CP    = 0;
constexpr Score INF_CP     = 20000;
constexpr Score TIMEOUT_CP = 30000;

} // namespace TuffChess
