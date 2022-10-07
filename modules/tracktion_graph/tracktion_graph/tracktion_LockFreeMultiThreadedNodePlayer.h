/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

namespace tracktion { inline namespace graph
{
#include "../3rd_party/farbot/include/farbot/fifo.hpp"
}}

#ifdef _MSC_VER
 #pragma warning (pop)
#endif

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
/**
    Plays back a node with mutiple threads.
    The setting of Nodes and processing are all lock-free.
    This means the player can be real-time safe as long as your provided ThreadPool
    doesn't do any non-real-time operations in the wait/signal methods.
*/
class LockFreeMultiThreadedNodePlayer
{
private:
    //==============================================================================
    template<typename Type>
    class LockFreeFifo
    {
    public:
        LockFreeFifo (int capacity)
            : fifo (std::make_unique<farbot::fifo<Type>> (juce::nextPowerOfTwo (capacity)))
        {}

        bool try_enqueue (Type&& item)      { return fifo->push (std::move (item)); }
        bool try_dequeue (Type& item)       { return fifo->pop (item); }

    private:
        std::unique_ptr<farbot::fifo<Type>> fifo;
    };

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
        std::unique_ptr<LockFreeFifo<Node*>> nodesReadyToBeProcessed;
        std::unique_ptr<AudioBufferPool> audioBufferPool;
    };

public:
    //==============================================================================
    /**
        Base class for thread pools which can be customised to determine how
        cooperative threads should behave.
    */
    struct ThreadPool
    {
        /** Constructs a ThreadPool for a given LockFreeMultiThreadedNodePlayer. */
        ThreadPool (LockFreeMultiThreadedNodePlayer& p)
            : player (p)
        {
        }
        
        /** Destructor. */
        virtual ~ThreadPool() = default;
        
        /** Subclasses should implement this to create the given number of threads. */
        virtual void createThreads (size_t numThreads) = 0;
        
        /** Subclasses should implement this to clear all the threads. */
        virtual void clearThreads() = 0;

        /** Called by the player when a Node becomes available to process.
            Subclasses should use this to try and get a thread to call process as soon as possible.
        */
        virtual void signalOne() = 0;

        /** Called by the player when more than one Node becomes available to process.
            Subclasses should use this to try and get a thread to call process as soon as possible.
        */
        virtual void signal (int numToSignal) = 0;

        /** Called by the player when more than one Node becomes available to process.
            Subclasses should use this to try and get a thread to call process as soon as possible.
        */
        virtual void signalAll() = 0;
        
        /** Called by the player when the audio thread has no free Nodes to process.
            Subclasses should can use this to either spin, pause or wait until a Node does
            become free or isFinalNodeReady returns true.
        */
        virtual void waitForFinalNode() = 0;

        //==============================================================================
        /** Signals the pool that all the threads should exit. */
        void signalShouldExit()
        {
            threadsShouldExit = true;
            signalAll();
        }
        
        /** Signals the pool that all the threads should continue to run and not exit. */
        void resetExitSignal()
        {
            threadsShouldExit = false;
        }
        
        /** Returns true if all the threads should exit. */
        bool shouldExit() const
        {
            return threadsShouldExit.load (std::memory_order_acquire);
        }

        /** Returns true if there are no free Nodes to be processed and the calling
            thread should wait until there are Nodes ready.
        */
        bool shouldWait()
        {
            if (shouldExit())
                return false;
            
            return player.numNodesQueued == 0;
        }

        /** Returns true if all the Nodes have been processed. */
        bool isFinalNodeReady()
        {
            assert (currentPreparedNode);

            if (! currentPreparedNode.load()->graph)
                return true;

            return currentPreparedNode.load()->graph->rootNode->hasProcessed();
        }

        /** Process the next chain of Nodes.
            Returns true if at least one Node was processed, false if no Nodes were processed.
            You can use this to determine how long to pause/wait for before processing again.
        */
        bool process()
        {
            if (auto cpn = currentPreparedNode.load())
                return player.processNextFreeNode (*cpn);

            return false;
        }

        /** Sets the current PreparedNode in use. This should live as long as the threads are running once set. */
        void setCurrentNode (LockFreeMultiThreadedNodePlayer::PreparedNode* nodeInUse)
        {
            currentPreparedNode = nodeInUse;

            // If a nullptr is being set, you must stop the threads first.
            assert (nodeInUse || shouldExit());
        }
        
    private:
        LockFreeMultiThreadedNodePlayer& player;
        std::atomic<bool> threadsShouldExit { false };
        std::atomic<LockFreeMultiThreadedNodePlayer::PreparedNode*> currentPreparedNode { nullptr };
    };

    //==============================================================================
    /** Creates an empty LockFreeMultiThreadedNodePlayer. */
    LockFreeMultiThreadedNodePlayer();
    
    using ThreadPoolCreator = std::function<std::unique_ptr<ThreadPool> (LockFreeMultiThreadedNodePlayer&)>;
    
    /** Creates an empty LockFreeMultiThreadedNodePlayer with a specified ThreadPool type. */
    LockFreeMultiThreadedNodePlayer (ThreadPoolCreator);
    
    /** Destructor. */
    ~LockFreeMultiThreadedNodePlayer();
    
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

    /** Prepares the current Node to be played.
        Calling this will cause a drop in the output stream as the Node is re-prepared.
    */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse);

    /** Returns the current Node. */
    Node* getNode()
    {
        return rootNode;
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

    //==============================================================================
    /** Enables or disables the use on an AudioBufferPool to reduce memory consumption.
        Don't rely on this, it is a temporary method used for benchmarking and will go
        away in the future.
    */
    void enablePooledMemoryAllocations (bool);

private:
    //==============================================================================
    std::atomic<size_t> numThreadsToUse { std::max ((size_t) 0, (size_t) std::thread::hardware_concurrency() - 1) };
    juce::Range<int64_t> referenceSampleRange;
    choc::buffer::FrameCount numSamplesToProcess = 0;
    std::atomic<bool> threadsShouldExit { false }, useMemoryPool { false };

    std::unique_ptr<ThreadPool> threadPool;
    
    LockFreeObject<PreparedNode> preparedNodeObject;
    Node* rootNode = nullptr;
    NodeGraph* lastGraphPosted = nullptr;
    AudioBufferPool* lastAudioBufferPoolPosted = nullptr;

    std::atomic<size_t> numNodesQueued { 0 };

    //==============================================================================
    std::atomic<double> sampleRate { 44100.0 };
    int blockSize = 512;
    
    //==============================================================================
    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::unique_ptr<NodeGraph> prepareToPlay (std::unique_ptr<Node>, NodeGraph* oldGraph,
                                              double sampleRateToUse, int blockSizeToUse,
                                              AudioBufferPool*);

    //==============================================================================
    void clearThreads();
    void createThreads();
    void pause();

    //==============================================================================
    void postNewGraph (std::unique_ptr<NodeGraph>);

    //==============================================================================
    static void buildNodesOutputLists (PreparedNode&);
    void resetProcessQueue (PreparedNode&);
    Node* updateProcessQueueForNode (PreparedNode&, Node&);
    void processNode (PreparedNode&, Node&);

    //==============================================================================
    bool processNextFreeNode (PreparedNode&);
};

}}
