/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_graph
{

#ifdef _WIN32
    bool setThreadPriority (void* handle, int priority)
    {
        int pri = THREAD_PRIORITY_TIME_CRITICAL;

        if (priority < 1)       pri = THREAD_PRIORITY_IDLE;
        else if (priority < 2)  pri = THREAD_PRIORITY_LOWEST;
        else if (priority < 5)  pri = THREAD_PRIORITY_BELOW_NORMAL;
        else if (priority < 7)  pri = THREAD_PRIORITY_NORMAL;
        else if (priority < 9)  pri = THREAD_PRIORITY_ABOVE_NORMAL;
        else if (priority < 10) pri = THREAD_PRIORITY_HIGHEST;

        if (handle == 0)
            handle = GetCurrentThread();

        return SetThreadPriority (handle, pri) != FALSE;
    }
#else
    bool setThreadPriority (void* handle, int priority)
    {
        struct sched_param param;
        int policy;
        priority = std::max (0, std::min (10, priority));

        if (handle == nullptr)
            handle = (void*) pthread_self();

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

} // namespace tracktion_engine
