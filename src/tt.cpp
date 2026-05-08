#include "tt.hpp"

namespace TuffFish {

namespace {

// Indexing strategy
// 
// We use a direct-mapped transposition table:
// - the index is gotten by using the lower n bits of the hash
// - lookup is O(1)
// - collision means the previous entry in that slot may be replaced.

int ttindex(HashKey key) {
    return int(key % TranspositionTable::ENTRIES);
}
}

std::array<HashEntry, TranspositionTable::ENTRIES> TranspositionTable::data{};

HashEntry* TranspositionTable::get(HashKey key) {
    HashEntry* e = &data[ttindex(key)];

    if (e->key == key) {
        return e;
    }
    return nullptr;
}

void TranspositionTable::put(HashKey key, Score score, Move move, int depth, TTFlag flag) {
    HashEntry* old = &data[ttindex(key)];

    // Replacement policy
    // - Always replace if this is a different position (new key in this slot).
    // - For the same position, keep the deeper (or equal-depth newer) result.
    // - Empty entries (VALUE_NONE) are always replaceable.

    if (key != old->key || depth >= old->depth || old->flag == VALUE_NONE) {
        old->depth     = depth;
        old->best_move = move;
        old->key       = key;
        old->score     = score; 
        old->flag       = flag;
    }
}

void TranspositionTable::clear() {
    data.fill(HashEntry{});
}

} // namespace TuffFish
