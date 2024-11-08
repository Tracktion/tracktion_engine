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

#ifndef CHOC_SINC_INTERPOLATE_HEADER_INCLUDED
#define CHOC_SINC_INTERPOLATE_HEADER_INCLUDED

#include <cmath>
#include "choc_SampleBuffers.h"

namespace choc::interpolation
{

/**
    Resamples the data from a choc::buffer::InterleavedView or choc::buffer::ChannelArrayView
    into a destination view, using a sinc interpolation algorithm.

    The source and destination buffers are expected to be different sizes, and the data will
    be resampled to fit the destination, which may involve up- or down-sampling depending on
    the sizes.

    The number of zero-crossings determines the quality, and larger numbers will be increasingly
    slow to calculate, with diminishing returns. A value of around 50 should be good enough for
    any normal purpose.
*/
template <typename DestBufferOrView, typename SourceBufferOrView, uint32_t numZeroCrossings = 50>
void sincInterpolate (DestBufferOrView&& destBuffer, const SourceBufferOrView& sourceBuffer)
{
    using Sample = typename std::remove_reference<DestBufferOrView>::type::Sample;
    using MonoBufferView = choc::buffer::MonoView<Sample>;

    constexpr auto floatZeroCrossings = static_cast<Sample> (numZeroCrossings);
    constexpr auto floatPi = static_cast<Sample> (3.141592653589793238);
    constexpr auto half = static_cast<Sample> (0.5);
    constexpr auto one = static_cast<Sample> (1);

    struct InterpolationFunctions
    {
        static void resampleMono (MonoBufferView destBuffer, MonoBufferView sourceBuffer)
        {
            auto destSize = destBuffer.getNumFrames();
            auto sourceSize = sourceBuffer.getNumFrames();

            if (destSize > sourceSize)
            {
                resampleMono (destBuffer, sourceBuffer, 1.0f);
            }
            else
            {
                choc::buffer::MonoBuffer<Sample> bandlimitedIntermediate (1, sourceSize);
                resampleMono (bandlimitedIntermediate, sourceBuffer, static_cast<float> (destSize) / static_cast<float> (sourceSize));
                resampleMono (destBuffer, bandlimitedIntermediate, 1.0f);
            }
        }

        static void resampleMono (MonoBufferView destBuffer, MonoBufferView sourceBuffer, float ratio) noexcept
        {
            CHOC_ASSERT (sourceBuffer.data.stride == 1);
            auto numFrames = destBuffer.getNumFrames();
            double sourcePos = 0;
            auto sourceStride = sourceBuffer.getNumFrames() / static_cast<double> (numFrames);
            auto dest = destBuffer.data.data;
            auto destStride = destBuffer.data.stride;

            for (decltype (numFrames) i = 0; i < numFrames; ++i)
            {
                *dest = calculateSample (sourceBuffer, sourcePos, ratio);
                dest += destStride;
                sourcePos += sourceStride;
            }
        }

        static Sample sincWindowFunction (Sample position)
        {
            return std::sin (position)
                    * (half + half * std::cos (position * (one / floatZeroCrossings)))
                    / position;
        }

        static Sample getSincWindowLevelAt (Sample position)
        {
            if (position == Sample())
                return one;

            if (position < -floatZeroCrossings || position > floatZeroCrossings)
                return {};

            return sincWindowFunction (position * floatPi);
        }

        static Sample calculateSample (MonoBufferView source, double position, float ratio) noexcept
        {
            auto sourcePosition = static_cast<int> (position);
            auto fractionalOffset = static_cast<float> (position - static_cast<double> (sourcePosition));

            if (fractionalOffset > 0)
            {
                fractionalOffset = one - fractionalOffset;
                ++sourcePosition;
            }

            auto data = source.data.data;
            auto numSourceFrames = source.getNumFrames();

            Sample total = {};
            auto numCrossings = static_cast<int> (floatZeroCrossings / ratio);

            for (int i = -numCrossings; i <= numCrossings; ++i)
            {
                auto sourceIndex = static_cast<uint32_t> (sourcePosition + i);

                if (sourceIndex < numSourceFrames)
                {
                    auto windowPos = static_cast<Sample> (fractionalOffset + (ratio * static_cast<float> (i)));
                    total += getSincWindowLevelAt (windowPos) * data[sourceIndex];
                }
            }

            return total * ratio;
        }
    };

    // The source and dest must have the same number of channels
    auto numChans = destBuffer.getNumChannels();
    CHOC_ASSERT (sourceBuffer.getNumChannels() == numChans);

    if (! destBuffer.getSize().isEmpty())
    {
        if (destBuffer.getNumFrames() != sourceBuffer.getNumFrames())
        {
            for (decltype (numChans) i = 0; i < numChans; ++i)
                InterpolationFunctions::resampleMono (destBuffer.getChannel(i), sourceBuffer.getChannel(i));
        }
        else
        {
            copy (destBuffer, sourceBuffer);
        }
    }
}

} // namespace choc::audio

#endif
