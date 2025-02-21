/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

    if (updateContinuityFlags == UpdateContinuityFlags::no)
        return;

    // Only update the sync range if the continuity is being updated as the player can call this
    // multiple times and we don't want to increment the monotonic beat each call
    const auto unloopTimelineSample = playHeadState.playHead.referenceSamplePositionToTimelinePositionUnlooped (referenceSampleRange.getEnd());
    auto oldSyncPoint = getSyncPoint();
    const auto newSyncPoint = SyncPoint ({
                                             referenceSampleRange.getEnd(),
                                             { oldSyncPoint.monotonicBeat.v + editBeatRange.getLength() },
                                             TimePosition::fromSamples (unloopTimelineSample, sampleRate),
                                             editTimeRange.getEnd(),
                                             beatEnd
                                         });

    oldSyncPoint.time = editTimeRange.getStart();
    oldSyncPoint.beat = beatStart;
    syncRange.store ({ oldSyncPoint, newSyncPoint });

    if (onContinuityUpdated)
        onContinuityUpdated();
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

SyncPoint ProcessState::getSyncPoint() const
{
    return getSyncRange().end;
}

SyncRange ProcessState::getSyncRange() const
{
    return syncRange.load();
}

void ProcessState::setSyncRange (SyncRange r)
{
    syncRange.store (std::move (r));
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
