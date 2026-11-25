/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
/**
    A handle used to control the launching of a clip.
    This doesn't directly map on to playback beat positions of the clip, it just
    handles the start/stop status in relation to the Edit timeline.
    Once this has been retrieved by calling update, this needs to be wrapped in
    to playback actions.
*/
class LaunchHandle
{
public:
    //==============================================================================
    enum class PlayState : char
    {
        stopped,
        playing
    };

    enum class QueueState : char
    {
        stopQueued,
        playQueued
    };

    //==============================================================================
    /** Creates a LaunchHandle. */
    LaunchHandle() = default;

    /** Creates a copy of another LaunchHandle. */
    LaunchHandle (const LaunchHandle&);

    /** Returns the current playback state. */
    PlayState getPlayingStatus() const;

    /** Returns the current queue state. */
    std::optional<QueueState> getQueuedStatus() const;

    /** Returns the current queued event time. */
    std::optional<MonotonicBeat> getQueuedEventPosition() const;

    /** Start playing, optionally at a given beat position. */
    void play (std::optional<MonotonicBeat>);

    /** Stop playing, optionally at a given beat position. */
    void stop (std::optional<MonotonicBeat>);

    /** Stops at the end of a timed play (a one-shot's length or a follow action's
        time), unless a play is queued, which then carries on instead.
        @returns true if this stop was queued and no stop already was, so a
                 follow action should run
    */
    bool stopAtEndOfPlay (MonotonicBeat);

    /** Starts playing, as if it was started at the same time as the given LaunchHandle.
        This can optionally be delayed by supplying a MonotonicBeat to start at.
    */
    void playSynced (const LaunchHandle&, std::optional<MonotonicBeat>);

    /** Sets a duration to loop.
        If enabled, this will loop the handle's play duration as if it was re-triggered every duration.
        Set the nullopt to cancel looping.
    */
    void setLooping (std::optional<BeatDuration>);

    /** Moves the playhead by a given number of beats. */
    void nudge (BeatDuration);

    //==============================================================================
    /** Represents two beat ranges where the play state can be different in each. */
    struct SplitStatus
    {
        bool playing1 = false, playing2 = false;
        BeatRange range1, range2;

        bool isSplit = false;
        std::optional<BeatPosition> playStartTime1, playStartTime2;
    };

    /** Advance the state.
        N.B. This should only be called by the audio thread.
        @param SyncRange    The current SyncRange. Used to sync launch positions to.
                            A playing handle runs on its monotonic beats, so if the
                            Edit position jumps (a relocate, stop-move-play or loop
                            wrap) it carries on from where it was.
        @returns            The unlooped Edit beat range split if there are
                            different play/stop states.
        [[ audio_thread ]]
    */
    SplitStatus advance (const SyncRange&);

    //==============================================================================
    /** Returns the Edit beat range this has been playing for: from the beat it
        would have started at to reach where it is now, to the current Edit
        position. An Edit position jump moves its start with it.
        N.B. The length is unlooped.
    */
    std::optional<BeatRange> getPlayedRange() const;

    /** Returns the monotonic beat range this has been playing for.
        N.B. This is really only useful for syncing to other timeline events. Unlike
        getPlayedRange() it's unaffected by Edit position jumps.
    */
    std::optional<MonotonicBeatRange> getPlayedMonotonicRange() const;

    /** @internal
        The range last played before stopping, as a start beat and the length of
        time it played for, which Edit position jumps don't change.
    */
    std::optional<BeatRange> getLastPlayedRange() const;

private:
    //==============================================================================
    // audio-write, message-read
    std::atomic<PlayState> currentPlayState { PlayState::stopped };

    //==============================================================================
    // audio-write, message-read
    static_assert (std::is_trivially_copyable_v<std::optional<BeatRange>>);
    crill::seqlock_object<std::optional<BeatRange>> previouslyPlayedRange;

    //==============================================================================
    // audio-write/read, message-read
    struct CurrentState
    {
        BeatPosition startBeat;
        MonotonicBeat startMonotonicBeat;
        BeatDuration duration;      // monotonic
        BeatPosition editEnd;       // the end of the last block played
    };
    static_assert (std::is_trivially_copyable_v<std::optional<CurrentState>>);

    crill::seqlock_object<std::optional<CurrentState>> currentState;
    crill::seqlock_object<std::optional<BeatDuration>> loopDuration;

    //==============================================================================
    // message-write, audio-read/write
    struct NextState
    {
        QueueState queuedState;
        std::optional<MonotonicBeat> queuedPosition;
        std::optional<CurrentState> stateToSyncFrom;    // a legato launch's state to carry on from
    };
    static_assert (std::is_trivially_copyable_v<NextState>);

    std::optional<NextState> nextState;
    mutable crill::spin_mutex nextStateMutex;

    void pushNextState (NextState);
    std::optional<NextState> peekNextState() const;

    //==============================================================================
    // audio-write/read, message-read/write
    std::atomic<double> nudgeBeats { 0.0 };

    static std::optional<BeatRange> getPlayedRange (const std::optional<CurrentState>&);
    static std::optional<BeatRange> getMonotonicLengthPlayedRange (const std::optional<CurrentState>&);
};

} // namespace tracktion::inline engine
