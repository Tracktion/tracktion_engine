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
#if JUCE_INTEL
 #include <emmintrin.h>
#endif

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
class MultiThreadedNodePlayer::ThreadPool
{
public:
    //==============================================================================
    ThreadPool (MultiThreadedNodePlayer& p)
        : player (p)
    {
    }

    void createThreads (size_t numThreads)
    {
        if (threads.size() == numThreads)
            return;

        resetExitSignal();

        for (size_t i = 0; i < numThreads; ++i)
        {
            threads.emplace_back ([this] { runThread(); });
            setThreadPriority (threads.back(), 10);
        }
    }

    void clearThreads()
    {
        signalShouldExit();

        for (auto& t : threads)
            t.join();

        threads.clear();
    }

    void signalOne()
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }
        
        condition.notify_one();
    }

    void signalAll()
    {
        {
            std::unique_lock<std::mutex> lock (mutex);
            triggered.store (true, std::memory_order_release);
        }

        condition.notify_all();
    }

    void wait()
    {
        thread_local int pauseCount = 0;

        if (shouldExit())
            return;

        if (shouldWait())
        {
            ++pauseCount;

            if (pauseCount < 25)
            {
                pause();
            }
            else if (pauseCount < 50)
            {
                std::this_thread::yield();
            }
            else
            {
                pauseCount = 0;

                // Fall back to locking
                std::unique_lock<std::mutex> lock (mutex);
                condition.wait (lock, [this] { return ! shouldWaitOrIsNotTriggered(); });
            }
        }
        else
        {
            pauseCount = 0;
        }
    }

    void waitForFinalNode()
    {
        pause();
    }

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

    /** Process the next chain of Nodes.
        Returns true if at least one Node was processed, false if no Nodes were processed.
        You can use this to determine how long to pause/wait for before processing again.
    */
    bool process()
    {
        return player.processNextFreeNode();
    }
    
private:
    //==============================================================================
    MultiThreadedNodePlayer& player;
    
    std::atomic<bool> threadsShouldExit { false };
    std::vector<std::thread> threads;
    mutable std::mutex mutex;
    mutable std::condition_variable condition;
    mutable std::atomic<bool> triggered { false };

    bool shouldWaitOrIsNotTriggered()
    {
        if (! triggered.load (std::memory_order_acquire))
            return false;
        
        return shouldWait();
    }
    
    void runThread()
    {
        for (;;)
        {
            if (shouldExit())
                return;

            if (! process())
                wait();
        }
    }
};


//==============================================================================
//==============================================================================
MultiThreadedNodePlayer::MultiThreadedNodePlayer()
{
    threadPool = std::make_unique<ThreadPool> (*this);
}

MultiThreadedNodePlayer::~MultiThreadedNodePlayer()
{
    clearThreads();
}

void MultiThreadedNodePlayer::setNumThreads (size_t newNumThreads)
{
    if (newNumThreads == numThreadsToUse)
        return;

    clearThreads();
    numThreadsToUse = newNumThreads;
    createThreads();
}

void MultiThreadedNodePlayer::setNode (std::unique_ptr<Node> newNode)
{
    setNode (std::move (newNode), getSampleRate(), blockSize);
}

void MultiThreadedNodePlayer::setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
{
    setNewGraph (prepareToPlay (std::move (newNode), preparedNode ? preparedNode->graph.get() : nullptr,
                                sampleRateToUse, blockSizeToUse));
}

void MultiThreadedNodePlayer::prepareToPlay (double sampleRateToUse, int blockSizeToUse)
{
    if (! preparedNode)
        return;

    // Don't pass in the old graph here as we're stealing the root from it
    setNewGraph (prepareToPlay (std::move (preparedNode->graph->rootNode), nullptr,
                                sampleRateToUse, blockSizeToUse));
}

int MultiThreadedNodePlayer::process (const Node::ProcessContext& pc)
{
    std::unique_lock<RealTimeSpinLock> tryLock (clearNodesLock, std::try_to_lock);
    
    if (! tryLock.owns_lock())
        return -1;

    if (getNode() == nullptr)
        return -1;

    const std::lock_guard<RealTimeSpinLock> lock (preparedNodeMutex);
    
    // Reset the stream range
    numSamplesToProcess = pc.numSamples;
    referenceSampleRange = pc.referenceSampleRange;

    // Prepare all the nodes to be played back
    for (auto node : preparedNode->graph->orderedNodes)
        node->prepareForNextBlock (referenceSampleRange);

    if (numThreadsToUse.load (std::memory_order_acquire) == 0)
    {
        for (auto node : preparedNode->graph->orderedNodes)
            node->process (numSamplesToProcess, referenceSampleRange);
    }
    else
    {
        // Reset the queue to be processed
        jassert (preparedNode->playbackNodes.size() == preparedNode->graph->orderedNodes.size());
        resetProcessQueue();

        // Try to process Nodes until the root is ready
        for (;;)
        {
            if (preparedNode->graph->rootNode->hasProcessed())
                break;

            if (! processNextFreeNode())
                threadPool->waitForFinalNode();
        }
    }

    // Add output from graph to buffers
    {
        auto output = preparedNode->graph->rootNode->getProcessedOutput();
        auto numAudioChannels = std::min (output.audio.getNumChannels(), pc.buffers.audio.getNumChannels());

        if (numAudioChannels > 0)
            add (pc.buffers.audio.getFirstChannels (numAudioChannels),
                 output.audio.getFirstChannels (numAudioChannels));

        pc.buffers.midi.mergeFrom (output.midi);
    }

    return -1;
}

void MultiThreadedNodePlayer::clearNode()
{
    std::lock_guard<RealTimeSpinLock> sl (clearNodesLock);
    
    // N.B. The threads will be trying to read the preparedNodes so we need to actually stop these first
    clearThreads();
    setNode (nullptr);
    createThreads();
    
    assert (preparedNode == nullptr);
    assert (getNode() == nullptr);
}

//==============================================================================
//==============================================================================
std::unique_ptr<NodeGraph> MultiThreadedNodePlayer::prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph,
                                                                   double sampleRateToUse, int blockSizeToUse)
{
    createThreads();

    sampleRate.store (sampleRateToUse, std::memory_order_release);
    blockSize = blockSizeToUse;

    return node_player_utils::prepareToPlay (std::move (node), oldGraph,
                                             sampleRateToUse, blockSizeToUse);
}

//==============================================================================
void MultiThreadedNodePlayer::clearThreads()
{
    threadPool->clearThreads();
}

void MultiThreadedNodePlayer::createThreads()
{
    threadPool->createThreads (numThreadsToUse.load());
}

inline void MultiThreadedNodePlayer::pause()
{
   #if JUCE_INTEL
    _mm_pause();
    _mm_pause();
   #else
    __asm__ __volatile__ ("yield");
    __asm__ __volatile__ ("yield");
   #endif
}

//==============================================================================
void MultiThreadedNodePlayer::setNewGraph (std::unique_ptr<NodeGraph> newGraph)
{
    if (! newGraph)
    {
        currentPreparedNode = {};
        preparedNode = {};
        return;
    }
    
    // Ensure the Nodes ready to be processed are at the front of the queue
    std::stable_sort (newGraph->orderedNodes.begin(), newGraph->orderedNodes.end(),
                      [] (auto n1, auto n2)
                      {
                          return n1->isReadyToProcess() && ! n2->isReadyToProcess();
                      });

    auto newPreparedNode = std::make_unique<PreparedNode>();
    newPreparedNode->graph = std::move (newGraph);
    newPreparedNode->nodesReadyToBeProcessed.reset (newPreparedNode->graph->orderedNodes.size());
    buildNodesOutputLists (newPreparedNode->graph->orderedNodes, newPreparedNode->playbackNodes);

    currentPreparedNode = newPreparedNode.get();

    // Then swap the storage under the lock so the old Node isn't being processed whilst it is deleted
    const std::lock_guard<RealTimeSpinLock> lock (preparedNodeMutex);
    preparedNode = std::move (newPreparedNode);
}

//==============================================================================
void MultiThreadedNodePlayer::buildNodesOutputLists (std::vector<Node*>& allNodes,
                                                     std::vector<std::unique_ptr<PlaybackNode>>& playbackNodes)
{
    playbackNodes.clear();
    playbackNodes.reserve (allNodes.size());

    // Create a PlaybackNode for each Node
    for (auto n : allNodes)
    {
       #if JUCE_DEBUG
        for (auto& pn : playbackNodes)
            jassert (&pn->node != n);
       #endif

        // Ensure no Nodes are present twice in the list
        jassert (std::count (allNodes.begin(), allNodes.end(), n) == 1);

        playbackNodes.push_back (std::make_unique<PlaybackNode> (*n));
        n->internal = playbackNodes.back().get();
    }

    // Iterate all Nodes, for each input, add to the current Nodes output list
    for (auto node : allNodes)
    {
        for (auto inputNode : node->getDirectInputNodes())
        {
            // Check the input is actually still in the graph
            jassert (std::find (allNodes.begin(), allNodes.end(), inputNode) != allNodes.end());
            static_cast<PlaybackNode*> (inputNode->internal)->outputs.push_back (node);
        }
    }
}

void MultiThreadedNodePlayer::resetProcessQueue()
{
    assert (preparedNode->nodesReadyToBeProcessed.getUsedSlots() == 0);
    preparedNode->nodesReadyToBeProcessed.reset();
    numNodesQueued.store (0, std::memory_order_release);

    // Reset all the counters
    // And then move any Nodes that are ready to the correct queue
    for (auto& playbackNode : preparedNode->playbackNodes)
    {
        jassert (playbackNode->hasBeenQueued);
        playbackNode->hasBeenQueued = false;
        playbackNode->numInputsToBeProcessed.store (playbackNode->numInputs, std::memory_order_release);

        // Check only ready nodes will be queued
       #if JUCE_DEBUG
        if (playbackNode->node.isReadyToProcess())
            jassert (playbackNode->numInputsToBeProcessed == 0);

        if (playbackNode->numInputsToBeProcessed == 0)
            jassert (playbackNode->node.isReadyToProcess());
       #endif
    }

   #if JUCE_DEBUG
    for (auto& playbackNode : preparedNode->playbackNodes)
    {
        playbackNode->hasBeenDequeued = false;
        jassert (! playbackNode->hasBeenQueued);
        jassert (playbackNode->numInputsToBeProcessed == playbackNode->numInputs);
        jassert (playbackNode->numInputsToBeProcessed.load (std::memory_order_acquire) == playbackNode->numInputs);
    }
   #endif

    size_t numNodesJustQueued = 0;

    // Make sure the counters are reset for all nodes before queueing any
    for (auto& playbackNode : preparedNode->playbackNodes)
    {
        if (playbackNode->numInputsToBeProcessed.load (std::memory_order_acquire) == 0)
        {
            jassert (! playbackNode->hasBeenQueued);
            playbackNode->hasBeenQueued = true;
            preparedNode->nodesReadyToBeProcessed.push (&playbackNode->node);
            ++numNodesJustQueued;
        }
    }

    // Make sure this is only incremented after all the nodes have been queued
    // or the threads will start queueing Nodes at the same time
    numNodesQueued += numNodesJustQueued;
    threadPool->signalAll();
}

Node* MultiThreadedNodePlayer::updateProcessQueueForNode (Node& node)
{
    auto playbackNode = static_cast<PlaybackNode*> (node.internal);

    for (auto output : playbackNode->outputs)
    {
        auto outputPlaybackNode = static_cast<PlaybackNode*> (output->internal);

        // fetch_sub returns the previous value so it will now be 0
        if (outputPlaybackNode->numInputsToBeProcessed.fetch_sub (1, std::memory_order_release) == 1)
        {
            jassert (outputPlaybackNode->node.isReadyToProcess());
            jassert (! outputPlaybackNode->hasBeenQueued);
            outputPlaybackNode->hasBeenQueued = true;

            // If there is only one Node or we're at the last Node we can reutrn this to be processed by the same thread
            if (playbackNode->outputs.size() == 1
                || output == playbackNode->outputs.back())
               return &outputPlaybackNode->node;
            
            preparedNode->nodesReadyToBeProcessed.push (&outputPlaybackNode->node);
            numNodesQueued.fetch_add (1, std::memory_order_release);
            threadPool->signalOne();
        }
    }

    return nullptr;
}

//==============================================================================
bool MultiThreadedNodePlayer::processNextFreeNode()
{
    Node* nodeToProcess = nullptr;

    if (numNodesQueued.load (std::memory_order_acquire) == 0)
        return false;

    if (! preparedNode->nodesReadyToBeProcessed.pop (nodeToProcess))
        return false;

    numNodesQueued.fetch_sub (1, std::memory_order_release);

    assert (nodeToProcess != nullptr);
    processNode (*nodeToProcess);

    return true;
}

void MultiThreadedNodePlayer::processNode (Node& node)
{
    auto* nodeToProcess = &node;

    // Attempt to process serial Node chains on this thread
    // to reduce context switches and overhead
    for (;;)
    {
        #if JUCE_DEBUG
         jassert (! static_cast<PlaybackNode*> (nodeToProcess->internal)->hasBeenDequeued);
         static_cast<PlaybackNode*> (nodeToProcess->internal)->hasBeenDequeued = true;
        #endif

        // Process Node
        nodeToProcess->process (numSamplesToProcess, referenceSampleRange);
        nodeToProcess = updateProcessQueueForNode (*nodeToProcess);

        if (! nodeToProcess)
            break;
    }
}

}}
