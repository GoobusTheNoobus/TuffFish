#pragma once

#include "types.hpp"

namespace TuffFish {

/*
Transposition Table (TT) overview
---------------------------------
A transposition is when different move orders reach the same position.
Instead of re-searching that position every time, we cache the result.

Each entry stores:
- key: full Zobrist hash to verify we hit the same position
- score: search result from that position
- best_move: move that looked best when this was searched
- depth: how deep the search was when score was computed
- flag:
  - VALUE_EXACT: score is exact for this node
  - VALUE_LOWER: score is at least this value (fail-high bound)
  - VALUE_UPPER: score is at most this value (fail-low bound)

Negamax uses these values to:
1) return immediately on exact hits,
2) tighten alpha/beta with bounds,
3) order moves by trying the cached best move first.
*/
struct HashEntry {
    HashKey key = 0;
    Score score = 0;
    Move best_move = 0;
    int depth = 0;
    TTFlag flag = VALUE_NONE;

    HashEntry() = default;

};

struct TranspositionTable {
    static constexpr int MEGABYTES = 16;
    static constexpr int ENTRIES = MEGABYTES * 1024 * 1024 / sizeof(HashEntry);

    /*
    Direct-mapped table:
    - We use key % ENTRIES to choose one slot.
    - Very fast and simple.
    - Collisions are handled by replacement policy in put().
    */
    static std::array<HashEntry, ENTRIES> data;

    static HashEntry* get(HashKey key);
    static void put(HashKey key, Score, Move, int depth, TTFlag flag);
    static void clear();
};


} // namespace TuffFish
