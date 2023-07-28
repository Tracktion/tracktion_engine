//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_XXHASH_HEADER_INCLUDED
#define CHOC_XXHASH_HEADER_INCLUDED

#include <cstring>
#include <memory>

namespace choc::hash
{

//==============================================================================
/// An implementation of the 32-bit xxHash algorithm, which is a hashing function
/// optimised for speed rather than cryptographic strength.
///
/// See https://github.com/Cyan4973/xxHash
///
/// See also xxHash64
class xxHash32
{
public:
    /// Creates a hashing state object with an optional seed value
    /// After creating an object, you can repeatedly call addInput() to feed
    /// the data into it, and when done, call getHash() to get the resulting
    /// hash value.
    explicit xxHash32 (uint32_t seed = 0);

    xxHash32 (const xxHash32&) = default;
    xxHash32& operator= (const xxHash32&) = default;

    /// Performs a hash on a single chunk of memory and returns the result with
    /// one function call.
    static uint32_t hash (const void* inputData, size_t numBytes, uint32_t seed = 0);

    /// Feeds a chunk of data into the hash state.
    void addInput (const void* inputData, size_t numBytes) noexcept;

    /// Adds a string to the hash state.
    void addInput (std::string_view) noexcept;

    /// Returns the finished hash value.
    uint32_t getHash() const;

private:
    //==============================================================================
    static constexpr uint32_t prime1 = 2654435761u, prime2 = 2246822519u,
                              prime3 = 3266489917u, prime4 = 668265263u,
                              prime5 = 374761393u;

    struct State
    {
        State (uint32_t) noexcept;
        void process (const void*) noexcept;
        uint32_t s0, s1, s2, s3;
    };

    uint64_t totalLength = 0;
    State state;
    static constexpr size_t bytesPerBlock = 16;
    uint8_t currentBlock[bytesPerBlock];
    uint32_t currentBlockSize = 0;
};


//==============================================================================
/// An implementation of the 64-bit xxHash algorithm, which is a hashing function
/// optimised for speed rather than cryptographic strength.
///
/// See https://github.com/Cyan4973/xxHash
///
/// See also xxHash32
class xxHash64
{
public:
    /// Creates a hashing state object with an optional seed value
    /// After creating an object, you can repeatedly call addInput() to feed
    /// the data into it, and when done, call getHash() to get the resulting
    /// hash value.
    explicit xxHash64 (uint64_t seed = 0);

    xxHash64 (const xxHash64&) = default;
    xxHash64& operator= (const xxHash64&) = default;

    /// Performs a hash on a single chunk of memory and returns the result with
    /// one function call.
    static uint64_t hash (const void* inputData, size_t numBytes, uint64_t seed = 0);

    /// Feeds a chunk of data into the hash state.
    void addInput (const void* inputData, size_t numBytes) noexcept;

    /// Adds a string to the hash state.
    void addInput (std::string_view) noexcept;

    /// Returns the finished hash value.
    uint64_t getHash() const;

private:
    //==============================================================================
    static constexpr uint64_t prime1 = 11400714785074694791ull,
                              prime2 = 14029467366897019727ull,
                              prime3 = 1609587929392839161ull,
                              prime4 = 9650029242287828579ull,
                              prime5 = 2870177450012600261ull;

    struct State
    {
        State (uint64_t) noexcept;
        void process (const void*) noexcept;
        uint64_t s0, s1, s2, s3;
    };

    uint64_t totalLength = 0;
    State state;
    static constexpr size_t bytesPerBlock = 32;
    uint8_t currentBlock[bytesPerBlock];
    uint32_t currentBlockSize = 0;
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

static inline uint32_t rotl (uint32_t value, uint32_t numBits) noexcept  { return (value << numBits) | (value >> (32 - numBits)); }
static inline uint64_t rotl (uint64_t value, uint32_t numBits) noexcept  { return (value << numBits) | (value >> (64 - numBits)); }

inline xxHash32::xxHash32 (uint32_t seed)  : state (seed) {}

inline uint32_t xxHash32::hash (const void* inputData, size_t numBytes, uint32_t seed)
{
    xxHash32 h (seed);
    h.addInput (inputData, numBytes);
    return h.getHash();
}

inline void xxHash32::addInput (const void* inputData, size_t numBytes) noexcept
{
    if (inputData == nullptr || numBytes == 0)
        return;

    totalLength += numBytes;
    auto input = static_cast<const uint8_t*> (inputData);

    if (currentBlockSize + numBytes < bytesPerBlock)
    {
        memcpy (currentBlock + currentBlockSize, input, numBytes);
        currentBlockSize += static_cast<uint32_t> (numBytes);
        return;
    }

    auto inputEnd = input + numBytes;
    auto lastFullInputBlock = inputEnd - bytesPerBlock;
    auto s = state;

    if (currentBlockSize != 0)
    {
        while (currentBlockSize < bytesPerBlock)
            currentBlock[currentBlockSize++] = *input++;

        s.process (currentBlock);
    }

    while (input <= lastFullInputBlock)
    {
        s.process (input);
        input += bytesPerBlock;
    }

    state = s;
    currentBlockSize = static_cast<uint32_t> (inputEnd - input);
    memcpy (currentBlock, input, currentBlockSize);
}

inline void xxHash32::addInput (std::string_view s) noexcept   { addInput (s.data(), s.length()); }

inline uint32_t xxHash32::getHash() const
{
    auto hash = static_cast<uint32_t> (totalLength);

    if (totalLength >= bytesPerBlock)
        hash += rotl (state.s0, 1)
              + rotl (state.s1, 7)
              + rotl (state.s2, 12)
              + rotl (state.s3, 18);
    else
        hash += state.s2 + prime5;

    uint32_t asInt;
    uint32_t i = 0;

    for (; i + sizeof (asInt) <= currentBlockSize; i += static_cast<uint32_t> (sizeof (asInt)))
    {
        memcpy (std::addressof (asInt), currentBlock + i, sizeof (asInt));
        hash = rotl (hash + asInt * prime3, 17) * prime4;
    }

    while (i < currentBlockSize)
        hash = rotl (hash + currentBlock[i++] * prime5, 11) * prime1;

    hash ^= hash >> 15;  hash *= prime2;
    hash ^= hash >> 13;  hash *= prime3;
    hash ^= hash >> 16;

    return hash;
}

inline xxHash32::State::State (uint32_t seed) noexcept
   : s0 (seed + prime1 + prime2), s1 (seed + prime2),
     s2 (seed), s3 (seed - prime1)
{}

inline void xxHash32::State::process (const void* block) noexcept
{
    uint32_t ints[4];
    memcpy (ints, block, bytesPerBlock);
    s0 = rotl (s0 + ints[0] * prime2, 13) * prime1;
    s1 = rotl (s1 + ints[1] * prime2, 13) * prime1;
    s2 = rotl (s2 + ints[2] * prime2, 13) * prime1;
    s3 = rotl (s3 + ints[3] * prime2, 13) * prime1;
}

//==============================================================================
inline xxHash64::xxHash64 (uint64_t seed)  : state (seed) {}

inline uint64_t xxHash64::hash (const void* inputData, size_t numBytes, uint64_t seed)
{
    xxHash64 h (seed);
    h.addInput (inputData, numBytes);
    return h.getHash();
}

inline void xxHash64::addInput (const void* inputData, size_t numBytes) noexcept
{
    if (inputData == nullptr || numBytes == 0)
        return;

    totalLength += numBytes;
    auto input = static_cast<const uint8_t*> (inputData);

    if (currentBlockSize + numBytes < bytesPerBlock)
    {
        memcpy (currentBlock + currentBlockSize, input, numBytes);
        currentBlockSize += static_cast<uint32_t> (numBytes);
        return;
    }

    auto inputEnd = input + numBytes;
    auto lastFullInputBlock = inputEnd - bytesPerBlock;
    auto s = state;

    if (currentBlockSize != 0)
    {
        while (currentBlockSize < bytesPerBlock)
            currentBlock[currentBlockSize++] = *input++;

        s.process (currentBlock);
    }

    while (input <= lastFullInputBlock)
    {
        s.process (input);
        input += bytesPerBlock;
    }

    state = s;
    currentBlockSize = static_cast<uint32_t> (inputEnd - input);
    memcpy (currentBlock, input, currentBlockSize);
}

inline void xxHash64::addInput (std::string_view s) noexcept   { addInput (s.data(), s.length()); }

inline uint64_t xxHash64::getHash() const
{
    uint64_t hash;

    if (totalLength >= bytesPerBlock)
    {
        hash = rotl (state.s0, 1)
             + rotl (state.s1, 7)
             + rotl (state.s2, 12)
             + rotl (state.s3, 18);

        hash = (hash ^ (prime1 * rotl (state.s0 * prime2, 31))) * prime1 + prime4;
        hash = (hash ^ (prime1 * rotl (state.s1 * prime2, 31))) * prime1 + prime4;
        hash = (hash ^ (prime1 * rotl (state.s2 * prime2, 31))) * prime1 + prime4;
        hash = (hash ^ (prime1 * rotl (state.s3 * prime2, 31))) * prime1 + prime4;
    }
    else
    {
        hash = state.s2 + prime5;
    }

    hash += totalLength;

    uint32_t i = 0;

    for (; i + sizeof (uint64_t) <= currentBlockSize; i += static_cast<uint32_t> (sizeof (uint64_t)))
    {
        uint64_t asInt;
        memcpy (std::addressof (asInt), currentBlock + i, sizeof (asInt));
        hash = rotl (hash ^ (rotl (asInt * prime2, 31) * prime1), 27) * prime1 + prime4;
    }

    if (i + sizeof (uint32_t) <= currentBlockSize)
    {
        uint32_t asInt;
        memcpy (std::addressof (asInt), currentBlock + i, sizeof (asInt));
        hash = rotl (hash ^ asInt * prime1, 23) * prime2 + prime3;
        i += static_cast<uint32_t> (sizeof (uint32_t));
    }

    while (i < currentBlockSize)
        hash = rotl (hash ^ currentBlock[i++] * prime5, 11) * prime1;

    hash ^= hash >> 33;   hash *= prime2;
    hash ^= hash >> 29;   hash *= prime3;
    hash ^= hash >> 32;

    return hash;
}

inline xxHash64::State::State (uint64_t seed) noexcept
   : s0 (seed + prime1 + prime2), s1 (seed + prime2),
     s2 (seed), s3 (seed - prime1)
{}

inline void xxHash64::State::process (const void* block) noexcept
{
    uint64_t ints[4];
    memcpy (ints, block, bytesPerBlock);

    s0 = rotl (s0 + ints[0] * prime2, 31) * prime1;
    s1 = rotl (s1 + ints[1] * prime2, 31) * prime1;
    s2 = rotl (s2 + ints[2] * prime2, 31) * prime1;
    s3 = rotl (s3 + ints[3] * prime2, 31) * prime1;
}

} // namespace choc::hash

#endif // CHOC_XXHASH_HEADER_INCLUDED
