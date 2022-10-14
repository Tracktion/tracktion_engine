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

namespace tracktion { inline namespace graph
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
    // The prepare and set the new Node, passing in the old graph
    postNewGraph (prepareToPlay (std::move (newNode), lastGraphPosted,
                                 sampleRateToUse, blockSizeToUse,
                                 lastAudioBufferPoolPosted));
}

void LockFreeMultiThreadedNodePlayer::prepareToPlay (double sampleRateToUse, int blockSizeToUse)
{
    std::unique_ptr<NodeGraph> currentGraph;
    AudioBufferPool* currentAudioBufferPool = nullptr;

    // Ensure we've flushed any pending Node to the current prepared Node
    {
        const auto scopedAccess = preparedNodeObject.getScopedAccess();

        if (auto pn = scopedAccess.get())
        {
            currentGraph = std::move (pn->graph);
            currentAudioBufferPool = pn->audioBufferPool.get();
        }
    }
    
    clearNode();

    // Don't pass in the old graph here as we're stealing the root from it
    postNewGraph (prepareToPlay (currentGraph != nullptr ? std::move (currentGraph->rootNode) : std::unique_ptr<Node>(), nullptr,
                                 sampleRateToUse, blockSizeToUse,
                                 useMemoryPool ? currentAudioBufferPool : nullptr));
}

int LockFreeMultiThreadedNodePlayer::process (const Node::ProcessContext& pc)
{
    const auto scopedAccess = preparedNodeObject.getScopedAccess();
    const auto preparedNode = scopedAccess.get();

    if (preparedNode == nullptr)
        return -1;

    if (! preparedNode->graph)
        return -1;

    if (! preparedNode->graph->rootNode)
        return -1;

    // Reset the stream range
    numSamplesToProcess = pc.numSamples;
    referenceSampleRange = pc.referenceSampleRange;

    // Prepare all the nodes to be played back
    for (auto node : preparedNode->graph->orderedNodes)
        node->prepareForNextBlock (referenceSampleRange);

    // We need to retain the root so we can get the output from it
    preparedNode->graph->rootNode->retain();

    if (numThreadsToUse.load (std::memory_order_acquire) == 0 || preparedNode->graph->orderedNodes.size() == 1)
    {
        for (auto node : preparedNode->graph->orderedNodes)
            node->process (numSamplesToProcess, referenceSampleRange);
    }
    else
    {
        // Reset the queue to be processed
        jassert (preparedNode->playbackNodes.size() == preparedNode->graph->orderedNodes.size());
        resetProcessQueue (*preparedNode);

        // Try to process Nodes until the root is ready
        for (;;)
        {
            if (preparedNode->graph->rootNode->hasProcessed())
                break;

            if (! processNextFreeNode (*preparedNode))
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

    // We need to release the root to match the previous retain
    preparedNode->graph->rootNode->release();

    return -1;
}

void LockFreeMultiThreadedNodePlayer::clearNode()
{
    // N.B. The threads will be trying to read the preparedNodes so we need to actually stop these first
    clearThreads();

    rootNode = nullptr;
    lastGraphPosted = nullptr;
    lastAudioBufferPoolPosted = nullptr;
    preparedNodeObject.clear();

    createThreads();
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::enablePooledMemoryAllocations (bool usePool)
{
    if (useMemoryPool.exchange (usePool) != usePool)
        prepareToPlay (sampleRate, blockSize);
}

//==============================================================================
//==============================================================================
std::unique_ptr<NodeGraph> LockFreeMultiThreadedNodePlayer::prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph,
                                                                           double sampleRateToUse, int blockSizeToUse,
                                                                           AudioBufferPool* pool)
{
    createThreads();

    sampleRate.store (sampleRateToUse, std::memory_order_release);
    blockSize = blockSizeToUse;

    if (pool == nullptr)
        return node_player_utils::prepareToPlay (std::move (node), oldGraph, sampleRateToUse, blockSizeToUse);

    return node_player_utils::prepareToPlay (std::move (node), oldGraph, sampleRateToUse, blockSizeToUse,
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
void LockFreeMultiThreadedNodePlayer::postNewGraph (std::unique_ptr<NodeGraph> newGraph)
{
    if (! newGraph)
    {
        clearNode();
        return;
    }

    std::stable_sort (newGraph->orderedNodes.begin(), newGraph->orderedNodes.end(),
                      [] (auto n1, auto n2)
                      {
                          return n1->isReadyToProcess() && ! n2->isReadyToProcess();
                      });

    rootNode = newGraph->rootNode.get();

    PreparedNode newPreparedNode;
    newPreparedNode.graph = std::move (newGraph);
    newPreparedNode.nodesReadyToBeProcessed = std::make_unique<LockFreeFifo<Node*>> ((int) newPreparedNode.graph->orderedNodes.size());
    buildNodesOutputLists (newPreparedNode);

    if (useMemoryPool)
    {
        const size_t poolCapacity = newPreparedNode.graph->orderedNodes.size();
        newPreparedNode.audioBufferPool = std::make_unique<AudioBufferPool> (poolCapacity);

        node_player_utils::reserveAudioBufferPool (newPreparedNode.graph->rootNode.get(),
                                                   newPreparedNode.graph->orderedNodes,
                                                   *newPreparedNode.audioBufferPool,
                                                   numThreadsToUse, blockSize);
    }

    lastGraphPosted = newPreparedNode.graph.get();
    lastAudioBufferPoolPosted = newPreparedNode.audioBufferPool.get();
    preparedNodeObject.pushNonRealTime (std::move (newPreparedNode));
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::buildNodesOutputLists (PreparedNode& preparedNode)
{
    preparedNode.playbackNodes.clear();
    preparedNode.playbackNodes.reserve (preparedNode.graph->orderedNodes.size());

    for (auto n : preparedNode.graph->orderedNodes)
    {
       #if JUCE_DEBUG
        for (auto& pn : preparedNode.playbackNodes)
            jassert (&pn->node != n);
       #endif

        jassert (std::count (preparedNode.graph->orderedNodes.begin(), preparedNode.graph->orderedNodes.end(), n) == 1);

        preparedNode.playbackNodes.push_back (std::make_unique<PlaybackNode> (*n));
        n->internal = preparedNode.playbackNodes.back().get();
    }

    // Iterate all nodes, for each input, add to the current Nodes output list
    for (auto node : preparedNode.graph->orderedNodes)
    {
        for (auto inputNode : node->getDirectInputNodes())
        {
            // Check the input is actually still in the graph
            jassert (std::find (preparedNode.graph->orderedNodes.begin(), preparedNode.graph->orderedNodes.end(), inputNode) != preparedNode.graph->orderedNodes.end());
            static_cast<PlaybackNode*> (inputNode->internal)->outputs.push_back (node);
            inputNode->numOutputNodes++;
        }
    }
}

void LockFreeMultiThreadedNodePlayer::resetProcessQueue (PreparedNode& preparedNode)
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

    threadPool->setCurrentNode (&preparedNode);

    if (int numThreadsToSignal = (int) numNodesQueued.load(); numThreadsToSignal > 1)
        threadPool->signal (numThreadsToSignal);
}

Node* LockFreeMultiThreadedNodePlayer::updateProcessQueueForNode (PreparedNode& preparedNode, Node& node)
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
bool LockFreeMultiThreadedNodePlayer::processNextFreeNode (PreparedNode& preparedNode)
{
    Node* nodeToProcess = nullptr;

    if (numNodesQueued.load (std::memory_order_acquire) == 0)
        return false;

    if (! preparedNode.nodesReadyToBeProcessed->try_dequeue (nodeToProcess))
        return false;

    numNodesQueued.fetch_sub (1, std::memory_order_acq_rel);

    assert (nodeToProcess != nullptr);
    processNode (preparedNode, *nodeToProcess);

    return true;
}

void LockFreeMultiThreadedNodePlayer::processNode (PreparedNode& preparedNode, Node& node)
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
        nodeToProcess = updateProcessQueueForNode (preparedNode, *nodeToProcess);

        if (! nodeToProcess)
            break;
    }
}

}}
