/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#ifdef _WIN32
 #include <windows.h>
#endif

namespace tracktion { inline namespace graph
{

#ifdef _WIN32
    bool setThreadPriority (void* handle, int priority)
    {
        assert (handle != nullptr);
        int pri = THREAD_PRIORITY_TIME_CRITICAL;

        if (priority < 1)       pri = THREAD_PRIORITY_IDLE;
        else if (priority < 2)  pri = THREAD_PRIORITY_LOWEST;
        else if (priority < 5)  pri = THREAD_PRIORITY_BELOW_NORMAL;
        else if (priority < 7)  pri = THREAD_PRIORITY_NORMAL;
        else if (priority < 9)  pri = THREAD_PRIORITY_ABOVE_NORMAL;
        else if (priority < 10) pri = THREAD_PRIORITY_HIGHEST;

        return SetThreadPriority (handle, pri) != FALSE;
    }
#else
    template<typename HandleType>
    bool setThreadPriority (HandleType handle, int priority)
    {
        assert (handle != HandleType());
        
        struct sched_param param;
        int policy;
        priority = std::max (0, std::min (10, priority));

        if (pthread_getschedparam ((pthread_t) handle, &policy, &param) != 0)
            return false;

        policy = priority == 0 ? SCHED_OTHER : SCHED_RR;

        const int minPriority = sched_get_priority_min (policy);
        const int maxPriority = sched_get_priority_max (policy);

        param.sched_priority = ((maxPriority - minPriority) * priority) / 10 + minPriority;
        return pthread_setschedparam ((pthread_t) handle, policy, &param) == 0;
    }
#endif

bool setThreadPriority (std::thread& t, int priority)
{
    return setThreadPriority (t.native_handle(), priority);
}

}} // namespace tracktion_engine
