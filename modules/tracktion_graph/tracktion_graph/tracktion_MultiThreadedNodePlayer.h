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

/**
    Plays back a node with mutiple threads.
    This uses a simpler internal mechanism than the LockFreeMultiThreadedNodePlayer but uses spin locks
    do do so so isn't completely wait-free.
 
    The thread pool uses a hybrid method of trying to keep processing threads spinning where possible but
    falling back to a condition variable if they spin for too long.
 
    This is mainly here to compare performance with the LockFreeMultiThreadedNodePlayer.
    @see LockFreeMultiThreadedNodePlayer
*/
class MultiThreadedNodePlayer
{
public:
    //==============================================================================
    /** Creates an empty MultiThreadedNodePlayer. */
    MultiThreadedNodePlayer();
    
    /** Destructor. */
    ~MultiThreadedNodePlayer();
    
    //==============================================================================
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
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse);

    /** Returns the current Node. */
    Node* getNode()
    {
        if (auto cpn = currentPreparedNode.load (std::memory_order_acquire);
            cpn != nullptr && cpn->graph != nullptr)
           return cpn->graph->rootNode.get();

        return nullptr;
    }

    /** Process a block of the Node. */
    int process (const Node::ProcessContext&);
    
    /** Clears the current Node.
        Note that this shouldn't be called concurrently with setNode.
        If it's called concurrently with process, it will block until the current process call has finished.
        This should be used sparingly as its a heavyweight operation which has to stop any
        processing threads and restart them again afterwrds.
    */
    void clearNode();

    /** Returns the current sample rate. */
    double getSampleRate() const
    {
        return sampleRate.load (std::memory_order_acquire);
    }

private:
    //==============================================================================
    std::atomic<size_t> numThreadsToUse { std::max ((size_t) 0, (size_t) std::thread::hardware_concurrency() - 1) };
    choc::buffer::FrameCount numSamplesToProcess;
    juce::Range<int64_t> referenceSampleRange;
    std::atomic<bool> threadsShouldExit { false };

    class ThreadPool;
    std::unique_ptr<ThreadPool> threadPool;
    
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
        std::unique_ptr<NodeGraph> graph;
        std::vector<std::unique_ptr<PlaybackNode>> playbackNodes;
        choc::fifo::MultipleReaderMultipleWriterFIFO<Node*> nodesReadyToBeProcessed;
    };
    
    RealTimeSpinLock preparedNodeMutex;
    std::unique_ptr<PreparedNode> preparedNode;
    std::atomic<PreparedNode*> currentPreparedNode { nullptr };

    std::atomic<size_t> numNodesQueued { 0 };
    RealTimeSpinLock clearNodesLock;

    //==============================================================================
    std::atomic<double> sampleRate { 44100.0 };
    int blockSize = 512;
    
    //==============================================================================
    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::unique_ptr<NodeGraph> prepareToPlay (std::unique_ptr<Node>, NodeGraph* oldGraph, double sampleRateToUse, int blockSizeToUse);

    //==============================================================================
    void clearThreads();
    void createThreads();
    static void pause();

    //==============================================================================
    void setNewGraph (std::unique_ptr<NodeGraph>);
    
    //==============================================================================
    static void buildNodesOutputLists (std::vector<Node*>&, std::vector<std::unique_ptr<PlaybackNode>>&);
    void resetProcessQueue();
    Node* updateProcessQueueForNode (Node&);
    void processNode (Node&);

    //==============================================================================
    bool processNextFreeNode();
};

}}
