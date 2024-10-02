/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_LaunchHandle.h"

namespace tracktion::inline engine
{

//==============================================================================
LaunchHandle::LaunchHandle (const LaunchHandle& o)
{
    currentState.store (o.currentState.load());
}

LaunchHandle::PlayState LaunchHandle::getPlayingStatus() const
{
    return currentPlayState.load (std::memory_order_acquire);
}

std::optional<LaunchHandle::QueueState> LaunchHandle::getQueuedStatus() const
{
    const auto next = peekNextState();
    return next ? std::optional (next->queuedState) : std::nullopt;
}

std::optional<MonotonicBeat> LaunchHandle::getQueuedEventPosition() const
{
    const auto next = peekNextState();
    return next ? next->queuedPosition : std::nullopt;
}

void LaunchHandle::play (std::optional<MonotonicBeat> pos)
{
    pushNextState (QueueState::playQueued, pos);
}

void LaunchHandle::playSynced (const LaunchHandle& otherHandle, std::optional<MonotonicBeat> pos)
{
    stateToSyncFrom.store (otherHandle.currentState.load());
    play (pos);
}

void LaunchHandle::setLooping (std::optional<BeatDuration> duration)
{
    loopDuration.store (duration);
}

void LaunchHandle::nudge (BeatDuration duration)
{
    if (currentPlayState.load (std::memory_order_acquire) != PlayState::playing)
        return;

   #if __APPLE__ && __clang_major__ < 16
    double expected = nudgeBeats.load (std::memory_order_acquire);
    while (nudgeBeats.compare_exchange_weak (expected, expected + duration.inBeats()))
        ;
   #else
    nudgeBeats.fetch_add (duration.inBeats(), std::memory_order_acq_rel);
   #endif
}

void LaunchHandle::stop (std::optional<MonotonicBeat> pos)
{
    if (currentPlayState.load (std::memory_order_acquire) == PlayState::stopped)
    {
        if (getQueuedStatus() == QueueState::playQueued)
        {
            const std::scoped_lock sl (nextStateMutex);
            nextState = std::nullopt;
        }

        return;
    }

    pushNextState (QueueState::stopQueued, pos);
}

std::optional<BeatRange> LaunchHandle::getPlayedRange() const
{
    if (auto c = currentState.load())
        return BeatRange (c->startBeat, c->duration);

    return {};
}

std::optional<MonotonicBeatRange> LaunchHandle::getPlayedMonotonicRange() const
{
    if (auto c = currentState.load())
        return MonotonicBeatRange { BeatRange (c->startMonotonicBeat.v, c->duration) };

    return {};
}

std::optional<BeatRange> LaunchHandle::getLastPlayedRange() const
{
    return previouslyPlayedRange.load();
}

auto LaunchHandle::advance (const SyncRange& syncRange) -> SplitStatus
{
    const auto blockEditBeatRange = getBeatRange (syncRange);
    const auto blockMonotonicBeatRange = getMonotonicBeatRange (syncRange);
    const auto duration = blockMonotonicBeatRange.v.getLength();

    const auto playState = currentPlayState.load (std::memory_order_acquire);

    std::optional<NextState> ns;

    const std::unique_lock sl (nextStateMutex, std::try_to_lock); // Keep this lock alive for the block

    if (sl.owns_lock() && nextState.has_value())
        ns = *nextState;

    auto clearNextState = [&]
                          {
                              // If ns has a value, nextState must be locked
                              if (ns.has_value())
                                  nextState = std::nullopt;
                          };

    std::optional<QueueState> queuedState       = ns ? std::optional (ns->queuedState) : std::nullopt;
    std::optional<MonotonicBeat> queuedPosition = ns ? std::optional (ns->queuedPosition) : std::nullopt;

    auto cs = currentState.load();

    // Playhead isn't moving so just apply stop queued states straight away
    if (duration == 0_bd)
    {
        SplitStatus splitStatus;

        if (queuedState == QueueState::stopQueued)
        {
            splitStatus.playing1 = false;

            if (cs)
                previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

            currentState.store (std::nullopt);
            currentPlayState.store (PlayState::stopped, std::memory_order_release);
            clearNextState();
        }

        return splitStatus;
    }

    SplitStatus splitStatus;

    auto continuePlayingOrStopped = [&]
    {
        splitStatus.playing1 = playState == PlayState::playing;
        splitStatus.range1 = blockEditBeatRange;
        splitStatus.playStartTime1 = cs ? std::optional (cs->startBeat)
                                        : std::nullopt;

        if (cs)
        {
            if (auto nudge = BeatDuration::fromBeats (nudgeBeats.exchange (0.0, std::memory_order_acq_rel));
                abs (nudge) > 0.01_bd)
            {
                const auto originalDuration = cs->duration;
                cs->duration            = std::max (cs->duration + nudge, 0_bd);

                const auto actualNudge = cs->duration - originalDuration;
                cs->startBeat           = cs->startBeat - actualNudge;
                cs->startMonotonicBeat  = { cs->startMonotonicBeat.v - actualNudge };
            }

            cs->duration = cs->duration + duration;
            currentState.store (cs);
        }

        // Apply repeat looping
        if (playState == PlayState::stopped)
            return;

        if (splitStatus.isSplit)
            return;

        if (auto ld = loopDuration.load())
        {
            const auto elapsedBeats = blockEditBeatRange.getEnd() - *splitStatus.playStartTime1;

            if (elapsedBeats <= *ld)
                return;

            if (elapsedBeats > (*ld + duration))
                return;

            const auto secondSplitLength    = BeatDuration::fromBeats (std::fmod (elapsedBeats.inBeats(), ld->inBeats()));
            const auto firstSplitLength     = duration - secondSplitLength;

            splitStatus.playing1 = true;
            splitStatus.range1 = blockEditBeatRange.withLength (firstSplitLength);
            splitStatus.playing2 = true;
            splitStatus.range2 = BeatRange (splitStatus.range1.getEnd(), secondSplitLength);
            splitStatus.playStartTime2 = splitStatus.range2.getStart();
            splitStatus.isSplit = true;

            currentState.store (CurrentState
                                {
                                    *splitStatus.playStartTime2,
                                    MonotonicBeat { blockMonotonicBeatRange.v.getStart() + firstSplitLength },
                                    secondSplitLength
                                });
        }
    };

    // Check if we need to change state
    if (queuedState)
    {
        switch (*queuedState)
        {
            case QueueState::playQueued:
            {
                if (queuedPosition)
                {
                    if (queuedPosition->v <= blockMonotonicBeatRange.v.getStart())
                    {
                        // Try and sync from another state first
                        if (auto syncFrom = stateToSyncFrom.load())
                        {
                            currentState.store (CurrentState
                                                {
                                                    syncFrom->startBeat,
                                                    syncFrom->startMonotonicBeat,
                                                    blockMonotonicBeatRange.v.getEnd() - syncFrom->startMonotonicBeat.v
                                                });
                            currentState.store (syncFrom);
                            stateToSyncFrom.store (std::nullopt);
                            currentPlayState.store (PlayState::playing, std::memory_order_release);
                            continuePlayingOrStopped();
                        }
                        else
                        {
                            splitStatus.playing1 = true;
                            splitStatus.range1 = blockEditBeatRange;
                            splitStatus.playStartTime1 = splitStatus.range1.getStart();

                            if (cs)
                                previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

                            const auto numBeatsSinceLaunch = blockMonotonicBeatRange.v.getEnd() - queuedPosition->v;

                            currentState.store (CurrentState
                                                {
                                                    blockEditBeatRange.getEnd() - numBeatsSinceLaunch,
                                                    *queuedPosition,
                                                    numBeatsSinceLaunch
                                                });
                            currentPlayState.store (PlayState::playing, std::memory_order_release);
                        }

                        clearNextState();
                    }
                    else if (blockMonotonicBeatRange.v.contains (queuedPosition->v))
                    {
                        const auto firstSplitLength     = queuedPosition->v - blockMonotonicBeatRange.v.getStart();
                        const auto secondSplitLength    = blockMonotonicBeatRange.v.getEnd() - queuedPosition->v;

                        // Try and sync from another state first
                        if (auto syncFrom = stateToSyncFrom.load())
                        {
                            splitStatus.playing1        = playState == PlayState::playing;
                            splitStatus.range1          = blockEditBeatRange.withLength (firstSplitLength);
                            splitStatus.playStartTime1  = cs ? std::optional (cs->startBeat) : std::nullopt;
                            splitStatus.playing2        = true;
                            splitStatus.range2          = BeatRange::endingAt (blockEditBeatRange.getEnd(), secondSplitLength);
                            splitStatus.playStartTime2  = syncFrom->startBeat;
                            splitStatus.isSplit         = true;

                            stateToSyncFrom.store (std::nullopt);
                            currentState.store (CurrentState
                                                {
                                                    syncFrom->startBeat,
                                                    syncFrom->startMonotonicBeat,
                                                    blockMonotonicBeatRange.v.getEnd() - syncFrom->startMonotonicBeat.v
                                                });
                            currentPlayState.store (PlayState::playing, std::memory_order_release);
                        }
                        else
                        {
                            splitStatus.playing1        = playState == PlayState::playing;
                            splitStatus.range1          = blockEditBeatRange.withLength (firstSplitLength);
                            splitStatus.playStartTime1  = cs ? std::optional (cs->startBeat) : std::nullopt;
                            splitStatus.playing2        = true;
                            splitStatus.range2          = BeatRange::endingAt (blockEditBeatRange.getEnd(), secondSplitLength);
                            splitStatus.playStartTime2  = splitStatus.range2.getStart();
                            splitStatus.isSplit         = true;

                            assert (! splitStatus.range1.isEmpty());
                            assert (! splitStatus.range2.isEmpty());

                            if (cs)
                                previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

                            currentState.store (CurrentState
                                                {
                                                    *splitStatus.playStartTime2,
                                                    *queuedPosition,
                                                    secondSplitLength
                                                });
                            currentPlayState.store (PlayState::playing, std::memory_order_release);
                        }

                        clearNextState();
                    }
                    else
                    {
                        continuePlayingOrStopped();
                    }
                }
                else
                {
                    splitStatus.playing1        = true;
                    splitStatus.range1          = blockEditBeatRange;
                    splitStatus.playStartTime1  = blockEditBeatRange.getStart();

                    currentState.store (CurrentState
                                        {
                                            blockEditBeatRange.getStart(),
                                            syncRange.start.monotonicBeat,
                                            duration
                                        });
                    currentPlayState.store (PlayState::playing, std::memory_order_release);
                    clearNextState();
                }

                break;
            }
            case QueueState::stopQueued:
            {
                if (playState == PlayState::stopped)
                {
                    splitStatus.playing1 = false;
                    splitStatus.range1 = blockEditBeatRange;
                    splitStatus.playStartTime1 = std::nullopt;

                    currentState.store (std::nullopt);
                    clearNextState();
                }
                else if (queuedPosition)
                {
                    if (queuedPosition->v <= blockMonotonicBeatRange.v.getStart())
                    {
                        splitStatus.playing1 = false;
                        splitStatus.range1 = blockEditBeatRange;
                        splitStatus.playStartTime1 = std::nullopt;

                        if (cs)
                            previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

                        currentState.store (std::nullopt);
                        currentPlayState.store (PlayState::stopped, std::memory_order_release);
                        clearNextState();
                    }
                    else if (blockMonotonicBeatRange.v.contains (queuedPosition->v))
                    {
                        const auto firstSplitLength     = queuedPosition->v - blockMonotonicBeatRange.v.getStart();
                        const auto secondSplitLength    = blockMonotonicBeatRange.v.getEnd() - queuedPosition->v;

                        splitStatus.playing1        = playState == PlayState::playing;
                        splitStatus.range1          = blockEditBeatRange.withLength (firstSplitLength);
                        splitStatus.playStartTime1  = cs ? std::optional (cs->startBeat) : std::nullopt;
                        splitStatus.playing2        = false;
                        splitStatus.range2          = BeatRange::endingAt (blockEditBeatRange.getEnd(), secondSplitLength);
                        splitStatus.playStartTime2  = std::nullopt;
                        splitStatus.isSplit         = true;

                        assert(! splitStatus.range1.isEmpty());
                        assert(! splitStatus.range2.isEmpty());

                        if (cs)
                            previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

                        currentState.store (std::nullopt);
                        currentPlayState.store (PlayState::stopped, std::memory_order_release);
                        clearNextState();
                    }
                    else
                    {
                        continuePlayingOrStopped();
                    }
                }
                else
                {
                    splitStatus.playing1        = false;
                    splitStatus.range1          = blockEditBeatRange;
                    splitStatus.playStartTime1  = std::nullopt;

                    if (cs)
                        previouslyPlayedRange.store (BeatRange (cs->startBeat, cs->duration));

                    currentState.store (std::nullopt);
                    currentPlayState.store (PlayState::stopped, std::memory_order_release);
                    clearNextState();
                }

                break;
            }
        };
    }
    else
    {
        continuePlayingOrStopped();
    }

    return splitStatus;
}

//==============================================================================
void LaunchHandle::pushNextState (QueueState s, std::optional<MonotonicBeat> b)
{
    const std::scoped_lock sl (nextStateMutex);
    nextState = NextState { s, b };
}

std::optional<LaunchHandle::NextState> LaunchHandle::peekNextState() const
{
    if (const std::unique_lock sl (nextStateMutex, std::try_to_lock);
        sl.owns_lock() && nextState.has_value())
    {
        return *nextState;
    }

    return {};
}

} // namespace tracktion::inline engine
