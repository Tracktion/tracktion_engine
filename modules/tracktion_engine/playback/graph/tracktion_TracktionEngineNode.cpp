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
    : playHeadState (phs),
      tempoSequence (&seq),
      tempoPosition (std::make_unique<tempo::Sequence::Position> (seq.getInternalSequence()))
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

    tempoPosition->set (editTimeRange.getStart() + tempoOffsetTime);
    const auto beatStart = tempoPosition->getBeats() - tempoOffsetBeats;
    tempoPosition->set (editTimeRange.getEnd() + tempoOffsetTime);
    const auto beatEnd = tempoPosition->getBeats() - tempoOffsetBeats;
    editBeatRange = { beatStart, beatEnd };
}

void ProcessState::setPlaybackSpeedRatio (double newRatio)
{
    playbackSpeedRatio = newRatio;
}

void ProcessState::setTempoSequence (const ProcessState& other, BeatDuration offset)
{
    setTempoSequence (other.tempoSequence, offset);
}

void ProcessState::setTempoSequence (const TempoSequence* ts, BeatDuration offset)
{
    if (tempoSequence == ts)
        return;

    tempoSequence = ts;

    if (tempoSequence)
    {
        tempoPosition = std::make_unique<tempo::Sequence::Position> (tempoSequence->getInternalSequence());
        tempoOffsetBeats = offset;
        tempoOffsetTime = toDuration (tempoSequence->toTime (toPosition (offset)));
    }
    else
    {
        tempoPosition.reset();
        tempoOffsetBeats = {};
        tempoOffsetTime = {};
    }
}

std::unique_ptr<tempo::Sequence::Position> ProcessState::createPosition()
{
    assert (tempoPosition != nullptr);
    return std::make_unique<tempo::Sequence::Position> (*tempoPosition);
}

bool ProcessState::hasTempoSequence() const
{
    assert (tempoSequence == nullptr || tempoPosition != nullptr);
    return tempoSequence != nullptr;
}

BeatPosition ProcessState::toBeats (TimePosition t)
{
    assert (hasTempoSequence());
    return tempoSequence->toBeats (t + tempoOffsetTime) - tempoOffsetBeats;
}

BeatRange ProcessState::toBeats (TimeRange t)
{
    assert (hasTempoSequence());
    return tempoSequence->toBeats (t + tempoOffsetTime) - tempoOffsetBeats;
}

TimePosition ProcessState::toTime (BeatPosition t)
{
    assert (hasTempoSequence());
    return tempoSequence->toTime (t + tempoOffsetBeats) - tempoOffsetTime;
}

TimeRange ProcessState::toTime (BeatRange t)
{
    assert (hasTempoSequence());
    return tempoSequence->toTime (t + tempoOffsetBeats) - tempoOffsetTime;
}

TimePosition ProcessState::getTimeOfNextChange() const
{
    assert (hasTempoSequence());
    return tempoPosition->getTimeOfNextChange() - tempoOffsetTime;
}

BeatPosition ProcessState::getBeatOfNextChange() const
{
    assert (hasTempoSequence());
    return tempoPosition->getBeatOfNextChange() - tempoOffsetBeats;
}

tempo::Key ProcessState::getKey() const
{
    assert (hasTempoSequence());
    return tempoPosition->getKey();
}


//==============================================================================
//==============================================================================
TracktionEngineNode::TracktionEngineNode (ProcessState& ps)
    : processState (&ps)
{
}

tempo::Key TracktionEngineNode::getKey() const
{
    if (processState->hasTempoSequence())
        return processState->getKey();

    return {};
}

double TracktionEngineNode::getPlaybackSpeedRatio() const
{
    return processState->playbackSpeedRatio;
}

std::optional<TimePosition> TracktionEngineNode::getTimeOfNextChange() const
{
    if (processState->hasTempoSequence())
        return processState->getTimeOfNextChange();

    return {};
}

std::optional<BeatPosition> TracktionEngineNode::getBeatOfNextChange() const
{
    if (processState->hasTempoSequence())
        return processState->getBeatOfNextChange();

    return {};
}

void TracktionEngineNode::setProcessState (ProcessState& newProcessState)
{
    processState = &newProcessState;
}

}} // namespace tracktion { inline namespace engine
