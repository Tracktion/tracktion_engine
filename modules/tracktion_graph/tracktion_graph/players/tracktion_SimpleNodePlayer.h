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

//==============================================================================
//==============================================================================
/**
    Simple player for a Node.
    This iterates all the nodes attempting to process them in a single thread.
*/
class SimpleNodePlayer
{
public:
    /** Creates a player to play a Node at a given sample rate and block size. */
    SimpleNodePlayer (std::unique_ptr<Node> nodeToPlay,
                      double sampleRateToUse, int blockSizeToUse)
    {
        assert (nodeToPlay);
        graph = node_player_utils::prepareToPlay (std::move (nodeToPlay), nullptr, sampleRateToUse, blockSizeToUse);
    }
    
    /** Processes a block of audio and MIDI data. */
    void process (const Node::ProcessContext& pc)
    {
        // Prepare all nodes for the next block
        for (auto node : graph->orderedNodes)
            node->prepareForNextBlock (pc.referenceSampleRange);
        
        // Then process them all in sequence
        for (auto node : graph->orderedNodes)
            node->process (pc.numSamples, pc.referenceSampleRange);

        // Finally copy the output from the root Node to our player buffers
        auto output = graph->rootNode->getProcessedOutput();
        auto numAudioChannels = std::min (output.audio.getNumChannels(),
                                          pc.buffers.audio.getNumChannels());
                
        if (numAudioChannels > 0)
            add (pc.buffers.audio.getFirstChannels (numAudioChannels),
                 output.audio.getFirstChannels (numAudioChannels));

        pc.buffers.midi.mergeFrom (output.midi);
    }
    
private:
    std::unique_ptr<NodeGraph> graph;
};

}}
