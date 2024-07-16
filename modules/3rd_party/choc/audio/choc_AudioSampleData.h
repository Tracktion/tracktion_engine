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

#ifndef CHOC_INT_TO_SAMPLE_CONVERSION_HEADER_INCLUDED
#define CHOC_INT_TO_SAMPLE_CONVERSION_HEADER_INCLUDED

#include "../memory/choc_Endianness.h"


//==============================================================================
/*
    This file contains functions to convert various formats of packed audio integer
    sample data to/from floating point.
*/
namespace choc::audio::sampledata
{

/// Reads and writes 8-bit signed samples.
struct Int8
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 1;
};

/// Reads and writes 8-bit unsigned samples, where the origin = 128.
struct UInt8
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 1;
};

/// Reads and writes 16-bit signed big-endian samples.
struct Int16BigEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 2;
};

/// Reads and writes 16-bit signed little-endian samples.
struct Int16LittleEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 2;
};

/// Reads and writes 24-bit signed big-endian samples.
struct Int24BigEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 3;
};

/// Reads and writes 24-bit signed little-endian samples.
struct Int24LittleEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 3;
};

/// Reads and writes 32-bit signed big-endian samples.
struct Int32BigEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 4;
};

/// Reads and writes 32-bit signed little-endian samples.
struct Int32LittleEndian
{
    /// Reads an integer in this format from the given raw memory location, and scales it to the range -1.0f to 1.0f
    template <typename FloatType> static FloatType read (const void* source) noexcept;
    /// Clamps the given floating point value (in the range -1.0 to 1.0f) to this integer range and writes it to the given raw memory
    template <typename FloatType> static void write (void* dest, FloatType v) noexcept;

    /// The size of the packed raw data for this format
    static constexpr size_t sizeInBytes = 4;
};


//==============================================================================
/// Copies data from an interleaved multi-channel integer format to a
/// choc::buffer::BufferView. The buffer view must have the same number of frames
/// and channels as the integer data you provide.
template <typename IntFormat, typename BufferType>
void copyFromInterleavedIntData (BufferType&& destBuffer,
                                 const void* source, size_t sourceStrideBytes);

/// Copies data from a choc::buffer::BufferView to an interleaved multi-channel
/// integer format. The buffer view must have the same number of frames
/// and channels as the integer data you provide.
template <typename IntFormat, typename BufferType>
void copyToInterleavedIntData (void* dest, size_t destStrideBytes,
                               const BufferType& sourceBuffer);


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

template <typename FloatType, int32_t maxVal, typename IntType>
FloatType convertToFloat (IntType v) noexcept
{
    return static_cast<FloatType> (1.0 / maxVal) * static_cast<FloatType> (v);
}

template <typename IntType, int32_t maxVal, typename FloatType>
IntType convertToInt (FloatType v) noexcept
{
    auto scaled = v * static_cast<FloatType> (maxVal);
    return scaled <= static_cast<FloatType> (-maxVal - 1)
                ? static_cast<IntType> (-maxVal - 1)
                : (scaled >= static_cast<FloatType> (maxVal)
                    ? static_cast<IntType> (maxVal)
                    : static_cast<IntType> (scaled));
}

template <typename FloatType> FloatType Int8::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7f> (*static_cast<const int8_t*> (source));
}

template <typename FloatType> void Int8::write (void* dest, FloatType v) noexcept
{
    *static_cast<int8_t*> (dest) = convertToInt<int8_t, 0x7f> (v);
}

template <typename FloatType> FloatType UInt8::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7f> (*static_cast<const uint8_t*> (source) - (uint8_t) 128);
}

template <typename FloatType> void UInt8::write (void* dest, FloatType v) noexcept
{
    *static_cast<uint8_t*> (dest) = static_cast<uint8_t> (convertToInt<int32_t, 0x7f> (v) + 128);
}

template <typename FloatType> FloatType Int16BigEndian::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7fff> (choc::memory::readBigEndian<int16_t> (source));
}

template <typename FloatType> void Int16BigEndian::write (void* dest, FloatType v) noexcept
{
    choc::memory::writeBigEndian (dest, convertToInt<int16_t, 0x7fff> (v));
}

template <typename FloatType> FloatType Int16LittleEndian::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7fff> (choc::memory::readLittleEndian<int16_t> (source));
}

template <typename FloatType> void Int16LittleEndian::write (void* dest, FloatType v) noexcept
{
    choc::memory::writeLittleEndian (dest, convertToInt<int16_t, 0x7fff> (v));
}

template <typename FloatType> FloatType Int24BigEndian::read (const void* source) noexcept
{
    auto s = static_cast<const uint8_t*> (source);
    auto i = (static_cast<uint32_t> (static_cast<int32_t> (static_cast<int8_t> (s[0]))) << 16) | static_cast<uint32_t> (s[1] << 8) | static_cast<uint32_t> (s[1]);
    return convertToFloat<FloatType, 0x7fffff> (static_cast<int32_t> (i));
}

template <typename FloatType> void Int24BigEndian::write (void* dest, FloatType v) noexcept
{
    auto i = convertToInt<int32_t, 0x7fffff> (v);
    auto d = static_cast<uint8_t*> (dest);
    d[0] = static_cast<uint8_t> (i >> 16);
    d[1] = static_cast<uint8_t> (i >> 8);
    d[2] = static_cast<uint8_t> (i);
}

template <typename FloatType> FloatType Int24LittleEndian::read (const void* source) noexcept
{
    auto s = static_cast<const uint8_t*> (source);
    auto i = (static_cast<uint32_t> (static_cast<int32_t> (static_cast<int8_t> (s[2]))) << 16) | static_cast<uint32_t> (s[1] << 8) | static_cast<uint32_t> (s[0]);
    return convertToFloat<FloatType, 0x7fffff> (static_cast<int32_t> (i));
}

template <typename FloatType> void Int24LittleEndian::write (void* dest, FloatType v) noexcept
{
    auto i = convertToInt<int32_t, 0x7fffff> (v);
    auto d = static_cast<uint8_t*> (dest);
    d[0] = static_cast<uint8_t> (i);
    d[1] = static_cast<uint8_t> (i >> 8);
    d[2] = static_cast<uint8_t> (i >> 16);
}

template <typename FloatType> FloatType Int32BigEndian::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7fffffff> (choc::memory::readBigEndian<int32_t> (source));
}

template <typename FloatType> void Int32BigEndian::write (void* dest, FloatType v) noexcept
{
    choc::memory::writeBigEndian (dest, convertToInt<int32_t, 0x7fffffff> (v));
}

template <typename FloatType> FloatType Int32LittleEndian::read (const void* source) noexcept
{
    return convertToFloat<FloatType, 0x7fffffff> (choc::memory::readLittleEndian<int32_t> (source));
}

template <typename FloatType> void Int32LittleEndian::write (void* dest, FloatType v) noexcept
{
    choc::memory::writeLittleEndian (dest, convertToInt<int32_t, 0x7fffffff> (v));
}

//==============================================================================
template <typename IntFormat, typename BufferType>
void copyFromInterleavedIntData (BufferType& destBuffer, const void* source, size_t sourceStride)
{
    using FloatType = typename BufferType::Sample;
    auto s = static_cast<const uint8_t*> (source);
    auto frameStride = sourceStride * destBuffer.getNumChannels();

    setAllSamples (destBuffer, [=] (choc::buffer::ChannelCount chan, choc::buffer::FrameCount frame)
    {
        return IntFormat::template read<FloatType> (s + frameStride * frame + sourceStride * chan);
    });
}

template <typename IntFormat, typename BufferType>
void copyToInterleavedIntData (void* dest, size_t destStride, const BufferType& sourceBuffer)
{
    auto d = static_cast<uint8_t*> (dest);
    auto frameStride = destStride * sourceBuffer.getNumChannels();
    auto size = sourceBuffer.getSize();

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto sourceSample = sourceBuffer.getIterator (chan);

        for (decltype (size.numFrames) frame = 0; frame < size.numFrames; ++frame)
            IntFormat::write (d + frameStride * frame + destStride * chan, *sourceSample++);
    }
}


} // namespace choc::buffer

#endif // CHOC_INT_TO_SAMPLE_CONVERSION_HEADER_INCLUDED
