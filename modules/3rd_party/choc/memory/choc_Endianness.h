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

#ifndef CHOC_ENDIANNESS_HEADER_INCLUDED
#define CHOC_ENDIANNESS_HEADER_INCLUDED

#include <cstring>

/*
   This file contains a few endianness and bit-cast functions which
   have been implemented using no platform or compiler-specific tricks
   or macros, so should be correct on any platform.
*/

namespace choc::memory
{
/// Performs a bitcast between primitive values.
/// (One day this won't be needed thanks to std::bit_cast, but at the time
/// I wrote this, there still wasn't much compiler support for it).
template <typename TargetType, typename SourceType>
TargetType bit_cast (SourceType sourceValue);

/// Reads any kind of primitive type from a native-endian
/// encoded value in memory.
/// As well as integers, this will also decode float and
/// double values.
template <typename Type>
Type readNativeEndian (const void* encodedValue);

/// Reads any kind of primitive type from a little-endian
/// encoded value in memory.
/// As well as integers, this will also decode float and
/// double values.
template <typename Type>
Type readLittleEndian (const void* encodedValue);

/// Reads any kind of primitive type from a big-endian
/// encoded value in memory.
/// As well as integers, this will also decode float and
/// double values.
template <typename Type>
Type readBigEndian (const void* encodedValue);

/// Writes a primitive value into memory, in a native-endian order.
/// The type can be an integer or float primitive.
template <typename Type>
void writeNativeEndian (void* destBuffer, Type valueToWrite);

/// Writes a primitive value into memory, in a little-endian order.
/// The type can be an integer or float primitive.
template <typename Type>
void writeLittleEndian (void* destBuffer, Type valueToWrite);

/// Writes a primitive value into memory, in a big-endian order.
/// The type can be an integer or float primitive.
template <typename Type>
void writeBigEndian (void* destBuffer, Type valueToWrite);

/// Returns a version of this integer with the byte-order reversed
uint16_t swapByteOrder (uint16_t valueToSwap) noexcept;

/// Returns a version of this integer with the byte-order reversed
uint32_t swapByteOrder (uint32_t valueToSwap) noexcept;

/// Returns a version of this integer with the byte-order reversed
uint64_t swapByteOrder (uint64_t valueToSwap) noexcept;

//==============================================================================
// This will define some macros CHOC_BIG_ENDIAN and CHOC_LITTLE_ENDIAN that
// should be pretty reliable on most platforms
#if (defined (__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || __BIG_ENDIAN__
 #define CHOC_LITTLE_ENDIAN 0
 #define CHOC_BIG_ENDIAN    1
#else
 #define CHOC_LITTLE_ENDIAN 1
 #define CHOC_BIG_ENDIAN    0
#endif

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

template <typename TargetType, typename SourceType>
inline TargetType bit_cast (SourceType source)
{
    static_assert (sizeof (SourceType) == sizeof (TargetType),
                   "Can only bitcast between objects of the same size");
    TargetType t;
    std::memcpy (std::addressof (t), std::addressof (source), sizeof (TargetType));
    return t;
}

template <typename Type>
inline Type readNativeEndian (const void* source)
{
    Type t;
    std::memcpy (std::addressof (t), source, sizeof (Type));
    return t;
}

template <typename Type>
inline Type readLittleEndian (const void* source)
{
    if constexpr (CHOC_LITTLE_ENDIAN)
    {
        return readNativeEndian<Type> (source);
    }
    else
    {
        auto src = static_cast<const uint8_t*> (source);

        if constexpr (sizeof (Type) == 8)
        {
            return bit_cast<Type> (static_cast<uint64_t> (src[0])
                                | (static_cast<uint64_t> (src[1]) << 8)
                                | (static_cast<uint64_t> (src[2]) << 16)
                                | (static_cast<uint64_t> (src[3]) << 24)
                                | (static_cast<uint64_t> (src[4]) << 32)
                                | (static_cast<uint64_t> (src[5]) << 40)
                                | (static_cast<uint64_t> (src[6]) << 48)
                                | (static_cast<uint64_t> (src[7]) << 56));
        }
        else if constexpr (sizeof (Type) == 4)
        {
            return bit_cast<Type> (static_cast<uint32_t> (src[0])
                                | (static_cast<uint32_t> (src[1]) << 8)
                                | (static_cast<uint32_t> (src[2]) << 16)
                                | (static_cast<uint32_t> (src[3]) << 24));
        }
        else
        {
            static_assert (sizeof (Type) == 2, "unsupported size");
            return static_cast<Type> (static_cast<uint16_t> (src[0])
                                   | (static_cast<uint16_t> (src[1]) << 8));
        }
    }
}

template <typename Type>
inline Type readBigEndian (const void* source)
{
    if constexpr (CHOC_BIG_ENDIAN)
    {
        return readNativeEndian<Type> (source);
    }
    else
    {
        auto src = static_cast<const uint8_t*> (source);

        if constexpr (sizeof (Type) == 8)
        {
            return bit_cast<Type> (static_cast<uint64_t> (src[7])
                                | (static_cast<uint64_t> (src[6]) << 8)
                                | (static_cast<uint64_t> (src[5]) << 16)
                                | (static_cast<uint64_t> (src[4]) << 24)
                                | (static_cast<uint64_t> (src[3]) << 32)
                                | (static_cast<uint64_t> (src[2]) << 40)
                                | (static_cast<uint64_t> (src[1]) << 48)
                                | (static_cast<uint64_t> (src[0]) << 56));
        }
        else if constexpr (sizeof (Type) == 4)
        {
            return bit_cast<Type> (static_cast<uint32_t> (src[3])
                                | (static_cast<uint32_t> (src[2]) << 8)
                                | (static_cast<uint32_t> (src[1]) << 16)
                                | (static_cast<uint32_t> (src[0]) << 24));
        }
        else
        {
            static_assert (sizeof (Type) == 2, "unsupported size");
            return static_cast<Type> (static_cast<uint16_t> (src[1])
                                   | (static_cast<uint16_t> (src[0]) << 8));
        }
    }
}

template <typename Type>
inline void writeNativeEndian (void* dest, Type source)
{
    std::memcpy (dest, std::addressof (source), sizeof (Type));
}

template <typename Type>
inline void writeLittleEndian (void* dest, Type source)
{
    if constexpr (CHOC_LITTLE_ENDIAN)
    {
        return writeNativeEndian<Type> (dest, source);
    }
    else
    {
        auto dst = static_cast<uint8_t*> (dest);

        if constexpr (sizeof (Type) == 8)
        {
            auto n = bit_cast<uint64_t> (source);

            dst[0] = static_cast<uint8_t> (n);
            dst[1] = static_cast<uint8_t> (n >> 8);
            dst[2] = static_cast<uint8_t> (n >> 16);
            dst[3] = static_cast<uint8_t> (n >> 24);
            dst[4] = static_cast<uint8_t> (n >> 32);
            dst[5] = static_cast<uint8_t> (n >> 40);
            dst[6] = static_cast<uint8_t> (n >> 48);
            dst[7] = static_cast<uint8_t> (n >> 56);
        }
        else if constexpr (sizeof (Type) == 4)
        {
            auto n = bit_cast<uint32_t> (source);

            dst[0] = static_cast<uint8_t> (n);
            dst[1] = static_cast<uint8_t> (n >> 8);
            dst[2] = static_cast<uint8_t> (n >> 16);
            dst[3] = static_cast<uint8_t> (n >> 24);
        }
        else
        {
            static_assert (sizeof (Type) == 2, "unsupported size");
            auto n = static_cast<uint16_t> (source);

            dst[0] = static_cast<uint8_t> (n);
            dst[1] = static_cast<uint8_t> (n >> 8);
        }
    }
}

template <typename Type>
inline void writeBigEndian (void* dest, Type source)
{
    if constexpr (CHOC_BIG_ENDIAN)
    {
        return writeNativeEndian<Type> (dest, source);
    }
    else
    {
        auto dst = static_cast<uint8_t*> (dest);

        if constexpr (sizeof (Type) == 8)
        {
            auto n = bit_cast<uint64_t> (source);

            dst[0] = static_cast<uint8_t> (n >> 56);
            dst[1] = static_cast<uint8_t> (n >> 48);
            dst[2] = static_cast<uint8_t> (n >> 40);
            dst[3] = static_cast<uint8_t> (n >> 32);
            dst[4] = static_cast<uint8_t> (n >> 24);
            dst[5] = static_cast<uint8_t> (n >> 16);
            dst[6] = static_cast<uint8_t> (n >> 8);
            dst[7] = static_cast<uint8_t> (n);
        }
        else if constexpr (sizeof (Type) == 4)
        {
            auto n = bit_cast<uint32_t> (source);

            dst[0] = static_cast<uint8_t> (n >> 24);
            dst[1] = static_cast<uint8_t> (n >> 16);
            dst[2] = static_cast<uint8_t> (n >> 8);
            dst[3] = static_cast<uint8_t> (n);
        }
        else
        {
            static_assert (sizeof (Type) == 2, "unsupported size");
            auto n = static_cast<uint16_t> (source);

            dst[0] = static_cast<uint8_t> (n >> 8);
            dst[1] = static_cast<uint8_t> (n);
        }
    }
}

inline uint16_t swapByteOrder (uint16_t n) noexcept
{
   #if defined(__llvm__) || (defined(__GNUC__) && ! defined (__ICC))
    return __builtin_bswap16 (n);
   #elif defined (_MSC_VER)
    return _byteswap_ushort (n);
   #else
    return (n >> 8) | (n << 8);
   #endif
}

inline uint32_t swapByteOrder (uint32_t n) noexcept
{
   #if defined(__llvm__) || (defined(__GNUC__) && ! defined (__ICC))
    return __builtin_bswap32 (n);
   #elif defined (_MSC_VER)
    return _byteswap_ulong (n);
   #else
    return (n >> 24) | (n << 24) | ((n << 8) & 0xff0000) | ((n >> 8) & 0xff00);
   #endif
}

inline uint64_t swapByteOrder (uint64_t n) noexcept
{
   #if defined(__llvm__) || (defined(__GNUC__) && ! defined (__ICC))
    return __builtin_bswap64 (n);
   #elif defined (_MSC_VER)
    return _byteswap_uint64 (n);
   #else
    return swapByteOrder (static_cast<uint32_t> (n >> 32))
            | (static_cast<uint64_t> (swapByteOrder (static_cast<uint32_t> (n))) << 32);
   #endif
}

} // namespace choc::memory

#endif // CHOC_ENDIANNESS_HEADER_INCLUDED
