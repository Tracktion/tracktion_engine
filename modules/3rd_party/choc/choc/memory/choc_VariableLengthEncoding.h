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

#ifndef CHOC_VAR_LEN_INTS_HEADER_INCLUDED
#define CHOC_VAR_LEN_INTS_HEADER_INCLUDED

#include <cstdint>
#include <cstddef>
#include <type_traits>

/*
   This file contains some functions for encoding/decoding packed
   integer representations. Handy for serialisation tasks.
*/

namespace choc::integer_encoding
{

/// Writes an integer into a block of memory using a variable-length encoding system.
/// This compression scheme encodes 7 bits per byte, using the upper bit as a flag to
/// indicate that another byte follows.
/// For speed, this function expects to always be given enough space to write into, so
/// make sure that you provide at least (sizeof(IntegerType) * 7) / 8 + 1 bytes of space.
/// Returns the number of bytes used.
/// If you want to encode signed numbers, you may want to team this up with zigzagEncode()
/// and zigzagDecode() to make it much more space-efficient for negative values.
template <typename IntegerType>
size_t encodeVariableLengthInt (void* destBuffer, IntegerType valueToEncode);

/// Decodes an integer from data that was produced by encodeVariableLengthInt().
/// This compression scheme encodes 7 bits per byte, using the upper bit as a flag to
/// indicate that another byte follows.
/// The bytesUsed parameter returns either a number of bytes that were needed, or 0
/// if something went wrong.
template <typename IntegerType>
IntegerType decodeVariableLengthInt (const void* encodedData, size_t availableSize, size_t& bytesUsed);

/// Zigzag encodes an integer.
/// Because negative numbers always have the high bit of an integer set, this means
/// that if you try to variable-length encode them, they will always take more space
/// than the original, uncompressed integer. Zigzag encoding is a trick that
/// converts small negative numbers to positive values that avoid high bits, so when
/// you're using variable-length encoding to store values which may be negative, it usually
/// makes sense to call zigzagEncode() on your values before storing them, then call
/// zigzagDecode() to get the original value back from the decompressed integer.
template <typename IntegerType>
IntegerType zigzagEncode (IntegerType);

/// Decodes an integer that was encoded with zigzagEncode().
/// See zigzagEncode() for details.
template <typename IntegerType>
IntegerType zigzagDecode (IntegerType);


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


template <typename IntegerType>
size_t encodeVariableLengthInt (void* destBuffer, IntegerType valueToEncode)
{
    size_t size = 0;
    auto n = static_cast<typename std::make_unsigned<IntegerType>::type> (valueToEncode);
    auto dest = static_cast<uint8_t*> (destBuffer);

    for (;;)
    {
        auto byte = static_cast<uint8_t> (n & static_cast<IntegerType> (127));
        n >>= 7;

        if (n == 0)
        {
            dest[size] = byte;
            return size + 1;
        }

        dest[size++] = static_cast<uint8_t> (byte | 0x80u);
    }
}

template <typename IntegerType>
IntegerType decodeVariableLengthInt (const void* encodedData, size_t availableSize, size_t& bytesUsed)
{
    typename std::make_unsigned<IntegerType>::type result = 0;
    auto source = static_cast<const uint8_t*> (encodedData);
    size_t size = 0;

    for (uint32_t bit = 0;; bit += 7)
    {
        if (size == availableSize)
        {
            bytesUsed = 0;
            return 0;
        }

        auto byte = source[size++];
        result |= (static_cast<typename std::make_unsigned<IntegerType>::type> (byte & 127) << bit);

        if (byte >> 7)
            continue;

        bytesUsed = size;
        return static_cast<IntegerType> (result);
    }
}

template <typename IntegerType>
IntegerType zigzagEncode (IntegerType n)
{
    auto un = static_cast<typename std::make_unsigned<IntegerType>::type> (n);
    auto sn = static_cast<typename std::make_signed<IntegerType>::type> (n);
    return static_cast<IntegerType> ((sn >> (sizeof (IntegerType) * 8 - 1)) ^ static_cast<IntegerType> (un << 1));
}

template <typename IntegerType>
IntegerType zigzagDecode (IntegerType n)
{
    auto un = static_cast<typename std::make_unsigned<IntegerType>::type> (n);
    auto sn = static_cast<typename std::make_signed<IntegerType>::type> (n);
    return static_cast<IntegerType> (static_cast<decltype (sn)> (un >> 1) ^ -(sn & static_cast<decltype (sn)> (1)));
}

} // namespace choc::integer_encoding

#endif // CHOC_VAR_LEN_INTS_HEADER_INCLUDED
