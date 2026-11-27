/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <thread>
#if JUCE_INTEL
 #include <emmintrin.h>
#endif

#define RETURN_MID_NODES_OPTIMISATION 1

namespace tracktion::inline graph {

LockFreeMultiThreadedNodePlayer::LockFreeMultiThreadedNodePlayer()
{
    threadPool = getPoolCreatorFunction (ThreadPoolStrategy::realTime) (*this);
}

LockFreeMultiThreadedNodePlayer::LockFreeMultiThreadedNodePlayer (ThreadPoolCreator poolCreator, juce::AudioWorkgroup audioWorkgroup_)
    : audioWorkgroup (std::move (audioWorkgroup_))
{
    threadPool = poolCreator (*this);
}

LockFreeMultiThreadedNodePlayer::~LockFreeMultiThreadedNodePlayer()
{
    namespace we = wake_experiment;
    const auto cpuSeconds = getProcessCpuTimeSeconds() - we::cpuSecondsAtStart;

    if (numThreadsToUse > 0)
        clearThreads();

    if (we::numBlocks > 0)
    {
        const auto n = (double) we::numBlocks;
        std::cout << "PHASES blocks=" << we::numBlocks
                  << " cpu_us_per_block=" << 1.0e6 * cpuSeconds / n
                  << " prepare_us=" << 1.0e6 * we::prepare / n
                  << " reset_us=" << 1.0e6 * we::reset / n
                  << " signal_us=" << 1.0e6 * we::signalTime / n
                  << " processing_us=" << 1.0e6 * we::processing / n
                  << " output_us=" << 1.0e6 * we::output / n
                  << " wakes_per_block=" << (double) we::numWakes.load() / n
                  << " empty_wakes_per_block=" << (double) we::numEmptyWakes.load() / n
                  << " nodes_to_prepare=" << we::numToPrepare << "\n";
        we::prepare = we::reset = we::signalTime = we::processing = we::output = 0;
        we::numBlocks = 0;
        we::numWakes = 0;
        we::numEmptyWakes = 0;
    }
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
                                 useMemoryPool,
                                 disableLatencyComp));
}

void LockFreeMultiThreadedNodePlayer::prepareToPlay (double sampleRateToUse, int blockSizeToUse)
{
    if (sampleRateToUse == sampleRate && blockSizeToUse == blockSize)
        return;

    std::unique_ptr<NodeGraph> currentGraph;

    // Ensure we've flushed any pending Node to the current prepared Node
    {
        const auto scopedAccess = preparedNodeObject.getScopedAccess();

        if (auto pn = scopedAccess.get())
            currentGraph = std::move (pn->graph);
    }

    clearNode();

    // Don't pass in the old graph here as we're stealing the root from it
    postNewGraph (prepareToPlay (currentGraph != nullptr ? std::move (currentGraph->rootNode) : std::unique_ptr<Node>(), nullptr,
                                 sampleRateToUse, blockSizeToUse,
                                 useMemoryPool,
                                 disableLatencyComp));
}

int LockFreeMultiThreadedNodePlayer::process (const Node::ProcessContext& pc)
{
    const std::unique_lock<RealTimeSpinLock> l (processMutex, std::try_to_lock);

    // If this fails, it's because the clearNode function is being called
    if (! l.owns_lock())
        return -1;

    const auto scopedAccess = preparedNodeObject.getScopedAccess();
    const auto preparedNode = scopedAccess.get();

    if (preparedNode == nullptr)
        return -1;

    if (! preparedNode->graph)
        return -1;

    if (! preparedNode->graph->rootNode)
        return -1;

    // Publish the current PreparedNode to the pool threads. This must happen in
    // every process call that could have swapped a new PreparedNode in via
    // retainRealTime above (including the single-threaded paths below), so that
    // by the time postNewGraph destroys a retired PreparedNode, the pool
    // threads can no longer pick it up
    threadPool->setCurrentNode (preparedNode);

    // Reset the stream range
    numSamplesToProcess = pc.numSamples;
    referenceSampleRange = pc.referenceSampleRange;

    wake_experiment::PhaseTimer phaseTimer;

    if (phaseTimer.enabled && wake_experiment::numBlocks == 0)
        wake_experiment::cpuSecondsAtStart = getProcessCpuTimeSeconds();

    // Changing the block counter marks all of the Nodes as not processed, which
    // avoids writing to every Node here after another thread has processed it
    preparedNode->graph->blockCounter.fetch_add (1, std::memory_order_release);

    // That leaves only the Nodes that prefetch or have pooled buffers to prepare
    for (auto node : preparedNode->nodesToPrepare)
        node->prepareForNextBlock (referenceSampleRange);

    wake_experiment::numToPrepare = (int64_t) preparedNode->nodesToPrepare.size();
    phaseTimer.lap (wake_experiment::prepare);

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
        isProcessingBlock.store (true, std::memory_order_release);
        resetProcessQueue (*preparedNode);
        phaseTimer.lap (wake_experiment::reset);

        // Try to process Nodes until the root is ready
        for (;;)
        {
            if (preparedNode->graph->rootNode->hasProcessed())
                break;

            if (! processNextFreeNode (*preparedNode))
                threadPool->waitForFinalNode();
        }

        isProcessingBlock.store (false, std::memory_order_release);
    }

    phaseTimer.lap (wake_experiment::processing);

    if (phaseTimer.enabled)
        ++wake_experiment::numBlocks;

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
    phaseTimer.lap (wake_experiment::output);

    return -1;
}

void LockFreeMultiThreadedNodePlayer::clearNode()
{
    // N.B. The threads will be trying to read the preparedNodes so we need to actually stop these first
    clearThreads();

    // The threads have now been stopped so it's safe to reset the current node
    // pointer, ensuring newly created threads can't read the cleared objects
    threadPool->setCurrentNode (nullptr);

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

void LockFreeMultiThreadedNodePlayer::enableNodeMemorySharing (bool shouldBeEnabled)
{
    if (std::exchange (nodeMemorySharingEnabled, shouldBeEnabled) != shouldBeEnabled)
        prepareToPlay (sampleRate, blockSize);
}

void LockFreeMultiThreadedNodePlayer::setLatencyCompensationEnabled (bool shouldEnable)
{
    if (disableLatencyComp.exchange (! shouldEnable) != ! shouldEnable)
        prepareToPlay (sampleRate, blockSize);
}

//==============================================================================
//==============================================================================
std::unique_ptr<NodeGraph> LockFreeMultiThreadedNodePlayer::prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph,
                                                                           double sampleRateToUse, int blockSizeToUse,
                                                                           bool useCurrentAudioBufferPool,
                                                                           bool disableLatencyCompensation)
{
    // These need to be set first as the threads use them for their real-time options
    sampleRate.store (sampleRateToUse, std::memory_order_release);
    blockSize.store (blockSizeToUse, std::memory_order_release);

    createThreads();

    if (! useCurrentAudioBufferPool)
        return node_player_utils::prepareToPlay (std::move (node), oldGraph,
                                                 sampleRateToUse, blockSizeToUse,
                                                 nullptr, nullptr,
                                                 nodeMemorySharingEnabled,
                                                 disableLatencyCompensation);

    return node_player_utils::prepareToPlay (std::move (node), oldGraph,
                                             sampleRateToUse, blockSizeToUse,
                                             [this] (auto s) -> NodeBuffer
                                             {
                                                auto data = lastAudioBufferPoolPosted->allocate (s);
                                                return { data.getView().getFirstChannels (s.numChannels).getStart (s.numFrames), std::move (data) };
                                             },
                                             [this] (auto b)
                                             {
                                                lastAudioBufferPoolPosted->release (std::move (b.data));
                                             },
                                             nodeMemorySharingEnabled,
                                             disableLatencyCompensation);
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::clearThreads()
{
    const std::scoped_lock<RealTimeSpinLock> sl (processMutex);
    threadPool->clearThreads();
}

void LockFreeMultiThreadedNodePlayer::createThreads()
{
    const std::scoped_lock<RealTimeSpinLock> sl (processMutex);
    threadPool->createThreads (numThreadsToUse.load(), audioWorkgroup);
}

inline void LockFreeMultiThreadedNodePlayer::pause()
{
    tracktion::core::pause();
    tracktion::core::pause();
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

    if (auto retiredPreparedNode = preparedNodeObject.pushNonRealTime (std::move (newPreparedNode)))
    {
        // If an old PreparedNode was returned, it was either never swapped in
        // on the audio thread (so no pool thread can have seen it), or it was
        // retired by a swap, in which case the audio thread has since published
        // a newer PreparedNode via setCurrentNode. Either way, no pool thread
        // can newly obtain it, but one could still be part-way through reading
        // it, so wait for them to finish before it is destroyed
        threadPool->waitForThreadsToQuiesce();
    }
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
        n->numOutputNodes = 0;
        n->setSharedBlockCounter (&preparedNode.graph->blockCounter);
    }

    // The Nodes have been initialised by now so this won't change
    preparedNode.nodesToPrepare.clear();

    for (auto n : preparedNode.graph->orderedNodes)
        if (n->needsPreparingForEachBlock())
            preparedNode.nodesToPrepare.push_back (n);

    // Iterate all nodes, for each input, add to the current Nodes output list
    for (auto node : preparedNode.graph->orderedNodes)
    {
        for (auto inputNode : node->getDirectInputNodes())
        {
            // Check the input is actually still in the graph
            jassert (std::find (preparedNode.graph->orderedNodes.begin(), preparedNode.graph->orderedNodes.end(), inputNode) != preparedNode.graph->orderedNodes.end());
            static_cast<PlaybackNode*> (inputNode->internal)->outputs.push_back (node);
            ++inputNode->numOutputNodes;
        }
    }

    // Find the Nodes that are queued at the start of each block
    preparedNode.leafNodes.clear();

    for (auto& playbackNode : preparedNode.playbackNodes)
        if (playbackNode->numInputs == 0)
            preparedNode.leafNodes.push_back (&playbackNode->node);
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

    // The input counters don't need resetting here as they are reset by the
    // thread that queues each Node, so they should all be ready for this block
   #if JUCE_DEBUG
    for (auto& playbackNode : preparedNode.playbackNodes)
    {
        jassert (playbackNode->hasBeenQueued);
        playbackNode->hasBeenQueued = false;
        playbackNode->hasBeenDequeued = false;
        jassert (playbackNode->numInputsToBeProcessed.load (std::memory_order_acquire) == playbackNode->numInputs);

        // Check only ready nodes will be queued
        jassert (playbackNode->node.isReadyToProcess() == (playbackNode->numInputs == 0));
    }
   #endif

    // Queue the Nodes which don't have any inputs
    for (auto node : preparedNode.leafNodes)
    {
       #if JUCE_DEBUG
        static_cast<PlaybackNode*> (node->internal)->hasBeenQueued = true;
       #endif

        preparedNode.nodesReadyToBeProcessed->try_enqueue (std::move (node));
    }

    const auto numNodesJustQueued = preparedNode.leafNodes.size();

    // Make sure this is only incremented after all the nodes have been queued
    // or the threads will start queueing Nodes at the same time
    numNodesQueued += numNodesJustQueued;

    if (int numThreadsToSignal = (int) numNodesQueued.load(); numThreadsToSignal > 1)
    {
        wake_experiment::PhaseTimer signalTimer;
        threadPool->signal (numThreadsToSignal);
        signalTimer.lap (wake_experiment::signalTime);
    }
}

Node* LockFreeMultiThreadedNodePlayer::updateProcessQueueForNode (PreparedNode& preparedNode, Node& node)
{
    auto playbackNode = static_cast<PlaybackNode*> (node.internal);

   #if RETURN_MID_NODES_OPTIMISATION
    Node* nodeToReturn = nullptr;
   #endif

    for (auto output : playbackNode->outputs)
    {
        auto outputPlaybackNode = static_cast<PlaybackNode*> (output->internal);

        // fetch_sub returns the previous value so it will now be 0
        if (outputPlaybackNode->numInputsToBeProcessed.fetch_sub (1, std::memory_order_acq_rel) == 1)
        {
            // All of the inputs have now been processed so nothing else will
            // change this until the next block and it can be reset for that now
            outputPlaybackNode->numInputsToBeProcessed.store (outputPlaybackNode->numInputs, std::memory_order_release);

            jassert (outputPlaybackNode->node.isReadyToProcess());
           #if JUCE_DEBUG
            jassert (! outputPlaybackNode->hasBeenQueued);
            outputPlaybackNode->hasBeenQueued = true;
           #endif

           #if RETURN_MID_NODES_OPTIMISATION
            // We can return one Node to be processed on this thread, otherwise we can
            // queue it for another thread to possibly process
            if (nodeToReturn == nullptr)
            {
                nodeToReturn = &outputPlaybackNode->node;
            }
            else
            {
                preparedNode.nodesReadyToBeProcessed->try_enqueue (&outputPlaybackNode->node);
                numNodesQueued.fetch_add (1, std::memory_order_acq_rel);
            }
           #else
            // If there is only one Node or we're at the last Node we can return this to be processed by the same thread
            if (playbackNode->outputs.size() == 1
                || output == playbackNode->outputs.back())
                return &outputPlaybackNode->node;

            preparedNode.nodesReadyToBeProcessed->try_enqueue (&outputPlaybackNode->node);
            numNodesQueued.fetch_add (1, std::memory_order_acq_rel);
           #endif
        }
    }

   #if RETURN_MID_NODES_OPTIMISATION
    return nodeToReturn;
   #else
    return nullptr;
   #endif
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

}
