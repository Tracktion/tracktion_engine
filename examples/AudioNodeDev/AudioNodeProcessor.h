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
        : input (std::move (nodeToProcess))
    {
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *input };
        std::function<void (AudioNode&)> visitor = [&] (AudioNode& n) { n.initialise (info); };
        visitInputs (*input, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes.clear();
        
        std::function<void (AudioNode&)> visitor2 = [&] (AudioNode& n) { allNodes.push_back (&n); };
        visitInputs (*input, visitor2);
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
                    node->process (juce::Range<int64_t>::withStartAndLength (0, (int64_t) pc.buffers.audio.getNumSamples()));
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                auto output = input->getProcessedOutput();
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.addEvents (output.midi, 0, -1, 0);
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<AudioNode> input;
    std::vector<AudioNode*> allNodes;
};
