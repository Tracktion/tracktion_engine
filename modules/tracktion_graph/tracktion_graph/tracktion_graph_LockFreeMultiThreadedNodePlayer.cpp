/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <thread>
#if JUCE_INTEL
 #include <emmintrin.h>
#endif

namespace tracktion_graph
{

LockFreeMultiThreadedNodePlayer::LockFreeMultiThreadedNodePlayer()
{
    threadPool = getPoolCreatorFunction (ThreadPoolStrategy::realTime) (*this);
}

LockFreeMultiThreadedNodePlayer::LockFreeMultiThreadedNodePlayer (ThreadPoolCreator poolCreator)
{
    threadPool = poolCreator (*this);
}

LockFreeMultiThreadedNodePlayer::~LockFreeMultiThreadedNodePlayer()
{
    if (numThreadsToUse > 0)
        clearThreads();
}

void LockFreeMultiThreadedNodePlayer::setNumThreads (size_t newNumThreads)
{
    if (newNumThreads == numThreadsToUse)
        return;

    clearThreads();
    numThreadsToUse = newNumThreads;
    createThreads();
}

void LockFreeMultiThreadedNodePlayer::setNode (std::unique_ptr<Node> newNode)
{
    setNode (std::move (newNode), getSampleRate(), blockSize);
}

void LockFreeMultiThreadedNodePlayer::setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
{
    setNewCurrentNode (std::move (newNode), sampleRateToUse, blockSizeToUse);
}

void LockFreeMultiThreadedNodePlayer::prepareToPlay (double sampleRateToUse, int blockSizeToUse)
{
    std::unique_ptr<Node> currentNode;
    
    {
        std::lock_guard<RealTimeSpinLock> sl (clearNodesLock);
        updatePreparedNode();
        currentNode = std::move (preparedNode.rootNode);
    }
    
    clearNode();

    setNode (std::move (currentNode), sampleRateToUse, blockSizeToUse);
}

int LockFreeMultiThreadedNodePlayer::process (const Node::ProcessContext& pc)
{
    std::unique_lock<RealTimeSpinLock> tryLock (clearNodesLock, std::try_to_lock);
    
    if (! tryLock.owns_lock())
        return -1;

    updatePreparedNode();

    if (! preparedNode.rootNode)
        return -1;

    // Reset the stream range
    referenceSampleRange = pc.referenceSampleRange;

    // Prepare all the nodes to be played back
    for (auto node : preparedNode.allNodes)
        node->prepareForNextBlock (referenceSampleRange);

    // We need to retain the root so we can get the output from it
    preparedNode.rootNode->retain();

    if (numThreadsToUse.load (std::memory_order_acquire) == 0 || preparedNode.allNodes.size() == 1)
    {
        for (auto node : preparedNode.allNodes)
            node->process (referenceSampleRange);
    }
    else
    {
        // Reset the queue to be processed
        jassert (preparedNode.playbackNodes.size() == preparedNode.allNodes.size());
        resetProcessQueue();

        // Try to process Nodes until the root is ready
        for (;;)
        {
            if (preparedNode.rootNode->hasProcessed())
                break;

            if (! processNextFreeNode())
                threadPool->waitForFinalNode();
        }
    }

    // Add output from graph to buffers
    {
        auto output = preparedNode.rootNode->getProcessedOutput();
        auto numAudioChannels = std::min (output.audio.getNumChannels(), pc.buffers.audio.getNumChannels());

        if (numAudioChannels > 0)
            add (pc.buffers.audio.getFirstChannels (numAudioChannels),
                 output.audio.getFirstChannels (numAudioChannels));

        pc.buffers.midi.mergeFrom (output.midi);
    }

    // We need to retain the root so we can get the output from it
    preparedNode.rootNode->release();

    return -1;
}

void LockFreeMultiThreadedNodePlayer::clearNode()
{
    std::lock_guard<RealTimeSpinLock> sl (clearNodesLock);
    
    // N.B. The threads will be trying to read the preparedNodes so we need to actually stop these first
    clearThreads();
    
    setNode (nullptr);
    updatePreparedNode();

    // N.B. This needs to be called twice to clear the current and prepared Nodes
    setNode (nullptr);
    updatePreparedNode();

    createThreads();
    
    assert (pendingPreparedNode == nullptr);
    assert (preparedNode.rootNode == nullptr);
    assert (pendingPreparedNodeStorage.rootNode == nullptr);
    assert (rootNode == nullptr);
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::enablePooledMemoryAllocations (bool usePool)
{
    if (useMemoryPool.exchange (usePool) != usePool)
        prepareToPlay (sampleRate, blockSize);
}

//==============================================================================
//==============================================================================
std::vector<Node*> LockFreeMultiThreadedNodePlayer::prepareToPlay (Node* node, Node* oldNode,
                                                                   double sampleRateToUse, int blockSizeToUse,
                                                                   AudioBufferPool* pool)
{
    createThreads();

    sampleRate.store (sampleRateToUse, std::memory_order_release);
    blockSize = blockSizeToUse;
    
    if (pool == nullptr)
        return node_player_utils::prepareToPlay (node, oldNode, sampleRateToUse, blockSizeToUse);

    return node_player_utils::prepareToPlay (node, oldNode, sampleRateToUse, blockSizeToUse,
                                             [pool] (auto s) -> NodeBuffer
                                             {
                                                auto data = pool->allocate (s);
                                                return { data.getView().getFirstChannels (s.numChannels).getStart (s.numFrames), std::move (data) };
                                             },
                                             [pool] (auto b)
                                             {
                                                 pool->release (std::move (b.data));
                                             });
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::updatePreparedNode()
{
    isUpdatingPreparedNode = true;

    if (auto newPreparedNode = pendingPreparedNode.exchange (nullptr))
        std::swap (preparedNode, *newPreparedNode);

    isUpdatingPreparedNode = false;
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::clearThreads()
{
    threadPool->clearThreads();
}

void LockFreeMultiThreadedNodePlayer::createThreads()
{
    threadPool->createThreads (numThreadsToUse.load());
}

inline void LockFreeMultiThreadedNodePlayer::pause()
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
void LockFreeMultiThreadedNodePlayer::setNewCurrentNode (std::unique_ptr<Node> newRoot,
                                                         double sampleRateToUse, int blockSizeToUse)
{
    while (isUpdatingPreparedNode)
        pause();

    const bool useAudioBufferPool = useMemoryPool;
    auto currentRoot = preparedNode.rootNode.get();
    auto newNodes = prepareToPlay (newRoot.get(), currentRoot,
                                   sampleRateToUse, blockSizeToUse,
                                   useAudioBufferPool ? pendingPreparedNodeStorage.audioBufferPool.get() : nullptr);

    std::stable_sort (newNodes.begin(), newNodes.end(),
                      [] (auto n1, auto n2)
                      {
                          return n1->isReadyToProcess() && ! n2->isReadyToProcess();
                      });

    pendingPreparedNode = nullptr;
    rootNode = newRoot.get();
    pendingPreparedNodeStorage.rootNode = std::move (newRoot);
    pendingPreparedNodeStorage.allNodes = std::move (newNodes);
    pendingPreparedNodeStorage.nodesReadyToBeProcessed = std::make_unique<LockFreeFifo<Node*>> ((int) pendingPreparedNodeStorage.allNodes.size());
    buildNodesOutputLists (pendingPreparedNodeStorage);
    
    if (useAudioBufferPool)
    {
        const size_t poolCapacity = pendingPreparedNodeStorage.allNodes.size();
        
        if (! pendingPreparedNodeStorage.audioBufferPool)
            pendingPreparedNodeStorage.audioBufferPool = std::make_unique<AudioBufferPool> (poolCapacity);
        else if (pendingPreparedNodeStorage.audioBufferPool->getCapacity() < poolCapacity)
            pendingPreparedNodeStorage.audioBufferPool->setCapacity (poolCapacity);
        
        node_player_utils::reserveAudioBufferPool (pendingPreparedNodeStorage.rootNode.get(),
                                                   pendingPreparedNodeStorage.allNodes,
                                                   *pendingPreparedNodeStorage.audioBufferPool,
                                                   numThreadsToUse, blockSize);
    }
    else
    {
        pendingPreparedNodeStorage.audioBufferPool.reset();
    }

    pendingPreparedNode = &pendingPreparedNodeStorage;
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::buildNodesOutputLists (PreparedNode& preparedNode)
{
    preparedNode.playbackNodes.clear();
    preparedNode.playbackNodes.reserve (preparedNode.allNodes.size());

    for (auto n : preparedNode.allNodes)
    {
       #if JUCE_DEBUG
        for (auto& pn : preparedNode.playbackNodes)
            jassert (&pn->node != n);
       #endif

        jassert (std::count (preparedNode.allNodes.begin(), preparedNode.allNodes.end(), n) == 1);

        preparedNode.playbackNodes.push_back (std::make_unique<PlaybackNode> (*n));
        n->internal = preparedNode.playbackNodes.back().get();
    }

    // Iterate all nodes, for each input, add to the current Nodes output list
    for (auto node : preparedNode.allNodes)
    {
        for (auto inputNode : node->getDirectInputNodes())
        {
            // Check the input is actually still in the graph
            jassert (std::find (preparedNode.allNodes.begin(), preparedNode.allNodes.end(), inputNode) != preparedNode.allNodes.end());
            static_cast<PlaybackNode*> (inputNode->internal)->outputs.push_back (node);
            inputNode->numOutputNodes++;
        }
    }
}

void LockFreeMultiThreadedNodePlayer::resetProcessQueue()
{
    // Clear the nodesReadyToBeProcessed list
    for (;;)
    {
        Node* temp;

        if (! preparedNode.nodesReadyToBeProcessed->try_dequeue (temp))
            break;
    }

    numNodesQueued.store (0, std::memory_order_release);

    // Reset all the counters
    // And then move any Nodes that are ready to the correct queue
    for (auto& playbackNode : preparedNode.playbackNodes)
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
    for (auto& playbackNode : preparedNode.playbackNodes)
    {
        playbackNode->hasBeenDequeued = false;
        jassert (! playbackNode->hasBeenQueued);
        jassert (playbackNode->numInputsToBeProcessed == playbackNode->numInputs);
        jassert (playbackNode->numInputsToBeProcessed.load (std::memory_order_acquire) == playbackNode->numInputs);
    }
   #endif

    size_t numNodesJustQueued = 0;

    // Make sure the counters are reset for all nodes before queueing any
    for (auto& playbackNode : preparedNode.playbackNodes)
    {
        if (playbackNode->numInputsToBeProcessed.load (std::memory_order_acquire) == 0)
        {
            jassert (! playbackNode->hasBeenQueued);
            playbackNode->hasBeenQueued = true;
            preparedNode.nodesReadyToBeProcessed->try_enqueue (&playbackNode->node);
            ++numNodesJustQueued;
        }
    }

    // Make sure this is only incremented after all the nodes have been queued
    // or the threads will start queueing Nodes at the same time
    numNodesQueued += numNodesJustQueued;
    threadPool->signalAll();
}

Node* LockFreeMultiThreadedNodePlayer::updateProcessQueueForNode (Node& node)
{
    auto playbackNode = static_cast<PlaybackNode*> (node.internal);

    for (auto output : playbackNode->outputs)
    {
        auto outputPlaybackNode = static_cast<PlaybackNode*> (output->internal);

        // fetch_sub returns the previous value so it will now be 0
        if (outputPlaybackNode->numInputsToBeProcessed.fetch_sub (1, std::memory_order_acq_rel) == 1)
        {
            jassert (outputPlaybackNode->node.isReadyToProcess());
            jassert (! outputPlaybackNode->hasBeenQueued);
            outputPlaybackNode->hasBeenQueued = true;

            // If there is only one Node or we're at the last Node we can reutrn this to be processed by the same thread
            if (playbackNode->outputs.size() == 1
                || output == playbackNode->outputs.back())
               return &outputPlaybackNode->node;
            
            preparedNode.nodesReadyToBeProcessed->try_enqueue (&outputPlaybackNode->node);
            numNodesQueued.fetch_add (1, std::memory_order_acq_rel);
            threadPool->signalOne();
        }
    }

    return nullptr;
}

//==============================================================================
bool LockFreeMultiThreadedNodePlayer::processNextFreeNode()
{
    Node* nodeToProcess = nullptr;

    if (numNodesQueued.load (std::memory_order_acquire) == 0)
        return false;

    if (! preparedNode.nodesReadyToBeProcessed->try_dequeue (nodeToProcess))
        return false;

    numNodesQueued.fetch_sub (1, std::memory_order_acq_rel);

    assert (nodeToProcess != nullptr);
    processNode (*nodeToProcess);

    return true;
}

void LockFreeMultiThreadedNodePlayer::processNode (Node& node)
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
        nodeToProcess->process (referenceSampleRange);
        nodeToProcess = updateProcessQueueForNode (*nodeToProcess);

        if (! nodeToProcess)
            break;
    }
}

}
