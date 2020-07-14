/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_graph
{

LockFreeMultiThreadedNodePlayer::~LockFreeMultiThreadedNodePlayer()
{
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
    setNode (std::move (newNode), sampleRate, blockSize);
}

void LockFreeMultiThreadedNodePlayer::setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
{
    auto currentRoot = preparedNode.rootNode.get();
    auto newNodes = prepareToPlay (newNode.get(), currentRoot, sampleRateToUse, blockSizeToUse);
    setNewCurrentNode (std::move (newNode), newNodes);
}

void LockFreeMultiThreadedNodePlayer::prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode)
{
    preparedNode.allNodes = prepareToPlay (preparedNode.rootNode.get(), oldNode, sampleRateToUse, blockSizeToUse);
}

int LockFreeMultiThreadedNodePlayer::process (const Node::ProcessContext& pc)
{
    updatePreparedNode();
    
    if (! preparedNode.rootNode)
        return -1;

    // Reset the stream range
    referenceSampleRange = pc.referenceSampleRange;
    
    // Prepare all the nodes to be played back
    for (auto node : preparedNode.allNodes)
        node->prepareForNextBlock (referenceSampleRange);
    
    if (numThreadsToUse.load (std::memory_order_acquire) == 0)
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
                pause();
        }
    }

    // Add output from graph to buffers
    {
        auto output = preparedNode.rootNode->getProcessedOutput();
        const size_t numAudioChannels = std::min (output.audio.getNumChannels(), pc.buffers.audio.getNumChannels());
        
        if (numAudioChannels > 0)
            pc.buffers.audio.getSubsetChannelBlock (0, numAudioChannels).add (output.audio.getSubsetChannelBlock (0, numAudioChannels));
        
        pc.buffers.midi.mergeFrom (output.midi);
    }

    return -1;
}

//==============================================================================
//==============================================================================
std::vector<Node*> LockFreeMultiThreadedNodePlayer::prepareToPlay (Node* node, Node* oldNode, double sampleRateToUse, int blockSizeToUse)
{
    if (threads.empty())
        createThreads();

    sampleRate = sampleRateToUse;
    blockSize = blockSizeToUse;
    
    return node_player_utils::prepareToPlay (node, oldNode, sampleRateToUse, blockSizeToUse);
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
    threadsShouldExit = true;

    for (auto& t : threads)
        t.join();
    
    threads.clear();
}

void LockFreeMultiThreadedNodePlayer::createThreads()
{
    threadsShouldExit = false;
    
    // If there is 0 threads, simply process on the audio thread
    for (size_t i = 0; i < numThreadsToUse.load(); ++i)
    {
        threads.emplace_back ([this] { processNextFreeNodeOrWait(); });
        setThreadPriority (threads.back(), 10);
    }
}

inline void pause8()
{
   #if JUCE_INTEL
    _mm_pause();
    _mm_pause();
    _mm_pause();
    _mm_pause();
    _mm_pause();
    _mm_pause();
    _mm_pause();
    _mm_pause();
   #else
    #warning Implement for ARM
   #endif
}

inline void LockFreeMultiThreadedNodePlayer::pause()
{
    pause8();
    pause8();
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::setNewCurrentNode (std::unique_ptr<Node> newRoot, std::vector<Node*> newNodes)
{
    while (isUpdatingPreparedNode)
        pause();
    
    pendingPreparedNode = nullptr;
    rootNode = newRoot.get();
    pendingPreparedNodeStorage.rootNode = std::move (newRoot);
    pendingPreparedNodeStorage.allNodes = std::move (newNodes);
    buildNodesOutputLists (pendingPreparedNodeStorage);
    
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
        }
    }
}

void LockFreeMultiThreadedNodePlayer::resetProcessQueue()
{
    // Clear the nodesReadyToBeProcessed list
    for (;;)
    {
        Node* temp;
        
        if (! preparedNode.nodesReadyToBeProcessed.try_dequeue (temp))
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
        if (playbackNode->node.isReadyToProcess())
            jassert (playbackNode->numInputsToBeProcessed == 0);

        if (playbackNode->numInputsToBeProcessed == 0)
            jassert (playbackNode->node.isReadyToProcess());
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
            preparedNode.nodesReadyToBeProcessed.enqueue (&playbackNode->node);
            ++numNodesJustQueued;
        }
    }

    // Make sure this is only incremented after all the nodes have been queued
    // or the threads will start queueing Nodes at the same time
    numNodesQueued += numNodesJustQueued;
}

void LockFreeMultiThreadedNodePlayer::updateProcessQueueForNode (Node& node)
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
            preparedNode.nodesReadyToBeProcessed.enqueue (&outputPlaybackNode->node);
            numNodesQueued.fetch_add (1, std::memory_order_release);
        }
    }
}

//==============================================================================
void LockFreeMultiThreadedNodePlayer::processNextFreeNodeOrWait()
{
    // The pause and sleep counts avoid starving the CPU if there aren't enough queued nodes
    // This only happens on the worker threads so the main audio thread never interacts with the thread scheduler
    thread_local int pauseCount = 0, sleepCount = 0;
    
    for (;;)
    {
        if (threadsShouldExit.load (std::memory_order_acquire))
            return;
        
        if (! processNextFreeNode())
        {
            if (++pauseCount == 100)
            {
                std::this_thread::yield();
                pauseCount = 0;
                ++sleepCount;
            }
            else
            {
                pause();
            }
            
            if (sleepCount == 100)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
                sleepCount = 0;
            }
        }
        else
        {
            pauseCount = 0;
            sleepCount = 0;
        }
    }
}

bool LockFreeMultiThreadedNodePlayer::processNextFreeNode()
{
    Node* nodeToProcess = nullptr;
    
    if (numNodesQueued.load (std::memory_order_acquire) == 0)
        return false;
        
    if (! preparedNode.nodesReadyToBeProcessed.try_dequeue (nodeToProcess))
        return false;

   #if JUCE_DEBUG
    jassert (nodeToProcess != nullptr);
    jassert (! static_cast<PlaybackNode*> (nodeToProcess->internal)->hasBeenDequeued);
    static_cast<PlaybackNode*> (nodeToProcess->internal)->hasBeenDequeued = true;
   #endif
    numNodesQueued.fetch_sub (1, std::memory_order_release);

    nodeToProcess->process (referenceSampleRange);
    updateProcessQueueForNode (*nodeToProcess);
    
    return true;
}

}
