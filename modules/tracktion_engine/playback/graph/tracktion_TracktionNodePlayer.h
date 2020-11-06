/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_engine
{

//==============================================================================
//==============================================================================
/**
    Plays back a Node with PlayHeadState and ProcessState.
*/
class TracktionNodePlayer
{
public:
    /** Creates an NodePlayer to process a Node. */
    TracktionNodePlayer (ProcessState& processStateToUse,
                         tracktion_graph::LockFreeMultiThreadedNodePlayer::ThreadPoolCreator poolCreator)
        : playHeadState (processStateToUse.playHeadState),
          processState (processStateToUse),
          nodePlayer (std::move (poolCreator))
    {
    }

    /** Creates an NodePlayer to process a Node. */
    TracktionNodePlayer (std::unique_ptr<tracktion_graph::Node> node,
                         ProcessState& processStateToUse,
                         double sampleRate, int blockSize,
                         tracktion_graph::LockFreeMultiThreadedNodePlayer::ThreadPoolCreator poolCreator)
        : TracktionNodePlayer (processStateToUse, std::move (poolCreator))
    {
        nodePlayer.setNode (std::move (node), sampleRate, blockSize);
    }
    
    /** Sets the number of threads to use for rendering.
        This can be 0 in which case only the process calling thread will be used for processing.
        N.B. this will pause processing whilst updating the threads so there will be a gap in the audio.
    */
    void setNumThreads (size_t numThreads)
    {
        nodePlayer.setNumThreads (numThreads);
    }

    tracktion_graph::Node* getNode()
    {
        return nodePlayer.getNode();
    }

    void setNode (std::unique_ptr<tracktion_graph::Node> newNode)
    {
        nodePlayer.setNode (std::move (newNode));
    }

    void setNode (std::unique_ptr<tracktion_graph::Node> newNode, double sampleRateToUse, int blockSizeToUse)
    {
        nodePlayer.setNode (std::move (newNode), sampleRateToUse, blockSizeToUse);
    }
    
    void prepareToPlay (double sampleRateToUse, int blockSizeToUse)
    {
        nodePlayer.prepareToPlay (sampleRateToUse, blockSizeToUse);
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const tracktion_graph::Node::ProcessContext& pc)
    {
        int numMisses = 0;
        playHeadState.playHead.setReferenceSampleRange (pc.referenceSampleRange);
        
        // Check to see if the timeline needs to be processed in two halves due to looping
        const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange);
        
        if (splitTimelineRange.isSplit)
        {
            const auto firstNumSamples = splitTimelineRange.timelineRange1.getLength();
            const auto firstRange = pc.referenceSampleRange.withLength (firstNumSamples);

            {
                auto destAudio = pc.buffers.audio.getSubBlock (0, (size_t) firstNumSamples);
                auto& destMidi = pc.buffers.midi;
                
                processState.update (nodePlayer.getSampleRate(), firstRange);
                tracktion_graph::Node::ProcessContext pc1 { firstRange, { destAudio , destMidi } };
                numMisses += nodePlayer.process (pc1);
            }
            
            {
                const double firstDuration = processState.editTimeRange.getLength();
                const auto secondNumSamples = splitTimelineRange.timelineRange2.getLength();
                const auto secondRange = juce::Range<int64_t>::withStartAndLength (firstRange.getEnd(), secondNumSamples);
                
                auto destAudio = pc.buffers.audio.getSubBlock ((size_t) firstNumSamples, (size_t) secondNumSamples);
                scratchMidi.clear();
                
                tracktion_graph::Node::ProcessContext pc2 { secondRange, { destAudio , scratchMidi } };
                processState.update (nodePlayer.getSampleRate(), secondRange);
                numMisses += nodePlayer.process (pc2);

                // Merge back MIDI from end of block
                pc.buffers.midi.mergeFromWithOffset (scratchMidi, firstDuration);
            }
        }
        else
        {
            processState.update (nodePlayer.getSampleRate(), pc.referenceSampleRange);
            numMisses += nodePlayer.process (pc);
        }
        
        return numMisses;
    }
    
    /** Clears the Node currently playing. */
    void clearNode()
    {
        nodePlayer.clearNode();
    }
    
    /** Returns the current sample rate. */
    double getSampleRate() const
    {
        return nodePlayer.getSampleRate();        
    }
    
private:
    tracktion_graph::PlayHeadState& playHeadState;
    ProcessState& processState;
    MidiMessageArray scratchMidi;
    tracktion_graph::LockFreeMultiThreadedNodePlayer nodePlayer;
};

}
