/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion { inline namespace graph
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
    static std::unique_ptr<NodeGraph> prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph,
                                                     double sampleRate, int blockSize,
                                                     std::function<NodeBuffer (choc::buffer::Size)> allocateAudioBuffer = nullptr,
                                                     std::function<void (NodeBuffer&&)> deallocateAudioBuffer = nullptr)
    {
        if (node == nullptr)
            return {};
        
        // First give the Nodes a chance to transform
        auto nodeGraph = createNodeGraph (std::move (node));

        // Next, initialise all the nodes, this will call prepareToPlay on them
        const PlaybackInitialisationInfo info { sampleRate, blockSize,
                                                *nodeGraph, oldGraph,
                                                allocateAudioBuffer, deallocateAudioBuffer };

        for (auto n : nodeGraph->orderedNodes)
            n->initialise (info);

        return nodeGraph;
    }

    inline void reserveAudioBufferPool (Node* rootNode, const std::vector<Node*>& allNodes,
                                        AudioBufferPool& audioBufferPool, size_t numThreads, int blockSize)
    {
        if (rootNode == nullptr)
            return;

        // To find the number of buffers required:
        // - Find the maximum buffer::Size in the graph
        // - Multiply it by the maximum number of inputs any Node has
        // - Then multiply that by the number of threads that will be used (or the num leaf Nodes if that’s smaller)
        // - Add one for the root node so the ouput can be retained
        [[ maybe_unused ]] size_t maxNumChannels = 0, maxNumInputs = 0, numLeafNodes = 0;
        
        // However, this algorithm is too pessimistic as it assumes there can be
        // numThreads * maxNumInputs which is unlikely to be true.
        // It's probably better to stack up numThreads maxNumInputs and use the min of that size and numThreads

        for (auto n : allNodes)
        {
            const auto numInputs = n->getDirectInputNodes().size();
            const auto props = n->getNodeProperties();
            maxNumInputs    = std::max (maxNumInputs, numInputs);
            maxNumChannels  = std::max (maxNumChannels, (size_t) props.numberOfChannels);
            
            if (numInputs == 0)
                ++numLeafNodes;
        }
        
        const size_t numBuffersRequired = std::max ((size_t) 2, std::min (allNodes.size(), 1 + numThreads));
        audioBufferPool.reserve (numBuffersRequired, choc::buffer::Size::create (maxNumChannels, blockSize));
    }
}

}}
