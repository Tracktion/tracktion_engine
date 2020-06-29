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

//==============================================================================
//==============================================================================
/**
    Simple player for an Node.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class NodePlayer
{
public:
    /** Creates an empty NodePlayer. */
    NodePlayer () = default;

    /** Creates an NodePlayer to process a Node. */
    NodePlayer (std::unique_ptr<Node> nodeToProcess, PlayHeadState* playHeadStateToUse = nullptr)
        : input (std::move (nodeToProcess)), playHeadState (playHeadStateToUse)
    {
    }
    
    /** Returns the current Node. */
    Node* getNode()
    {
        return input.get();
    }

    /** Returns the Node to process. */
    void setNode (std::unique_ptr<Node> newNode)
    {
        setNode (std::move (newNode), sampleRate, blockSize);
    }

    /** Returns the Node to process with the current sample rate and block size. */
    void setNode (std::unique_ptr<Node> newNode, double sampleRateToUse, int blockSizeToUse)
    {
        auto newNodes = prepareToPlay (newNode.get(), input.get(), sampleRateToUse, blockSizeToUse);
        std::unique_ptr<Node> oldNode;
        
        {
            const juce::SpinLock::ScopedLockType sl (inputAndNodesLock);
            oldNode = std::move (input);
            input = std::move (newNode);
            allNodes = std::move (newNodes);
        }
    }

    /** Prepares the current Node to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr)
    {
        allNodes = prepareToPlay (input.get(), oldNode, sampleRateToUse, blockSizeToUse);
    }

    /** Prepares a specific Node to be played and returns all the Nodes. */
    std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRateToUse, int blockSizeToUse)
    {
        sampleRate = sampleRateToUse;
        blockSize = blockSizeToUse;
        
        if (playHeadState != nullptr)
            playHeadState->playHead.setScrubbingBlockLength (timeToSample (0.08, sampleRate));
        
        if (node == nullptr)
            return {};
        
        // First give the Nodes a chance to transform
        transformNodes (*node);
        
        // Next, initialise all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *node, oldNode };
        visitNodes (*node, [&] (Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        return tracktion_graph::getNodes (*node, tracktion_graph::VertexOrdering::postordering);
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const Node::ProcessContext& pc)
    {
        if (inputAndNodesLock.tryEnter())
        {
            if (! input)
            {
                inputAndNodesLock.exit();
                return 0;
            }
            
            int numMisses = 0;
            
            if (playHeadState != nullptr)
                numMisses = processWithPlayHeadState (*playHeadState, *input, allNodes, pc);
            else
                numMisses = processPostorderedNodes (*input, allNodes, pc);
            
            inputAndNodesLock.exit();
            
            return numMisses;
        }
        
        return 0;
    }
    
    double getSampleRate() const
    {
        return sampleRate;
    }
    
private:
    std::unique_ptr<Node> input;
    PlayHeadState* playHeadState = nullptr;
    
    std::vector<Node*> allNodes;
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    juce::SpinLock inputAndNodesLock;
    
    static int processWithPlayHeadState (PlayHeadState& playHeadState, Node& rootNode, const std::vector<Node*>& allNodes, const Node::ProcessContext& pc)
    {
        int numMisses = 0;
        
        // Check to see if the timeline needs to be processed in two halves due to looping
        const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange);
        
        if (splitTimelineRange.isSplit)
        {
            const auto firstNumSamples = splitTimelineRange.timelineRange1.getLength();
            const auto firstRange = pc.referenceSampleRange.withLength (firstNumSamples);
            
            {
                auto inputAudio = pc.buffers.audio.getSubBlock (0, (size_t) firstNumSamples);
                auto& inputMidi = pc.buffers.midi;
                
                playHeadState.update (firstRange);
                tracktion_graph::Node::ProcessContext pc1 { firstRange, { inputAudio , inputMidi } };
                numMisses += processPostorderedNodes (rootNode, allNodes, pc1);
            }
            
            {
                const auto secondNumSamples = splitTimelineRange.timelineRange2.getLength();
                const auto secondRange = juce::Range<int64_t>::withStartAndLength (firstRange.getEnd(), secondNumSamples);
                
                auto inputAudio = pc.buffers.audio.getSubBlock ((size_t) firstNumSamples, (size_t) secondNumSamples);
                auto& inputMidi = pc.buffers.midi;
                
                //TODO: Use a scratch MidiMessageArray and then merge it back with the offset time
                tracktion_graph::Node::ProcessContext pc2 { secondRange, { inputAudio , inputMidi } };
                playHeadState.update (secondRange);
                numMisses += processPostorderedNodes (rootNode, allNodes, pc2);
            }
        }
        else
        {
            playHeadState.update (pc.referenceSampleRange);
            numMisses += processPostorderedNodes (rootNode, allNodes, pc);
        }
        
        return numMisses;
    }

    /** Processes a group of Nodes assuming a postordering VertexOrdering.
        If these conditions are met the Nodes should be processed in a single loop iteration.
    */
    static int processPostorderedNodes (Node& rootNode, const std::vector<Node*>& allNodes, const Node::ProcessContext& pc)
    {
        for (auto node : allNodes)
            node->prepareForNextBlock (pc.referenceSampleRange);
        
        int numMisses = 0;
        size_t numNodesProcessed = 0;

        for (;;)
        {
            for (auto node : allNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (pc.referenceSampleRange);
                    ++numNodesProcessed;
                }
                else
                {
                    ++numMisses;
                }
            }

            if (numNodesProcessed == allNodes.size())
            {
                auto output = rootNode.getProcessedOutput();
                const size_t numAudioChannels = std::min (output.audio.getNumChannels(), pc.buffers.audio.getNumChannels());
                
                if (numAudioChannels > 0)
                    pc.buffers.audio.getSubsetChannelBlock (0, numAudioChannels).add (output.audio.getSubsetChannelBlock (0, numAudioChannels));
                
                pc.buffers.midi.mergeFrom (output.midi);

                break;
            }
        }
        
        return numMisses;
    }
};

}
