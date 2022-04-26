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
class TracktionNodePlayer
{
public:
    /** Creates an NodePlayer to process a Node. */
    TracktionNodePlayer (ProcessState& processStateToUse,
                         tracktion::graph::LockFreeMultiThreadedNodePlayer::ThreadPoolCreator poolCreator)
        : playHeadState (processStateToUse.playHeadState),
          processState (processStateToUse),
          nodePlayer (std::move (poolCreator))
    {
    }

    /** Creates an NodePlayer to process a Node. */
    TracktionNodePlayer (std::unique_ptr<tracktion::graph::Node> node,
                         ProcessState& processStateToUse,
                         double sampleRate, int blockSize,
                         tracktion::graph::LockFreeMultiThreadedNodePlayer::ThreadPoolCreator poolCreator)
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
            const auto firstRangeLength = splitTimelineRange.timelineRange1.getLength();

            numMisses += processReferenceRange (pc, pc.referenceSampleRange.withLength (firstRangeLength));
            numMisses += processReferenceRange (pc, pc.referenceSampleRange.withStart (pc.referenceSampleRange.getStart() + firstRangeLength));
        }
        else
        {
            numMisses += processReferenceRange (pc, pc.referenceSampleRange);
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
    
    /** @internal */
    void enablePooledMemoryAllocations (bool enablePooledMemory)
    {
        nodePlayer.enablePooledMemoryAllocations (enablePooledMemory);
    }
    
private:
    tracktion::graph::PlayHeadState& playHeadState;
    ProcessState& processState;
    MidiMessageArray scratchMidi;
    tracktion::graph::LockFreeMultiThreadedNodePlayer nodePlayer;

    tracktion::graph::Node::ProcessContext getSubProcessContext (const tracktion::graph::Node::ProcessContext& pc, juce::Range<int64_t> subReferenceSampleRange)
    {
        jassert (! pc.referenceSampleRange.isEmpty());
        jassert (pc.referenceSampleRange.contains (subReferenceSampleRange));

        const auto originalReferenceLength = (double) pc.referenceSampleRange.getLength();
        const juce::Range<double> proportion ((subReferenceSampleRange.getStart() - pc.referenceSampleRange.getStart()) / originalReferenceLength,
                                              (subReferenceSampleRange.getEnd() - pc.referenceSampleRange.getStart()) / originalReferenceLength);

        const auto startSample  = (choc::buffer::FrameCount) std::llround (proportion.getStart() * pc.numSamples);
        const auto endSample    = (choc::buffer::FrameCount) std::llround (proportion.getEnd() * pc.numSamples);
        const choc::buffer::FrameRange sampleRange { startSample, endSample };

        auto destAudio = pc.buffers.audio.getFrameRange (sampleRange);

        return { sampleRange.size(), subReferenceSampleRange, { destAudio, pc.buffers.midi } };
    }

    int processReferenceRange (const tracktion::graph::Node::ProcessContext& pc, juce::Range<int64_t> referenceSampleRange)
    {
        return processTempoChanges (getSubProcessContext (pc, referenceSampleRange));
    }

    int processTempoChanges (const tracktion::graph::Node::ProcessContext& pc)
    {
        int numMisses = 0;
        playHeadState.playHead.setReferenceSampleRange (pc.referenceSampleRange);

        const auto sampleRate = nodePlayer.getSampleRate();
        processState.update (sampleRate, pc.referenceSampleRange, ProcessState::UpdateContinuityFlags::no);
        const auto timeRange = processState.editTimeRange;

        if (processState.tempoPosition)
        {
            double startProportion = 0.0;
            auto lastEventPosition = timeRange.getStart();

            for (;;)
            {
                const auto nextTempoChangePosition = processState.tempoPosition->getTimeOfNextChange();

                if (nextTempoChangePosition == lastEventPosition)
                    break;

                if (! timeRange.contains (nextTempoChangePosition))
                    break;

                const double proportion = (nextTempoChangePosition - timeRange.getStart()) / timeRange.getLength();
                const auto numSamples = static_cast<decltype(pc.numSamples)> (std::llround (pc.numSamples * proportion));
                lastEventPosition = nextTempoChangePosition;

                // Min chunk size of 128 samples to avoid large jitter
                if (numSamples < 128)
                    continue;

                processSubRange (pc, { startProportion, proportion });
                startProportion = proportion;
            }

            // Process any remaining buffer proportion
            if (startProportion < 1.0)
                processSubRange (pc, { startProportion, 1.0 });
        }
        else
        {
            processSubRange (pc, { 0.0, 1.0 });
        }

        return numMisses;
    }

    int processSubRange (const tracktion::graph::Node::ProcessContext& pc, juce::Range<double> proportion)
    {
        assert (pc.numSamples > 0);
        assert (proportion.getStart() >= 0.0);
        assert (proportion.getEnd() <= 1.0);
        const auto sampleRate = nodePlayer.getSampleRate();

        const auto startReferenceSample = pc.referenceSampleRange.getStart() + (int64_t) std::llround (proportion.getStart() * pc.referenceSampleRange.getLength());
        const auto endReferenceSample   = pc.referenceSampleRange.getStart() + (int64_t) std::llround (proportion.getEnd() * pc.referenceSampleRange.getLength());
        const juce::Range<int64_t> referenceRange (startReferenceSample, endReferenceSample);

        const auto startSample  = (choc::buffer::FrameCount) std::llround (proportion.getStart() * pc.numSamples);
        const auto endSample    = (choc::buffer::FrameCount) std::llround (proportion.getEnd() * pc.numSamples);
        const choc::buffer::FrameRange sampleRange { startSample, endSample };

        auto destAudio = pc.buffers.audio.getFrameRange (sampleRange);
        scratchMidi.clear();

        tracktion::graph::Node::ProcessContext pc2 { sampleRange.size(), referenceRange, { destAudio, scratchMidi } };
        processState.update (sampleRate, referenceRange, ProcessState::UpdateContinuityFlags::yes);
        const auto numMisses = nodePlayer.process (pc2);

        // Merge back MIDI from end of block
        const auto offset = TimeDuration::fromSamples (startReferenceSample - pc.referenceSampleRange.getStart(), sampleRate);
        pc.buffers.midi.mergeFromWithOffset (scratchMidi, offset.inSeconds());

        return numMisses;
    }
};

}} // namespace tracktion { inline namespace engine
