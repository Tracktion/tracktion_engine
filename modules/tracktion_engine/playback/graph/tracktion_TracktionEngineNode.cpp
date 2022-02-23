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

ProcessState::ProcessState (tracktion::graph::PlayHeadState& phs)
    : playHeadState (phs)
{
}

ProcessState::ProcessState (tracktion::graph::PlayHeadState& phs, const TempoSequence& seq)
    : playHeadState (phs), tempoPosition (std::make_unique<TempoSequencePosition> (seq))
{
}

void ProcessState::update (double newSampleRate, juce::Range<int64_t> newReferenceSampleRange)
{
    if (sampleRate != newSampleRate)
        playHeadState.playHead.setScrubbingBlockLength (toSamples (TimeDuration (0.08s), newSampleRate));
    
    playHeadState.playHead.setReferenceSampleRange (newReferenceSampleRange);
    playHeadState.update (newReferenceSampleRange);

    sampleRate = newSampleRate;
    numSamples = (int) newReferenceSampleRange.getLength();
    referenceSampleRange = newReferenceSampleRange;
    
    const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, newReferenceSampleRange);
    jassert (! splitTimelineRange.isSplit);
    timelineSampleRange = splitTimelineRange.timelineRange1;
    jassert (timelineSampleRange.getLength() == 0
             || timelineSampleRange.getLength() == newReferenceSampleRange.getLength());

    editTimeRange = timeRangeFromSamples (timelineSampleRange, sampleRate);

    if (! tempoPosition)
        return;
    
    tempoPosition->setTime (editTimeRange.getStart());
    const auto beatStart = tempoPosition->getBeats();
    tempoPosition->setTime (editTimeRange.getEnd());
    const auto beatEnd = tempoPosition->getBeats();
    editBeatRange = { beatStart, beatEnd };
}

//==============================================================================
//==============================================================================
TracktionEngineNode::TracktionEngineNode (ProcessState& ps)
    : processState (ps)
{
}


}} // namespace tracktion { inline namespace engine
