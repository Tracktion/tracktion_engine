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
TimedMutingNode::TimedMutingNode (std::unique_ptr<tracktion::graph::Node> inputNode,
                                  juce::Array<TimeRange> muteTimes_,
                                  tracktion::graph::PlayHeadState& playHeadStateToUse)
    : input (std::move (inputNode)),
      playHeadState (playHeadStateToUse),
      muteTimes (std::move (muteTimes_))
{
    jassert (! muteTimes.isEmpty());

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties TimedMutingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion::graph::Node*> TimedMutingNode::getDirectInputNodes()
{
    return { input.get() };
}

void TimedMutingNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

bool TimedMutingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TimedMutingNode::process (ProcessContext& pc)
{
    const auto timelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange).timelineRange1;
    
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    auto& destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getSize() == destAudioBlock.getSize());

    destMidiBlock.copyFrom (sourceBuffers.midi);

    if (! playHeadState.playHead.isPlaying())
    {
        // If we're not playing, jas pass the source to our destination
        setAudioOutput (input.get(), sourceBuffers.audio);
        return;
    }

    copy (destAudioBlock, sourceBuffers.audio);
    processSection (destAudioBlock, tracktion::timeRangeFromSamples (timelineRange, sampleRate));
}

void TimedMutingNode::processSection (choc::buffer::ChannelArrayView<float> view, TimeRange editTime)
{
    for (auto r : muteTimes)
    {
        if (r.overlaps (editTime))
        {
            auto mute = r.getIntersectionWith (editTime);

            if (! mute.isEmpty())
            {
                if (mute == editTime)
                {
                    view.clear();
                }
                else if (editTime.contains (mute))
                {
                    auto startSample = tracktion::toSamples (mute.getStart() - editTime.getStart(), sampleRate);
                    auto numSamples  = tracktion::toSamples (mute.getLength(), sampleRate);
                    muteSection (view, startSample, numSamples);
                }
                else if (mute.getEnd() <= editTime.getEnd())
                {
                    auto numSamples = tracktion::toSamples (editTime.getEnd() - mute.getEnd(), sampleRate);
                    muteSection (view, 0, numSamples);
                }
                else if (mute.getStart() >= editTime.getStart())
                {
                    auto startSample = tracktion::toSamples (mute.getStart() - editTime.getStart(), sampleRate);
                    muteSection (view, startSample, choc::buffer::FrameCount (view.getNumFrames()) - startSample);
                }
            }
        }

        if (r.getEnd() >= editTime.getEnd())
            return;
    }
}

void TimedMutingNode::muteSection (choc::buffer::ChannelArrayView<float> block, int64_t startSample, int64_t numSamples)
{
    if (numSamples > 0)
        block.getFrameRange ({ (choc::buffer::FrameCount) startSample,
                               (choc::buffer::FrameCount) (startSample + numSamples) }).clear();
}

}} // namespace tracktion { inline namespace engine
