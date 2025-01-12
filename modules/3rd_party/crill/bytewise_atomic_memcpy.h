// crill - the Cross-platform Real-time, I/O, and Low-Latency Library
// Copyright (c) 2022 - Timur Doumler and Fabian Renn-Giles
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef CRILL_BYTEWISE_ATOMIC_MEMCPY_H
#define CRILL_BYTEWISE_ATOMIC_MEMCPY_H

#include <atomic>
#include "contracts.h"
#include "platform.h"

namespace crill {
    // These are implementations of the corresponding functions
    // atomic_load/store_per_byte_memcpy from the Concurrency TS 2.
    // They behave as if the source and dest bytes respectively
    // were individual atomic objects.
    // The implementations provided below is portable, but slow.
    // PRs with platform-optimised versions are welcome :)
    // The implementations provided below are also *technically*
    // UB because C++ does not let us loop over the bytes of
    // an object representation, but that is a known wording bug that
    // will be fixed by P1839; the technique should work on any
    // major compiler.

    // Preconditions:
    // - order is std::memory_order::acquire or std::memory_order::relaxed
    // - (char*)dest + [0, count) and (const char*)source + [0, count)
    //   are valid ranges that do not overlap
    // Effects:
    //   Copies count consecutive bytes pointed to by source into consecutive
    //   bytes pointed to by dest. Each individual load operation from a source
    //   byte is atomic with memory order order. These individual loads are
    //   unsequenced with respect to each other.
    inline void* atomic_load_per_byte_memcpy
    (void* dest, const void* source, size_t count, std::memory_order order)
    {
        CRILL_PRE(order == std::memory_order_acquire || order == std::memory_order_relaxed);

        char* dest_bytes = reinterpret_cast<char*>(dest);
        const char* src_bytes = reinterpret_cast<const char*>(source);

        for (std::size_t i = 0; i < count; ++i) {
              #if __cpp_lib_atomic_ref
                dest_bytes[i] = std::atomic_ref<char>(src_bytes[i]).load(std::memory_order_relaxed);
              #elif CRILL_CLANG || CRILL_GCC
                dest_bytes[i] = __atomic_load_n(src_bytes + i, __ATOMIC_RELAXED);
              #else
                // No atomic_ref or equivalent functionality available on this platform!
              #endif
        }

        std::atomic_thread_fence(order);

        return dest;
    }

    // Preconditions:
    // - order is std::memory_order::release or std::memory_order::relaxed
    // - (char*)dest + [0, count) and (const char*)source + [0, count)
    //   are valid ranges that do not overlap
    // Effects:
    //   Copies count consecutive bytes pointed to by source into consecutive
    //   bytes pointed to by dest. Each individual store operation to a
    //   destination byte is atomic with memory order order. These individual
    //   stores are unsequenced with respect to each other.
    inline void* atomic_store_per_byte_memcpy
    (void* dest, const void* source, size_t count, std::memory_order order)
    {
        CRILL_PRE(order == std::memory_order_release || order == std::memory_order_relaxed);

        std::atomic_thread_fence(order);

        char* dest_bytes = reinterpret_cast<char*>(dest);
        const char* src_bytes = reinterpret_cast<const char*>(source);

        for (size_t i = 0; i < count; ++i) {
          #if __cpp_lib_atomic_ref
            std::atomic_ref<char>(dest_bytes[i]).store(src_bytes[i], std::memory_order_relaxed);
          #elif CRILL_CLANG || CRILL_GCC
            __atomic_store_n(dest_bytes + i, src_bytes[i], __ATOMIC_RELAXED);
          #else
            // No atomic_ref or equivalent functionality available on this platform!
          #endif
        }

        return dest;
    }
}

#endif //CRILL_BYTEWISE_ATOMIC_MEMCPY_H
