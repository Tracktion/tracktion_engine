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
    /** Creates an NodePlayer to process an Node. */
    NodePlayer (std::unique_ptr<Node> nodeToProcess, PlayHeadState* playHeadStateToUse = nullptr)
        : input (std::move (nodeToProcess)), playHeadState (playHeadStateToUse)
    {
    }
    
    Node& getNode()
    {
        return *input;
    }

    void setNode (std::unique_ptr<Node> newNode)
    {
        auto oldNode = std::move (input);
        input = std::move (newNode);
        prepareToPlay (sampleRate, blockSize, oldNode.get());
    }
    
    /** Prepares the processor to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse, Node* oldNode = nullptr)
    {
        sampleRate = sampleRateToUse;
        blockSize = blockSizeToUse;
        
        if (playHeadState != nullptr)
            playHeadState->playHead.setScrubbingBlockLength (timeToSample (0.08, sampleRate));
        
        // First give the Nodes a chance to transform
        transformNodes (*input);
        
        // Next, initialise all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *input, oldNode };
        visitNodes (*input, [&] (Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes = tracktion_graph::getNodes (*input, tracktion_graph::VertexOrdering::postordering);
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const Node::ProcessContext& pc)
    {
        if (playHeadState != nullptr)
            return processWithPlayHeadState (*playHeadState, *input, allNodes, pc);
        
        return processPostorderedNodes (*input, allNodes, pc);
    }
    
private:
    std::unique_ptr<Node> input;
    PlayHeadState* playHeadState = nullptr;
    
    std::vector<Node*> allNodes;
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    static int processWithPlayHeadState (PlayHeadState& playHeadState, Node& rootNode, const std::vector<Node*>& allNodes, const Node::ProcessContext& pc)
    {
        int numMisses = 0;
        
        // Check to see if the timeline needs to be processed in two halves due to looping
        const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.streamSampleRange);
        
        if (splitTimelineRange.isSplit)
        {
            const auto firstNumSamples = splitTimelineRange.timelineRange1.getLength();
            const auto firstRange = pc.streamSampleRange.withLength (firstNumSamples);
            
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
            playHeadState.update (pc.streamSampleRange);
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
            node->prepareForNextBlock();
        
        int numMisses = 0;
        size_t numNodesProcessed = 0;

        for (;;)
        {
            for (auto node : allNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (pc.streamSampleRange);
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
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.copyFrom (output.midi);

                break;
            }
        }
        
        return numMisses;
    }
};

}
