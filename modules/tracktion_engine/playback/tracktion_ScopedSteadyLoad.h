/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class ScopedSteadyLoad
{
public:
    //==============================================================================
    struct Context
    {
        Context() {}

        Context (double sampleRateToUse)
        {
            setSampleRate (sampleRateToUse);
        }

        void setSampleRate (double newSampleRate) noexcept
        {
            jassert (newSampleRate > 0);

            sampleRateHz = newSampleRate;
        }

        void setLoadLevel (float newValue) noexcept
        {
            jassert (juce::isPositiveAndBelow (newValue, 1.0f));
            loadLevel = newValue;
        }

        void setNumNopsPerIteration (int newValue) noexcept
        {
            jassert (newValue > 0);
            numNopsPerIteration = newValue;
        }

        void setNumCallbacksToIgnore (int newValue) noexcept
        {
            jassert (newValue >= 0);
            ignoreCounter = newValue;
        }

    private:
        friend class ScopedSteadyLoad;

        double sampleRateHz       { 0.0 };
        int64_t callbackEpoch     { 0 };
        float loadLevel         { 0.8f };
        int numNopsPerIteration { 10000 };
        int callbackCount       { 0 };
        int ignoreCounter       { 4 };

        int64_t getCallbackPeriod (int bufferSize) const noexcept
        {
            jassert (bufferSize > 0 && sampleRateHz > 0.0);

            return juce::Time::secondsToHighResolutionTicks (bufferSize / sampleRateHz);
        }
    };

    //==============================================================================
    ScopedSteadyLoad (Context& contextToUse, int bufferSize)
        : context (contextToUse)
    {
        if (context.ignoreCounter > 0)
            return;

        if (context.callbackCount == 0)
            context.callbackEpoch = startTime;

        auto callbackPeriod = context.getCallbackPeriod (bufferSize);

        // get the deadline for this callback by calculating the periods since the first callback
        auto timeSinceEpoch = startTime - context.callbackEpoch;
        auto numPeriodsSinceEpoch = timeSinceEpoch / callbackPeriod;

        if (numPeriodsSinceEpoch < context.callbackCount)
        {
            // Previous epoch was set using a late callback
            // reset to this new earlier (more accurate) callback time
            context.callbackEpoch = startTime;
            context.callbackCount = 0;
            timeSinceEpoch = 0;
            numPeriodsSinceEpoch = 0;
        }

        const auto startedLateDuration = timeSinceEpoch - numPeriodsSinceEpoch * callbackPeriod;
        const auto availableDuration = callbackPeriod - startedLateDuration
                                        - juce::Time::secondsToHighResolutionTicks (loadGenerationEndEarlyDurationSec);

        targetDuration = int64_t (context.loadLevel * callbackPeriod);

        if (targetDuration >= availableDuration)
            targetDuration = availableDuration;
    }

    ~ScopedSteadyLoad()
    {
        if (context.ignoreCounter > 0)
        {
            --context.ignoreCounter;
            return;
        }

        const auto callbackEndTime = juce::Time::getHighResolutionTicks();
        const auto realExecutionDuration = callbackEndTime - startTime;
        const auto artificialLoadDuration = targetDuration - realExecutionDuration;

        while (juce::Time::getHighResolutionTicks() - callbackEndTime <= artificialLoadDuration)
            for (int i = 0; i < context.numNopsPerIteration; ++i)
                asm ("nop");

        ++context.callbackCount;
    }

private:
    const int64_t startTime   { juce::Time::getHighResolutionTicks() };
    Context& context;

    int64_t targetDuration;

    // We never want to exceed our callback deadline, so when we're close to it we'll end early
    // this defines how early we should end in nanos
    static constexpr double loadGenerationEndEarlyDurationSec = 10000e-9;
};

} // namespace tracktion_engine
