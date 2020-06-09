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

namespace tracktion_graph
{

/**
    Plays back a node with mutiple threads.
*/
class MultiThreadedNodePlayer
{
public:
    MultiThreadedNodePlayer (std::unique_ptr<Node> node)
        : rootNode (std::move (node))
    {
    }
    
    ~MultiThreadedNodePlayer()
    {
        clearThreads();
    }
    
    Node& getNode()
    {
        return *rootNode;
    }
    
    void setNode (std::unique_ptr<Node> newNode)
    {
        clearThreads();

        auto oldNode = std::move (rootNode);
        rootNode = std::move (newNode);
        prepareToPlay (sampleRate, blockSize, oldNode.get());
    }

    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr)
    {
        sampleRate = sampleRateToUse;
        blockSize = blockSizeToUse;
        
        // First give the Nodes a chance to transform
        transformNodes (*rootNode);
        
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *rootNode, oldNode };
        visitNodes (*rootNode, [&] (Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes = tracktion_graph::getNodes (*rootNode, tracktion_graph::VertexOrdering::postordering);

        createThreads();
    }

    int process (const Node::ProcessContext& pc)
    {
        // Reset the stream range
        referenceSampleRange = pc.referenceSampleRange;
        
        // Prepare all the nodes to be played back
        for (auto node : allNodes)
            node->prepareForNextBlock (referenceSampleRange);

        // Then set the vector to be processed
        // Threads are always running so will process as soon numNodesLeftToProcess is non-zero
        numNodesLeftToProcess = allNodes.size();
        
        // Try to process Nodes until they're all processed
        for (;;)
        {
            if (! processNextFreeNode())
                break;
        }
        
        // Wait for any threads to finish processing
        while (! rootNode->hasProcessed())
            pause();

        auto output = rootNode->getProcessedOutput();
        pc.buffers.audio.copyFrom (output.audio);
        pc.buffers.midi.copyFrom (output.midi);
        
        return -1;
    }
    
private:
    //==============================================================================
    std::unique_ptr<Node> rootNode;
    std::vector<std::thread> threads;
    std::vector<Node*> allNodes;
    
    juce::Range<int64_t> referenceSampleRange;
    std::atomic<bool> threadsShouldExit { false };
    std::atomic<size_t> numNodesLeftToProcess { 0 };

    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    //==============================================================================
    void clearThreads()
    {
        threadsShouldExit = true;

        for (auto& t : threads)
            t.join();
        
        threads.clear();
    }
    
    void createThreads()
    {
        size_t numThreadsToUse = 0;
        
        for (auto node : allNodes)
            if (node->isReadyToProcess())
                ++numThreadsToUse;
        
        numThreadsToUse = std::min (numThreadsToUse, (size_t) std::thread::hardware_concurrency()) - 1;
             
        for (size_t i = 0; i < numThreadsToUse; ++i)
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
        size_t expectedNumNodesLeft = numNodesLeftToProcess;
        
        if (expectedNumNodesLeft == 0)
            return false;

        const size_t nodeToReserve = expectedNumNodesLeft - 1;

        if (numNodesLeftToProcess.compare_exchange_strong (expectedNumNodesLeft, nodeToReserve))
        {
            const size_t nodeIndex = allNodes.size() - nodeToReserve - 1;
            auto node = allNodes[nodeIndex];

            // Wait until this node is actually ready to be processed
            // It might be waiting for other Nodes
            while (! node->isReadyToProcess())
                pause();
            
            node->process (referenceSampleRange);
            
            return true;
        }
        
        return false;
    }
};

}
