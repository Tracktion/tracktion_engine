/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

#if JUCE_MAC
    template<typename Type>
    std::optional<Type> firstOptionalWithValue (const std::initializer_list<std::optional<Type>>& optionals)
    {
        for (const auto& optional : optionals)
            if (optional.has_value())
                return optional;

        return {};
    }

    bool tryToUpgradeCurrentThreadToRealtime (const juce::Thread::RealtimeOptions& options)
    {
        const auto periodMs = options.getPeriodMs().value_or (0.0);

        const auto processingTimeMs = firstOptionalWithValue (
        {
            options.getProcessingTimeMs(),
            options.getMaximumProcessingTimeMs(),
            options.getPeriodMs()
        }).value_or (10.0);

        const auto maxProcessingTimeMs = options.getMaximumProcessingTimeMs()
                                                .value_or (processingTimeMs);

        // The processing time can not exceed the maximum processing time!
        jassert (maxProcessingTimeMs >= processingTimeMs);

        thread_time_constraint_policy_data_t policy;
        policy.period = (uint32_t) juce::Time::secondsToHighResolutionTicks (periodMs / 1'000.0);
        policy.computation = (uint32_t) juce::Time::secondsToHighResolutionTicks (processingTimeMs / 1'000.0);
        policy.constraint = (uint32_t) juce::Time::secondsToHighResolutionTicks (maxProcessingTimeMs / 1'000.0);
        policy.preemptible = true;

        const auto result = thread_policy_set (pthread_mach_thread_np (pthread_self()),
                                               THREAD_TIME_CONSTRAINT_POLICY,
                                               (thread_policy_t) &policy,
                                               THREAD_TIME_CONSTRAINT_POLICY_COUNT);

        if (result == KERN_SUCCESS)
            return true;

        // testing has shown that passing a computation value > 50ms can
        // lead to thread_policy_set returning an error indicating that an
        // invalid argument was passed. If that happens this code tries to
        // limit that value in the hope of resolving the issue.

        if (result == KERN_INVALID_ARGUMENT && options.getProcessingTimeMs() > 50.0)
            return tryToUpgradeCurrentThreadToRealtime (options.withProcessingTimeMs (50.0));

        return false;
    }
#else
    bool tryToUpgradeCurrentThreadToRealtime (const juce::Thread::RealtimeOptions&)
    {
        return false;
    }
#endif

bool setThreadPriority (std::thread& t, int priority)
{
    return setThreadPriority (t.native_handle(), priority);
}

}} // namespace tracktion_engine
