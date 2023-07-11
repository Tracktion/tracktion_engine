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
    : ProcessState (phs, seq.getInternalSequence())
{
}

ProcessState::ProcessState (tracktion::graph::PlayHeadState& phs, const tempo::Sequence& seq)
    : playHeadState (phs),
      tempoSequence (&seq),
      tempoPosition (std::make_unique<tempo::Sequence::Position> (seq))
{
}

void ProcessState::update (double newSampleRate, juce::Range<int64_t> newReferenceSampleRange, UpdateContinuityFlags updateContinuityFlags)
{
    if (sampleRate != newSampleRate)
        playHeadState.playHead.setScrubbingBlockLength (toSamples (TimeDuration (0.08s), newSampleRate));
    
    playHeadState.playHead.setReferenceSampleRange (newReferenceSampleRange);

    if (updateContinuityFlags == UpdateContinuityFlags::yes)
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

    tempoPosition->set (editTimeRange.getStart());
    const auto beatStart = tempoPosition->getBeats();
    tempoPosition->set (editTimeRange.getEnd());
    const auto beatEnd = tempoPosition->getBeats();
    editBeatRange = { beatStart, beatEnd };
}

void ProcessState::setPlaybackSpeedRatio (double newRatio)
{
    playbackSpeedRatio = newRatio;
}

void ProcessState::setTempoSequence (const tempo::Sequence* ts)
{
    if (tempoSequence == ts)
        return;

    tempoSequence = ts;

    if (tempoSequence)
        tempoPosition = std::make_unique<tempo::Sequence::Position> (*tempoSequence);
    else
        tempoPosition.reset();
}

const tempo::Sequence* ProcessState::getTempoSequence() const
{
    return tempoSequence;
}

const tempo::Sequence::Position* ProcessState::getTempoSequencePosition() const
{
    return tempoPosition.get();
}


//==============================================================================
//==============================================================================
TracktionEngineNode::TracktionEngineNode (ProcessState& ps)
    : processState (&ps)
{
}

tempo::Key TracktionEngineNode::getKey() const
{
    if (auto tempoPosition = processState->getTempoSequencePosition())
        return tempoPosition->getKey();

    return {};
}

double TracktionEngineNode::getPlaybackSpeedRatio() const
{
    return processState->playbackSpeedRatio;
}

std::optional<TimePosition> TracktionEngineNode::getTimeOfNextChange() const
{
    if (auto tempoPosition = processState->getTempoSequencePosition())
        return tempoPosition->getTimeOfNextChange();

    return {};
}

std::optional<BeatPosition> TracktionEngineNode::getBeatOfNextChange() const
{
    if (auto tempoPosition = processState->getTempoSequencePosition())
        return tempoPosition->getBeatOfNextChange();

    return {};
}

void TracktionEngineNode::setProcessState (ProcessState& newProcessState)
{
    processState = &newProcessState;
}

}} // namespace tracktion { inline namespace engine
