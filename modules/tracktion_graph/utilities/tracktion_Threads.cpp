/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <ctime>

#ifdef _WIN32
 #include <windows.h>
#endif

#if JUCE_MAC
 #include <AudioToolbox/AudioWorkInterval.h>
 #include <mach/mach_time.h>
 #include <os/workgroup.h>

 // JUCE defines this in an Objective-C++ file, where os_workgroup_t is a pointer
 // to this type, so it has to be declared with it to be linked to from C++
 struct OS_os_workgroup;
 namespace juce { AudioWorkgroup makeRealAudioWorkgroup (OS_os_workgroup*); }
#endif

namespace tracktion::inline graph {

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

double getProcessCpuTimeSeconds()
{
   #ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;

    if (! GetProcessTimes (GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
        return 0.0;

    const auto toSeconds = [] (const FILETIME& ft)
    {
        return static_cast<double> ((static_cast<uint64_t> (ft.dwHighDateTime) << 32) | ft.dwLowDateTime) * 100.0e-9;
    };

    return toSeconds (kernelTime) + toSeconds (userTime);
   #else
    return static_cast<double> (std::clock()) / static_cast<double> (CLOCKS_PER_SEC);
   #endif
}

bool setCurrentThreadPriority (int priority)
{
   #ifdef _WIN32
    return setThreadPriority (GetCurrentThread(), priority);
   #else
    return setThreadPriority (pthread_self(), priority);
   #endif
}

//==============================================================================
#ifdef _WIN32
    std::string getCurrentThreadPriorityDescription()
    {
        auto getThreadPriorityName = []() -> std::string
        {
            switch (GetThreadPriority (GetCurrentThread()))
            {
                case THREAD_PRIORITY_TIME_CRITICAL:     return "TIME_CRITICAL";
                case THREAD_PRIORITY_HIGHEST:           return "HIGHEST";
                case THREAD_PRIORITY_ABOVE_NORMAL:      return "ABOVE_NORMAL";
                case THREAD_PRIORITY_NORMAL:            return "NORMAL";
                case THREAD_PRIORITY_BELOW_NORMAL:      return "BELOW_NORMAL";
                case THREAD_PRIORITY_LOWEST:            return "LOWEST";
                case THREAD_PRIORITY_IDLE:              return "IDLE";
                default:                                return "unknown";
            }
        };

        auto getPriorityClassName = []() -> std::string
        {
            switch (GetPriorityClass (GetCurrentProcess()))
            {
                case REALTIME_PRIORITY_CLASS:           return "REALTIME";
                case HIGH_PRIORITY_CLASS:               return "HIGH";
                case ABOVE_NORMAL_PRIORITY_CLASS:       return "ABOVE_NORMAL";
                case NORMAL_PRIORITY_CLASS:             return "NORMAL";
                case BELOW_NORMAL_PRIORITY_CLASS:       return "BELOW_NORMAL";
                case IDLE_PRIORITY_CLASS:               return "IDLE";
                default:                                return "unknown";
            }
        };

        return "thread priority " + getThreadPriorityName() + " in a " + getPriorityClassName() + " priority class process";
    }

    HighResolutionSleeper::HighResolutionSleeper()
    {
        // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, which needs Windows 10 1803 and isn't in older SDKs
        constexpr DWORD highResolutionFlag = 0x00000002;
        timer = CreateWaitableTimerExW (nullptr, nullptr, highResolutionFlag, TIMER_ALL_ACCESS);
        isHighResolution = timer != nullptr;

        if (timer == nullptr)
            timer = CreateWaitableTimerExW (nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }

    HighResolutionSleeper::~HighResolutionSleeper()
    {
        if (timer != nullptr)
            CloseHandle (timer);
    }

    void HighResolutionSleeper::sleepUntil (std::chrono::steady_clock::time_point wakeUpTime)
    {
        const auto timeToWait = wakeUpTime - std::chrono::steady_clock::now();

        if (timeToWait <= std::chrono::steady_clock::duration::zero())
            return;

        // A negative due time is relative, in 100ns units
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -static_cast<LONGLONG> (std::chrono::duration_cast<std::chrono::nanoseconds> (timeToWait).count() / 100);

        if (timer != nullptr && dueTime.QuadPart < 0 && SetWaitableTimer (timer, &dueTime, 0, nullptr, nullptr, FALSE) != FALSE)
            WaitForSingleObject (timer, INFINITE);
        else
            std::this_thread::sleep_until (wakeUpTime);
    }

    std::string HighResolutionSleeper::getDescription() const
    {
        return isHighResolution ? "high-resolution waitable timer"
                                : (timer != nullptr ? "standard-resolution waitable timer" : "std::this_thread::sleep_until");
    }
#else
    std::string getCurrentThreadPriorityDescription()
    {
       #if JUCE_MAC
        {
            thread_time_constraint_policy_data_t policy;
            mach_msg_type_number_t count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
            boolean_t isDefault = FALSE;

            if (thread_policy_get (pthread_mach_thread_np (pthread_self()),
                                   THREAD_TIME_CONSTRAINT_POLICY,
                                   (thread_policy_t) &policy, &count, &isDefault) == KERN_SUCCESS
                && ! isDefault)
            {
                auto toMs = [] (uint32_t ticks) { return juce::String (juce::Time::highResolutionTicksToSeconds (ticks) * 1000.0, 2); };

                return ("real-time time-constraint policy (period " + toMs (policy.period)
                        + "ms, computation " + toMs (policy.computation)
                        + "ms, constraint " + toMs (policy.constraint) + "ms)").toStdString();
            }
        }
       #endif

        struct sched_param param;
        int policy = 0;

        if (pthread_getschedparam (pthread_self(), &policy, &param) != 0)
            return "unknown";

        const auto policyName = policy == SCHED_FIFO ? "SCHED_FIFO"
                                                     : (policy == SCHED_RR ? "SCHED_RR"
                                                                           : (policy == SCHED_OTHER ? "SCHED_OTHER (not real-time)"
                                                                                                    : "unknown policy"));

        return std::string (policyName) + ", priority " + std::to_string (param.sched_priority)
                + " in the range " + std::to_string (sched_get_priority_min (policy))
                + " to " + std::to_string (sched_get_priority_max (policy));
    }

    HighResolutionSleeper::HighResolutionSleeper() = default;
    HighResolutionSleeper::~HighResolutionSleeper() = default;

    void HighResolutionSleeper::sleepUntil (std::chrono::steady_clock::time_point wakeUpTime)
    {
        std::this_thread::sleep_until (wakeUpTime);
    }

    std::string HighResolutionSleeper::getDescription() const
    {
        return "std::this_thread::sleep_until";
    }
#endif

//==============================================================================
#if JUCE_MAC
struct RealTimeWorkgroup::Impl
{
    Impl()
    {
        if (__builtin_available (macOS 11.0, *))
            workgroup = AudioWorkIntervalCreate ("tracktion_graph real-time workgroup", OS_CLOCK_MACH_ABSOLUTE_TIME, nullptr);

        mach_timebase_info (&timebase);
    }

    ~Impl()
    {
        if (workgroup != nullptr)
            os_release (workgroup);
    }

    os_workgroup_interval_t workgroup = nullptr;
    mach_timebase_info_data_t timebase {};
};

RealTimeWorkgroup::RealTimeWorkgroup()
    : impl (std::make_unique<Impl>())
{
}

RealTimeWorkgroup::~RealTimeWorkgroup() = default;

bool RealTimeWorkgroup::isValid() const
{
    return impl->workgroup != nullptr;
}

juce::AudioWorkgroup RealTimeWorkgroup::getAudioWorkgroup() const
{
    if (! isValid())
        return {};

    return juce::makeRealAudioWorkgroup (reinterpret_cast<OS_os_workgroup*> (impl->workgroup));
}

bool RealTimeWorkgroup::blockWillStart (std::chrono::duration<double> timeUntilDeadline)
{
    if (! isValid())
        return false;

    if (__builtin_available (macOS 11.0, *))
    {
        const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds> (timeUntilDeadline).count();
        const auto ticks = static_cast<uint64_t> (nanoseconds) * impl->timebase.denom / impl->timebase.numer;
        const auto now = mach_absolute_time();

        return os_workgroup_interval_start (impl->workgroup, now, now + ticks, nullptr) == 0;
    }

    return false;
}

void RealTimeWorkgroup::blockDidFinish()
{
    if (! isValid())
        return;

    if (__builtin_available (macOS 11.0, *))
        juce::ignoreUnused (os_workgroup_interval_finish (impl->workgroup, nullptr));
}
#else
struct RealTimeWorkgroup::Impl {};

RealTimeWorkgroup::RealTimeWorkgroup() = default;
RealTimeWorkgroup::~RealTimeWorkgroup() = default;
bool RealTimeWorkgroup::isValid() const                                     { return false; }
juce::AudioWorkgroup RealTimeWorkgroup::getAudioWorkgroup() const           { return {}; }
bool RealTimeWorkgroup::blockWillStart (std::chrono::duration<double>)      { return false; }
void RealTimeWorkgroup::blockDidFinish()                                    {}
#endif

} // namespace tracktion::inline graph
