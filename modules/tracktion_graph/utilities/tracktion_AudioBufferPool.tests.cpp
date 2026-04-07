/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline graph {

} // namespace tracktion::inline graph

#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_AUDIOBUFFERPOOL

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("AudioBufferPool")
    {
        using namespace choc::buffer;

        SUBCASE ("Allocation")
        {
            const auto size = Size::create (2, 128);

            {
                AudioBufferPool pool;
                pool.setCapacity (1);
                CHECK_EQ ((int) pool.getNumBuffers(), 0);
                pool.release (ChannelArrayBuffer<float> (size));
                CHECK_EQ ((int) pool.getNumBuffers(), 1);
                CHECK_EQ ((int) pool.getAllocatedSize(), (int) SeparateChannelLayout<float>::getBytesNeeded (size));
            }

            {
                AudioBufferPool pool (1);
                CHECK_EQ ((int) pool.getNumBuffers(), 0);
                pool.release (ChannelArrayBuffer<float> (size));
                CHECK_EQ ((int) pool.getNumBuffers(), 1);
                CHECK_EQ ((int) pool.getAllocatedSize(), (int) SeparateChannelLayout<float>::getBytesNeeded (size));
            }

            {
                AudioBufferPool pool (2);
                pool.reserve (1, size);

                pool.release (ChannelArrayBuffer<float> (size));

                auto buffer = pool.allocate (size);
                const auto bufferSize = buffer.getSize();
                CHECK_GE ((int) bufferSize.numFrames, (int) size.numFrames);
                CHECK_GE ((int) bufferSize.numChannels, (int) size.numChannels);
                CHECK_EQ ((int) pool.getAllocatedSize(), (int) SeparateChannelLayout<float>::getBytesNeeded (size));

                CHECK_EQ ((int) pool.getNumBuffers(), 1);
                pool.release (std::move (buffer));
                CHECK_EQ ((int) pool.getNumBuffers(), 2);
            }
        }
    }
}

} // namespace tracktion::inline graph

#endif
