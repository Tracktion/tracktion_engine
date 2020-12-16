/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
//==============================================================================
FadeInOutNode::FadeInOutNode (std::unique_ptr<tracktion_graph::Node> inputNode,
                              tracktion_graph::PlayHeadState& playHeadStateToUse,
                              EditTimeRange in, EditTimeRange out,
                              AudioFadeCurve::Type fadeInType_, AudioFadeCurve::Type fadeOutType_,
                              bool clearSamplesOutsideFade)
    : input (std::move (inputNode)),
      playHeadState (playHeadStateToUse),
      fadeIn (in),
      fadeOut (out),
      fadeInType (fadeInType_),
      fadeOutType (fadeOutType_),
      clearExtraSamples (clearSamplesOutsideFade)
{
    jassert (! (fadeIn.isEmpty() && fadeOut.isEmpty()));

    setOptimisations ({ tracktion_graph::ClearBuffers::no,
                        tracktion_graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion_graph::NodeProperties FadeInOutNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion_graph::Node*> FadeInOutNode::getDirectInputNodes()
{
    return { input.get() };
}

void FadeInOutNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    fadeInSampleRange = tracktion_graph::timeToSample (fadeIn, info.sampleRate);
    fadeOutSampleRange = tracktion_graph::timeToSample (fadeOut, info.sampleRate);
}

bool FadeInOutNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void FadeInOutNode::process (ProcessContext& pc)
{
    const auto timelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange).timelineRange1;
    
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    auto& destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getSize() == destAudioBlock.getSize());

    destMidiBlock.copyFrom (sourceBuffers.midi);

    if (! renderingNeeded (timelineRange))
    {
        // If we don't need to apply the fade, just pass through the buffer
        setAudioOutput (sourceBuffers.audio);
        return;
    }

    // Otherwise copy the source in to the dest ready for fading
    copy (destAudioBlock, sourceBuffers.audio);

    auto numSamples = destAudioBlock.getNumFrames();
    jassert (numSamples == timelineRange.getLength());

    if (timelineRange.intersects (fadeInSampleRange) && fadeInSampleRange.getLength() > 0)
    {
        double alpha1 = 0;
        auto startSamp = int (fadeInSampleRange.getStart() - timelineRange.getStart());

        if (startSamp > 0)
        {
            if (clearExtraSamples)
                destAudioBlock.getStart ((choc::buffer::FrameCount) startSamp).clear();
        }
        else
        {
            alpha1 = (timelineRange.getStart() - fadeInSampleRange.getStart()) / (double) fadeInSampleRange.getLength();
            startSamp = 0;
        }

        int endSamp;
        double alpha2;

        if (timelineRange.getEnd() >= fadeInSampleRange.getEnd())
        {
            endSamp = int (timelineRange.getEnd() - fadeInSampleRange.getEnd());
            alpha2 = 1.0;
        }
        else
        {
            endSamp = (int) timelineRange.getLength();
            alpha2 = juce::jmax (0.0, (timelineRange.getEnd() - fadeInSampleRange.getStart()) / (double) fadeInSampleRange.getLength());
        }

        if (endSamp > startSamp)
        {
            auto buffer = tracktion_graph::toAudioBuffer (destAudioBlock);
            AudioFadeCurve::applyCrossfadeSection (buffer,
                                                   startSamp, endSamp - startSamp,
                                                   fadeInType,
                                                   (float) alpha1,
                                                   (float) alpha2);
        }
    }
    
    if (timelineRange.intersects (fadeOutSampleRange) && fadeOutSampleRange.getLength() > 0)
    {
        double alpha1 = 0;
        auto startSamp = int (fadeOutSampleRange.getStart() - timelineRange.getStart());

        if (startSamp <= 0)
        {
            startSamp = 0;
            alpha1 = (timelineRange.getStart() - fadeOutSampleRange.getStart()) / (double) fadeOutSampleRange.getLength();
        }

        uint32_t endSamp;
        double alpha2;

        if (timelineRange.getEnd() >= fadeOutSampleRange.getEnd())
        {
            endSamp = (uint32_t) (timelineRange.getEnd() - fadeOutSampleRange.getEnd());
            alpha2 = 1.0;

            if (clearExtraSamples && endSamp < numSamples)
                destAudioBlock.fromFrame (endSamp).clear();
        }
        else
        {
            endSamp = numSamples;
            alpha2 = (timelineRange.getEnd() - fadeOutSampleRange.getStart()) / (double) fadeOutSampleRange.getLength();
        }

        if (endSamp > (uint32_t) startSamp)
        {
            auto buffer = tracktion_graph::toAudioBuffer (destAudioBlock);
            AudioFadeCurve::applyCrossfadeSection (buffer,
                                                   startSamp, (int) endSamp - startSamp,
                                                   fadeOutType,
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha1)),
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha2)));
        }
    }
}

bool FadeInOutNode::renderingNeeded (const juce::Range<int64_t>& timelineSampleRange) const
{
    if (! playHeadState.playHead.isPlaying())
        return false;

    return fadeInSampleRange.intersects (timelineSampleRange)
        || fadeOutSampleRange.intersects (timelineSampleRange);
}

} // namespace tracktion_engine
