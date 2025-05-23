/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <span>

namespace tracktion { inline namespace graph
{

namespace node_player_utils
{
    /** Returns true if all the nodes in this collection have a unique nodeID. */
    template<typename Collection>
    bool areNodeIDsUnique (Collection&& nodes, bool ignoreZeroIDs)
    {
        std::vector<size_t> nodeIDs;

        for (auto n : nodes)
            nodeIDs.push_back (n->getNodeProperties().nodeID);

        std::sort (nodeIDs.begin(), nodeIDs.end());

        if (ignoreZeroIDs)
            nodeIDs.erase (std::remove_if (nodeIDs.begin(), nodeIDs.end(),
                                           [] (auto nID) { return nID == 0; }),
                           nodeIDs.end());

        auto duplicateIDs = [] (auto input)
                            {
                                std::vector<size_t> duplicates;
                                std::unordered_set<size_t> seen;
                                seen.reserve (input.size());

                                for (auto num : input)
                                {
                                    if (seen.find (num) != seen.end())
                                        duplicates.push_back (num);
                                    else
                                        seen.insert (num);
                                }

                                return duplicates;
                            } (nodeIDs);

       #if JUCE_DEBUG
        if (! duplicateIDs.empty())
        {
            auto getNodeTypeStrings = [&nodes] (auto id)
            {
                std::string idStrings;

                for (auto n : nodes)
                    if (n->getNodeProperties().nodeID == id)
                        idStrings += std::string (typeid (*n).name()) += ", ";

                return idStrings;
            };

            for (auto id : duplicateIDs)
                DBG("\t" << id << ": " << getNodeTypeStrings (id));
        }
       #endif

        return duplicateIDs.empty();
    }

    /** Returns true if all the nodes in the graph have a unique nodeID. */
    static inline bool areNodeIDsUnique (Node& node, bool ignoreZeroIDs)
    {
        std::vector<Node*> nodes;
        visitNodes (node, [&] (Node& n) { nodes.push_back (&n); }, false);
        return areNodeIDsUnique (nodes, ignoreZeroIDs);
    }

    /** Returns true if all there are any feedback loops in the graph. */
    static inline bool areThereAnyCycles (const std::vector<Node*>& orderedNodes)
    {
        size_t numCycles = 0;

        // Iterate from the first node to the last
        // Find each input of the Node
        // Ensure that the input is in a lower position than the current
        for (auto iter = orderedNodes.begin(); iter != orderedNodes.end(); ++iter)
        {
            auto node = *iter;
            auto position = std::distance (orderedNodes.begin(), iter);

            for (auto inputNode : node->getDirectInputNodes())
            {
                const auto inputPosition = std::distance (orderedNodes.begin(),
                                                          std::find (orderedNodes.begin(), orderedNodes.end(), inputNode));

                if (inputPosition > position)
                    ++numCycles;
            }
        }

        return numCycles > 0;
    }

    /** Prepares a specific Node to be played and returns all the Nodes. */
    static std::unique_ptr<NodeGraph> prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph,
                                                     double sampleRate, int blockSize,
                                                     std::function<NodeBuffer (choc::buffer::Size)> allocateAudioBuffer = nullptr,
                                                     std::function<void (NodeBuffer&&)> deallocateAudioBuffer = nullptr,
                                                     bool nodeMemorySharingEnabled = false,
                                                     bool disableLatencyCompensation = false)
    {
        if (node == nullptr)
            return {};

        // First give the Nodes a chance to transform
        auto nodeGraph = createNodeGraph (std::move (node), disableLatencyCompensation);
        assert (! areThereAnyCycles (nodeGraph->orderedNodes));
        jassert (areNodeIDsUnique (nodeGraph->orderedNodes, true));

        // Next, initialise all the nodes, this will call prepareToPlay on them
        const PlaybackInitialisationInfo info { sampleRate, blockSize,
                                                *nodeGraph, oldGraph,
                                                allocateAudioBuffer, deallocateAudioBuffer,
                                                nodeMemorySharingEnabled };

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
