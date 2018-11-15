/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct ScopedCpuMeter
{
    ScopedCpuMeter (std::atomic<double>& valueToUpdate_, double filterAmount_) noexcept
        : valueToUpdate (valueToUpdate_), filterAmount (filterAmount_)
    {
    }

    ~ScopedCpuMeter() noexcept
    {
        const double msTaken = juce::Time::getMillisecondCounterHiRes() - callbackStartTime;
        valueToUpdate.store (filterAmount * (msTaken - valueToUpdate));
    }

private:
    std::atomic<double>& valueToUpdate;
    const double filterAmount, callbackStartTime { juce::Time::getMillisecondCounterHiRes() };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedCpuMeter)
};

//==============================================================================
struct StopwatchTimer
{
    StopwatchTimer() noexcept    : time (juce::Time::getCurrentTime()) {}

    juce::RelativeTime getTime() const noexcept  { return juce::Time::getCurrentTime() - time; }
    juce::String getDescription() const          { return getTime().getDescription(); }
    double getSeconds() const noexcept           { return getTime().inSeconds(); }

private:
    const juce::Time time;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StopwatchTimer)
};

//==============================================================================
#if TRACKTION_CHECK_FOR_SLOW_RENDERING
 struct RealtimeCheck
 {
    RealtimeCheck (const char* f, int l, double maxMillisecs) noexcept
        : start (juce::Time::getMillisecondCounterHiRes()),
          end (start + maxMillisecs),
          file (f), line (l)
    {
    }

    ~RealtimeCheck() noexcept
    {
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (now > end)
            juce::Logger::outputDebugString (juce::String (now - start) + " " + file + ":" + juce::String (line));
    }

    double start, end;
    const char* file;
    int line;
 };

 #define SCOPED_REALTIME_CHECK           const RealtimeCheck JUCE_JOIN_MACRO(__realtimeCheck, __LINE__) (__FILE__, __LINE__, 1.5);
 #define SCOPED_REALTIME_CHECK_LONGER    const RealtimeCheck JUCE_JOIN_MACRO(__realtimeCheck, __LINE__) (__FILE__, __LINE__, 200 / 44.1);
#else
 #define SCOPED_REALTIME_CHECK
 #define SCOPED_REALTIME_CHECK_LONGER
#endif

} // namespace tracktion_engine
