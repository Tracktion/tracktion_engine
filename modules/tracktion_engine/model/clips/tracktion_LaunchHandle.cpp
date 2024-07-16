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

LaunchHandle::State::State() = default;

//==============================================================================
LaunchHandle::PlayState LaunchHandle::getPlayingStatus() const
{
    return getState().status;
}

std::optional<LaunchHandle::QueueState> LaunchHandle::getQueuedStatus() const
{
    return getState().nextStatus;
}

std::optional<MonotonicBeat> LaunchHandle::getQueuedEventPosition() const
{
    return getState().nextEventTime;
}

void LaunchHandle::play (std::optional<MonotonicBeat> pos)
{
    auto s = getState();

    s.nextStatus = QueueState::playQueued;
    s.nextEventTime = pos;

    setState (std::move (s));
}

void LaunchHandle::stop (std::optional<MonotonicBeat> pos)
{
    auto s = getState();

    if (s.status == PlayState::stopped)
    {
        if (s.nextStatus == QueueState::playQueued)
        {
            s.nextStatus = {};
            s.nextEventTime = {};
            setState (std::move (s));
        }

        return;
    }

    s.nextStatus = QueueState::stopQueued;
    s.nextEventTime = pos;

    setState (std::move (s));
}

std::optional<BeatRange> LaunchHandle::getPlayedRange() const
{
    return getState().playedRange;
}

std::optional<MonotonicBeatRange> LaunchHandle::getPlayedMonotonicRange() const
{
    return getState().playedMonotonicRange;
}

std::optional<BeatRange> LaunchHandle::getLastPlayedRange() const
{
    return getState().lastPlayedRange;
}

auto LaunchHandle::advance (const SyncRange& syncRange) -> SplitStatus
{
    auto s = getState();

    const auto blockMonotonicBeatRange = getMonotonicBeatRange (syncRange);
    const auto duration = blockMonotonicBeatRange.v.getLength();

    // Playhead isn't moving so just apply stop queued states straight away
    if (duration == 0_bd)
    {
        SplitStatus splitStatus;

        if (s.nextStatus == QueueState::stopQueued)
        {
            splitStatus.playing1 = false;

            if (s.playedRange)
                s.lastPlayedRange = s.playedRange;

            s.playedRange = std::nullopt;
            s.playedMonotonicRange = std::nullopt;
            s.status = PlayState::stopped;
            s.nextStatus = std::nullopt;

            setState (std::move (s));
        }

        return splitStatus;
    }

    SplitStatus splitStatus;

    auto continuePlayingOrStopped = [&]
    {
        splitStatus.playing1 = s.status == PlayState::playing;
        splitStatus.range1 = getBeatRange (syncRange);
        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart())
                                                   : std::nullopt;

        if (s.playedRange)
            s.playedRange = withEndExtended (*s.playedRange, duration);

        if (s.playedMonotonicRange)
            s.playedMonotonicRange = MonotonicBeatRange { withEndExtended (s.playedMonotonicRange->v, duration) };
    };

    // Check if we need to change state
    if (s.nextStatus)
    {
        switch (*s.nextStatus)
        {
            case QueueState::playQueued:
            {
                if (s.nextEventTime)
                {
                    if (s.nextEventTime->v <= blockMonotonicBeatRange.v.getStart())
                    {
                        splitStatus.playing1 = true;
                        splitStatus.range1 = getBeatRange (syncRange);
                        splitStatus.playStartTime1 = splitStatus.range1.getStart();

                        if (s.playedRange)
                            s.lastPlayedRange = s.playedRange;

                        const auto numBeatsSinceLaunch = blockMonotonicBeatRange.v.getEnd() - s.nextEventTime->v;

                        s.playedRange = BeatRange::endingAt (splitStatus.range1.getEnd(), numBeatsSinceLaunch);
                        s.playedMonotonicRange = MonotonicBeatRange { BeatRange::endingAt (blockMonotonicBeatRange.v.getEnd(), numBeatsSinceLaunch) };
                        s.status = PlayState::playing;
                        s.nextStatus = std::nullopt;
                    }
                    else if (blockMonotonicBeatRange.v.contains (s.nextEventTime->v))
                    {
                        const auto blockEditBeatRange = getBeatRange (syncRange);

                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = blockEditBeatRange.withLength (s.nextEventTime->v - blockMonotonicBeatRange.v.getStart());
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = true;
                        splitStatus.range2 = { splitStatus.range1.getEnd(), blockEditBeatRange.getEnd() };
                        splitStatus.playStartTime2 = splitStatus.range2.getStart();
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        if (s.playedRange)
                            s.lastPlayedRange = s.playedRange;

                        s.playedRange = { splitStatus.range2 };
                        s.playedMonotonicRange = MonotonicBeatRange { BeatRange (s.nextEventTime->v, splitStatus.range2.getLength()) };
                        s.status = PlayState::playing;
                        s.nextEventTime = std::nullopt;
                        s.nextStatus = std::nullopt;
                    }
                    else
                    {
                        continuePlayingOrStopped();
                    }
                }
                else
                {
                    const auto blockEditBeatRange = getBeatRange (syncRange);

                    splitStatus.playing1 = true;
                    splitStatus.range1 = blockEditBeatRange;
                    splitStatus.playStartTime1 = blockEditBeatRange.getStart();

                    s.playedRange = blockEditBeatRange;
                    s.playedMonotonicRange = getMonotonicBeatRange (syncRange);
                    s.status = PlayState::playing;
                    s.nextStatus = std::nullopt;
                }

                break;
            }
            case QueueState::stopQueued:
            {
                if (s.status == PlayState::stopped)
                {
                    splitStatus.playing1 = false;
                    splitStatus.range1 = getBeatRange (syncRange);
                    splitStatus.playStartTime1 = std::nullopt;

                    s.playedRange = std::nullopt;
                    s.playedMonotonicRange = std::nullopt;
                    s.nextStatus = std::nullopt;
                    s.nextEventTime = std::nullopt;
                }
                else if (s.nextEventTime)
                {
                    if (s.nextEventTime->v <= blockMonotonicBeatRange.v.getStart())
                    {
                        splitStatus.playing1 = false;
                        splitStatus.range1 = getBeatRange (syncRange);
                        splitStatus.playStartTime1 = std::nullopt;

                        if (s.playedRange)
                            s.lastPlayedRange = s.playedRange;

                        s.playedRange = std::nullopt;
                        s.playedMonotonicRange = std::nullopt;
                        s.status = PlayState::stopped;
                        s.nextStatus = std::nullopt;
                    }
                    else if (blockMonotonicBeatRange.v.contains (s.nextEventTime->v))
                    {
                        const auto blockEditBeatRange = getBeatRange (syncRange);
                        const auto playingDuration = s.nextEventTime->v - blockMonotonicBeatRange.v.getStart();

                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = blockEditBeatRange.withLength (playingDuration);
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = false;
                        splitStatus.range2 = { splitStatus.range1.getEnd(), blockEditBeatRange.getEnd() };
                        splitStatus.playStartTime1 = std::nullopt;
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        if (s.playedRange)
                            s.lastPlayedRange = withEndExtended (*s.playedRange, playingDuration);

                        s.playedRange = std::nullopt;
                        s.playedMonotonicRange = std::nullopt;
                        s.status = PlayState::stopped;
                        s.nextEventTime = std::nullopt;
                        s.nextStatus = std::nullopt;
                    }
                    else
                    {
                        continuePlayingOrStopped();
                    }
                }
                else
                {
                    const auto blockEditBeatRange = getBeatRange (syncRange);

                    splitStatus.playing1 = false;
                    splitStatus.range1 = blockEditBeatRange;
                    splitStatus.playStartTime1 = std::nullopt;

                    if (s.playedRange)
                        s.lastPlayedRange = s.playedRange;

                    s.playedRange = std::nullopt;
                    s.playedMonotonicRange = std::nullopt;
                    s.status = PlayState::stopped;
                    s.nextStatus = std::nullopt;
                }

                break;
            }
        };
    }
    else
    {
        continuePlayingOrStopped();
    }

    setState (std::move (s));

    return splitStatus;
}

}} // namespace tracktion { inline namespace engine
