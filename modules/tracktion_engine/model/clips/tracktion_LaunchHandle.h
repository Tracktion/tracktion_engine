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
struct LaunchHandle
{
public:
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

    /** Creates a LaunchHandle for a given BeatDuration. */
    LaunchHandle (BeatDuration);

    /** Returns the current playback state. */
    PlayState getPlayingStatus() const;

    /** Returns the current queue state. */
    std::optional<QueueState> getQueuedStatus() const;

    /** Returns the current progress if playing. */
    std::optional<float> getProgress() const;

    /** Start playing, optionally at a given beat position. */
    void play (std::optional<BeatPosition>);

    /** Stop playing, optionally at a given beat position. */
    void stop (std::optional<BeatPosition>);

    /** Represents two beat ranges where the play state can be different in each. */
    struct SplitStatus
    {
        bool playing1 = false, playing2 = false;
        BeatRange range1, range2;

        bool isSplit = false;
    };

    /** Update the state.
        @param BeatRange The timeline's current beat range. N.B. this ignores
                         any looping to ensure the slots continue to play.
        @returns         The unlooped Edit beat range split if there are
                         different play/stop states.
    */
    SplitStatus update (BeatRange unloopedEditBeatRange);

private:
    struct State
    {
        PlayState status;
        BeatRange lastBeatRange;

        std::optional<BeatPosition> playStartTime;

        std::optional<QueueState> nextStatus;
        std::optional<BeatPosition> nextEventTime;
        std::optional<float> progress;
    };

    const BeatDuration duration;

    std::atomic<State> state { State {} };

    State getState() const      { return state; }
    void setState (State s)     { state.store (std::move (s)); }
};

}} // namespace tracktion { inline namespace engine
