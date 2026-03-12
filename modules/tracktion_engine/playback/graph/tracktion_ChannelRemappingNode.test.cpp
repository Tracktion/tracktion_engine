/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if GRAPH_UNIT_TESTS_CHANNELREMAPPINGNODE

using namespace tracktion::graph;
using namespace tracktion::graph::test_utilities;

//==============================================================================
//==============================================================================
class ChannelRemappingNodeTests : public juce::UnitTest
{
public:
    ChannelRemappingNodeTests()
        : juce::UnitTest ("ChannelRemappingNode", "tracktion_graph")
    {
    }

    void runTest() override
    {
        runChannelMapTests();

        for (auto setup : getTestSetups (*this))
        {
            logMessage (juce::String ("Test setup: sample rate SR, block size BS")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize)));

            runExplicitMappingTests (setup);
        }
    }

private:
    //==============================================================================
    void runChannelMapTests()
    {
        beginTest ("ChannelMap - empty");
        {
            ChannelMap map;
            expect (map.isEmpty());
            expectEquals (map.getRequiredOutputChannels(), 0);
            expect (map.isIdentity()); // Empty map is trivially identity
        }

        beginTest ("ChannelMap - identity");
        {
            auto map = ChannelMap::identity (4);
            expect (! map.isEmpty());
            expectEquals (static_cast<int> (map.size()), 4);
            expect (map.isIdentity());
            expectEquals (map.getRequiredOutputChannels(), 4);
        }

        beginTest ("ChannelMap - mono to stereo");
        {
            auto map = ChannelMap::monoToStereo();
            expect (! map.isEmpty());
            expectEquals (static_cast<int> (map.size()), 2);
            expect (! map.isIdentity());
            expectEquals (map.getRequiredOutputChannels(), 2);
        }

        beginTest ("ChannelMap - duplicate to channels");
        {
            auto map = ChannelMap::duplicateToChannels (0, 6);
            expect (! map.isEmpty());
            expectEquals (static_cast<int> (map.size()), 6);
            expect (! map.isIdentity());
            expectEquals (map.getRequiredOutputChannels(), 6);
        }

        beginTest ("ChannelMap - custom remapping");
        {
            // Swap channels: 0->1, 1->0
            ChannelMap map ({ { 0, 1 }, { 1, 0 } });
            expect (! map.isIdentity());
            expectEquals (map.getRequiredOutputChannels(), 2);
        }

        beginTest ("ChannelMap - stereo to mono");
        {
            auto map = ChannelMap::stereoToMono();
            expect (! map.isEmpty());
            expectEquals (static_cast<int> (map.size()), 2);
            expect (! map.isIdentity());
            expectEquals (map.getRequiredOutputChannels(), 1);
        }

        beginTest ("ChannelMap::conversion - same count gives identity");
        {
            auto map = ChannelMap::conversion (2, 2);
            expect (map.isIdentity());
            expectEquals (static_cast<int> (map.size()), 2);
            expectEquals (map.getRequiredOutputChannels(), 2);
        }

        beginTest ("ChannelMap::conversion - mono to stereo");
        {
            auto map = ChannelMap::conversion (1, 2);
            auto ref = ChannelMap::monoToStereo();
            expectEquals (static_cast<int> (map.size()), static_cast<int> (ref.size()));
            expectEquals (map.getRequiredOutputChannels(), 2);

            for (size_t i = 0; i < map.entries.size(); ++i)
            {
                expectEquals (map.entries[i].first, ref.entries[i].first);
                expectEquals (map.entries[i].second, ref.entries[i].second);
            }
        }

        beginTest ("ChannelMap::conversion - stereo to mono");
        {
            auto map = ChannelMap::conversion (2, 1);
            auto ref = ChannelMap::stereoToMono();
            expectEquals (static_cast<int> (map.size()), static_cast<int> (ref.size()));
            expectEquals (map.getRequiredOutputChannels(), 1);

            for (size_t i = 0; i < map.entries.size(); ++i)
            {
                expectEquals (map.entries[i].first, ref.entries[i].first);
                expectEquals (map.entries[i].second, ref.entries[i].second);
            }
        }

        beginTest ("ChannelMap::conversion - general upmix 2 to 4");
        {
            auto map = ChannelMap::conversion (2, 4);
            // Should be: 0->0, 1->1, 1->2, 1->3
            expectEquals (static_cast<int> (map.size()), 4);
            expectEquals (map.getRequiredOutputChannels(), 4);
            expectEquals (map.entries[0].first, 0); expectEquals (map.entries[0].second, 0);
            expectEquals (map.entries[1].first, 1); expectEquals (map.entries[1].second, 1);
            expectEquals (map.entries[2].first, 1); expectEquals (map.entries[2].second, 2);
            expectEquals (map.entries[3].first, 1); expectEquals (map.entries[3].second, 3);
        }

        beginTest ("ChannelMap::conversion - general downmix 4 to 2");
        {
            auto map = ChannelMap::conversion (4, 2);
            // Should be: 0->0, 1->1, 2->1, 3->1
            expectEquals (static_cast<int> (map.size()), 4);
            expectEquals (map.getRequiredOutputChannels(), 2);
            expectEquals (map.entries[0].first, 0); expectEquals (map.entries[0].second, 0);
            expectEquals (map.entries[1].first, 1); expectEquals (map.entries[1].second, 1);
            expectEquals (map.entries[2].first, 2); expectEquals (map.entries[2].second, 1);
            expectEquals (map.entries[3].first, 3); expectEquals (map.entries[3].second, 1);
        }
    }

    //==============================================================================
    void runExplicitMappingTests (TestSetup ts)
    {
        beginTest ("Stereo sin - channel 0 to channel 1");
        {
            // Create a mono sin and remap it to channel 1
            auto sinNode = makeNode<SinNode> (220.0f, 1);

            ChannelMap map ({ { 0, 1 } });
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), std::move (map));

            expectEquals (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Channel 0 should be silent, channel 1 should have the sin
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getMagnitude (1, 0, buffer.getNumSamples()), 1.0f, 0.001f);
        }

        beginTest ("Mono to stereo duplication");
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), ChannelMap::monoToStereo());

            expectEquals (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Both channels should have the sin
            for (int channel : { 0, 1 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Stereo to mono summing");
        {
            // Create stereo sin at 0.5 gain, sum to mono
            auto sinNode = makeNode<SinNode> (220.0f, 2);
            sinNode = makeGainNode (std::move (sinNode), 0.5f);

            // Sum both channels to channel 0
            ChannelMap map ({ { 0, 0 }, { 1, 0 } });
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode), std::move (map));

            expectEquals (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            auto& buffer = testContext->buffer;

            // Result should be ~1.0 magnitude (0.5 + 0.5)
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
        }

        beginTest ("Mono to 6 channel duplication");
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode),
                                                        ChannelMap::duplicateToChannels (0, 6));

            expectEquals (node->getNodeProperties().numberOfChannels, 6);

            auto testContext = createBasicTestContext (std::move (node), ts, 6, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel = 0; channel < 6; ++channel)
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Mono to stereo via conversion factory");
        {
            auto sinNode = makeNode<SinNode> (220.0f, 1);
            auto node = makeNode<ChannelRemappingNode> (std::move (sinNode),
                                                        ChannelMap::conversion (1, 2));

            expectEquals (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), ts, 2, 5.0);
            auto& buffer = testContext->buffer;

            // Both channels should have the sin signal
            for (int channel : { 0, 1 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Phase cancellation - mono sins summed to mono");
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

            expectEquals (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            auto& buffer = testContext->buffer;

            // Should cancel to silence
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }
};

static ChannelRemappingNodeTests channelRemappingNodeTests;

#endif

}} // namespace tracktion { inline namespace engine
