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
    
    LockFreeMultiThreadedNodePlayer (std::unique_ptr<Node>);
    
    /** Destructor. */
    ~LockFreeMultiThreadedNodePlayer();
    
    /** Sets the Node to process. */
    void setNode (std::unique_ptr<Node>);

    /** Sets the Node to process with a new sample rate and block size. */
    void setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse);

    /** Prepares the current Node to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr);

    /** Returns the current Node. */
    Node* getNode()
    {
        return rootNode;
    }

    /** Process a block of the Node. */
    int process (const Node::ProcessContext&);
    
    /** Returns the current sample rate. */
    double getSampleRate() const
    {
        return sampleRate;
    }
    
private:
    //==============================================================================
    std::vector<std::thread> threads;
    juce::Range<int64_t> referenceSampleRange;
    std::atomic<bool> threadsShouldExit { false };

    struct PlaybackNode
    {
        PlaybackNode (Node& n)
            : node (n), numInputs (node.getDirectInputNodes().size())
        {}
        
        Node& node;
        const size_t numInputs;
        std::vector<Node*> outputs;
        std::atomic<size_t> numInputsToBeProcessed { 0 };
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
    std::atomic<size_t> numNodesQueued { 0 };

    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    //==============================================================================
    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRateToUse, int blockSizeToUse);

    //==============================================================================
    void updatePreparedNode();

    //==============================================================================
    static size_t getNumThreadsToUse();
    void clearThreads();
    void createThreads();
    void pause();

    //==============================================================================
    void setNewCurrentNode (std::unique_ptr<Node> newRoot, std::vector<Node*> newNodes);
    
    //==============================================================================
    static void buildNodesOutputLists (PreparedNode&);
    void resetProcessQueue();
    void updateProcessQueueForNode (Node&);

    //==============================================================================
    void processNextFreeNodeOrWait();
    bool processNextFreeNode();
};

}
