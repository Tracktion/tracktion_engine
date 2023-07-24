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
LaunchHandle::LaunchHandle (BeatDuration dur)
        : duration (dur)
{
}

LaunchHandle::PlayState LaunchHandle::getPlayingStatus() const
{
    return getState().status;
}

std::optional<LaunchHandle::QueueState> LaunchHandle::getQueuedStatus() const
{
    return getState().nextStatus;
}

inline std::optional<float> LaunchHandle::getProgress() const
{
    return getState().progress;
}

inline void LaunchHandle::play (std::optional<BeatPosition> pos)
{
    auto s = getState();

    s.nextStatus = QueueState::playQueued;
    s.nextEventTime = pos;

    setState (s);
}

inline void LaunchHandle::stop (std::optional<BeatPosition> pos)
{
    auto s = getState();

    s.nextStatus = QueueState::stopQueued;
    s.nextEventTime = pos;

    setState (s);
}

inline LaunchHandle::SplitStatus LaunchHandle::update (BeatRange unloopedEditBeatRange)
{
    auto s = getState();

    SplitStatus splitStatus;

    // Check if we need to change state
    if (s.nextStatus)
    {
        switch (*s.nextStatus)
        {
            case QueueState::playQueued:
            {
                if (s.nextEventTime)
                {
                    if (unloopedEditBeatRange.contains (*s.nextEventTime))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = unloopedEditBeatRange.withEnd (*s.nextEventTime);
                        splitStatus.playing2 = true;
                        splitStatus.range2 = unloopedEditBeatRange.withStart (*s.nextEventTime);
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
                    splitStatus.range1 = unloopedEditBeatRange;

                    s.playStartTime = unloopedEditBeatRange.getStart();
                    s.status = PlayState::playing;
                    s.nextStatus = std::nullopt;
                }

                break;
            }
            case QueueState::stopQueued:
            {
                if (s.nextEventTime)
                {
                    if (unloopedEditBeatRange.contains (*s.nextEventTime))
                    {
                        splitStatus.playing1 = s.status == PlayState::playing;
                        splitStatus.range1 = unloopedEditBeatRange.withEnd (*s.nextEventTime);
                        splitStatus.playing2 = false;
                        splitStatus.range2 = unloopedEditBeatRange.withStart (*s.nextEventTime);
                        splitStatus.isSplit = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        s.playStartTime = std::nullopt;
                        s.status = PlayState::stopped;
                        s.nextEventTime = std::nullopt;
                        s.nextStatus = std::nullopt;
                        s.progress = std::nullopt;
                    }
                }
                else
                {
                    splitStatus.playing1 = false;
                    splitStatus.range1 = unloopedEditBeatRange;

                    s.playStartTime = std::nullopt;
                    s.status = PlayState::stopped;
                    s.nextStatus = std::nullopt;
                    s.progress = std::nullopt;
                }

                break;
            }
        };
    }
    else
    {
        splitStatus.playing1 = s.status == PlayState::playing;
        splitStatus.range1 = unloopedEditBeatRange;
    }

    // Then update progress based on the end of the play position
    if (s.status == PlayState::playing)
    {
        assert (s.playStartTime.has_value());
        const auto numIterationsPlayed = (unloopedEditBeatRange.getEnd() - *s.playStartTime) / duration;
        s.progress = static_cast<float> (numIterationsPlayed - static_cast<int> (numIterationsPlayed));
    }
    else
    {
        s.progress = std::nullopt;
    }

    // Finally update the current state
    setState (s);

    return splitStatus;
}

}} // namespace tracktion { inline namespace engine
