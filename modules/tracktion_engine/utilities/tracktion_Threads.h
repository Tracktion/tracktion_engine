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


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================
namespace details
{
    struct ExitMapWithMutex
    {
        std::mutex idMapMutex;
        std::unordered_map<std::thread::id, std::atomic<bool>> map;
    };

    static ExitMapWithMutex& getShouldExitMap()
    {
        static ExitMapWithMutex map;
        return map;
    }

    inline std::atomic<bool>& getShouldExitFlag (std::thread::id id, bool canCreate)
    {
        auto& map = getShouldExitMap();
        const std::scoped_lock sl (map.idMapMutex);

        if (auto found = map.map.find (id);
            found != map.map.end())
            return found->second;

        assert (canCreate && "Must call prepareThreadForShouldExit before signaling");
        auto& inserted = map.map[id];
        inserted = false;
        return inserted;
    }

} // namespace details


inline void prepareThreadForShouldExit (std::thread::id id)
{
    details::getShouldExitFlag (id, true);
}

inline bool isCurrentThreadSupplyingExitStatus()
{
    auto& map = details::getShouldExitMap();
    const std::scoped_lock sl (map.idMapMutex);
    return map.map.contains (std::this_thread::get_id());
}

inline void signalThreadShouldExit (std::thread::id id)
{
    details::getShouldExitFlag (id, false) = true;
}

inline bool shouldCurrentThreadExit()
{
    return details::getShouldExitFlag (std::this_thread::get_id(), false);
}

inline void threadHasExited (std::thread::id id)
{
    auto& map = details::getShouldExitMap();

    const std::scoped_lock sl (map.idMapMutex);
    map.map.erase (id);
}

inline ScopedThreadExitStatusEnabler::ScopedThreadExitStatusEnabler()
{
    prepareThreadForShouldExit (std::this_thread::get_id());
}

inline ScopedThreadExitStatusEnabler::~ScopedThreadExitStatusEnabler()
{
    threadHasExited (std::this_thread::get_id());
}


} // namespace tracktion::inline engine
