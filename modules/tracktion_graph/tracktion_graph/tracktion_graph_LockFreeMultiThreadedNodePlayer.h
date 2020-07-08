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

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

#include "../3rd_party/concurrentqueue.h"

#ifdef _MSC_VER
 #pragma warning (pop)
#endif

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
    
    /** Destructor. */
    ~LockFreeMultiThreadedNodePlayer();
    
    /** Sets the number of threads to use for rendering.
        This can be 0 in which case only the process calling thread will be used for processing.
        N.B. this will pause processing whilst updating the threads so there will be a gap in the audio.
    */
    void setNumThreads (size_t);
    
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
    std::atomic<size_t> numThreadsToUse { std::max ((size_t) 0, (size_t) std::thread::hardware_concurrency() - 1) };
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
        std::atomic<bool> hasBeenQueued { true };
       #if JUCE_DEBUG
        std::atomic<bool> hasBeenDequeued { false };
       #endif
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
