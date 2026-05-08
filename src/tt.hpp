#pragma once

#include "types.hpp"

namespace TuffFish {


// Transposition Table
//
// A transposition is when different move orders reach the same position. 
// For example, from the starting position:
// 1. Nf3 Nf6 2. Nc3 Nc6 
// 1. Nc3 Nf6 2. Nf3 Nc6
// These 2 move orders give the same end position
// Instead of re-searching that position every time, we cache the result.

struct HashEntry {
    HashKey key = 0;
    Score score = 0;
    Move best_move = 0;
    int depth = 0;
    TTFlag flag = VALUE_NONE;

    HashEntry() = default;

};

// Each entry is a struct that stores:
// - key: full Zobrist hash to verify we hit the same position
// - score: search result from that position
// - best_move: move that looked best when this was searched
// - depth: how deep the search was when score was computed
// - flag:
//   - VALUE_EXACT: score is exact for this node
//   - VALUE_LOWER: score is at least this value (fail-high bound)
//   - VALUE_UPPER: score is at most this value (fail-low bound)


struct TranspositionTable {
    static constexpr int MEGABYTES = 16;
    static constexpr int ENTRIES = MEGABYTES * 1024 * 1024 / sizeof(HashEntry);

    static std::array<HashEntry, ENTRIES> data;

    static HashEntry* get(HashKey key);
    static void put(HashKey key, Score, Move, int depth, TTFlag flag);
    static void clear();
};


} // namespace TuffFish
