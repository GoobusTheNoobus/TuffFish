#include "position.hpp"
#include "bitboards.hpp"
#include "evaluate.hpp"

#include <sstream>

namespace TuffChess {

inline constexpr const std::string_view piece_chars("PNBRQKpnbrqk ");

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

    // Parse side
    if (!(iss >> token)) return;

    if (token == "b") side_to_move = BLACK;

    // Castling Rights
    if (!(iss >> token)) return;

    for (char c: token) {
        switch (c) {
            case 'K':
                castling = CastlingRight(castling | WHITE_KS);
                break;
            case 'Q':
                castling = CastlingRight(castling | WHITE_QS);
                break;
            case 'k':
                castling = CastlingRight(castling | BLACK_KS);
                break;
            case 'q':
                castling = CastlingRight(castling | BLACK_QS);
                break;
            default:
                return;
        }
    }

    // En Passant
    if (!(iss >> token)) return;

    for (Square squ = A1; squ < SQUARE_NB; ++squ) {
        if (token == square_str[squ]) {
            en_passant_square = squ;
            break;
        }
    }

    // Rule 50
    if (!(iss >> token)) return;

    rule_50 = std::stoi(token);
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
    PieceType pt = type_of(piece);
    Color color = color_of(piece);
    Bitboard mask = ~(1ULL << square);

    board[square] = NO_PIECE;
    piece_bb[piece] &= mask;
    color_bb[color] &= mask;
    occupancy &= mask;

    mg_psqt_score = color == WHITE ? 
                            (mg_psqt_score - Evaluate::PSQT_VALUE_MG[pt][square ^ 56]): 
                            (mg_psqt_score + Evaluate::PSQT_VALUE_MG[pt][square]);
    
    eg_psqt_score = color == WHITE ? 
                            (eg_psqt_score - Evaluate::PSQT_VALUE_EG[pt][square ^ 56]): 
                            (eg_psqt_score + Evaluate::PSQT_VALUE_EG[pt][square]);

}

void Position::place(Square square, Piece piece) {
    if (piece == NO_PIECE) {
        clear(square);
        return;
    }

    Bitboard mask = 1ULL << square;
    PieceType pt = type_of(piece);
    Color color = color_of(piece);

    board[square] = piece;
    piece_bb[piece] |= mask;
    color_bb[color_of(piece)] |= mask;
    occupancy |= mask;

    mg_psqt_score = color == WHITE ? 
                            (mg_psqt_score + Evaluate::PSQT_VALUE_MG[pt][square ^ 56]): 
                            (mg_psqt_score - Evaluate::PSQT_VALUE_MG[pt][square]);
    
    eg_psqt_score = color == WHITE ? 
                            (eg_psqt_score + Evaluate::PSQT_VALUE_EG[pt][square ^ 56]): 
                            (eg_psqt_score - Evaluate::PSQT_VALUE_EG[pt][square]);
}

Score Position::psqt_eval() const {
    int phase = Evaluate::game_phase(this);
    int mg_phase = phase;
    int eg_phase = 24 - phase;

    Score score = (mg_psqt_score * mg_phase + eg_psqt_score * eg_phase) / 24;

    // Normalize score if eg_phase is low
    double weight = std::min<double>(std::max<double>(-phase / 60.0 + 1.1, 0.7), 1.2);
    score = int(weight * score);

    return side_to_move == WHITE ? score: -score;
}

namespace {
template <MoveFlag flag>
inline void extract(Bitboard& bb, MoveList& moves, Direction dir) {
    Square squ = Square(pop_lsb(bb));
    moves.add(create_move(squ - dir, squ, flag));
}
}

void Position::generate_moves(MoveList& moves) const {

    Color us = side_to_move;
    Color them = !us;
    bool  is_white = us == WHITE;

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

    auto extract_promo = [&](Move base) {

        moves.add(apply_flag(base, NPROMO));
        moves.add(apply_flag(base, BPROMO));
        moves.add(apply_flag(base, RPROMO));
        moves.add(apply_flag(base, QPROMO));
    };

    while (single_push_normal)
        extract<NORMAL>(single_push_normal, moves, pawn_push_dir);
    while (double_push) 
        extract<DOUBLE_PUSH>(double_push, moves, pawn_push_dir * 2); 

    while (left_capture_normal)
        extract<NORMAL>(left_capture_normal, moves, pawn_left_capture_dir);

    while (right_capture_normal)
        extract<NORMAL>(right_capture_normal, moves, pawn_right_capture_dir);

    while (single_push_promo) { 
        Square squ = Square(pop_lsb(single_push_promo));
        Move base = create_move(squ - pawn_push_dir, squ, NORMAL);

        extract_promo(base);
    }
    while (left_capture_promo) { 
        Square squ = Square(pop_lsb(left_capture_promo));
        Move base = create_move(squ - pawn_left_capture_dir, squ, NORMAL);

        extract_promo(base);
    }
    while (right_capture_promo) {
        Square squ = Square(pop_lsb(right_capture_promo));
        Move base = create_move(squ - pawn_right_capture_dir, squ, NORMAL);

        extract_promo(base);
    }

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

    moves.prev_castling = castling;
    moves.prev_ep       = en_passant_square;
    moves.prev_r50      = rule_50;

}

template <PieceType pt>
void Position::generate_piece_moves(MoveList& list) const {
    Color us = side_to_move;
    Bitboard our_pieces = color_bb[us];

    Bitboard pieces = piece_bb[make_piece(pt, us)];

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

        while (attacks) { 
            Square squ = Square(pop_lsb(attacks));
            list.add(create_move(square, squ, NORMAL));
        }
    }
}

bool Position::is_attacked(Square square, Color by) const {

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
    
    return false;
}

bool Position::is_in_check(Color c) const {
    return is_attacked(Square(ctz(piece_bb[make_piece(KING, c)])), !c);
}
    
void Position::make_move(Move move) {
    bool is_white = side_to_move == WHITE;

    Square from_squ = from(move);
    Square dest_squ = dest(move);
    MoveFlag flag_  = flag(move);

    Piece moving = board[from_squ];
    Piece captured = flag_ == EN_PASSANT ? make_piece(PAWN, !side_to_move): board[dest_squ];

    capture_stack[ply++] = captured;

    en_passant_square = NO_SQUARE;

    clear(from_squ);

    switch (flag_) {
        case CASTLING: {
            bool is_kingside = file_of(dest_squ) == FILE_G;

            Square rook_from = is_white ? (is_kingside ? H1: A1): (is_kingside ? H8: A8);
            Square rook_dest = is_white ? (is_kingside ? F1: D1): (is_kingside ? F8: D8);

            // Move king
            place(dest_squ, moving);

            // Move rook
            clear(rook_from);
            place(rook_dest, make_piece(ROOK, side_to_move));

            break;
        }
        case EN_PASSANT: {
            Square capture_squ = is_white ? (dest_squ + SOUTH): (dest_squ + NORTH);

            place(dest_squ, moving);
            clear(capture_squ);

            break;
        }
        case DOUBLE_PUSH: {
            place(dest_squ, moving);

            en_passant_square = is_white ? (dest_squ + SOUTH): (dest_squ + NORTH);

            break;
        }
        case NPROMO: 
        case BPROMO:
        case RPROMO:
        case QPROMO: {

            static constexpr const PieceType pt[] = {KNIGHT, BISHOP, ROOK, QUEEN};

            clear(dest_squ);
            place(dest_squ, make_piece(pt[flag_ - NPROMO], side_to_move));

            break;
        }
        default: {
            clear(dest_squ);
            place(dest_squ, moving);

            break;
        }
    }

    // Remove castling rights if moved to rook starting square
    if (from_squ == A1 || dest_squ == A1) {
        castling = CastlingRight(castling & ~WHITE_QS);
    } else if (from_squ == H1 || dest_squ == H1) {
        castling = CastlingRight(castling & ~WHITE_KS);
    } else if (from_squ == A8 || dest_squ == A8) {
        castling = CastlingRight(castling & ~BLACK_QS);
    } else if (from_squ == H8 || dest_squ == H8) {
        castling = CastlingRight(castling & ~BLACK_KS);
    }

    // Remove castling rights if king moved
    if (moving == W_KING) {
        castling = CastlingRight(castling & ~WHITE_CASTLING);
    } else if (moving == B_KING) {
        castling = CastlingRight(castling & ~BLACK_CASTLING);
    }

    if (captured == NO_PIECE && type_of(moving) != PAWN) {
        rule_50++;
    }  else {
        rule_50 = 0;
    }

    side_to_move = !side_to_move;

}

void Position::make_move(const std::string& str) {
    MoveList list;
    generate_moves(list);

    for (Move m: list) {
        if (algebraic(m) == str) {
            make_move(m);
            return;
        }
    }
}

// list is needed to reset game states
void Position::undo_move(Move move, const MoveList& list) {
    side_to_move = !side_to_move;

    bool is_white = side_to_move == WHITE;

    Square from_squ = from(move);
    Square dest_squ = dest(move);
    MoveFlag flag_  = flag(move);

    Piece moving = board[dest_squ];
    Piece captured = capture_stack[--ply];

    // Restore previous states
    castling = list.prev_castling;
    en_passant_square = list.prev_ep;
    rule_50 = list.prev_r50;

    clear(dest_squ);

    switch (flag_) {
        case CASTLING: {
            bool is_kingside = file_of(dest_squ) == FILE_G;

            Square rook_from = is_white ? (is_kingside ? H1: A1): (is_kingside ? H8: A8);
            Square rook_dest = is_white ? (is_kingside ? F1: D1): (is_kingside ? F8: D8);

            // Move king
            place(from_squ, moving);
            
            // Move rook
            clear(rook_dest);
            place(rook_from, make_piece(ROOK, side_to_move));

            break;
        }

        case EN_PASSANT: {
            Square capture_squ = is_white ? (dest_squ + SOUTH): (dest_squ + NORTH);

            place(from_squ, moving);
            place(capture_squ, make_piece(PAWN, !side_to_move));

            break;
        }

        case DOUBLE_PUSH: {
            place(from_squ, moving);

            break;
        }

        case NPROMO: 
        case BPROMO:
        case RPROMO:
        case QPROMO: {
            // Restore capture
            place(dest_squ, captured);
            place(from_squ, make_piece(PAWN, side_to_move));

            break;
        }

        default: {
            // Restore capture
            place(dest_squ, captured);
            place(from_squ, moving);

            break;
        }
    }

}

} // namespace TuffChess
