/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_CHANNELREMAPPINGNODE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion { inline namespace engine
{

using namespace tracktion::graph;
using namespace tracktion::graph::test_utilities;

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("ChannelRemappingNode")
{
    // ChannelMap tests
    // ChannelMap - empty
    {
        ChannelMap map;
        CHECK (map.isEmpty());
        CHECK_EQ (map.getRequiredOutputChannels(), 0);
        CHECK (map.isIdentity()); // Empty map is trivially identity
    }

    // ChannelMap - identity
    {
        auto map = ChannelMap::identity (4);
        CHECK (! map.isEmpty());
        CHECK_EQ (static_cast<int> (map.size()), 4);
        CHECK (map.isIdentity());
        CHECK_EQ (map.getRequiredOutputChannels(), 4);
    }

    // ChannelMap - mono to stereo
    {
        auto map = ChannelMap::monoToStereo();
        CHECK (! map.isEmpty());
        CHECK_EQ (static_cast<int> (map.size()), 2);
        CHECK (! map.isIdentity());
        CHECK_EQ (map.getRequiredOutputChannels(), 2);
    }

    // ChannelMap - duplicate to channels
    {
        auto map = ChannelMap::duplicateToChannels (0, 6);
        CHECK (! map.isEmpty());
        CHECK_EQ (static_cast<int> (map.size()), 6);
        CHECK (! map.isIdentity());
        CHECK_EQ (map.getRequiredOutputChannels(), 6);
    }

    // ChannelMap - custom remapping
    {
        // Swap channels: 0->1, 1->0
        ChannelMap map ({ { 0, 1 }, { 1, 0 } });
        CHECK (! map.isIdentity());
        CHECK_EQ (map.getRequiredOutputChannels(), 2);
    }

    // ChannelMap - stereo to mono
    {
        auto map = ChannelMap::stereoToMono();
        CHECK (! map.isEmpty());
        CHECK_EQ (static_cast<int> (map.size()), 2);
        CHECK (! map.isIdentity());
        CHECK_EQ (map.getRequiredOutputChannels(), 1);
    }

    // ChannelMap::conversion - same count gives identity
    {
        auto map = ChannelMap::conversion (2, 2);
        CHECK (map.isIdentity());
        CHECK_EQ (static_cast<int> (map.size()), 2);
        CHECK_EQ (map.getRequiredOutputChannels(), 2);
    }

    // ChannelMap::conversion - mono to stereo
    {
        auto map = ChannelMap::conversion (1, 2);
        auto ref = ChannelMap::monoToStereo();
        CHECK_EQ (static_cast<int> (map.size()), static_cast<int> (ref.size()));
        CHECK_EQ (map.getRequiredOutputChannels(), 2);

        for (size_t i = 0; i < map.entries.size(); ++i)
        {
            CHECK_EQ (map.entries[i].source, ref.entries[i].source);
            CHECK_EQ (map.entries[i].dest, ref.entries[i].dest);
        }
    }

    // ChannelMap::conversion - stereo to mono
    {
        auto map = ChannelMap::conversion (2, 1);
        auto ref = ChannelMap::stereoToMono();
        CHECK_EQ (static_cast<int> (map.size()), static_cast<int> (ref.size()));
        CHECK_EQ (map.getRequiredOutputChannels(), 1);

        for (size_t i = 0; i < map.entries.size(); ++i)
        {
            CHECK_EQ (map.entries[i].source, ref.entries[i].source);
            CHECK_EQ (map.entries[i].dest, ref.entries[i].dest);
        }
    }

    // ChannelMap::conversion - general upmix 2 to 4
    {
        auto map = ChannelMap::conversion (2, 4);
        // Should be: 0->0, 1->1, 1->2, 1->3
        CHECK_EQ (static_cast<int> (map.size()), 4);
        CHECK_EQ (map.getRequiredOutputChannels(), 4);
        CHECK_EQ (map.entries[0].source, 0); CHECK_EQ (map.entries[0].dest, 0);
        CHECK_EQ (map.entries[1].source, 1); CHECK_EQ (map.entries[1].dest, 1);
        CHECK_EQ (map.entries[2].source, 1); CHECK_EQ (map.entries[2].dest, 2);
        CHECK_EQ (map.entries[3].source, 1); CHECK_EQ (map.entries[3].dest, 3);
    }

    // ChannelMap::conversion - general downmix 4 to 2
    {
        auto map = ChannelMap::conversion (4, 2);
        // Should be: 0->0, 1->1, 2->1, 3->1
        CHECK_EQ (static_cast<int> (map.size()), 4);
        CHECK_EQ (map.getRequiredOutputChannels(), 2);
        CHECK_EQ (map.entries[0].source, 0); CHECK_EQ (map.entries[0].dest, 0);
        CHECK_EQ (map.entries[1].source, 1); CHECK_EQ (map.entries[1].dest, 1);
        CHECK_EQ (map.entries[2].source, 2); CHECK_EQ (map.entries[2].dest, 1);
        CHECK_EQ (map.entries[3].source, 3); CHECK_EQ (map.entries[3].dest, 1);
    }

    // Explicit mapping tests
    for (auto setup : getTestSetups())
    {
        MESSAGE (juce::String ("Test setup: sample rate SR, block size BS")
                    .replace ("SR", juce::String (setup.sampleRate))
                    .replace ("BS", juce::String (setup.blockSize)).toRawUTF8());

        auto ts = setup;

        // Stereo sin - channel 0 to channel 1
        {
            // Create a mono sin and remap it to channel 1
            auto sinNode = makeNode<SinNode> (220.0f, 1);

            ChannelMap map ({ { 0, 1 } });
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), std::move (map));

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 2);

            [[maybe_unused]] auto testRandom = ts.random.nextInt();
            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Channel 0 should be silent, channel 1 should have the sin
            CHECK (std::abs (buffer.getMagnitude (0, 0, buffer.getNumSamples()) - 0.0f) <= 0.001f);
            CHECK (std::abs (buffer.getMagnitude (1, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
        }

        // Mono to stereo duplication
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), ChannelMap::monoToStereo());

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Both channels should have the sin
            for (int channel : { 0, 1 })
            {
                CHECK (std::abs (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
                CHECK (std::abs (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()) - 0.707f) <= 0.001f);
            }
        }

        // Stereo to mono summing
        {
            // Create stereo sin at 0.5 gain, sum to mono
            auto sinNode = makeNode<SinNode> (220.0f, 2);
            sinNode = makeGainNode (std::move (sinNode), 0.5f);

            // Sum both channels to channel 0
            ChannelMap map ({ { 0, 0 }, { 1, 0 } });
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), std::move (map));

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            auto& buffer = testContext->buffer;

            // Result should be ~1.0 magnitude (0.5 + 0.5)
            CHECK (std::abs (buffer.getMagnitude (0, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
        }

        // Mono to 6 channel duplication
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode),
                                                        ChannelMap::duplicateToChannels (0, 6));

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 6);

            auto testContext = createBasicTestContext (std::move (node), ts, 6, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel = 0; channel < 6; ++channel)
            {
                CHECK (std::abs (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
                CHECK (std::abs (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()) - 0.707f) <= 0.001f);
            }
        }

        // Mono to stereo via conversion factory
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode),
                                                        ChannelMap::conversion (1, 2));

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Both channels should have the sin signal
            for (int channel : { 0, 1 })
            {
                CHECK (std::abs (buffer.getMagnitude (channel, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
                CHECK (std::abs (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()) - 0.707f) <= 0.001f);
            }
        }

        // Phase cancellation - mono sins summed to mono
        {
            // Two mono sins with opposite phase that cancel
            auto leftNode = makeNode<SinNode> (220.0f, 1);

            auto rightNode = makeNode<SinNode> (220.0f, 1);
            rightNode = makeNode<FunctionNode> (std::move (rightNode), [] (float s) { return s * -1.0f; });

            // Remap right to channel 1
            rightNode = makeNode<ChannelRemappingNode> (std::move (rightNode),
                                                        ChannelMap ({ { 0, 1 } }));

            auto sumNode = makeSummingNode ({ leftNode.release(), rightNode.release() });

            // Now sum both channels to mono
            auto node = makeNode<ChannelRemappingNode> (std::move (sumNode),
                                                        ChannelMap ({ { 0, 0 }, { 1, 0 } }));

            CHECK_EQ (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            auto& buffer = testContext->buffer;

            // Should cancel to silence
            CHECK (std::abs (buffer.getMagnitude (0, 0, buffer.getNumSamples()) - 0.0f) <= 0.001f);
        }
    }
}

} // TEST_SUITE

}} // namespace tracktion { inline namespace engine

#endif
