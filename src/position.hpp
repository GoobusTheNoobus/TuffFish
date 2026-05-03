#pragma once

#include "types.hpp"

namespace TuffFish {

struct StoredGameState;

inline constexpr const char* const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";  


class Position {
    public:
    // Constructors

    // Default constructor: sets up starting position
    Position();

    // fen constructor: constructs the position based 
    // on a Forsyth Edward Notation string
    Position(const std::string& fen);

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
    inline uint8_t get_rule_50() const { return rule_50; }

    Score evaluate() const;

    void generate_moves(MoveList& moves) const;

    // Helpers
    void parse_fen(const std::string& fen);

    bool is_attacked(Square square, Color by) const;
    bool is_in_check(Color c) const;

    void make_move(Move move);
    void make_move(const std::string& str);
    void undo_move(Move move, const StoredGameState& gs);

    

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
    int ply = 0;

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
    uint8_t prev_r50;

    inline StoredGameState(const Position& pos) {
        prev_castling = pos.get_castling();
        prev_ep = pos.get_ep_square();
        prev_r50 = pos.get_rule_50();
    }
};

} // namespace TuffChess

