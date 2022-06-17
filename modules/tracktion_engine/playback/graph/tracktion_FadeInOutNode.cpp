/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
FadeInOutNode::FadeInOutNode (std::unique_ptr<tracktion::graph::Node> inputNode,
                              ProcessState& ps,
                              TimeRange in, TimeRange out,
                              AudioFadeCurve::Type fadeInType_, AudioFadeCurve::Type fadeOutType_,
                              bool clearSamplesOutsideFade)
    : TracktionEngineNode (ps),
      input (std::move (inputNode)),
      fadeIn (in),
      fadeOut (out),
      fadeInType (fadeInType_),
      fadeOutType (fadeOutType_),
      clearExtraSamples (clearSamplesOutsideFade)
{
    jassert (! (fadeIn.isEmpty() && fadeOut.isEmpty()));

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties FadeInOutNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion::graph::Node*> FadeInOutNode::getDirectInputNodes()
{
    return { input.get() };
}

bool FadeInOutNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void FadeInOutNode::process (ProcessContext& pc)
{
    const auto editTimeRange = getEditTimeRange();

    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    auto& destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getSize() == destAudioBlock.getSize());

    destMidiBlock.copyFrom (sourceBuffers.midi);

    if (! renderingNeeded (editTimeRange))
    {
        // If we don't need to apply the fade, just pass through the buffer
        setAudioOutput (input.get(), sourceBuffers.audio);
        return;
    }

    // Otherwise copy the source in to the dest ready for fading
    tracktion::graph::copyIfNotAliased (destAudioBlock, sourceBuffers.audio);

    processSection (destAudioBlock, editTimeRange);
}

void FadeInOutNode::processSection (choc::buffer::ChannelArrayView<float> audio, TimeRange editTime)
{
    const auto numSamples = (int) audio.getNumFrames();

    if (editTime.overlaps (fadeIn) && fadeIn.getLength() > 0s)
    {
        double alpha1 = 0;
        auto startSamp = timeToSample (numSamples, editTime, fadeIn.getStart());

        if (startSamp > 0)
        {
            if (clearExtraSamples)
                audio.getStart ((choc::buffer::FrameCount) startSamp).clear();
        }
        else
        {
            alpha1 = (editTime.getStart() - fadeIn.getStart()) / fadeIn.getLength();
            startSamp = 0;
        }

        int endSamp;
        double alpha2;

        if (editTime.getEnd() >= fadeIn.getEnd())
        {
            endSamp = timeToSample (numSamples, editTime, fadeIn.getEnd());
            alpha2 = 1.0;
        }
        else
        {
            endSamp = numSamples;
            alpha2 = std::max (0.0, (editTime.getEnd() - fadeIn.getStart()) / fadeIn.getLength());
        }

        if (endSamp > startSamp)
        {
            auto buffer = tracktion::graph::toAudioBuffer (audio);
            AudioFadeCurve::applyCrossfadeSection (buffer,
                                                   startSamp, endSamp - startSamp,
                                                   fadeInType,
                                                   (float) alpha1,
                                                   (float) alpha2);
        }
    }

    if (editTime.overlaps (fadeOut) && fadeOut.getLength() > 0s)
    {
        double alpha1 = 0;
        auto startSamp = timeToSample (numSamples, editTime, fadeOut.getStart());

        if (startSamp <= 0)
        {
            startSamp = 0;
            alpha1 = (editTime.getStart() - fadeOut.getStart()) / fadeOut.getLength();
        }

        int endSamp;
        double alpha2;

        if (editTime.getEnd() >= fadeOut.getEnd())
        {
            endSamp = timeToSample (numSamples, editTime, fadeOut.getEnd());
            alpha2 = 1.0;

            if (clearExtraSamples && endSamp < numSamples)
                audio.getEnd ((choc::buffer::FrameCount) endSamp);
        }
        else
        {
            endSamp = numSamples;
            alpha2 = (editTime.getEnd() - fadeOut.getStart()) / fadeOut.getLength();
        }

        if (endSamp > startSamp)
        {
            auto buffer = tracktion::graph::toAudioBuffer (audio);
            AudioFadeCurve::applyCrossfadeSection (buffer,
                                                   startSamp, endSamp - startSamp,
                                                   fadeOutType,
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha1)),
                                                   juce::jlimit (0.0f, 1.0f, (float) (1.0 - alpha2)));
        }
    }
}

bool FadeInOutNode::renderingNeeded (const TimeRange timelineRange)
{
    if (! getPlayHead().isPlaying())
        return false;

    return fadeIn.intersects (timelineRange)
        || fadeOut.intersects (timelineRange)
        || (clearExtraSamples && (timelineRange.getStart() <= fadeIn.getStart()
                                  || timelineRange.getEnd() >= fadeOut.getEnd()));
}

int FadeInOutNode::timeToSample (int numSamples, TimeRange editTime, TimePosition t)
{
    return (int) (((t - editTime.getStart()) / editTime.getLength() * numSamples) + 0.5);
}

}} // namespace tracktion { inline namespace engine
