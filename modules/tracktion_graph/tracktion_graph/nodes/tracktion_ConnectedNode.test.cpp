/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if GRAPH_UNIT_TESTS_CONNECTEDNODE

#include "tracktion_ConnectedNode.h"

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
class ConnectedNodeTests : public juce::UnitTest
{
public:
    ConnectedNodeTests()
        : juce::UnitTest ("ConnectedNode", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto ts : tracktion::graph::test_utilities::getTestSetups (*this))
        {
            runBasicTests (ts);
        }
    }

private:
    //==============================================================================
    //==============================================================================
    void runBasicTests (test_utilities::TestSetup ts)
    {
        beginTest ("Two sin waves");
        {
            auto sinNode1 = std::make_shared<SinNode> (220.0f);
            auto sinNode2 = std::make_shared<SinNode> (220.0f);
            auto connectedNode = std::make_unique<ConnectedNode>();
            
            connectedNode->addAudioConnection (sinNode1, { 0, 0 });
            connectedNode->addAudioConnection (sinNode2, { 0, 0 });
            
            // Reduce by 0.5 to avoid clipping
            auto node = makeGainNode (std::move (connectedNode), 0.5f);

            // Ensure level is 1.0
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }

        beginTest ("Two sin waves, one with latency");
        {
            const double sampleRate = ts.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            auto sinNode1 = std::make_shared<SinNode> ((float) sinFrequency);
            auto sinNode2 = std::make_shared<LatencyNode> (makeNode<SinNode> ((float) sinFrequency), numLatencySamples);
            auto connectedNode = std::make_unique<ConnectedNode>();
            
            connectedNode->addAudioConnection (sinNode1, { 0, 0 });
            connectedNode->addAudioConnection (sinNode2, { 0, 0 });
            
            // Reduce by 0.5 to avoid clipping
            auto node = makeGainNode (std::move (connectedNode), 0.5f);

            // Start of buffer is +-1, after latency comp kicks in
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Cycle");
        {
            auto sinNode = std::make_shared<SinNode> (220.0f);
            auto connectedNode1 = std::make_shared<ConnectedNode>();
            auto connectedNode2 = std::make_shared<ConnectedNode>();
            auto node = makeNode<ForwardingNode> (connectedNode2);

            expect (connectedNode1->addAudioConnection (sinNode, { 0, 0 }));
            expect (connectedNode2->addAudioConnection (connectedNode1, { 0, 0 }));
            expect (! connectedNode1->addAudioConnection (connectedNode2, { 0, 0 }));

            // This should drop the cycle
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }
    }
};

static ConnectedNodeTests connectedNodeTests;

}}

#endif
