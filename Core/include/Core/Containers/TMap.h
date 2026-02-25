#pragma once
#include <utility>
#include <functional>
#include <emmintrin.h> // SSE2
#include "TArray.h"
#include "Core/CoreTypes.h"
#include "Core/CoreMemory.h"

namespace Rebel::Core::Memory {

template<typename K, typename V>
struct TPair {
    K Key;
    V Value;
    uint8 TopHash = 0; // 0 = empty, 1 = tombstone, 2-255 = valid

    TPair() = default;
    TPair(const K& k, const V& v, uint8 th) : Key(k), Value(v), TopHash(th) {}
    TPair(K&& k, V&& v, uint8 th) : Key(std::move(k)), Value(std::move(v)), TopHash(th) {}
};

template<typename Key, typename Value>
class TMap {
public:
    using PairType = TPair<Key, Value>;

    TMap() {
        capacity = 16; // start with 16 for SIMD alignment
        buckets.Resize(capacity);
        count = 0;
    }

    ~TMap() = default;
    TMap(const TMap&) = default;
    TMap& operator=(const TMap&) = default;
    TMap(TMap&&) noexcept = default;
    TMap& operator=(TMap&&) noexcept = default;

    Bool Add(const Key& key, const Value& value) { if (NeedsGrow()) Grow(); return InsertInternal(key, value); }
    Bool Add(Key&& key, Value&& value) { if (NeedsGrow()) Grow(); return InsertInternal(std::move(key), std::move(value)); }

    Value& operator[](const Key& key) {
        size_t idx;
        if (FindIndex(key, idx)) return buckets[idx].Value;
        Add(key, Value());
        FindIndex(key, idx);
        return buckets[idx].Value;
    }

    Value* Find(const Key& key) {
        size_t idx;
        if (!FindIndex(key, idx)) return nullptr;
        return &buckets[idx].Value;
    }

    const Value* Find(const Key& key) const {
        size_t idx;
        if (!FindIndex(key, idx)) return nullptr;
        return &buckets[idx].Value;
    }

    Bool Contains(const Key& key) const
    {
        size_t idx;
        return FindIndex(key, idx);   // or: return Find(key) != nullptr;
    }

    Bool Remove(const Key& key) {
        size_t idx;
        if (!FindIndex(key, idx)) return false;
        buckets[idx].TopHash = 1; // tombstone
        count--;
        return true;
    }
    void Clear()
    {
        buckets.Clear();
        capacity = 16;
        buckets.Resize(capacity);
        count = 0;
    }

    MemSize Num() const { return count; }
    Bool IsEmpty() const { return count == 0; }

    class iterator {
    public:
        iterator(TArray<PairType>* b, size_t i) : buckets(b), idx(i) { Advance(); }
        PairType& operator*() { return (*buckets)[idx]; }
        PairType* operator->() { return &(*buckets)[idx]; }
        iterator& operator++() { ++idx; Advance(); return *this; }
        bool operator!=(const iterator& other) const { return idx != other.idx; }
    private:
        void Advance() { while (idx < buckets->Num() && (*buckets)[idx].TopHash < 2) idx++; }
        TArray<PairType>* buckets;
        size_t idx;
    };

    class const_iterator {
    public:
        const_iterator(const TArray<PairType>* b, size_t i) : buckets(b), idx(i) { Advance(); }
        const PairType& operator*() const { return (*buckets)[idx]; }
        const PairType* operator->() const { return &(*buckets)[idx]; }
        const_iterator& operator++() { ++idx; Advance(); return *this; }
        bool operator!=(const const_iterator& other) const { return idx != other.idx; }

    private:
        void Advance() { while (idx < buckets->Num() && (*buckets)[idx].TopHash < 2) idx++; }
        const TArray<PairType>* buckets;
        size_t idx;
    };

    iterator begin() { return iterator(&buckets, 0); }
    iterator end() { return iterator(&buckets, buckets.Num()); }

    const_iterator begin() const { return const_iterator(&buckets, 0); }
    const_iterator end() const { return const_iterator(&buckets, buckets.Num()); }
    const_iterator cbegin() const { return const_iterator(&buckets, 0); }
    const_iterator cend() const { return const_iterator(&buckets, buckets.Num()); }

private:
    TArray<PairType> buckets;
    MemSize capacity = 0;
    MemSize count = 0;

    size_t HashKey(const Key& key) const { return std::hash<Key>{}(key); }
    uint8 TopHash(size_t hash) const { return static_cast<uint8>((hash >> (sizeof(size_t)*8 - 8)) | 0x80); }
    Bool NeedsGrow() const { return count * 2 >= capacity; }

    void Grow() {
        size_t newCap = capacity * 2;
        TArray<PairType> old = std::move(buckets);
        buckets.Resize(newCap);
        capacity = newCap;
        count = 0;
        for (size_t i = 0; i < old.Num(); i++) {
            if (old[i].TopHash >= 2) InsertInternal(std::move(old[i].Key), std::move(old[i].Value));
        }
    }

    template<typename KArg, typename VArg>
    Bool InsertInternal(KArg&& key, VArg&& value) {
        size_t hash = HashKey(key);
        uint8 th = TopHash(hash);
        size_t idx = hash & (capacity - 1);
        size_t firstTombstone = SIZE_MAX;

        while (true) {
            auto& slot = buckets[idx];
            if (slot.TopHash == 0) {
                if (firstTombstone != SIZE_MAX) idx = firstTombstone;
                buckets[idx] = PairType(std::forward<KArg>(key), std::forward<VArg>(value), th);
                count++;
                return true;
            }
            if (slot.TopHash == 1) { if (firstTombstone == SIZE_MAX) firstTombstone = idx; }
            else if (slot.TopHash == th && slot.Key == key) return false;
            idx = (idx + 1) & (capacity - 1);
        }
    }

    Bool FindIndex(const Key& key, size_t& outIdx) const {

        ENSURE(this != nullptr);
        ENSURE(capacity != 0);
        ENSURE(buckets.Num() == capacity);          // veya >= capacity, tasarımına göre
        ENSURE(buckets.Data() != nullptr); 

        size_t hash = HashKey(key);
        uint8 th = TopHash(hash);
        size_t idx = hash & (capacity - 1);

        while (true) {
            // SIMD check 16 TopHash bytes at a time
            size_t step = 16;
            for (size_t i = 0; i < step; i += 16) {
                if (idx + i >= capacity) break;
                __m128i topHashes = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&buckets[idx + i].TopHash));
                __m128i target = _mm_set1_epi8(th);
                __m128i cmp = _mm_cmpeq_epi8(topHashes, target);
                int mask = _mm_movemask_epi8(cmp);
                if (mask != 0) {
                    // At least one candidate, check Key equality
                    for (int j = 0; j < 16; j++) {
                        if ((mask >> j) & 1) {
                            size_t checkIdx = (idx + i + j) & (capacity - 1);
                            const auto& slot = buckets[checkIdx];
                            if (slot.TopHash >= 2 && slot.Key == key) { outIdx = checkIdx; return true; }
                        }
                    }
                }
            }
            // fallback linear probe
            if (buckets[idx].TopHash == 0) return false;
            idx = (idx + 1) & (capacity - 1);
        }
    }
};

} // namespace Rebel::Core::Memory
// Alias for TMap
template<typename Key, typename Value>
using TMap = Rebel::Core::Memory::TMap<Key, Value>;
