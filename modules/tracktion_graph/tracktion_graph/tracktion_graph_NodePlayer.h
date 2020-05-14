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

/** Processes a group of Nodes assuming a postordering VertexOrdering.
    If these conditions are met the Nodes should be processed in a single loop iteration.
*/
static inline int processPostorderedNodes (Node& rootNode, const std::vector<Node*>& allNodes, const Node::ProcessContext& pc)
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
    NodePlayer (std::unique_ptr<Node> nodeToProcess)
        : input (std::move (nodeToProcess))
    {
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
        
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *input, oldNode };
        std::function<void (Node&)> visitor = [&] (Node& n) { n.initialise (info); };
        visitInputs (*input, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        // Then find all the nodes as it might have changed after initialisation
        allNodes = tracktion_graph::getNodes (*input, tracktion_graph::VertexOrdering::postordering);
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const Node::ProcessContext& pc)
    {
        return processPostorderedNodes (*input, allNodes, pc);
    }
    
private:
    std::unique_ptr<Node> input;
    std::vector<Node*> allNodes;
    double sampleRate = 44100.0;
    int blockSize = 512;
};

}
