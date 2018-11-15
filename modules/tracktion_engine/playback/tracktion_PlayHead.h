/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class PlayHead
{
public:
    PlayHead() = default;

    //==============================================================================
    void setPosition (double newTime)
    {
        if (newTime != getPosition())
            userInteraction();

        overridePosition (newTime);
    }

    void play (EditTimeRange rangeToPlay, bool looped)
    {
        const juce::ScopedLock sl (lock);

        playRange = rangeToPlay;
        setPosition (playRange.start);
        speed = 1.0;
        looping = looped && (playRange.getLength() > 0.001);
    }

    // carries on from the last position
    void play()
    {
        const juce::ScopedLock sl (lock);

        setPosition (getPosition());
        speed = 1.0;
    }

    // takes the play time directly from the engine's time - for recording, where it needs to be fixed
    void playLockedToEngine (EditTimeRange rangeToPlay)
    {
        const juce::ScopedLock sl (lock);

        play (rangeToPlay, false);
        playoutSyncTime = 0;
        streamSyncTime = 0;
    }

    void stop()
    {
        const juce::ScopedLock sl (lock);

        auto t = getPosition();
        speed = 0;
        setPosition (t);
    }

    //==============================================================================
    void setUserIsDragging (bool b)
    {
        userInteraction();
        userDragging = b;
    }

    bool isUserDragging() const                     { return userDragging; }
    juce::Time getLastUserInteractionTime() const   { return userInteractionTime; }

    //==============================================================================
    struct EditTimeWindow
    {
        EditTimeWindow (EditTimeRange range1)
            : editRange1 (range1), isSplit (false) {}

        EditTimeWindow (EditTimeRange range1, EditTimeRange range2)
            : editRange1 (range1), editRange2 (range2), isSplit (true) {}

        EditTimeRange editRange1, editRange2;
        bool isSplit;
    };

    double streamTimeToSourceTime (double streamTime) const
    {
        const juce::ScopedLock sl (lock);

        if (userDragging)
            return playoutSyncTime + std::fmod (streamTime - streamSyncTime, getScrubbingBlockLengthSeconds());

        if (looping && ! rollInToLoop)
            return linearTimeToLoopTime (streamTimeToSourceTimeUnlooped (streamTime), playRange);

        return streamTimeToSourceTimeUnlooped (streamTime);
    }

    double streamTimeToSourceTimeUnlooped (double streamTime) const
    {
        const juce::ScopedLock sl (lock);
        return playoutSyncTime + (streamTime - streamSyncTime) * speed;
    }

    static double linearTimeToLoopTime (double time, EditTimeRange loop)
    {
        return linearTimeToLoopTime (time, loop.start, loop.getLength());
    }

    static double linearTimeToLoopTime (double time, double loopStart, double loopLen)
    {
        return loopStart + std::fmod (time - loopStart, loopLen);
    }

    EditTimeWindow streamTimeToEditWindow (EditTimeRange streamTime) const
    {
        constexpr double errorMargin = 0.000001;

        auto s = streamTimeToSourceTimeUnlooped (streamTime.start);
        auto e = streamTimeToSourceTimeUnlooped (streamTime.end);

        if (userDragging)
        {
            auto loopStart = [this] { const juce::ScopedLock sl (lock); return playoutSyncTime; }();
            auto loopLen = getScrubbingBlockLengthSeconds();
            auto loopEnd = loopStart + loopLen;

            s = linearTimeToLoopTime (s, loopStart, loopLen);
            e = linearTimeToLoopTime (e, loopStart, loopLen);

            if (s > e)
            {
                if (s >= loopEnd - errorMargin)
                    return EditTimeWindow ({ loopStart, e });

                if (e <= loopStart + errorMargin)
                    return EditTimeWindow ({ s, loopEnd });

                return EditTimeWindow ({ s, loopEnd }, { loopStart, e });
            }
        }

        if (looping && ! rollInToLoop)
        {
            const auto pr = getLoopTimes();
            s = linearTimeToLoopTime (s, pr);
            e = linearTimeToLoopTime (e, pr);

            if (s > e)
            {
                if (s >= pr.end   - errorMargin)    return EditTimeWindow ({ pr.start, e });
                if (e <= pr.start + errorMargin)    return EditTimeWindow ({ s, pr.end });

                return EditTimeWindow ({ s, pr.end }, { pr.start, e });
            }
        }

        return EditTimeWindow ({ s, e });
    }

    double getPosition() const
    {
        const juce::ScopedLock sl (lock);
        return streamTimeToSourceTime (lastStreamTime);
    }

    double getUnloopedPosition() const
    {
        const juce::ScopedLock sl (lock);
        return streamTimeToSourceTimeUnlooped (lastStreamTime);
    }

    // Adjust position without triggering a 'user interaction' change.
    // Use when the position change actually maintains continuity - e.g. a tempo change.
    void overridePosition (double newTime)
    {
        const juce::ScopedLock sl (lock);

        if (looping && rollInToLoop)
            newTime = juce::jmin (newTime, playRange.end);
        else if (looping)
            newTime = playRange.clipValue (newTime);

        streamSyncTime = lastStreamTimeEnd;
        playoutSyncTime = newTime;
    }

    //==============================================================================
    bool isPlaying() const noexcept                     { return speed.load (std::memory_order_relaxed) != 0; }
    bool isStopped() const noexcept                     { return speed.load (std::memory_order_relaxed) == 0; }
    bool isLooping() const noexcept                     { return looping.load (std::memory_order_relaxed); }
    bool isRollingIntoLoop() const noexcept             { return rollInToLoop.load (std::memory_order_relaxed); }

    EditTimeRange getLoopTimes() const noexcept         { const juce::ScopedLock sl (lock); return playRange; }

    void setLoopTimes (bool loop, EditTimeRange times)
    {
        if (looping != loop || (loop && times != getLoopTimes()))
        {
            const juce::ScopedLock sl (lock);

            auto lastPos = getPosition();
            looping = loop;
            playRange = times;
            setPosition (lastPos);
        }
    }

    void setRollInToLoop (double t)
    {
        const juce::ScopedLock sl (lock);

        rollInToLoop = true;
        streamSyncTime = lastStreamTime;
        playoutSyncTime = juce::jmin (t, playRange.end);
    }

    //==============================================================================
    /** called by the DeviceManager */
    void deviceManagerPositionUpdate (double newTime, double newTimeEnd)
    {
        const juce::ScopedLock sl (lock);

        lastStreamTime = newTime;
        lastStreamTimeEnd = newTimeEnd;

        if (rollInToLoop && getPosition() > playRange.start + 1.0)
            rollInToLoop = false;
    }

private:
    std::atomic<double> speed { 0 };
    double streamSyncTime = 0, playoutSyncTime = 0;
    EditTimeRange playRange;
    double lastStreamTime = 0, lastStreamTimeEnd = 0;
    juce::Time userInteractionTime;
    juce::CriticalSection lock;
    std::atomic<bool> looping { false }, userDragging { false }, rollInToLoop { false };

    /** the length of the small looped blocks to play while scrubbing */
    static constexpr double getScrubbingBlockLengthSeconds()      { return 0.08; }

    void userInteraction()  { userInteractionTime = juce::Time::getCurrentTime(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayHead)
};

} // namespace tracktion_engine
