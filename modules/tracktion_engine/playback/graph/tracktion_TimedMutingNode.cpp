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
TimedMutingNode::TimedMutingNode (std::unique_ptr<tracktion_graph::Node> inputNode,
                                  juce::Array<EditTimeRange> muteTimes_,
                                  tracktion_graph::PlayHeadState& playHeadStateToUse)
    : input (std::move (inputNode)),
      playHeadState (playHeadStateToUse),
      muteTimes (std::move (muteTimes_))
{
    jassert (! muteTimes.isEmpty());
}

//==============================================================================
tracktion_graph::NodeProperties TimedMutingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion_graph::Node*> TimedMutingNode::getDirectInputNodes()
{
    return { input.get() };
}

void TimedMutingNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

bool TimedMutingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TimedMutingNode::process (const ProcessContext& pc)
{
    const auto timelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange).timelineRange1;
    
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    auto destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getNumChannels() == destAudioBlock.getNumChannels());

    destMidiBlock.copyFrom (sourceBuffers.midi);
    destAudioBlock.copyFrom (sourceBuffers.audio);

    if (! playHeadState.playHead.isPlaying())
        return;
    
    processSection (destAudioBlock, sampleToTime (timelineRange, sampleRate));
}

void TimedMutingNode::processSection (const juce::dsp::AudioBlock<float>& block, EditTimeRange editTime)
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
                    block.clear();
                }
                else if (editTime.contains (mute))
                {
                    auto startSample = timeToSample (mute.getStart() - editTime.getStart(), sampleRate);
                    auto numSamples = timeToSample (mute.getEnd() - mute.getStart(), sampleRate);
                    muteSection (block, startSample, numSamples);
                }
                else if (mute.getEnd() <= editTime.getEnd())
                {
                    auto numSamples = timeToSample (editTime.getEnd() - mute.getEnd(), sampleRate);
                    muteSection (block, 0, numSamples);
                }
                else if (mute.getStart() >= editTime.getStart())
                {
                    auto startSample = timeToSample (mute.getStart() - editTime.getStart(), sampleRate);
                    muteSection (block, startSample, int64_t (block.getNumSamples()) - startSample);
                }
            }
        }

        if (r.getEnd() >= editTime.getEnd())
            return;
    }
}

void TimedMutingNode::muteSection (const juce::dsp::AudioBlock<float>& block, int64_t startSample, int64_t numSamples)
{
    if (numSamples == 0)
        return;
    
    block.getSubBlock ((size_t) startSample, (size_t) numSamples).clear();
}

} // namespace tracktion_engine
