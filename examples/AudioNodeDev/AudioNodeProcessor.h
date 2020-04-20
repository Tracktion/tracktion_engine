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
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *node };
        std::function<void (AudioNode&)> visitor = [&] (AudioNode& n) { n.initialise (info); };
        visitInputs (*node, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes.clear();
        
        std::function<void (AudioNode&)> visitor2 = [&] (AudioNode& n) { allNodes.push_back (&n); };
        visitInputs (*node, visitor2);
    }

    /** Processes a block of audio and MIDI data. */
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
