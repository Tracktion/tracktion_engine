/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
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

    /** Start playing, optionally at a given beat position. */
    void play (std::optional<BeatPosition>);

    /** Stop playing, optionally at a given beat position. */
    void stop (std::optional<BeatPosition>);

    //==============================================================================
    /** Start the syncronisation.
        Call this once to sync the clip to the timeline.
        N.B. This should only be called by the audio thread.
        @param BeatPosition The start point of the timeline.
        [[ audio_thread ]]
    */
    void sync (BeatPosition);

    /** Returns true if this handle has been synced. */
    bool hasSynced() const;

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
        @param BeatDuration The duration to increment the timeline by.
        @returns            The unlooped Edit beat range split if there are
                            different play/stop states.
        [[ audio_thread ]]
    */
    SplitStatus advance (BeatDuration);

    //==============================================================================
    /** Returns the position this was launched from. */
    std::optional<BeatPosition> getPlayStart() const;

    /** Returns the last position this was updated from. */
    std::optional<BeatPosition> getPosition() const;

private:
    //==============================================================================
    struct State
    {
        std::optional<BeatPosition> position;

        PlayState status = PlayState::stopped;
        std::optional<BeatPosition> playStartTime;

        std::optional<QueueState> nextStatus;
        std::optional<BeatPosition> nextEventTime;
    };

    std::atomic<State> state { State {} };

    State getState() const      { return state; }
    void setState (State s)     { state.store (std::move (s)); }
};

}} // namespace tracktion { inline namespace engine
