/*
  ==============================================================================

    AudioNodeProcessor.h
    Created: 15 Apr 2020 10:43:09am
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

#include "AudioNode.h"

//==============================================================================
//==============================================================================
/**
    Simple processor for an AudioNode.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class AudioNodeProcessor
{
public:
    /** Creates an AudioNodeProcessor to process an AudioNode. */
    AudioNodeProcessor (std::unique_ptr<AudioNode> nodeToProcess)
        : node (std::move (nodeToProcess))
    {
        auto nodes = node->getAllInputNodes();
        nodes.push_back (node.get());
        std::unique_copy (nodes.begin(), nodes.end(), std::back_inserter (allNodes),
                          [] (auto n1, auto n2) { return n1 == n2; });
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        const PlaybackInitialisationInfo info { sampleRate, blockSize, allNodes };
        
        for (auto& node : allNodes)
        {
            node->initialise (info);
            node->prepareToPlay (info);
        }
    }

    /** Processes an block of audio and MIDI data. */
    void process (const AudioNode::ProcessContext& pc)
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
                    node->process ((int) pc.buffers.audio.getNumSamples());
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                auto output = node->getProcessedOutput();
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.addEvents (output.midi, 0, -1, 0);
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::vector<AudioNode*> allNodes;
};
