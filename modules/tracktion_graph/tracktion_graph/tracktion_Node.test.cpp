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

#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_NODE

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

using namespace test_utilities;

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("Node")
    {
        for (auto setup : getTestSetups())
        {
            MESSAGE ((juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N")).toStdString());

            // Sin
            {
                auto sinNode = std::make_unique<SinNode> (220.0f);

                auto testContext = createBasicTestContext (std::move (sinNode), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            // Sin cancelling
            {
                std::vector<std::unique_ptr<Node>> nodes;
                nodes.push_back (std::make_unique<SinNode> (220.0f));

                auto sinNode = std::make_unique<SinNode> (220.0f);
                auto invertedSinNode = std::make_unique<FunctionNode> (std::move (sinNode), [] (float s) { return -s; });
                nodes.push_back (std::move (invertedSinNode));

                auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));

                auto testContext = createBasicTestContext (std::move (sumNode), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 0.0f, 0.0f);
            }

            // This is just a check to ensure the following code compiles
            {
                float clipGain = 1.0f;
                std::vector<std::unique_ptr<Node>> trackOneClipNodes;
                trackOneClipNodes.push_back (std::make_unique<SinNode> (220.0f, 1));
                trackOneClipNodes.push_back (std::make_unique<SinNode> (220.0f, 1));

                auto trackOneNode = std::make_unique<SummingNode> (std::move (trackOneClipNodes));

                auto trackTwoClipNode = std::make_unique<SinNode> (220.0f, 1);
                auto trackTwoNode = std::make_unique<GainNode> (std::move (trackTwoClipNode), [clipGain] { return clipGain; });

                std::vector<std::unique_ptr<Node>> trackNodes;
                trackNodes.push_back (std::move (trackOneNode));
                trackNodes.push_back (std::move (trackTwoNode));
                auto mainOutput = std::make_unique<SummingNode> (std::move (trackNodes));
                juce::ignoreUnused (mainOutput);
            }

            // Sin octave
            {
                std::vector<std::unique_ptr<Node>> nodes;
                nodes.push_back (std::make_unique<SinNode> (220.0f));
                nodes.push_back (std::make_unique<SinNode> (440.0f));

                auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));
                auto node = std::make_unique<FunctionNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 0.885f, 0.5f);
            }

            // Sin send/return
            {
                auto sinLowerNode = std::make_unique<SinNode> (220.0f);
                auto sendNode = std::make_unique<SendNode> (std::move (sinLowerNode), 1);
                auto track1Node = std::make_unique<FunctionNode> (std::move (sendNode), [] (float) { return 0.0f; });

                auto sinUpperNode = std::make_unique<SinNode> (440.0f);
                auto silentNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
                auto track2Node = std::make_unique<ReturnNode> (std::move (silentNode), 1);

                auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            // Sin send/return different bus#
            {
                auto sinLowerNode = std::make_unique<SinNode> (220.0f);
                auto sendNode = std::make_unique<SendNode> (std::move (sinLowerNode), 1);
                auto track1Node = std::make_unique<FunctionNode> (std::move (sendNode), [] (float) { return 0.0f; });

                auto sinUpperNode = std::make_unique<SinNode> (440.0f);
                auto silentNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
                auto track2Node = std::make_unique<ReturnNode> (std::move (silentNode), 2);

                auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 0.0f, 0.0f);
            }

            // Sin send/return non-blocking
            {
                auto sinLowerNode = std::make_unique<SinNode> (220.0f);
                auto attenuatedSinLowerNode = std::make_unique<FunctionNode> (std::move (sinLowerNode), [] (float s) { return s * 0.25f; });
                auto track1Node = std::make_unique<SendNode> (std::move (attenuatedSinLowerNode), 1);

                auto sinUpperNode = std::make_unique<SinNode> (440.0f);
                auto attenuatedSinUpperNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float s) { return s * 0.5f; });
                auto track2Node = std::make_unique<ReturnNode> (std::move (attenuatedSinUpperNode), 1);

                auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 0.885f, 0.5f);
            }

            // Basic latency test cancelling sin
            {
                const double sampleRate = setup.sampleRate;
                const double sinFrequency = sampleRate / 100.0;
                const double numSamplesPerCycle = sampleRate / sinFrequency;
                const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

                std::vector<std::unique_ptr<Node>> nodes;
                nodes.push_back (std::make_unique<SinNode> ((float) sinFrequency));

                auto sinNode = makeNode<SinNode> ((float) sinFrequency);
                auto latencySinNode = makeNode<LatencyNode> (std::move (sinNode), numLatencySamples);
                nodes.push_back (std::move (latencySinNode));

                auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));

                auto testContext = createBasicTestContext (std::move (sumNode), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 1.0f, 0.707f, 0.0f, 0.0f);
            }

            // Basic latency test doubling sin
            {
                const double sampleRate = setup.sampleRate;
                const double sinFrequency = sampleRate / 100.0;
                const double numSamplesPerCycle = sampleRate / sinFrequency;
                const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

                std::vector<std::unique_ptr<Node>> nodes;
                nodes.push_back (makeGainNode (makeNode<SinNode> ((float) sinFrequency), 0.5f));

                auto sinNode = makeGainNode (makeNode<SinNode> ((float) sinFrequency), 0.5f);
                auto latencySinNode = makeNode<LatencyNode> (std::move (sinNode), numLatencySamples);
                nodes.push_back (std::move (latencySinNode));

                auto sumNode = makeNode<SummingNode> (std::move (nodes));

                auto testContext = createBasicTestContext (std::move (sumNode), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Send/return with latency
            {
                const double sinFrequency = setup.sampleRate / 100.0;
                const double numSamplesPerCycle = setup.sampleRate / sinFrequency;
                const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

                auto track1 = makeNode<SinNode> ((float) sinFrequency);
                track1 = makeNode<LatencyNode> (std::move (track1), numLatencySamples);
                track1 = makeGainNode (std::move (track1), 0.5f);
                track1 = makeNode<SendNode> (std::move (track1), 1);
                track1 = makeGainNode (std::move (track1), 0.0f);

                auto track2 = makeNode<SinNode> ((float) sinFrequency);
                track2 = makeGainNode (std::move (track2), 0.5f);
                track2 = makeNode<ReturnNode> (std::move (track2), 1);

                auto node = makeSummingNode ({ track1.release(), track2.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Multiple send/return with latency
            {
                const double sinFrequency = setup.sampleRate / 100.0;
                const double numSamplesPerCycle = setup.sampleRate / sinFrequency;
                const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

                auto track1 = makeNode<SinNode> ((float) sinFrequency);
                track1 = makeNode<LatencyNode> (std::move (track1), numLatencySamples);
                track1 = makeGainNode (std::move (track1), 0.5f);
                track1 = makeNode<SendNode> (std::move (track1), 1);
                track1 = makeGainNode (std::move (track1), 0.0f);

                auto track2 = makeNode<SinNode> ((float) sinFrequency);
                track2 = makeNode<LatencyNode> (std::move (track2), numLatencySamples * 2);
                track2 = makeGainNode (std::move (track2), 0.5f);
                track2 = makeNode<SendNode> (std::move (track2), 1);
                track2 = makeGainNode (std::move (track2), 0.0f);

                auto track3 = makeNode<SinNode> ((float) sinFrequency);
                track3 = makeGainNode (std::move (track3), 0.0f);
                track3 = makeNode<ReturnNode> (std::move (track3), 1);

                auto node = makeSummingNode ({ track1.release(), track2.release(), track3.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Send, send/return with two stage latency
            {
                const double sinFrequency = setup.sampleRate / 100.0;
                const double numSamplesPerCycle = setup.sampleRate / sinFrequency;
                const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

                auto track1 = makeNode<SinNode> ((float) sinFrequency);
                track1 = makeNode<LatencyNode> (std::move (track1), numLatencySamples);
                track1 = makeGainNode (std::move (track1), 0.5f);
                track1 = makeNode<SendNode> (std::move (track1), 1);
                track1 = makeNode<LatencyNode> (std::move (track1), numLatencySamples);
                track1 = makeNode<SendNode> (std::move (track1), 2);
                track1 = makeGainNode (std::move (track1), 0.0f);

                auto track2 = makeNode<SilentNode> (1);
                track2 = makeNode<ReturnNode> (std::move (track2), 1);

                auto track3 = makeNode<SilentNode> (1);
                track3 = makeNode<ReturnNode> (std::move (track3), 2);

                auto node = makeSummingNode ({ track1.release(), track2.release(), track3.release() });

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Basic MIDI
            {
                const double sampleRate = 44100.0;
                const double duration = 5.0;
                const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, setup.random);

                auto node = std::make_unique<MidiNode> (sequence);

                auto testContext = createBasicTestContext (std::move (node), setup, 1, duration);

                CHECK_GT (sequence.getNumEvents(), 0);
                test_utilities::expectMidiBuffer (testContext->midi, sampleRate, sequence);
            }

            // Delayed MIDI
            {
                const double sampleRate = 44100.0;
                const double duration = 5.0;
                const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, setup.random);
                CHECK_GT (sequence.getNumEvents(), 0);

                const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
                const double delayedTime = latencyNumSamples / sampleRate;
                auto midiNode = makeNode<MidiNode> (sequence);
                auto delayedNode = makeNode<LatencyNode> (std::move (midiNode), latencyNumSamples);

                auto testContext = createBasicTestContext (std::move (delayedNode), setup, 1, duration);

                auto extectedSequence = sequence;
                extectedSequence.addTimeToMessages (delayedTime);
                test_utilities::expectMidiBuffer (testContext->midi, sampleRate, extectedSequence);
            }

            // Compensated MIDI
            {
                const double sampleRate = 44100.0;
                const double duration = 5.0;
                const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, setup.random);
                CHECK_GT (sequence.getNumEvents(), 0);

                const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
                const double delayedTime = latencyNumSamples / sampleRate;

                auto sinNode = makeNode<SinNode> (220.0f);
                auto delayedNode = makeNode<LatencyNode> (std::move (sinNode), latencyNumSamples);

                auto midiNode = makeNode<MidiNode> (sequence);
                auto summedNode = makeSummingNode ({ delayedNode.release(), midiNode.release() });

                auto testContext = createBasicTestContext (std::move (summedNode), setup, 1, duration);

                auto extectedSequence = sequence;
                extectedSequence.addTimeToMessages (delayedTime);
                test_utilities::expectMidiBuffer (testContext->midi, sampleRate, extectedSequence);
            }

            // Send/return MIDI
            {
                const double sampleRate = 44100.0;
                const double duration = 5.0;
                const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, setup.random);
                const int busNum = 1;

                auto track1 = makeNode<MidiNode> (sequence);
                track1 = makeNode<SendNode> (std::move (track1), busNum);
                track1 = makeNode<FunctionNode> (std::move (track1), [] (float) { return 0.0f; });

                auto track2 = makeNode<ReturnNode> (makeNode<SinNode> (220.0f), busNum);

                auto sumNode = makeSummingNode ({ track1.release(), track2.release() });

                auto testContext = createBasicTestContext (std::move (sumNode), setup, 1, duration);

                CHECK_GT (sequence.getNumEvents(), 0);
                test_utilities::expectMidiBuffer (testContext->midi, sampleRate, sequence);
            }

            // Send/return MIDI passthrough
            {
                const double sampleRate = 44100.0;
                const double duration = 5.0;
                const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, setup.random);
                const int busNum = 1;

                auto track1 = makeNode<MidiNode> (sequence);
                track1 = makeNode<SendNode> (std::move (track1), busNum);

                auto track2 = makeNode<ReturnNode> (makeNode<SinNode> (220.0f), busNum);
                track2 = makeNode<FunctionNode> (std::move (track2), [] (float) { return 0.0f; });

                auto sumNode = makeSummingNode ({ track1.release(), track2.release() });

                auto testContext = createBasicTestContext (std::move (sumNode), setup, 1, duration);

                CHECK_GT (sequence.getNumEvents(), 0);
                test_utilities::expectMidiBuffer (testContext->midi, sampleRate, sequence);
            }

            // Stereo sin
            {
                auto node = makeNode<SinNode> (220.0f, 2);

                auto testContext = createBasicTestContext (std::move (node), setup, 2, 5.0);
                auto& buffer = testContext->buffer;

                CHECK (std::abs (buffer.getMagnitude (0, 0, buffer.getNumSamples()) - 1.0f) <= 0.001f);
                CHECK (std::abs (buffer.getRMSLevel (0, 0, buffer.getNumSamples()) - 0.707f) <= 0.001f);
            }

            // Sin rebuild
            {
                const double totalDuration = 5.0;
                const int totalNumSamples = (int) std::floor (totalDuration * setup.sampleRate);
                TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::make_unique<SinNode> (220.0f)),
                                                       setup, 1, totalDuration, true);
                const int firstHalfNumSamples = totalNumSamples / 2;

                playerContext.process (firstHalfNumSamples);
                auto testContext = playerContext.getTestResult();
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples);

                playerContext.setNode (std::make_unique<SinNode> (220.0f));
                const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
                playerContext.process (secondHalfNumSamples);
                testContext = playerContext.getTestResult();

                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            // Sin with latency rebuild, non-replacing
            {
                const int latencyNumSamples = (int) std::floor (setup.sampleRate / 2.0);
                auto makeSinNode = [latencyNumSamples]
                {
                    size_t nodeID = 1234;
                    return makeNode<LatencyNode> (makeNode<SinNode> (220.0f, 1, nodeID), latencyNumSamples);
                };

                const double totalDuration = 5.0;
                const int totalNumSamples = (int) std::floor (totalDuration * setup.sampleRate);
                auto node = makeSinNode();
                const size_t expectedNodeID = node->getNodeProperties().nodeID;
                TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::move (node)),
                                                       setup, 1, totalDuration, true);
                const int firstHalfNumSamples = totalNumSamples / 2;

                playerContext.process (firstHalfNumSamples);
                auto testContext = playerContext.getTestResult();
                test_utilities::expectAudioBuffer (testContext->buffer, 0, latencyNumSamples,
                                                   0.0f, 0.0f, 1.0f, 0.707f);
                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples);

                node = makeSinNode();
                CHECK_EQ (uint64_t (node->getNodeProperties().nodeID), uint64_t (expectedNodeID));
                playerContext.setPlayer (std::make_unique<NodePlayer> (std::move (node)));
                const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
                playerContext.process (secondHalfNumSamples);
                testContext = playerContext.getTestResult();

                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
                test_utilities::expectAudioBuffer (testContext->buffer, 0,
                                                   juce::Range<int>::withStartAndLength (firstHalfNumSamples, latencyNumSamples),
                                                   0.0f, 0.0f);
                test_utilities::expectAudioBuffer (testContext->buffer, 0,
                                                   juce::Range<int>::withStartAndLength (firstHalfNumSamples + latencyNumSamples,
                                                                                         secondHalfNumSamples - latencyNumSamples),
                                                   1.0f, 0.707f);
            }

            // Sin with latency rebuild, replacing
            {
                const int latencyNumSamples = (int) std::floor (setup.sampleRate / 2.0);
                auto makeSinNode = [latencyNumSamples]
                {
                    size_t nodeID = 1234;
                    return makeNode<LatencyNode> (makeNode<SinNode> (220.0f, 1, nodeID), latencyNumSamples);
                };

                const double totalDuration = 5.0;
                const int totalNumSamples = (int) std::floor (totalDuration * setup.sampleRate);
                auto node = makeSinNode();
                const size_t expectedNodeID = node->getNodeProperties().nodeID;
                TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::move (node)),
                                                       setup, 1, totalDuration, true);
                const int firstHalfNumSamples = totalNumSamples / 2;

                playerContext.process (firstHalfNumSamples);
                auto testContext = playerContext.getTestResult();
                test_utilities::expectAudioBuffer (testContext->buffer, 0, latencyNumSamples,
                                                   0.0f, 0.0f, 1.0f, 0.707f);
                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples);

                node = makeSinNode();
                test_utilities::expectUniqueNodeIDs (*node, false);
                CHECK_EQ (uint64_t (node->getNodeProperties().nodeID), uint64_t (expectedNodeID));
                playerContext.setNode (std::move (node));
                const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
                playerContext.process (secondHalfNumSamples);
                testContext = playerContext.getTestResult();

                CHECK_EQ (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, latencyNumSamples,
                                                   0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Cycles
            {
                auto node1 = makeNode<SinNode> (220.0f, 1);
                node1 = makeGainNode (std::move (node1), 0.333f);
                node1 = makeNode<ReturnNode> (std::move (node1), 2);
                node1 = makeNode<SendNode> (std::move (node1), 1);

                auto node2 = makeNode<SinNode> (220.0f, 1);
                node2 = makeGainNode (std::move (node2), 0.333f);
                node2 = makeNode<ReturnNode> (std::move (node2), 1);
                node2 = makeNode<SendNode> (std::move (node2), 2);

                auto node = makeSummingNode ({ node1.release(), node2.release() });

                CHECK_EQ (node->getNodeProperties().numberOfChannels, 1);

                auto testContext = createBasicTestContext (std::move (node), setup, 1, 5.0);
                expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }
        }
    }
}

} // namespace tracktion::inline graph

#endif
