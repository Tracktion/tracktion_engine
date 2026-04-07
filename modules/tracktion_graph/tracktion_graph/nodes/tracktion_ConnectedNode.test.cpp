/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_CONNECTEDNODE

#include "tracktion_ConnectedNode.h"
#include "../../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("ConnectedNode")
    {
        for (auto ts : tracktion::graph::test_utilities::getTestSetups())
        {
            // Two sin waves
            {
                auto sinNode1 = std::make_shared<SinNode> (220.0f, 1);
                auto sinNode2 = std::make_shared<SinNode> (220.0f, 2);
                auto connectedNode = std::make_unique<ConnectedNode>();

                connectedNode->addAudioConnection (sinNode1, { 0, 0 });
                connectedNode->addAudioConnection (sinNode2, { 0, 0 });

                // Reduce by 0.5 to avoid clipping
                auto node = makeGainNode (std::move (connectedNode), 0.5f);

                // Ensure level is 1.0
                auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }

            // Two sin waves, one with latency
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
                test_utilities::expectAudioBuffer (testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
            }

            // Cycle
            {
                auto sinNode = std::make_shared<SinNode> (220.0f);
                auto connectedNode1 = std::make_shared<ConnectedNode>();
                auto connectedNode2 = std::make_shared<ConnectedNode>();
                auto node = makeNode<ForwardingNode> (connectedNode2);

                CHECK (connectedNode1->addAudioConnection (sinNode, { 0, 0 }));
                CHECK (connectedNode2->addAudioConnection (connectedNode1, { 0, 0 }));
                CHECK (! connectedNode1->addAudioConnection (connectedNode2, { 0, 0 }));

                // This should drop the cycle
                auto testContext = createBasicTestContext (std::move (node), ts, 1, 5.0);
                test_utilities::expectAudioBuffer (testContext->buffer, 0, 1.0f, 0.707f);
            }
        }
    }
}

}

#endif
