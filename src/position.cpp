#include "position.hpp"
#include "bitboards.hpp"
#include "evaluate.hpp"

#include <sstream>
#include <random>

namespace TuffFish {

// =========================== Zobrist Hashing Impl ===========================

namespace Zobrist {
    namespace {
    HashKey PIECE_SQUARE_KEYS[PIECE_NB][SQUARE_NB];
    HashKey SIDE_MOVING_KEY;
    HashKey CASTLING_KEYS[CASTLING_RIGHTS_NB];
    HashKey EN_PASSANT_KEYS[FILE_NB];
    }

    void initialize() {
        std::mt19937_64 rng(0x9e3779b97f4a7c15ULL);
        auto r64 = [&]() { return rng(); };

        for (int p = 0; p < PIECE_NB; ++p)
            for (int s = 0; s < SQUARE_NB; ++s)
                PIECE_SQUARE_KEYS[p][s] = r64();

        SIDE_MOVING_KEY = r64();

        for (int c = 0; c < CASTLING_RIGHTS_NB; ++c) CASTLING_KEYS[c] = r64();
        for (int f = 0; f < FILE_NB; ++f) EN_PASSANT_KEYS[f] = r64();
    }

    HashKey ps_key(Piece p, Square squ) { return PIECE_SQUARE_KEYS[p][squ]; }
    HashKey side_key() { return SIDE_MOVING_KEY; }
    HashKey castling_key(CastlingRight r) { return CASTLING_KEYS[r]; }
    HashKey ep_key(Square squ) { return squ != NO_SQUARE ? EN_PASSANT_KEYS[file_of(squ)]: 0; }

    
    
} // namespace Zobrist


inline constexpr const std::string_view piece_chars("PNBRQKpnbrqk ");

// =========================== Constructors ===========================


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

// =========================== Editting Functions ===========================

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

    if (token == "b") {
        hash ^= Zobrist::side_key();
        side_to_move = BLACK;
    }

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
            case '-':
                break;
            default:
                return;
        }
    }

    hash ^= Zobrist::castling_key(castling);

    // En Passant
    if (!(iss >> token)) return;

    for (Square squ = A1; squ < SQUARE_NB; ++squ) {
        if (token == square_str[squ]) {
            en_passant_square = squ;
            hash ^= Zobrist::ep_key(squ);
            break;
        }
    }

    // Rule 50
    if (!(iss >> token)) return;

    rule_50 = std::stoi(token);
}

// Clears the entire position
void Position::clear() {
    std::fill(board.begin(), board.end(), NO_PIECE);
    std::fill(piece_bb.begin(), piece_bb.end(), 0);
    std::fill(color_bb.begin(), color_bb.end(), 0);
    occupancy = 0;

    side_to_move      = WHITE;
    castling          = NO_CASTLING;
    en_passant_square = NO_SQUARE;
    rule_50           = 0;

    mg_psqt_score = 0;
    eg_psqt_score = 0;

    // We don't need to reset the stacks, just the stack
    // counter, since when appending to the stack, the value
    // gets replaced anyways
    ply = 0;

    hash = 0;
}

// Clears just one square
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

    hash ^= Zobrist::ps_key(piece, square);

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

    hash ^= Zobrist::ps_key(piece, square);
}

// Make a move based on the given Move "object," which
// is actually just an integer with information packed 
// in the bits
void Position::make_move(Move move) {
    bool is_white = side_to_move == WHITE;

    Square from_squ = from(move);
    Square dest_squ = dest(move);
    MoveFlag flag_  = flag(move);

    Piece moving = board[from_squ];
    Piece captured = flag_ == EN_PASSANT ? make_piece(PAWN, !side_to_move): board[dest_squ];

    hash_stack[ply] = hash;
    capture_stack[ply++] = captured;
    

    if (en_passant_square != NO_SQUARE)
        hash ^= Zobrist::ep_key(en_passant_square);
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
            hash ^= Zobrist::ep_key(en_passant_square);


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
    hash ^= Zobrist::castling_key(castling);

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
    hash ^= Zobrist::castling_key(castling);

    if (captured == NO_PIECE && type_of(moving) != PAWN) {
        rule_50++;
    }  else {
        rule_50 = 0;
    }

    side_to_move = !side_to_move;
    hash ^= Zobrist::side_key();
}

// Makes a move based on an algebraic string, like
// e2e4, or a7a8q
void Position::make_move(const std::string& str) {
    // Approach: we generate every single move
    // in the position, and then see which one matches
    // the given string. We make_move the move "object"
    // that matches
    MoveList list;
    generate_moves(list);

    for (Move m: list) {
        if (algebraic(m) == str) {
            make_move(m);
            return;
        }
    }
}

void Position::undo_move(Move move, const StoredGameState& gs) {
    side_to_move = !side_to_move;

    bool is_white = white_to_move();

    Square from_squ = from(move);
    Square dest_squ = dest(move);
    MoveFlag flag_  = flag(move);

    Piece moving = board[dest_squ];
    Piece captured = capture_stack[--ply];

    // Restore previous states
    castling = gs.prev_castling;
    en_passant_square = gs.prev_ep;
    rule_50 = gs.prev_r50;

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

    hash = hash_stack[ply];
}

// =========================== Evaluation Function ===========================

Score Position::evaluate() const {

    // Piece Square Table
    int phase = Evaluate::game_phase(this);
    int mg_phase = phase;
    int eg_phase = 24 - phase;

    Score mg_score = mg_psqt_score + (white_to_move() ? TEMPO_BONUS_MG: -TEMPO_BONUS_MG);
    Score eg_score = eg_psqt_score + (white_to_move() ? TEMPO_BONUS_EG: -TEMPO_BONUS_EG);

    // Taper between EG and MG score based on game phase
    Score score = (mg_score * mg_phase + eg_score * eg_phase) / 24;

    return side_to_move == WHITE ? score: -score;
}

namespace {
template <MoveFlag flag>
inline void extract(Bitboard& bb, MoveList& moves, Direction dir) {
    Square squ = Square(pop_lsb(bb));
    moves.add(create_move(squ - dir, squ, flag));
}
}

// =========================== Search Related Function ===========================

void Position::generate_moves(MoveList& moves) const {

    Color us = side_to_move;
    Color them = !us;
    bool  is_white = us == WHITE;

    // Kings are never capturable in legal chess. Keep pseudo-legal generation
    // from producing king-capture moves, which can poison search scores.
    Bitboard their_pieces = color_bb[them] & ~piece_bb[make_piece(KING, them)];

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
}

template <PieceType pt>
void Position::generate_piece_moves(MoveList& list) const {
    Color us = side_to_move;
    Bitboard our_pieces = color_bb[us];
    Bitboard their_king = piece_bb[make_piece(KING, !us)];

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

        attacks &= ~(our_pieces | their_king);

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
    Bitboard king = piece_bb[make_piece(KING, c)];
    if (king == 0) return true;

    return is_attacked(Square(ctz(king)), !c);
}

bool Position::is_repetition() const {
    int start = std::max(0, ply - rule_50);
    int count = 1;

    for (int i = ply - 2; i >= start; i -= 2) {
        if (hash_stack[i] == hash) {
            count += 1;
        }

        if (count == 3) return true;
    }
    return false;
}



} // namespace TuffChess
