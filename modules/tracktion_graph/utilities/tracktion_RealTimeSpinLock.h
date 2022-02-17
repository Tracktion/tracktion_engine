/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if JUCE_INTEL
 #include <emmintrin.h>
#endif

namespace tracktion { inline namespace graph
{

/** A basic spin lock that uses an atomic_flag to store the locked state so should never result in a system call.
    Note that only try_lock should be called from a real-time thread. If you can't take the lock you should exit quickly.
*/
class RealTimeSpinLock
{
public:
    /** Takes the lock, blocking if necessary. */
    void lock() noexcept
    {
        for (;;)
        {
            for (int i = 0; i < 5; ++i)
                if (try_lock())
                    return;

            for (int i = 0; i < 10; ++i)
            {
                #if JUCE_INTEL
                 _mm_pause();
                #else
                 __asm__ __volatile__ ("yield");
                #endif

                if (try_lock())
                    return;
            }
        }
    }
    
    /** Releases the lock, this should only be called after a successful call to try_lock or lock. */
    void unlock() noexcept
    {
        flag.clear (std::memory_order_release);
    }
    
    /** Attempts to take the lock once, returning true if successful. */
    bool try_lock() noexcept
    {
        return ! flag.test_and_set (std::memory_order_acquire);
    }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

}}
