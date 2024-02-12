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

    setState (s);
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
            setState (s);
        }

        return;
    }

    s.nextStatus = QueueState::stopQueued;
    s.nextEventTime = pos;

    setState (s);
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

auto LaunchHandle::advance (SyncPoint syncPoint, BeatDuration duration) -> SplitStatus
{
    auto s = getState();

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

            setState (s);
        }

        return splitStatus;
    }

    SplitStatus splitStatus;
    const auto blockMonotonicBeatRange = MonotonicBeatRange { BeatRange::endingAt (syncPoint.monotonicBeat.v, duration) };

    auto blockBeatRange = BeatRange::endingAt (syncPoint.beat, duration);
    auto playedMonotonicRange = MonotonicBeatRange { BeatRange::endingAt (syncPoint.monotonicBeat.v, duration) };

    if (s.playedRange)
        blockBeatRange = { s.playedRange->getEnd(), s.playedRange->getEnd() + duration };

    if (s.playedMonotonicRange)
        playedMonotonicRange = MonotonicBeatRange { s.playedMonotonicRange->v.withEnd (s.playedMonotonicRange->v.getEnd() + duration) };

    auto continuePlayingOrStopped = [&]
    {
        splitStatus.playing1 = s.status == PlayState::playing;
        splitStatus.range1 = blockBeatRange;
        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart())
                                                   : std::nullopt;

        if (s.playedRange)
            s.playedRange = s.playedRange->withEnd (s.playedRange->getEnd() + duration);

        if (s.playedMonotonicRange)
            s.playedMonotonicRange = playedMonotonicRange;
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
                        splitStatus.range1 = BeatRange::endingAt (syncPoint.beat, duration);;
                        splitStatus.playStartTime1 = splitStatus.range1.getStart();

                        if (s.playedRange)
                            s.lastPlayedRange = s.playedRange;

                        const auto numBeatsSinceLaunch = blockMonotonicBeatRange.v.getEnd() - s.nextEventTime->v;

                        s.playedRange = splitStatus.range1.withLength (numBeatsSinceLaunch);
                        s.playedMonotonicRange = MonotonicBeatRange { BeatRange (s.nextEventTime->v, blockMonotonicBeatRange.v.getEnd()) };
                        s.status = PlayState::playing;
                        s.nextStatus = std::nullopt;
                    }
                    else if (blockMonotonicBeatRange.v.contains (s.nextEventTime->v))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = blockBeatRange.withLength (s.nextEventTime->v - blockMonotonicBeatRange.v.getStart());
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = true;
                        splitStatus.range2 = blockBeatRange.withStart (blockBeatRange.getStart() + splitStatus.range1.getLength());
                        splitStatus.playStartTime2 = blockBeatRange.getStart() + splitStatus.range1.getLength();
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
                    splitStatus.playing1 = true;
                    splitStatus.range1 = blockBeatRange;
                    splitStatus.playStartTime1 = blockBeatRange.getStart();

                    s.playedRange = blockBeatRange;
                    s.playedMonotonicRange = playedMonotonicRange;
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
                    splitStatus.range1 = blockBeatRange;
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
                        splitStatus.range1 = blockBeatRange;
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
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = blockBeatRange.withLength (s.nextEventTime->v - blockMonotonicBeatRange.v.getStart());
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = false;
                        splitStatus.range2 = blockBeatRange.withStart (blockBeatRange.getStart() + splitStatus.range1.getLength());
                        splitStatus.playStartTime1 = std::nullopt;
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        if (s.playedRange)
                            s.lastPlayedRange = s.playedRange;

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
                    splitStatus.playing1 = false;
                    splitStatus.range1 = blockBeatRange;
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

    setState (s);

    return splitStatus;
}

}} // namespace tracktion { inline namespace engine
