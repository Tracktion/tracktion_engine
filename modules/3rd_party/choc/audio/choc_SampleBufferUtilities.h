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

#ifndef CHOC_SAMPLE_BUFFER_UTILS_HEADER_INCLUDED
#define CHOC_SAMPLE_BUFFER_UTILS_HEADER_INCLUDED

#include "choc_SampleBuffers.h"
#include "../containers/choc_Value.h"

//==============================================================================
namespace choc::buffer
{

/// Helper class which holds an InterleavedBuffer which it re-uses as intermediate
/// storage when creating a temporary interleaved copy of a ChannelArrayView
template <typename SampleType>
struct InterleavingScratchBuffer
{
    [[nodiscard]] InterleavedView<SampleType> getInterleavedBuffer (choc::buffer::Size size)
    {
        auto spaceNeeded = size.numChannels * size.numFrames;

        if (spaceNeeded > buffer.size())
            buffer.resize (spaceNeeded);

        return createInterleavedView (buffer.data(), size.numChannels, size.numFrames);
    }

    template <typename SourceBufferType>
    [[nodiscard]] InterleavedView<SampleType> interleave (const SourceBufferType& source)
    {
        auto dest = getInterleavedBuffer (source.getSize());
        copy (dest, source);
        return dest;
    }

private:
    std::vector<SampleType> buffer;
};

/// Helper class which holds a ChannelArrayBuffer which it re-uses as intermediate
/// storage when creating a temporary channel-based copy of an InterleavedView
template <typename SampleType>
struct DeinterleavingScratchBuffer
{
    [[nodiscard]] ChannelArrayView<SampleType> getDeinterleavedBuffer (choc::buffer::Size size)
    {
        if (buffer.getNumChannels() < size.numChannels || buffer.getNumFrames() < size.numFrames)
        {
            buffer.resize (size);
            return buffer.getView();
        }

        return buffer.getSection ({ 0, size.numChannels },
                                  { 0, size.numFrames });
    }

    template <typename SourceBufferType>
    [[nodiscard]] ChannelArrayView<SampleType> deinterleave (const SourceBufferType& source)
    {
        auto dest = getDeinterleavedBuffer (source.getSize());
        copy (dest, source);
        return dest;
    }

private:
    ChannelArrayBuffer<SampleType> buffer;
};


//==============================================================================
/// Creates an InterleavedBufferView which points to the data in a choc::value::ValueView.
/// The ValueView must be an array of either primitive values or vectors.
/// @see createValueViewFromBuffer()
template <typename SampleType>
inline InterleavedView<SampleType> createInterleavedViewFromValue (const choc::value::ValueView& value)
{
    auto& arrayType = value.getType();
    CHOC_ASSERT (arrayType.isArray());
    auto numFrames = arrayType.getNumElements();
    auto sourceData = const_cast<SampleType*> (reinterpret_cast<const SampleType*> (value.getRawData()));
    auto frameType = arrayType.getElementType();

    if (frameType.isVector() || frameType.isUniformArray())
    {
        CHOC_ASSERT (frameType.getElementType().isPrimitiveType<SampleType>());
        return createInterleavedView (sourceData, frameType.getNumElements(), numFrames);
    }

    CHOC_ASSERT (frameType.isPrimitiveType<SampleType>());
    return createInterleavedView (sourceData, 1, numFrames);
}

//==============================================================================
/// Creates a ValueView for an array of vectors that represents the given 2D buffer.
/// @see createInterleavedViewFromValue()
template <typename SampleType>
inline choc::value::ValueView createValueViewFromBuffer (const InterleavedView<SampleType>& source)
{
    return choc::value::create2DArrayView (source.data.data, source.getNumFrames(), source.getNumChannels());
}


} // namespace choc::buffer

#endif
