/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
{

namespace node_player_utils
{
    /** Returns true if all the nodes in the graph have a unique nodeID. */
    static inline bool areNodeIDsUnique (Node& node, bool ignoreZeroIDs)
    {
        std::vector<size_t> nodeIDs;
        visitNodes (node, [&] (Node& n) { nodeIDs.push_back (n.getNodeProperties().nodeID); }, false);
        std::sort (nodeIDs.begin(), nodeIDs.end());

        if (ignoreZeroIDs)
            nodeIDs.erase (std::remove_if (nodeIDs.begin(), nodeIDs.end(),
                                           [] (auto nID) { return nID == 0; }),
                           nodeIDs.end());

        auto uniqueEnd = std::unique (nodeIDs.begin(), nodeIDs.end());
        return uniqueEnd == nodeIDs.end();
    }

    /** Prepares a specific Node to be played and returns all the Nodes. */
    static std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRate, int blockSize)
    {
        if (node == nullptr)
            return {};
        
        // First give the Nodes a chance to transform
        transformNodes (*node);
        
       #if JUCE_DEBUG
        // If we don't have unique IDs there can be problems copying internal buffers etc.
        const bool uniqueIDs = areNodeIDsUnique (*node, true);
        jassert (uniqueIDs);
       #endif

        // Next, initialise all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *node, oldNode };
        visitNodes (*node, [&] (Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        return tracktion_graph::getNodes (*node, tracktion_graph::VertexOrdering::postordering);
    }
}

}
