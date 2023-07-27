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

void LaunchHandle::play (std::optional<BeatPosition> pos)
{
    auto s = getState();

    s.nextStatus = QueueState::playQueued;
    s.nextEventTime = pos;

    setState (s);
}

void LaunchHandle::stop (std::optional<BeatPosition> pos)
{
    auto s = getState();

    s.nextStatus = QueueState::stopQueued;
    s.nextEventTime = pos;

    setState (s);
}

std::optional<BeatPosition> LaunchHandle::getPlayStart() const
{
    return getState().playStartTime;
}

std::optional<BeatPosition> LaunchHandle::getPosition() const
{
    return getState().position;
}

void LaunchHandle::sync (BeatPosition p)
{
    auto s = getState();
    s.position = p;
    setState (s);
}

bool LaunchHandle::hasSynced() const
{
    return getState().position.has_value();
}

LaunchHandle::SplitStatus LaunchHandle::advance (BeatDuration duration)
{
    assert (duration > 0_bd);

    auto s = getState();
    assert (s.position.has_value());

    SplitStatus splitStatus;
    const auto beatRange = BeatRange (*s.position, duration);

    // Check if we need to change state
    if (s.nextStatus)
    {
        switch (*s.nextStatus)
        {
            case QueueState::playQueued:
            {
                if (s.nextEventTime)
                {
                    if (beatRange.contains (*s.nextEventTime))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = beatRange.withEnd (*s.nextEventTime);
                        splitStatus.playStartTime1 = s.playStartTime;
                        splitStatus.playing2 = true;
                        splitStatus.range2 = beatRange.withStart (*s.nextEventTime);
                        splitStatus.playStartTime2 = *s.nextEventTime;
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        s.playStartTime = *s.nextEventTime;
                        s.status = PlayState::playing;
                        s.nextEventTime = std::nullopt;
                        s.nextStatus = std::nullopt;
                    }
                }
                else
                {
                    splitStatus.playing1 = true;
                    splitStatus.range1 = beatRange;
                    splitStatus.playStartTime1 = beatRange.getStart();

                    s.playStartTime = beatRange.getStart();
                    s.status = PlayState::playing;
                    s.nextStatus = std::nullopt;
                }

                break;
            }
            case QueueState::stopQueued:
            {
                if (s.nextEventTime)
                {
                    if (beatRange.contains (*s.nextEventTime))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = beatRange.withEnd (*s.nextEventTime);
                        splitStatus.playStartTime1 = s.playStartTime;
                        splitStatus.playing2 = false;
                        splitStatus.range2 = beatRange.withStart (*s.nextEventTime);
                        splitStatus.playStartTime1 = std::nullopt;
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        s.playStartTime = std::nullopt;
                        s.status = PlayState::stopped;
                        s.nextEventTime = std::nullopt;
                        s.nextStatus = std::nullopt;
                    }
                }
                else
                {
                    splitStatus.playing1 = false;
                    splitStatus.range1 = beatRange;
                    splitStatus.playStartTime1 = std::nullopt;

                    s.playStartTime = std::nullopt;
                    s.status = PlayState::stopped;
                    s.nextStatus = std::nullopt;
                }

                break;
            }
        };
    }
    else
    {
        splitStatus.playing1 = s.status == PlayState::playing;
        splitStatus.range1 = beatRange;
        splitStatus.playStartTime1 = s.playStartTime;
    }

    // Finally update the current state
    s.position = *s.position + duration;
    setState (s);

    return splitStatus;
}

}} // namespace tracktion { inline namespace engine
