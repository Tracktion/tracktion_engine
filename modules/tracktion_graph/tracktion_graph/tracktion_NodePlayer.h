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
    Simple player for an Node.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class NodePlayer
{
public:
    /** Creates an empty NodePlayer. */
    NodePlayer() = default;
    
    /** Creates an NodePlayer to process a Node. */
    NodePlayer (std::unique_ptr<Node> nodeToProcess, PlayHeadState* playHeadStateToUse = nullptr)
        : input (std::move (nodeToProcess)), playHeadState (playHeadStateToUse)
    {
    }
    
    /** Returns the current Node. */
    Node* getNode()
    {
        return input ? input.get()
                     : nodeGraph ? nodeGraph->rootNode.get()
                                 : nullptr;
    }

    /** Sets the Node to process. */
    void setNode (std::unique_ptr<Node> newNode)
    {
        setNode (std::move (newNode), sampleRate, blockSize);
    }

    /** Sets the Node to process with a new sample rate and block size. */
    void setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
    {
        auto newGraph = prepareToPlay (std::move (newNode), nodeGraph.get(), sampleRateToUse, blockSizeToUse);
        std::unique_ptr<NodeGraph> oldGraph;
        
        {
            const juce::SpinLock::ScopedLockType sl (inputAndNodesLock);
            oldGraph = std::move (nodeGraph);
            nodeGraph = std::move (newGraph);
        }
    }

    /** Prepares the current Node to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse)
    {
        auto oldGraph = std::move (nodeGraph);
        auto rootNodeToPrepare = input ? std::move (input)
                                       : oldGraph ? std::move (oldGraph->rootNode)
                                                  : nullptr;
        nodeGraph = prepareToPlay (std::move (rootNodeToPrepare), nullptr, sampleRateToUse, blockSizeToUse);
    }

    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::unique_ptr<NodeGraph> prepareToPlay (std::unique_ptr<Node> node, NodeGraph* oldGraph, double sampleRateToUse, int blockSizeToUse)
    {
        sampleRate = sampleRateToUse;
        blockSize = blockSizeToUse;
        
        if (playHeadState != nullptr)
            playHeadState->playHead.setScrubbingBlockLength (timeToSample (0.08, sampleRate));
        
        return node_player_utils::prepareToPlay (std::move (node), oldGraph, sampleRateToUse, blockSizeToUse);
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const Node::ProcessContext& pc)
    {
        if (inputAndNodesLock.tryEnter())
        {
            if (! nodeGraph)
            {
                inputAndNodesLock.exit();
                return 0;
            }
            
            int numMisses = 0;
            
            if (playHeadState != nullptr)
                numMisses = processWithPlayHeadState (*playHeadState, *nodeGraph, pc);
            else
                numMisses = processPostorderedNodes (*nodeGraph, pc);
            
            inputAndNodesLock.exit();
            
            return numMisses;
        }
        
        return 0;
    }
    
    double getSampleRate() const
    {
        return sampleRate;
    }
    
    int processPostorderedNodes (NodeGraph& graphToProcess, const Node::ProcessContext& pc)
    {
        return processPostorderedNodesSingleThreaded (graphToProcess, pc);
    }

protected:
    std::unique_ptr<Node> input;
    std::unique_ptr<NodeGraph> nodeGraph;
    PlayHeadState* playHeadState = nullptr;
    
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    juce::SpinLock inputAndNodesLock;
    
    int processWithPlayHeadState (PlayHeadState& phs, NodeGraph& graphToProcess,
                                  const Node::ProcessContext& pc)
    {
        int numMisses = 0;

        // Check to see if the timeline needs to be processed in two halves due to looping
        const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (phs.playHead, pc.referenceSampleRange);
        
        if (splitTimelineRange.isSplit)
        {
            const auto firstProportion = splitTimelineRange.timelineRange1.getLength() / (double) pc.referenceSampleRange.getLength();

            const auto firstReferenceRange  = pc.referenceSampleRange.withEnd (pc.referenceSampleRange.getStart() + (int64_t) std::llround (pc.referenceSampleRange.getLength() * firstProportion));
            const auto secondReferenceRange = juce::Range<int64_t> (firstReferenceRange.getEnd(), pc.referenceSampleRange.getEnd());
            jassert (firstReferenceRange.getLength() + secondReferenceRange.getLength() == pc.referenceSampleRange.getLength());

            const auto firstNumSamples  = (choc::buffer::FrameCount) std::llround (pc.numSamples * firstProportion);
            const auto secondNumSamples = pc.numSamples - firstNumSamples;
            jassert (firstNumSamples + secondNumSamples == pc.numSamples);

            {
                auto destAudio = pc.buffers.audio.getStart (firstNumSamples);
                auto& destMidi = pc.buffers.midi;

                phs.update (firstReferenceRange);
                tracktion::graph::Node::ProcessContext pc1 { firstNumSamples, firstReferenceRange, { destAudio , destMidi } };
                numMisses += processPostorderedNodes (graphToProcess, pc1);
            }

            {
                auto destAudio = pc.buffers.audio.getFrameRange ({ firstNumSamples, firstNumSamples + secondNumSamples });
                auto& destMidi = pc.buffers.midi;

                //TODO: Use a scratch MidiMessageArray and then merge it back with the offset time
                tracktion::graph::Node::ProcessContext pc2 { secondNumSamples, secondReferenceRange, { destAudio, destMidi } };
                phs.update (secondReferenceRange);
                numMisses += processPostorderedNodes (graphToProcess, pc2);
            }
        }
        else
        {
            phs.update (pc.referenceSampleRange);
            numMisses += processPostorderedNodes (graphToProcess, pc);
        }
        
        return numMisses;
    }

    /** Processes a group of Nodes assuming a postordering VertexOrdering.
        If these conditions are met the Nodes should be processed in a single loop iteration.
    */
    static int processPostorderedNodesSingleThreaded (NodeGraph& nodeGraph, const Node::ProcessContext& pc)
    {
        for (auto node : nodeGraph.orderedNodes)
            node->prepareForNextBlock (pc.referenceSampleRange);
        
        int numMisses = 0;
        size_t numNodesProcessed = 0;

        for (;;)
        {
            for (auto node : nodeGraph.orderedNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (pc.numSamples, pc.referenceSampleRange);
                    ++numNodesProcessed;
                }
                else
                {
                    ++numMisses;
                }
            }

            if (numNodesProcessed == nodeGraph.orderedNodes.size())
            {
                auto output = nodeGraph.rootNode->getProcessedOutput();
                auto numAudioChannels = std::min (output.audio.getNumChannels(),
                                                  pc.buffers.audio.getNumChannels());
                
                if (numAudioChannels > 0)
                    add (pc.buffers.audio.getFirstChannels (numAudioChannels),
                         output.audio.getFirstChannels (numAudioChannels));
                
                pc.buffers.midi.mergeFrom (output.midi);

                break;
            }
        }
        
        return numMisses;
    }
};

}}
