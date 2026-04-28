#include "position.hpp"

#include "bitboards.hpp"

#include <sstream>

namespace TuffChess {

inline constexpr const std::string_view piece_chars("PNBRQKpnbrqk.");

// Constructors
Position::Position() { parse_fen(START_FEN); }
Position::Position(const std::string& fen) { parse_fen(fen); }

// Operator overload used automatically when printing
// Usage: std::cout << Position();
std::ostream& operator<<(std::ostream& os, const Position& pos) {
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        os << "  +---+---+---+---+---+---+---+---+\n";
        os << (r + 1) << " ";
        for (File f = FILE_A; f <= FILE_H; ++f) {
            os << "| " << piece_chars[pos.piece_on(parse_square(r, f))] << " ";
        }
        os << "|\n";
        
    }
    os << "  +---+---+---+---+---+---+---+---+\n";
    os << "    A   B   C   D   E   F   G   H\n\n";
    os << "   Evaluation: +6.7 Haha SIX SEVENNNN\n\n";

    return os;
}

void Position::parse_fen(const std::string& fen) {
    std::istringstream iss(fen);
    std::string token;

    clear();

    // Parse pieces
    if (!(iss >> token)) return;

    int r = 7, f = 0;
    for (char c: token) {
        if (c == '/') {
            r -= 1;
            f = 0;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            f += c - '0';
        } else {
            size_t piece_id = piece_chars.find(c);

            if (piece_id == std::string_view::npos) {
                return;
            }

            Piece p = Piece(piece_id);
            place(parse_square(Rank(r), File(f)), p);
            ++f;
        }
    }


}

void Position::clear() {
    std::fill(board.begin(), board.end(), NO_PIECE);
    std::fill(piece_bb.begin(), piece_bb.end(), 0);
    std::fill(color_bb.begin(), color_bb.end(), 0);
    occupancy = 0;

    side_to_move = WHITE;
    castling = NO_CASTLING;
    en_passant_square = NO_SQUARE;
    rule_50 = 0;
}

void Position::clear(Square square) {
    if (board[square] == NO_PIECE) return;

    // Piece there already
    Piece piece = board[square];
    Bitboard mask = ~(1ULL << square);

    board[square] = NO_PIECE;
    piece_bb[piece] &= mask;
    color_bb[color_of(piece)] &= mask;
    occupancy &= mask;

}

void Position::place(Square square, Piece piece) {
    if (piece == NO_PIECE) {
        clear(square);
        return;
    }

    Bitboard mask = 1ULL << square;

    board[square] = piece;
    piece_bb[piece] |= mask;
    color_bb[color_of(piece)] |= mask;
    occupancy |= mask;
}

void Position::generate_moves(MoveList& moves) {

    Color us = side_to_move;
    Color them = !us;
    bool  is_white = us == WHITE;

    Bitboard our_pieces = color_bb[us];
    Bitboard their_pieces = color_bb[them];

    // ============= Pawns ==============
    Bitboard pawns = piece_bb[make_piece(PAWN, us)];

    Direction pawn_push_dir          = is_white ? NORTH: SOUTH;
    Direction pawn_left_capture_dir  = is_white ? NORTH_WEST: SOUTH_WEST;
    Direction pawn_right_capture_dir = is_white ? NORTH_EAST: SOUTH_EAST;

    Bitboard rank3 = Bitboards::RANK_BB[is_white ? RANK_3: RANK_6];
    Bitboard rank8 = Bitboards::RANK_BB[is_white ? RANK_8: RANK_1];

    Bitboard single_push = is_white ? pawns << 8 : pawns >> 8;
    single_push &= ~occupancy;
    Bitboard double_push = single_push & rank3;
    double_push = is_white ? double_push << 8 : double_push >> 8;
    double_push &= ~occupancy;
    Bitboard left_capture  = is_white ? pawns << 7 : pawns >> 9;
    Bitboard right_capture = is_white ? pawns << 9 : pawns >> 7;
    left_capture  &= their_pieces;
    right_capture &= their_pieces;
    // Prevent wrap-arounds
    left_capture  &= ~Bitboards::FILE_BB[FILE_H];
    right_capture &= ~Bitboards::FILE_BB[FILE_A];

    // Split into promotion or not
    Bitboard single_push_normal   = single_push & ~rank8;
    Bitboard single_push_promo    = single_push & rank8;
    Bitboard left_capture_normal  = left_capture & ~rank8;
    Bitboard left_capture_promo   = left_capture & rank8;
    Bitboard right_capture_normal = right_capture & ~rank8;
    Bitboard right_capture_promo  = right_capture & rank8;

    // Extract into movelist via while loop

    auto extract = [&](Bitboard& bb, Direction dir, MoveFlag flag){
        Square squ = Square(pop_lsb(bb));
        moves.add(create_move(squ - dir, squ, flag));
    };

    auto extract_promo = [&](Bitboard& bb, Direction dir) {
        Square squ = Square(pop_lsb(bb));
        Move base = create_move(squ - dir, squ, NORMAL);
        moves.add(apply_flag(base, NPROMO));
        moves.add(apply_flag(base, BPROMO));
        moves.add(apply_flag(base, RPROMO));
        moves.add(apply_flag(base, QPROMO));
    };

    while (single_push_normal) { extract(single_push_normal, pawn_push_dir, NORMAL); }
    while (double_push) { extract(double_push, pawn_push_dir * 2, DOUBLE_PUSH); }
    while (left_capture_normal) { extract(left_capture_normal, pawn_left_capture_dir, NORMAL); }
    while (right_capture_normal) { extract(right_capture_normal, pawn_right_capture_dir, NORMAL); }

    while (single_push_promo) { extract_promo(single_push_promo, pawn_push_dir); }
    while (left_capture_promo) { extract_promo(left_capture_promo, pawn_left_capture_dir); }
    while (right_capture_promo) { extract_promo(right_capture_promo, pawn_right_capture_dir); }

    // En passant
    if (en_passant_square != NO_SQUARE) {
        Bitboard ep_from_bb = Bitboards::pawn_attack(en_passant_square, them) & pawns;
        while (ep_from_bb) {
            Square squ = Square(pop_lsb(ep_from_bb));
            moves.add(create_move(squ, en_passant_square, EN_PASSANT));
        }
    }

    // ============= Other Pieces ==============

    generate_piece_moves<KNIGHT>(moves);
    generate_piece_moves<BISHOP>(moves);
    generate_piece_moves<ROOK>(moves);
    generate_piece_moves<QUEEN>(moves);
    generate_piece_moves<KING>(moves);
    

    // Castling logic

    
    if (us == WHITE) {
        // King in check
        if (is_attacked(E1, BLACK)) return;

        if (castling & WHITE_KS && piece_on(G1) == NO_PIECE && piece_on(F1) == NO_PIECE && 
            !is_attacked(F1, BLACK) && !is_attacked(G1, BLACK)) {
            moves.add(create_move(E1, G1, CASTLING));
        } 
        
        if (castling & WHITE_QS && piece_on(D1) == NO_PIECE && piece_on(C1) == NO_PIECE && piece_on(B1) == NO_PIECE &&
            !is_attacked(D1, BLACK) && !is_attacked(C1, BLACK)) {
            moves.add(create_move(E1, C1, CASTLING));
        }
    } else {
        // King in check
        if (is_attacked(E8, WHITE)) return;

        if (castling & BLACK_KS && piece_on(G8) == NO_PIECE && piece_on(F8) == NO_PIECE &&
        !is_attacked(F8, WHITE) && !is_attacked(G8, WHITE)) {
            moves.add(create_move(E8, G8, CASTLING));
        } 
        
        if (castling & BLACK_QS && piece_on(D8) == NO_PIECE && piece_on(C8) == NO_PIECE && piece_on(B8) == NO_PIECE && 
        !is_attacked(D8, WHITE) && !is_attacked(C8, WHITE)) {
            moves.add(create_move(E8, C8, CASTLING));
        }
    }
}

template <PieceType pt>
void Position::generate_piece_moves(MoveList& list) {
    Color us = side_to_move;
    Bitboard our_pieces = color_bb[us];

    Bitboard pieces = piece_bb[make_piece(pt, us)];

    auto extract_pieces = [&](Bitboard& bb, Square from) {
        Square squ = Square(pop_lsb(bb));
        list.add(create_move(from, squ, NORMAL));
    };

    while (pieces) {
        Square square = Square(pop_lsb(pieces));

        Bitboard attacks = 0;

        if constexpr (pt == KNIGHT) {
            attacks = Bitboards::knight_attack(square);
        } else if constexpr (pt == BISHOP) {
            attacks = Bitboards::bishop_attack(square, occupancy);
        } else if constexpr (pt == ROOK) {
            attacks = Bitboards::rook_attack(square, occupancy);
        } else if constexpr (pt == QUEEN) {
            attacks = Bitboards::queen_attack(square, occupancy);
        } else if constexpr (pt == KING) {
            attacks = Bitboards::king_attack(square);
        }

        attacks &= ~our_pieces;

        while (attacks) { extract_pieces(attacks, square); }
    }
}

bool Position::is_attacked(Square square, Color by) {

    Bitboard pawns = piece_bb[make_piece(PAWN, by)];
    if (pawns & Bitboards::pawn_attack(square, !by)) return true;

    Bitboard knights = piece_bb[make_piece(KNIGHT, by)];
    if (knights & Bitboards::knight_attack(square)) return true;
    
    Bitboard bishops = piece_bb[make_piece(BISHOP, by)];
    if (bishops & Bitboards::bishop_attack(square, occupancy)) return true;

    Bitboard rooks = piece_bb[make_piece(ROOK, by)];
    if (rooks & Bitboards::rook_attack(square, occupancy)) return true;

    Bitboard queens = piece_bb[make_piece(QUEEN, by)];
    if (queens & Bitboards::queen_attack(square, occupancy)) return true;

    Bitboard kings = piece_bb[make_piece(KING, by)];
    if (kings & Bitboards::king_attack(square)) return true;
    
}

bool Position::is_in_check(Color c) {
    return is_attacked(Square(ctz(piece_bb[make_piece(KING, c)])), !c);
}
    
} // namespace TuffChess
