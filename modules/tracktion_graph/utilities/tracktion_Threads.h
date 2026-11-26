/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline graph {

/** Changes the thread's priority.

    May return false if for some reason the priority can't be changed.

    @param priority     the new priority, in the range 0 (lowest) to 10 (highest). A priority
                        of 5 is normal.
*/
bool setThreadPriority (std::thread&, int priority);

/** Tries to upgrade the current thread to realtime priority. */
bool tryToUpgradeCurrentThreadToRealtime (const juce::Thread::RealtimeOptions&);

/** Returns a description of the scheduling policy and priority the current
    thread actually has, which may not be the one that was asked for.
*/
std::string getCurrentThreadPriorityDescription();

//==============================================================================
/**
    Blocks a thread until a point in time using the most accurate timer the
    platform has, for threads that need to wake up at the cadence of audio blocks.

    On Windows this uses a high-resolution waitable timer, as the default
    timers are only accurate to between 1 and 15ms.
*/
class HighResolutionSleeper
{
public:
    /** Creates a HighResolutionSleeper. */
    HighResolutionSleeper();

    /** Destructor. */
    ~HighResolutionSleeper();

    /** Blocks until the given time, returning straight away if it has passed. */
    void sleepUntil (std::chrono::steady_clock::time_point);

    /** Returns a description of the timer being used. */
    std::string getDescription() const;

private:
    [[ maybe_unused ]] void* timer = nullptr;
    [[ maybe_unused ]] bool isHighResolution = false;
};

} // namespace tracktion::inline graph
