/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace graph
{

#if GRAPH_UNIT_TESTS_NODE

using namespace test_utilities;

//==============================================================================
//==============================================================================
class NodeTests : public juce::UnitTest
{
public:
    NodeTests()
        : juce::UnitTest ("Node", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto setup : getTestSetups (*this))
        {
            logMessage (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N"));

            // Mono tests
            runSinTests (setup);
            runSinCancellingTests (setup);
            runSinOctaveTests (setup);
            runSendReturnTests (setup);
            runLatencyTests (setup);

            // MIDI tests
            runMidiTests (setup);

            // Multi channel tests
            runStereoTests (setup);
            
            // Tests rebuilding the graph mid render
            runRebuildTests (setup);
            runCycleTests (setup);
        }
    }

private:
    //==============================================================================
    //==============================================================================
    void runSinTests (TestSetup testSetup)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinNode> (220.0f);
            
            auto testContext = createBasicTestContext (std::move (sinNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }
    }

    void runSinCancellingTests (TestSetup testSetup)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<Node>> nodes;
            nodes.push_back (std::make_unique<SinNode> (220.0f));

            auto sinNode = std::make_unique<SinNode> (220.0f);
            auto invertedSinNode = std::make_unique<FunctionNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));
            
            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
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
    }

    void runSinOctaveTests (TestSetup testSetup)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<Node>> nodes;
            nodes.push_back (std::make_unique<SinNode> (220.0f));
            nodes.push_back (std::make_unique<SinNode> (440.0f));

            auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));
            auto node = std::make_unique<FunctionNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runSendReturnTests (TestSetup testSetup)
    {
        beginTest ("Sin send/return");
        {
            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinNode> (220.0f);
            auto sendNode = std::make_unique<SendNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinNode> (440.0f);
            auto silentNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnNode> (std::move (silentNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }

        beginTest ("Sin send/return different bus#");
        {
            // This test is the same as before but uses a different bus number for the return so the output should be silent

            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinNode> (220.0f);
            auto sendNode = std::make_unique<SendNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinNode> (440.0f);
            auto silentNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnNode> (std::move (silentNode), 2);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
        }
        
        beginTest ("Sin send/return non-blocking");
        {
            // Track 1 sends a sin tone to a send with a gain of 0.25
            auto sinLowerNode = std::make_unique<SinNode> (220.0f);
            auto attenuatedSinLowerNode = std::make_unique<FunctionNode> (std::move (sinLowerNode), [] (float s) { return s * 0.25f; });
            auto track1Node = std::make_unique<SendNode> (std::move (attenuatedSinLowerNode), 1);

            // Track 2 has a sin source  of gain 0.5 and receives input from the send
            auto sinUpperNode = std::make_unique<SinNode> (440.0f);
            auto attenuatedSinUpperNode = std::make_unique<FunctionNode> (std::move (sinUpperNode), [] (float s) { return s * 0.5f; });
            auto track2Node = std::make_unique<ReturnNode> (std::move (attenuatedSinUpperNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingNode ({ track1Node.release(), track2Node.release() });
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runLatencyTests (TestSetup testSetup)
    {
        beginTest ("Basic latency test cancelling sin");
        {
            /*  Has two sin nodes at the same frequency and delays one of them by half a period
                so they should cancel out completely.
                This uses the test sample rate to determine the sin frequency to avoid rounding errors.
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<Node>> nodes;
            nodes.push_back (std::make_unique<SinNode> ((float) sinFrequency));

            auto sinNode = makeNode<SinNode> ((float) sinFrequency);
            auto latencySinNode = makeNode<LatencyNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<BasicSummingNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);

            // Start of buffer is +-1, after latency comp kicks in, the second half will be silent
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 1.0f, 0.707f, 0.0f, 0.0f);
        }

        beginTest ("Basic latency test doubling sin");
        {
            /*  This is the same test as before, two sin waves with one delayed but now the second one is compensated for.
                This has two implications:
                 1. There will be a half period of silence at the start of the output
                 2. Instead of cancelling, the sins will now constructively interfere, doubling the magnitude
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<Node>> nodes;
            nodes.push_back (makeGainNode (makeNode<SinNode> ((float) sinFrequency), 0.5f));

            auto sinNode = makeGainNode (makeNode<SinNode> ((float) sinFrequency), 0.5f);
            auto latencySinNode = makeNode<LatencyNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = makeNode<SummingNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Send/return with latency");
        {
            /*  This has a sin input to a latency node leading to a send.
                The return branch also has a sin input. The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
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

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Multiple send/return with latency");
        {
            /*  This two tracks with sin input to a latency node leading to a send. The latency is different on each branch.
                The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
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

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Send, send/return with two stage latency");
        {
            /*  This has a sin input to a latency node leading to a another latency block and another send on a different bus.
                There are then two other tracks that receive each of the send nodes.
                The latency should be compensated for and the output a mag 1 sin.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
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

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
    }
        
    void runMidiTests (TestSetup testSetup)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        
        // Avoid creating events at the end of the duration as they'll get lost after latency is applied
        const auto sequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, testSetup.random);
        
        beginTest ("Basic MIDI");
        {
            auto node = std::make_unique<MidiNode> (sequence);
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Delayed MIDI");
        {
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;
            auto midiNode = makeNode<MidiNode> (sequence);
            auto delayedNode = makeNode<LatencyNode> (std::move (midiNode), latencyNumSamples);

            auto testContext = createBasicTestContext (std::move (delayedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Compensated MIDI");
        {
            // This test has a sin node being delayed by a block which then gets mixed with a non-deleayed MIDI stream
            // The MIDI stream should be delayed by the same amount as the sin stream
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;

            auto sinNode = makeNode<SinNode> (220.0f);
            auto delayedNode = makeNode<LatencyNode> (std::move (sinNode), latencyNumSamples);
            
            auto midiNode = makeNode<MidiNode> (sequence);
            auto summedNode = makeSummingNode ({ delayedNode.release(), midiNode.release() });

            auto testContext = createBasicTestContext (std::move (summedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Send/return MIDI");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the original
            const int busNum = 1;
            
            auto track1 = makeNode<MidiNode> (sequence);
            track1 = makeNode<SendNode> (std::move (track1), busNum);
            track1 = makeNode<FunctionNode> (std::move (track1), [] (float) { return 0.0f; });

            auto track2 = makeNode<ReturnNode> (makeNode<SinNode> (220.0f), busNum);
            
            auto sumNode = makeSummingNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Send/return MIDI passthrough");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the return path
            const int busNum = 1;
            
            auto track1 = makeNode<MidiNode> (sequence);
            track1 = makeNode<SendNode> (std::move (track1), busNum);

            auto track2 = makeNode<ReturnNode> (makeNode<SinNode> (220.0f), busNum);
            track2 = makeNode<FunctionNode> (std::move (track2), [] (float) { return 0.0f; });

            auto sumNode = makeSummingNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
    }
    
    void runStereoTests (TestSetup testSetup)
    {
        beginTest ("Stereo sin");
        {
            auto node = makeNode<SinNode> (220.0f, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }

        beginTest ("Stereo sin");
        {
            // Two mono sin nodes summed to L/R stereo
            auto leftSin = makeNode<SinNode> (220.0f, 1);
            auto rightSin = makeNode<SinNode> (220.0f, 1);

            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 1 });
            auto rightRemapped = makeNode<ChannelRemappingNode> (std::move (rightSin), channelMap, true);

            auto node = makeSummingNode ({ leftSin.release(), rightRemapped.release() });

            expectEquals (node->getNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Stereo sin summed to mono");
        {
            // A stero sin node at 0.5 is summed to mono to produce a 1.0 mono sin
            auto node = makeNode<SinNode> (220.0f, 2);
            node = makeGainNode (std::move (node), 0.5f);

            // Merge channe 1 with channel 2
            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 0 });
            channelMap.push_back ({ 1, 0 });
            node = makeNode<ChannelRemappingNode> (std::move (node), channelMap, true);

            expectEquals (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
        
        beginTest ("Twin mono sin summed to mono cancelling");
        {
            // L/R sin with inverted phase that cancel
            auto leftNode = makeNode<SinNode> (220.0f, 1);

            auto rightNode = makeNode<SinNode> (220.0f, 1);
            rightNode = makeNode<FunctionNode> (std::move (rightNode), [] (float s) { return s * -1.0f; });
            rightNode = makeNode<ChannelRemappingNode> (std::move (rightNode), makeChannelMap ({ { 0, 1 } }), true);

            auto sumNode = makeSummingNode ({ leftNode.release(), rightNode.release() });

            // Merge channe 1 with channel 2
            auto node = makeNode<ChannelRemappingNode> (std::move (sumNode), makeChannelMap ({ { 0, 0 }, { 1, 0 } }), true);

            expectEquals (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Mono sin duplicated to 6 channel");
        {
            // Create a single mono sin and then copy that to 6 channels
            auto node = makeNode<SinNode> (220.0f, 1);
            node = makeNode<ChannelRemappingNode> (std::move (node),
                                                   makeChannelMap ({ { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 } }),
                                                   true);

            expectEquals (node->getNodeProperties().numberOfChannels, 6);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 6, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1, 2, 3, 4, 5 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }
    }
    
    void runRebuildTests (TestSetup testSetup)
    {
        beginTest ("Sin rebuild");
        {
            const double totalDuration = 5.0;
            const int totalNumSamples = (int) std::floor (totalDuration * testSetup.sampleRate);
            TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::make_unique<SinNode> (220.0f)),
                                                   testSetup, 1, totalDuration, true);
            const int firstHalfNumSamples = totalNumSamples / 2;
            
            playerContext.process (firstHalfNumSamples);
            auto testContext = playerContext.getTestResult();
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples);

            // Make a new sin node and switch that in to the test context
            playerContext.setNode (std::make_unique<SinNode> (220.0f));
            const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
            playerContext.process (secondHalfNumSamples);
            testContext = playerContext.getTestResult();
            
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }

        beginTest ("Sin with latency rebuild, non-replacing");
        {
            // This creates a sin node with 0.5s latency, processes 2.5s then re-creates the node and processes another 0.5s
            // This test uses setPlayer which doesn't pass on the old node to the new prepareToPlay method
            // so after replacing you'd expect 0.5s silence
            const int latencyNumSamples = (int) std::floor (testSetup.sampleRate / 2.0);
            auto makeSinNode = [latencyNumSamples]
            {
                size_t nodeID = 1234;
                return makeNode<LatencyNode> (makeNode<SinNode> (220.0f, 1, nodeID), latencyNumSamples);
            };
            
            const double totalDuration = 5.0;
            const int totalNumSamples = (int) std::floor (totalDuration * testSetup.sampleRate);
            auto node = makeSinNode();
            const size_t expectedNodeID = node->getNodeProperties().nodeID;
            TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::move (node)),
                                                   testSetup, 1, totalDuration, true);
            const int firstHalfNumSamples = totalNumSamples / 2;
            
            playerContext.process (firstHalfNumSamples);
            auto testContext = playerContext.getTestResult();
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples,
                                               0.0f, 0.0f, 1.0f, 0.707f);
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples);

            // Make a new sin node and switch that in to the test context
            node = makeSinNode();
            expectEquals (uint64_t (node->getNodeProperties().nodeID), uint64_t (expectedNodeID));
            playerContext.setPlayer (std::make_unique<NodePlayer> (std::move (node)));
            const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
            playerContext.process (secondHalfNumSamples);
            testContext = playerContext.getTestResult();
            
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0,
                                               juce::Range<int>::withStartAndLength (firstHalfNumSamples, latencyNumSamples),
                                               0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0,
                                               juce::Range<int>::withStartAndLength (firstHalfNumSamples + latencyNumSamples,
                                                                                     secondHalfNumSamples - latencyNumSamples),
                                               1.0f, 0.707f);
        }
        
        beginTest ("Sin with latency rebuild, replacing");
        {
            // This is the same as the previous text except that it uses setNode which passes the old
            // node in the prepareToPlay call so the latency buffer can be obtained from the old graph
            // and the 0.5s silence after swapping is avoided
            const int latencyNumSamples = (int) std::floor (testSetup.sampleRate / 2.0);
            auto makeSinNode = [latencyNumSamples]
            {
                size_t nodeID = 1234;
                return makeNode<LatencyNode> (makeNode<SinNode> (220.0f, 1, nodeID), latencyNumSamples);
            };
            
            const double totalDuration = 5.0;
            const int totalNumSamples = (int) std::floor (totalDuration * testSetup.sampleRate);
            auto node = makeSinNode();
            const size_t expectedNodeID = node->getNodeProperties().nodeID;
            TestProcess<NodePlayer> playerContext (std::make_unique<NodePlayer> (std::move (node)),
                                                   testSetup, 1, totalDuration, true);
            const int firstHalfNumSamples = totalNumSamples / 2;
            
            playerContext.process (firstHalfNumSamples);
            auto testContext = playerContext.getTestResult();
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples,
                                               0.0f, 0.0f, 1.0f, 0.707f);
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples);

            // Make a new sin node and switch that in to the test context
            node = makeSinNode();
            test_utilities::expectUniqueNodeIDs (*this, *node, false);
            expectEquals (uint64_t (node->getNodeProperties().nodeID), uint64_t (expectedNodeID));
            playerContext.setNode (std::move (node));
            const int secondHalfNumSamples = totalNumSamples - firstHalfNumSamples;
            playerContext.process (secondHalfNumSamples);
            testContext = playerContext.getTestResult();
            
            expectEquals (testContext->buffer.getNumSamples(), firstHalfNumSamples + secondHalfNumSamples);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, latencyNumSamples,
                                               0.0f, 0.0f, 1.0f, 0.707f);
        }
    }
    
    void runCycleTests (TestSetup testSetup)
    {
        beginTest ("Cycles");
        {
            // This graph has a cycle in it, it should still render with an output of 1,
            // the second sin will not get sent to the return
            auto node1 = makeNode<SinNode> (220.0f, 1);
            node1 = makeGainNode (std::move (node1), 0.333f);
            node1 = makeNode<ReturnNode> (std::move (node1), 2);
            node1 = makeNode<SendNode> (std::move (node1), 1);

            auto node2 = makeNode<SinNode> (220.0f, 1);
            node2 = makeGainNode (std::move (node2), 0.333f);
            node2 = makeNode<ReturnNode> (std::move (node2), 1);
            node2 = makeNode<SendNode> (std::move (node2), 2);

            auto node = makeSummingNode ({ node1.release(), node2.release() });

            expectEquals (node->getNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }
    }
};

static NodeTests nodeTests;

#endif

}}
