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
    /** Creates an NodePlayer to process an Node. */
    NodePlayer (std::unique_ptr<Node> nodeToProcess)
        : input (std::move (nodeToProcess))
    {
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *input };
        std::function<void (Node&)> visitor = [&] (Node& n) { n.initialise (info); };
        visitInputs (*input, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes.clear();
        
        std::function<void (Node&)> visitor2 = [&] (Node& n) { allNodes.push_back (&n); };
        visitInputs (*input, visitor2);
    }

    /** Processes a block of audio and MIDI data. */
    void process (const Node::ProcessContext& pc)
    {
        for (auto node : allNodes)
            node->prepareForNextBlock();
        
        for (;;)
        {
            int processedAnyNodes = false;
            
            for (auto node : allNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (pc.streamSampleRange);
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                auto output = input->getProcessedOutput();
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.copyFrom (output.midi);
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<Node> input;
    std::vector<Node*> allNodes;
};

}
