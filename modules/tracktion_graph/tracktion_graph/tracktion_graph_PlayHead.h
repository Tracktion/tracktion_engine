/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion { inline namespace graph
{

class PlayHead;

//==============================================================================
//==============================================================================
/**
    Represents a pair of timeline ranges which could be wraped around the loop end.
*/
struct SplitTimelineRange
{
    SplitTimelineRange (juce::Range<int64_t> range1)
        : timelineRange1 (range1), isSplit (false) {}

    SplitTimelineRange (juce::Range<int64_t> range1, juce::Range<int64_t> range2)
        : timelineRange1 (range1), timelineRange2 (range2), isSplit (true) {}

    const juce::Range<int64_t> timelineRange1, timelineRange2;
    const bool isSplit;
};


/** Converts a reference sample range to a TimelinePositionWindow which could have two time
    ranges if the stream range overlaps the end of the loop range.
*/
static inline SplitTimelineRange referenceSampleRangeToSplitTimelineRange (const PlayHead&, juce::Range<int64_t> referenceSampleRange);


//==============================================================================
//==============================================================================
/**
    Converts a monotonically increasing reference range in to a timeline range.
    This can then get converted to a real-time range using the playback sample rate.
    This also handles looping and scrubbing and keeps track of when the play position
    was last set.
*/
class PlayHead
{
public:
    /** Creates a PlayHead. */
    PlayHead() noexcept;

    //==============================================================================
    /** Sets the timeline position of the play head and if it is different logs a user interaction. */
    void setPosition (int64_t newPosition);

    /** Plays a range in a loop. */
    void play (juce::Range<int64_t> rangeToPlay, bool looped);

    /** Starts playback from the last position. */
    void play();

    /** Takes the play position directly from the playout range.
        This is for rendering, where there will be no master stream to sync to.
        N.B. this disables looping.
    */
    void playSyncedToRange (juce::Range<int64_t> rangeToPlay);

    /** Stops the play head. */
    void stop();

    /** Returns the current timeline position. */
    int64_t getPosition() const;

    /** Returns the current timeline position ignoring any loop range which might have been set. */
    int64_t getUnloopedPosition() const;

    /** Adjust position without triggering a 'user interaction' change.
        Use when the position change actually maintains continuity - e.g. a tempo change.
    */
    void overridePosition (int64_t newPosition);

    //==============================================================================
    /** Returns true is the play head is currently playing. */
    bool isPlaying() const noexcept;

    /** Returns true is the play head is currently stopped. */
    bool isStopped() const noexcept;

    /** Returns true is the play head is in loop mode. */
    bool isLooping() const noexcept;

    /** Returns true is the play head is looping but playing before the loop start position. */
    bool isRollingIntoLoop() const noexcept;

    /** Returns the looped playback range. */
    juce::Range<int64_t> getLoopRange() const noexcept;

    /** Sets a playback range and whether to loop or not. */
    void setLoopRange (bool loop, juce::Range<int64_t> loopRange);

    /** Puts the play head in to roll in to loop mode.
        If this position is before the loop start position, the play head won't
        be wrapped to enable count in to loop recordings etc.
    */
    void setRollInToLoop (int64_t playbackPosition);

    //==============================================================================
    /** Sets the user dragging which logs a user interaction and enables scrubbing mode. */
    void setUserIsDragging (bool);

    /** Returns true if the user is dragging. */
    bool isUserDragging() const;
    
    /** Returns the time of the last user interaction, either a setPosition or setUserIsDragging call. */
    std::chrono::system_clock::time_point getLastUserInteractionTime() const;

    //==============================================================================
    /** Sets the length of the small looped blocks to play while scrubbing. E.g. when the user is dragging. */
    void setScrubbingBlockLength (int64_t numSamples);
    
    /** Returns the length of the small looped blocks to play while scrubbing. */
    int64_t getScrubbingBlockLength() const;

    //==============================================================================
    /** Converts a reference sample position to a timeline position. */
    int64_t referenceSamplePositionToTimelinePosition (int64_t referenceSamplePosition) const;

    /** Converts a reference sample position to a timeline position as if there was no looping set. */
    int64_t referenceSamplePositionToTimelinePositionUnlooped (int64_t referenceSamplePosition) const;

    /** Converts a reference sample range to a timeline range as if there was no looping set. */
    juce::Range<int64_t> referenceSampleRangeToSourceRangeUnlooped (juce::Range<int64_t> sourceReferenceSampleRange) const;
    
    /** Converts a linear timeline position to a position wrapped in the loop. */
    static int64_t linearPositionToLoopPosition (int64_t position, juce::Range<int64_t> loopRange);

    //==============================================================================
    /** Sets the reference sample count, adjusting the timeline if the play head is playing. */
    void setReferenceSampleRange (juce::Range<int64_t> sampleRange);

    /** Returns the reference sample count. */
    juce::Range<int64_t> getReferenceSampleRange() const;

    /** Returns the playout sync position.
        For syncing a reference position to a timeline position.
    */
    int64_t getPlayoutSyncPosition() const;

private:
    //==============================================================================
    struct SyncPositions
    {
        SyncPositions() noexcept = default;
        int64_t referenceSyncPosition = 0;
        int64_t playoutSyncPosition = 0;
    };
    
    std::atomic<SyncPositions> syncPositions { SyncPositions() };

    //==============================================================================
    std::atomic<juce::Range<int64_t>> referenceSampleRange { juce::Range<int64_t>() };
    std::atomic<juce::Range<int64_t>> timelinePlayRange { juce::Range<int64_t>() };
    std::atomic<int64_t> scrubbingBlockLength { static_cast<int64_t> (0.08 * 44100.0) };

    std::atomic<int> speed { 0 };
    std::atomic<bool> looping { false }, userDragging { false }, rollInToLoop { false };

    std::atomic<std::chrono::system_clock::time_point> userInteractionTime { std::chrono::system_clock::now() };

    //==============================================================================
    SyncPositions getSyncPositions() const              { return syncPositions; }
    void setSyncPositions (SyncPositions newPositions)  { syncPositions = newPositions; }
    
    void userInteraction()  { userInteractionTime = std::chrono::system_clock::now(); }
};


//==============================================================================
//==============================================================================
inline PlayHead::PlayHead() noexcept
{
    // There isn't currently a lock-free implementation of this on Linux
   #if ! JUCE_LINUX && ! JUCE_WINDOWS
    assert (referenceSampleRange.is_lock_free());
    assert (syncPositions.is_lock_free());
   #endif
}

//==============================================================================
inline void PlayHead::setPosition (int64_t newPosition)
{
    if (newPosition != getPosition())
        userInteraction();

    overridePosition (newPosition);
}

inline void PlayHead::play (juce::Range<int64_t> rangeToPlay, bool looped)
{
    timelinePlayRange = rangeToPlay;
    looping = looped && (rangeToPlay.getLength() > 50);
    setPosition (rangeToPlay.getStart());
    speed = 1;
}

inline void PlayHead::play()
{
    setPosition (getPosition());
    speed = 1;
}

inline void PlayHead::playSyncedToRange (juce::Range<int64_t> rangeToPlay)
{
    play (rangeToPlay, false);
    setSyncPositions ({});
}

inline void PlayHead::stop()
{
    auto t = getPosition();
    speed = 0;
    setPosition (t);
}

inline int64_t PlayHead::getPosition() const
{
    return referenceSamplePositionToTimelinePosition (referenceSampleRange.load().getStart());
}

inline int64_t PlayHead::getUnloopedPosition() const
{
    return referenceSamplePositionToTimelinePositionUnlooped (referenceSampleRange.load().getStart());
}

inline void PlayHead::overridePosition (int64_t newPosition)
{
    if (looping && rollInToLoop)
        newPosition = std::min (newPosition, timelinePlayRange.load().getEnd());
    else if (looping)
        newPosition = timelinePlayRange.load().clipValue (newPosition);

    SyncPositions newSyncPositions;
    newSyncPositions.referenceSyncPosition = referenceSampleRange.load().getStart();
    newSyncPositions.playoutSyncPosition = newPosition;
    setSyncPositions (newSyncPositions);
}

//==============================================================================
inline bool PlayHead::isPlaying() const noexcept                    { return speed.load (std::memory_order_relaxed) != 0; }

inline bool PlayHead::isStopped() const noexcept                    { return speed.load (std::memory_order_relaxed) == 0; }

inline bool PlayHead::isLooping() const noexcept                    { return looping.load (std::memory_order_relaxed); }

inline bool PlayHead::isRollingIntoLoop() const noexcept            { return rollInToLoop.load (std::memory_order_relaxed); }

inline juce::Range<int64_t> PlayHead::getLoopRange() const noexcept { return timelinePlayRange; }

inline void PlayHead::setLoopRange (bool loop, juce::Range<int64_t> loopRange)
{
    if (looping != loop || (loop && loopRange != getLoopRange()))
    {
        auto lastPos = getPosition();
        looping = loop;
        timelinePlayRange = loopRange;
        setPosition (lastPos);
    }
}

inline void PlayHead::setRollInToLoop (int64_t position)
{
    rollInToLoop = true;
    
    SyncPositions newSyncPositions;
    newSyncPositions.referenceSyncPosition = referenceSampleRange.load().getStart();
    newSyncPositions.playoutSyncPosition = std::min (position, timelinePlayRange.load().getEnd());
    setSyncPositions (newSyncPositions);
}

//==============================================================================
inline void PlayHead::setUserIsDragging (bool b)
{
    userInteraction();
    userDragging = b;
}

inline bool PlayHead::isUserDragging() const
{
    return userDragging;
}

inline std::chrono::system_clock::time_point PlayHead::getLastUserInteractionTime() const
{
    return userInteractionTime;
}

//==============================================================================
inline void PlayHead::setScrubbingBlockLength (int64_t numSamples)
{
    scrubbingBlockLength = numSamples;
}

inline int64_t PlayHead::getScrubbingBlockLength() const
{
    return scrubbingBlockLength.load (std::memory_order_relaxed);
}

//==============================================================================
inline int64_t PlayHead::referenceSamplePositionToTimelinePosition (int64_t referenceSamplePosition) const
{
    const auto syncPos = getSyncPositions();
    
    if (userDragging)
        return syncPos.playoutSyncPosition + ((referenceSamplePosition - syncPos.referenceSyncPosition) % getScrubbingBlockLength());

    if (looping && ! rollInToLoop)
        return linearPositionToLoopPosition (referenceSamplePositionToTimelinePositionUnlooped (referenceSamplePosition), timelinePlayRange);

    return referenceSamplePositionToTimelinePositionUnlooped (referenceSamplePosition);
}

inline int64_t PlayHead::referenceSamplePositionToTimelinePositionUnlooped (int64_t referenceSamplePosition) const
{
    const auto syncPos = getSyncPositions();
    return syncPos.playoutSyncPosition + (referenceSamplePosition - syncPos.referenceSyncPosition) * speed;
}

inline juce::Range<int64_t> PlayHead::referenceSampleRangeToSourceRangeUnlooped (juce::Range<int64_t> sourceReferenceSampleRange) const
{
    const auto syncPos = getSyncPositions();
    return { syncPos.playoutSyncPosition + (sourceReferenceSampleRange.getStart() - syncPos.referenceSyncPosition) * speed,
             syncPos.playoutSyncPosition + (sourceReferenceSampleRange.getEnd() - syncPos.referenceSyncPosition) * speed };
}

inline int64_t PlayHead::linearPositionToLoopPosition (int64_t position, juce::Range<int64_t> loopRange)
{
    const auto loopStart = loopRange.getStart();
    return loopStart + ((position - loopStart) % loopRange.getLength());
}

//==============================================================================
inline void PlayHead::setReferenceSampleRange (juce::Range<int64_t> sampleRange)
{
    referenceSampleRange = sampleRange;

    if (rollInToLoop && getPosition() >= timelinePlayRange.load().getStart())
        rollInToLoop = false;
}

inline juce::Range<int64_t> PlayHead::getReferenceSampleRange() const
{
    return referenceSampleRange;
}

inline int64_t PlayHead::getPlayoutSyncPosition() const
{
    return getSyncPositions().playoutSyncPosition;
}

//==============================================================================
//==============================================================================
inline SplitTimelineRange referenceSampleRangeToSplitTimelineRange (const PlayHead& playHead, juce::Range<int64_t> referenceSampleRange)
{
    const auto unloopedRange = playHead.referenceSampleRangeToSourceRangeUnlooped (referenceSampleRange);
    auto s = unloopedRange.getStart();
    auto e = unloopedRange.getEnd();

    if (playHead.isUserDragging())
    {
        auto loopStart = playHead.getPlayoutSyncPosition();
        auto loopLen = playHead.getScrubbingBlockLength();
        auto loopEnd = loopStart + loopLen;

        s = PlayHead::linearPositionToLoopPosition (s, { loopStart, loopStart + loopLen });
        e = PlayHead::linearPositionToLoopPosition (e, { loopStart, loopStart + loopLen });

        if (s > e)
        {
            if (s >= loopEnd)
                return { { loopStart, e } };

            if (e <= loopStart)
                return { { s, loopEnd } };

            return { { s, loopEnd }, { loopStart, e } };
        }
    }

    if (playHead.isLooping() && ! playHead.isRollingIntoLoop())
    {
        const auto pr = playHead.getLoopRange();
        s = PlayHead::linearPositionToLoopPosition (s, pr);
        e = PlayHead::linearPositionToLoopPosition (e, pr);

        if (s > e)
        {
            if (s >= pr.getEnd())   return { { pr.getStart(), e } };
            if (e <= pr.getStart()) return { { s, pr.getEnd() } };

            return { { s, pr.getEnd() }, { pr.getStart(), e } };
        }
    }

    return { { s, e } };
}

}}
