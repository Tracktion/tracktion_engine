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
LaunchHandle::PlayState LaunchHandle::getPlayingStatus() const
{
    return getState().status;
}

std::optional<LaunchHandle::QueueState> LaunchHandle::getQueuedStatus() const
{
    return getState().nextStatus;
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
        return;

    s.nextStatus = QueueState::stopQueued;
    s.nextEventTime = pos;

    setState (s);
}

std::optional<BeatRange> LaunchHandle::getPlayedRange() const
{
    return getState().playedRange;
}

auto LaunchHandle::advance (SyncPoint syncPoint, BeatDuration duration) -> SplitStatus
{
    assert (duration > 0_bd);

    auto s = getState();

    SplitStatus splitStatus;
    const auto monotonicBeatRange = MonotonicBeatRange { BeatRange::endingAt (syncPoint.monotonicBeat.v, duration) };

    auto beatRange = BeatRange::endingAt (syncPoint.beat, duration);

    if (s.playedRange)
        beatRange = { s.playedRange->getEnd(), s.playedRange->getEnd() + duration };

    auto continuePlayingOrStopped = [&]
    {
        splitStatus.playing1 = s.status == PlayState::playing;
        splitStatus.range1 = beatRange;
        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart())
                                                   : std::nullopt;

        if (s.playedRange)
            s.playedRange = s.playedRange->withEnd (s.playedRange->getEnd() + duration);
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
                    if (s.nextEventTime->v <= monotonicBeatRange.v.getStart())
                    {
                        splitStatus.playing1 = true;
                        splitStatus.range1 = beatRange;
                        splitStatus.playStartTime1 = beatRange.getStart();

                        s.playedRange = { beatRange.getStart(), duration };
                        s.status = PlayState::playing;
                        s.nextStatus = std::nullopt;
                    }
                    else if (monotonicBeatRange.v.contains (s.nextEventTime->v))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = beatRange.withLength (s.nextEventTime->v - monotonicBeatRange.v.getStart());
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = true;
                        splitStatus.range2 = beatRange.withStart (beatRange.getStart() + splitStatus.range1.getLength());
                        splitStatus.playStartTime2 = beatRange.getStart() + splitStatus.range1.getLength();
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        s.playedRange = { splitStatus.range2 };
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
                    splitStatus.range1 = beatRange;
                    splitStatus.playStartTime1 = beatRange.getStart();

                    s.playedRange = beatRange;
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
                    splitStatus.range1 = beatRange;
                    splitStatus.playStartTime1 = std::nullopt;

                    s.playedRange = std::nullopt;
                    s.nextStatus = std::nullopt;
                    s.nextEventTime = std::nullopt;
                }
                else if (s.nextEventTime)
                {
                    if (s.nextEventTime->v <= monotonicBeatRange.v.getStart())
                    {
                        splitStatus.playing1 = false;
                        splitStatus.range1 = beatRange;
                        splitStatus.playStartTime1 = std::nullopt;

                        s.playedRange = std::nullopt;
                        s.status = PlayState::stopped;
                        s.nextStatus = std::nullopt;
                    }
                    else if (monotonicBeatRange.v.contains (s.nextEventTime->v))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = beatRange.withLength (s.nextEventTime->v - monotonicBeatRange.v.getStart());
                        splitStatus.playStartTime1 = s.playedRange ? std::optional (s.playedRange->getStart()) : std::nullopt;
                        splitStatus.playing2 = false;
                        splitStatus.range2 = beatRange.withStart (beatRange.getStart() + splitStatus.range1.getLength());
                        splitStatus.playStartTime1 = std::nullopt;
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        s.playedRange = std::nullopt;
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
                    splitStatus.range1 = beatRange;
                    splitStatus.playStartTime1 = std::nullopt;

                    s.playedRange = std::nullopt;
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
