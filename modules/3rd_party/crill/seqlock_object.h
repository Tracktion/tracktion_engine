// crill - the Cross-platform Real-time, I/O, and Low-Latency Library
// Copyright (c) 2022 - Timur Doumler and Fabian Renn-Giles
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef CRILL_SEQLOCK_OBJECT_H
#define CRILL_SEQLOCK_OBJECT_H

#include <array>
#include <atomic>
#include "bytewise_atomic_memcpy.h"

namespace crill {

// A portable C++ implementation of a seqlock inspired by Hans Boehm's paper
// "Can Seqlocks Get Along With Programming Language Memory Models?"
// and the C implementation in jemalloc.
//
// This version allows only a single writer. Writes are guaranteed wait-free.
// It also allows multiple concurrent readers, which are wait-free against
// each other, but can block if there is a concurrent write.
template <typename T>
class seqlock_object
{
public:
    static_assert(std::is_trivially_copyable_v<T>);

    // Creates a seqlock_object with a default-constructed value.
    seqlock_object()
    {
        store(T());
    }

    // Creates a seqlock_object with the given value.
    seqlock_object(T t)
    {
        store(t);
    }

    // Reads and returns the current value.
    // Non-blocking guarantees: wait-free if there are no concurrent writes,
    // otherwise none.
    T load() const noexcept
    {
        T t;
        while (!try_load(t)) /* keep trying */;
        return t;
    }

    // Attempts to read the current value and write it into the passed-in object.
    // Returns: true if the read succeeded, false otherwise.
    // Non-blocking guarantees: wait-free.
    bool try_load(T& t) const noexcept
    {
        std::size_t seq1 = seq.load(std::memory_order_acquire);
        if (seq1 % 2 != 0)
            return false;

        crill::atomic_load_per_byte_memcpy(&t, &data, sizeof(data), std::memory_order_acquire);

        std::size_t seq2 = seq.load(std::memory_order_relaxed);
        if (seq1 != seq2)
            return false;

        return true;
    }

    // Updates the current value to the value passed in.
    // Non-blocking guarantees: wait-free.
    void store(T t) noexcept
    {
        // Note: load + store usually has better performance characteristics than fetch_add(1)
        std::size_t old_seq = seq.load(std::memory_order_relaxed);
        seq.store(old_seq + 1, std::memory_order_relaxed);

        crill::atomic_store_per_byte_memcpy(&data, &t, sizeof(data), std::memory_order_release);

        seq.store(old_seq + 2, std::memory_order_release);
    }

private:
    char data[sizeof(T)];
    std::atomic<std::size_t> seq = 0;
    static_assert(decltype(seq)::is_always_lock_free);
};

} // namespace crill

#endif //CRILL_SEQLOCK_OBJECT_H
