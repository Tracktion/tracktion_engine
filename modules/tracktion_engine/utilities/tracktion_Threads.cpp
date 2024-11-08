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

    inline std::atomic<bool>& getShouldExitFlag (std::thread::id id, [[maybe_unused]] bool canCreate)
    {
        auto& map = getShouldExitMap();
        const std::scoped_lock sl (map.idMapMutex);

        if (auto found = map.map.find (id);
            found != map.map.end())
            return found->second;

        assert (canCreate && "Must call prepareThreadForShouldExit before signaling");
        assert (! map.map.contains (id));
        auto& inserted = map.map[id];
        inserted = false;
        assert (map.map.contains (id));
        return inserted;
    }

    inline bool isThreadSupplyingExitStatus (std::thread::id id)
    {
        auto& map = details::getShouldExitMap();
        const std::scoped_lock sl (map.idMapMutex);
        return map.map.contains (id);
    }

    inline void prepareThreadForShouldExit (std::thread::id id)
    {
        details::getShouldExitFlag (id, true);

       #if JUCE_DEBUG
        auto& map = details::getShouldExitMap();
        const std::scoped_lock sl (map.idMapMutex);
        assert (map.map.contains (id));
       #endif
    }

    inline void threadHasExited (std::thread::id id)
    {
        auto& map = details::getShouldExitMap();

        const std::scoped_lock sl (map.idMapMutex);
        assert (map.map.contains (id));
        map.map.erase (id);
        assert (! map.map.contains (id));
    }

} // namespace details


//==============================================================================
//==============================================================================
ScopedThreadExitStatusEnabler::ScopedThreadExitStatusEnabler()
{
    details::prepareThreadForShouldExit (threadID);
}

ScopedThreadExitStatusEnabler::~ScopedThreadExitStatusEnabler()
{
    details::threadHasExited (threadID);
}

bool isCurrentThreadSupplyingExitStatus()
{
    return details::isThreadSupplyingExitStatus (std::this_thread::get_id());
}

void signalThreadShouldExit (std::thread::id id)
{
    using namespace std::literals;

    // If you get a block here it means you haven't created a ScopedThreadExitStatusEnabler for the above thread::id
    while (! details::isThreadSupplyingExitStatus  (id))
        std::this_thread::sleep_for (1ms);

    details::getShouldExitFlag (id, false) = true;
}

bool shouldCurrentThreadExit()
{
    return details::getShouldExitFlag (std::this_thread::get_id(), false);
}

} // namespace tracktion::inline engine
