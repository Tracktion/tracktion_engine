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
/**
    Enables the calling thread to be cancelled from another thread and the
    calling thread to be able to query this. Put one of these in your thread
    body then you can use signalThreadShouldExit and shouldCurrentThreadExit

    @code
    std::thread t ([]
    {
        const ScopedThreadExitStatusEnabler ese;

        for (;;)
        {
            if (shouldCurrentThreadExit())
                break;

            // Some work...
        }
    });

    signalThreadShouldExit (t.get_id());
    t.join();
    @endcode
 */
struct ScopedThreadExitStatusEnabler
{
    /** Enables the current thread to query shouldCurrentThreadExit and other
        threads to call signalThreadShouldExit.
    */
    ScopedThreadExitStatusEnabler();

    /** Cleans up the exit status. */
    ~ScopedThreadExitStatusEnabler();

    /** Returns the thread_id. */
    std::thread::id getID() const   { return threadID; }

private:
    const std::thread::id threadID { std::this_thread::get_id() };
};


//==============================================================================
/** Returns true if the current thread has an active ScopedThreadExitStatusEnabler. */
[[ nodiscard ]] bool isCurrentThreadSupplyingExitStatus();

/** Can be used to signal that the current thread is waiting to exit.
    Only to be used in conjunction with a ScopedThreadExitStatusEnabler.
*/
void signalThreadShouldExit (std::thread::id);

/** Returns true if the current thread is waiting to exit.
    Only to be used in conjunction with a ScopedThreadExitStatusEnabler.
*/
[[ nodiscard ]] bool shouldCurrentThreadExit();


} // namespace tracktion::inline engine
