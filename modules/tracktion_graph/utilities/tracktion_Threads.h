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

/** Changes the priority of the thread this is called from.
    @see setThreadPriority
*/
bool setCurrentThreadPriority (int priority);

/** Tries to upgrade the current thread to realtime priority. */
bool tryToUpgradeCurrentThreadToRealtime (const juce::Thread::RealtimeOptions&);

/** Returns a description of the scheduling policy and priority the current
    thread actually has, which may not be the one that was asked for.
*/
std::string getCurrentThreadPriorityDescription();

//==============================================================================
/**
    A workgroup for a real-time thread that isn't driven by an audio device,
    and the threads that do work for it.

    On macOS this tells the OS when each block of work starts and has to be
    finished by, as an audio device's workgroup does, so that it can schedule
    the threads which have joined it accordingly. On other platforms this does
    nothing.
*/
class RealTimeWorkgroup
{
public:
    /** Creates a workgroup, if the platform has them. */
    RealTimeWorkgroup();

    /** Destructor. All threads must have left the workgroup by now. */
    ~RealTimeWorkgroup();

    /** Returns true if a workgroup was created. */
    bool isValid() const;

    /** Returns the workgroup for threads to join, which is empty if this isn't valid. */
    juce::AudioWorkgroup getAudioWorkgroup() const;

    /** Call this from the thread driving the work, once it has joined the
        workgroup, when it starts a block that has to be finished in the given time.
        Returns false if the OS didn't accept this.
    */
    bool blockWillStart (std::chrono::duration<double> timeUntilDeadline);

    /** Call this from the same thread when the block has finished. */
    void blockDidFinish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

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
