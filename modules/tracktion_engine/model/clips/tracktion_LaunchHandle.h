/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
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
    enum class PlayState
    {
        stopped,
        playing
    };

    enum class QueueState
    {
        stopQueued,
        playQueued
    };

    //==============================================================================
    /** Creates a LaunchHandle. */
    LaunchHandle() = default;

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
        @param SyncRange    The current SyncRange. Used to sync launch positions to
        @returns            The unlooped Edit beat range split if there are
                            different play/stop states.
        [[ audio_thread ]]
    */
    SplitStatus advance (const SyncRange&);

    //==============================================================================
    /** Returns the Edit beat range this has been playing for.
        N.B. The length is unlooped and so monotonically increasing.
    */
    std::optional<BeatRange> getPlayedRange() const;

    /** Returns the monotonic beat range this has been playing for.
        N.B. This is really only useful for syncing to other timeline events.
    */
    std::optional<MonotonicBeatRange> getPlayedMonotonicRange() const;

    /** @internal */
    std::optional<BeatRange> getLastPlayedRange() const;

private:
    //==============================================================================
    struct State
    {
        State();

        PlayState status = PlayState::stopped;
        std::optional<BeatRange> playedRange, lastPlayedRange;
        std::optional<MonotonicBeatRange> playedMonotonicRange;

        std::optional<QueueState> nextStatus;
        std::optional<MonotonicBeat> nextEventTime;
    };

    static_assert (std::is_trivially_copyable_v<State>);

    MultipleWriterSeqLock<State> state;

    State getState() const      { return state.load(); }
    void setState (State s)     { state.store (std::move (s)); }
};

}} // namespace tracktion { inline namespace engine
