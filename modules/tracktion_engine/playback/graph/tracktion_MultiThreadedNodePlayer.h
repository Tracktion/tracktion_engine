/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    Plays back a Node with PlayHeadState and ProcessState.
*/
class MultiThreadedNodePlayer
{
public:
    /** Creates an NodePlayer to process a Node. */
    MultiThreadedNodePlayer (ProcessState& processStateToUse)
        : playHeadState (processStateToUse.playHeadState),
          processState (processStateToUse)
    {
    }

    /** Creates an NodePlayer to process a Node. */
    MultiThreadedNodePlayer (std::unique_ptr<tracktion::graph::Node> node,
                             ProcessState& processStateToUse,
                             double sampleRate, int blockSize)
        : MultiThreadedNodePlayer (processStateToUse)
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

    tracktion::graph::Node* getNode()
    {
        return nodePlayer.getNode();
    }

    void setNode (std::unique_ptr<tracktion::graph::Node> newNode)
    {
        nodePlayer.setNode (std::move (newNode));
    }

    void setNode (std::unique_ptr<tracktion::graph::Node> newNode, double sampleRateToUse, int blockSizeToUse)
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
    int process (const tracktion::graph::Node::ProcessContext& pc)
    {
        int numMisses = 0;
        playHeadState.playHead.setReferenceSampleRange (pc.referenceSampleRange);
        
        // Check to see if the timeline needs to be processed in two halves due to looping
        const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange);
        
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

                processState.update (nodePlayer.getSampleRate(), firstReferenceRange, ProcessState::UpdateContinuityFlags::yes);
                tracktion::graph::Node::ProcessContext pc1 { firstNumSamples, firstReferenceRange, { destAudio , destMidi } };
                numMisses += nodePlayer.process (pc1);
            }

            {
                const auto firstDuration = processState.editTimeRange.getLength();

                auto destAudio = pc.buffers.audio.getFrameRange ({ firstNumSamples, firstNumSamples + secondNumSamples });
                scratchMidi.clear();

                tracktion::graph::Node::ProcessContext pc2 { secondNumSamples, secondReferenceRange, { destAudio, scratchMidi } };
                processState.update (nodePlayer.getSampleRate(), secondReferenceRange, ProcessState::UpdateContinuityFlags::yes);
                numMisses += nodePlayer.process (pc2);

                // Merge back MIDI from end of block
                pc.buffers.midi.mergeFromWithOffset (scratchMidi, firstDuration.inSeconds());
            }
        }
        else
        {
            processState.update (nodePlayer.getSampleRate(), pc.referenceSampleRange, ProcessState::UpdateContinuityFlags::yes);
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
    tracktion::graph::PlayHeadState& playHeadState;
    ProcessState& processState;
    MidiMessageArray scratchMidi;
    tracktion::graph::MultiThreadedNodePlayer nodePlayer;
};

}} // namespace tracktion { inline namespace engine
