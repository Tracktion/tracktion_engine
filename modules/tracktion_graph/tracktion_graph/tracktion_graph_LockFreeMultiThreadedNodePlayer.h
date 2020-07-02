/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <thread>
#include <emmintrin.h>
#include "../3rd_party/concurrentqueue.h"

namespace tracktion_graph
{

/**
    Plays back a node with mutiple threads in a lock free implementation.
*/
class LockFreeMultiThreadedNodePlayer
{
public:
    /** Creates an empty LockFreeMultiThreadedNodePlayer. */
    LockFreeMultiThreadedNodePlayer() = default;
    
    LockFreeMultiThreadedNodePlayer (std::unique_ptr<Node> node)
    {
        preparedNode.rootNode = (std::move (node));
    }
    
    ~LockFreeMultiThreadedNodePlayer()
    {
        clearThreads();
    }
    
    /** Sets the Node to process. */
    void setNode (std::unique_ptr<Node> newNode)
    {
        setNode (std::move (newNode), sampleRate, blockSize);
    }

    /** Sets the Node to process with a new sample rate and block size. */
    void setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
    {
        auto currentRoot = preparedNode.rootNode.get();
        auto newNodes = prepareToPlay (newNode.get(), currentRoot, sampleRateToUse, blockSizeToUse);
        setNewCurrentNode (std::move (newNode), newNodes);
    }

    /** Prepares the current Node to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr)
    {
        preparedNode.allNodes = prepareToPlay (preparedNode.rootNode.get(), oldNode, sampleRateToUse, blockSizeToUse);
    }

    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRateToUse, int blockSizeToUse)
    {
        if (threads.empty())
            createThreads();

        sampleRate = sampleRateToUse;
        blockSize = blockSizeToUse;
        
        return node_player_utils::prepareToPlay (node, oldNode, sampleRateToUse, blockSizeToUse);
    }

    /** Returns the current Node. */
    Node* getNode()
    {
        return rootNode;
    }

    int process (const Node::ProcessContext& pc)
    {
        updatePreparedNode();
        
        if (! preparedNode.rootNode)
            return -1;

        // Reset the stream range
        referenceSampleRange = pc.referenceSampleRange;
        
        // Prepare all the nodes to be played back
        for (auto node : preparedNode.allNodes)
            node->prepareForNextBlock (referenceSampleRange);

        // Then set the vector to be processed
        // Threads are always running so will process as soon numNodesLeftToProcess is non-zero
        resetProcessQueue (preparedNode);
//        numNodesLeftToProcess = preparedNode.allNodes.size();
        
        // Try to process Nodes until they're all processed
        for (;;)
        {
            if (! processNextFreeNode())
                break;
        }
        
        // Wait for any threads to finish processing
        while (! preparedNode.rootNode->hasProcessed())
            pause();

        auto output = preparedNode.rootNode->getProcessedOutput();
        pc.buffers.audio.copyFrom (output.audio);
        pc.buffers.midi.copyFrom (output.midi);

        return -1;
    }
    
    double getSampleRate() const
    {
        return sampleRate;
    }
    
private:
    //==============================================================================
    std::vector<std::thread> threads;
    juce::Range<int64_t> referenceSampleRange;
    std::atomic<bool> threadsShouldExit { false };
//    std::atomic<size_t> numNodesLeftToProcess { 0 };

    struct PlaybackNode
    {
        PlaybackNode (Node& n)
            : node (n) {}
        
        Node& node;
        std::vector<Node*> outputs;
        std::atomic<size_t> outputsProcessed { 0 };
    };
    
    struct PreparedNode
    {
        std::unique_ptr<Node> rootNode;
        std::vector<Node*> allNodes;
        std::vector<std::unique_ptr<PlaybackNode>> playbackNodes;
        moodycamel::ConcurrentQueue<Node*> nodesReadyToBeProcessed;
    };
    
    Node* rootNode = nullptr;
    PreparedNode preparedNode, pendingPreparedNodeStorage;
    std::atomic<PreparedNode*> pendingPreparedNode { nullptr };
    std::atomic<bool> isUpdatingPreparedNode { false };
    

    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    //==============================================================================
    void updatePreparedNode()
    {
        isUpdatingPreparedNode = true;
        
        if (auto newPreparedNode = pendingPreparedNode.exchange (nullptr))
            std::swap (preparedNode, *newPreparedNode);
        
        isUpdatingPreparedNode = false;
    }

    //==============================================================================
    static size_t getNumThreadsToUse()
    {
        return std::max ((size_t) 1, (size_t) std::thread::hardware_concurrency() - 1);
    }

    void clearThreads()
    {
        threadsShouldExit = true;

        for (auto& t : threads)
            t.join();
        
        threads.clear();
    }
    
    void createThreads()
    {
        for (size_t i = 0; i < getNumThreadsToUse(); ++i)
            threads.emplace_back ([this] { processNextFreeNodeOrWait(); });
    }

    inline void pause()
    {
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
    }

    //==============================================================================
    void setNewCurrentNode (std::unique_ptr<Node> newRoot, std::vector<Node*> newNodes)
    {
        while (isUpdatingPreparedNode)
            pause();
        
//ddd        std::stable_sort (newNodes.begin(), newNodes.end(),
//                          [] (auto n1, auto) { return n1->isReadyToProcess(); });
        
        pendingPreparedNode = nullptr;
        rootNode = newRoot.get();
        pendingPreparedNodeStorage.rootNode = std::move (newRoot);
        pendingPreparedNodeStorage.allNodes = std::move (newNodes);
        buildNodesOutputLists (pendingPreparedNodeStorage);
        
        pendingPreparedNode = &pendingPreparedNodeStorage;
    }
    
    static void buildNodesOutputLists (PreparedNode& preparedNode)
    {
        preparedNode.playbackNodes.clear();
        preparedNode.playbackNodes.reserve (preparedNode.allNodes.size());
        
        for (auto n : preparedNode.allNodes)
        {
            preparedNode.playbackNodes.push_back (std::make_unique<PlaybackNode> (*n));
            n->internal = preparedNode.playbackNodes.back().get();
        }
        
        // Iterate all nodes, for each input, add to the current Nodes output list
        for (auto& playbackNode : preparedNode.playbackNodes)
            for (auto inputNode : playbackNode->node.getDirectInputNodes())
                static_cast<PlaybackNode*> (inputNode->internal)->outputs.push_back (&playbackNode->node);
    }
    
    static void resetProcessQueue (PreparedNode& preparedNode)
    {
        // Clear the nodesReadyToBeProcessed list
        for (;;)
        {
            Node* temp;
            
            if (! preparedNode.nodesReadyToBeProcessed.try_dequeue (temp))
                break;
        }

        // Reset all the counters
        // And then move any Nodes that are ready to the correct queue
        for (auto& playbackNode : preparedNode.playbackNodes)
        {
            playbackNode->outputsProcessed = playbackNode->outputs.size();
            
            if (playbackNode->outputsProcessed == 0)
                preparedNode.nodesReadyToBeProcessed.enqueue (&playbackNode->node);
        }
    }

    static void updateProcessQueueForNode (PreparedNode& preparedNode, PlaybackNode& playbackNode)
    {
        for (auto output : playbackNode.outputs)
        {
            auto outputPlaybackNode = static_cast<PlaybackNode*> (output->internal);
            
            if (--outputPlaybackNode->outputsProcessed == 0)
            {
                jassert (outputPlaybackNode->node.isReadyToProcess());
                preparedNode.nodesReadyToBeProcessed.enqueue (&outputPlaybackNode->node);
            }
        }
    }

    //==============================================================================
    void processNextFreeNodeOrWait()
    {
        for (;;)
        {
            if (threadsShouldExit)
                return;
            
            if (! processNextFreeNode())
                pause();
        }
    }
    
    bool processNextFreeNode()
    {
        Node* nodeToProcess = nullptr;
        
        if (! preparedNode.nodesReadyToBeProcessed.try_dequeue (nodeToProcess))
            return false;
        
        nodeToProcess->process (referenceSampleRange);
        updateProcessQueueForNode (preparedNode, *(static_cast<PlaybackNode*> (nodeToProcess->internal)));
        
        return true;
    }

//    bool processNextFreeNode()
//    {
//        size_t expectedNumNodesLeft = numNodesLeftToProcess;
//
//        if (expectedNumNodesLeft == 0)
//            return false;
//
//        const size_t nodeToReserve = expectedNumNodesLeft - 1;
//
//        if (numNodesLeftToProcess.compare_exchange_strong (expectedNumNodesLeft, nodeToReserve))
//        {
//            const size_t nodeIndex = preparedNode.allNodes.size() - nodeToReserve - 1;
//            auto node = preparedNode.allNodes[nodeIndex];
//
//            // Wait until this node is actually ready to be processed
//            // It might be waiting for other Nodes
//            while (! node->isReadyToProcess())
//                pause();
//
//            node->process (referenceSampleRange);
//
//            return true;
//        }
//
//        return false;
//    }
};

}
