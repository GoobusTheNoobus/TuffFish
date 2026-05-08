#pragma once

#include "types.hpp"

namespace TuffFish {

// Zobrist Hashing
namespace Zobrist {
    void initialize();

    HashKey ps_key(Piece p, Square squ);
    HashKey side_key();
    HashKey castling_key(CastlingRight r);
    HashKey ep_key(Square squ);

} // namespace Zobrist


inline constexpr const char* const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";  


class Position {
    public:
    // Constructors

    // Default constructor: doesn't do anything
    // Must be set up before use, but after calling Zobrist::initialize();
    Position() = default;

    // fen constructor: constructs the position based 
    // on a Forsyth Edward Notation string
    Position(const std::string& fen);

    inline void set_up_startpos() { parse_fen(START_FEN); } 

    // Lookups
    inline Piece    piece_on(Square square) const { 
        return board[square]; 
    }
    inline Bitboard bitboard(Piece piece) const { return piece_bb[piece]; }
    inline Bitboard bitboard(Color color) const { return color_bb[color]; }
    inline Bitboard bitboard() const { return occupancy; }

    inline Color get_side() const { return side_to_move; }
    inline bool  white_to_move() const { return side_to_move == WHITE; }

    inline CastlingRight get_castling() const { return castling; }
    inline Square get_ep_square() const { return en_passant_square; }
    inline int get_rule_50() const { return rule_50; }

    HashKey zobrist() { return hash; }

    Score evaluate() const;

    void generate_moves(MoveList& moves) const;

    // Helpers
    void parse_fen(const std::string& fen);

    bool is_attacked(Square square, Color by) const;
    bool is_in_check(Color c) const;

    void make_move(Move move);
    void make_move(const std::string& str);
    void undo_move(Move move, const StoredGameState& gs);

    bool is_repetition() const;

    private:
    // Data members
    std::array<Piece, SQUARE_NB>   board;
    std::array<Bitboard, PIECE_NB> piece_bb;
    std::array<Bitboard, COLOR_NB> color_bb;
    Bitboard occupancy;

    Color         side_to_move;
    CastlingRight castling;
    Square        en_passant_square;
    int           rule_50 = 0;

    Score mg_psqt_score = 0;
    Score eg_psqt_score = 0;

    std::array<Piece, 1000> capture_stack;
    std::array<HashKey, 1000> hash_stack;
    int ply = 0;

    HashKey hash = 0;

    // Helper functions
    void clear();
    void clear(Square square);
    void place(Square square, Piece piece);

    

    template <PieceType pt>
    void generate_piece_moves(MoveList& list) const;

};    

std::ostream& operator<<(std::ostream& os, const Position& pos);

struct StoredGameState {
    CastlingRight prev_castling;
    Square prev_ep;
    int prev_r50;

    inline StoredGameState(const Position& pos) {
        prev_castling = pos.get_castling();
        prev_ep = pos.get_ep_square();
        prev_r50 = pos.get_rule_50();
    }
};

} // namespace TuffChess

