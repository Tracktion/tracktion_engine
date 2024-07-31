/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

///@internal
namespace tracktion::inline engine
{

#ifndef DOXYGEN
namespace test_utilities
{

static constexpr int testLength = 48000 / 3;

inline juce::Array<int> createSpikes()
{
    juce::Array<int> spikes;

    juce::Random rand;
    rand.setSeed (12345);

    int spikePos = 0;
    int spikeDelta = 50;

    while (spikePos < testLength)
    {
        spikes.add (spikePos);
        spikePos += spikeDelta;
        spikeDelta += spikeDelta / 6 + rand.nextInt (5);
    }

    return spikes;
}

inline juce::AudioBuffer<float> createSyncTestSound()
{
    juce::AudioBuffer<float> testSound;

    testSound.setSize (1, testLength);
    testSound.clear();

    auto s = testSound.getWritePointer (0);

    {
        juce::Random rand;

        for (int i = 0; i < testLength; ++i)
            s[i] = (rand.nextFloat() - rand.nextFloat() + rand.nextFloat() - rand.nextFloat()) * 0.06f;
    }

    for (auto spikePos : createSpikes())
    {
        spikePos += 10;

        if (spikePos < testLength)
        {
            s[spikePos] = 0.99f;
            s[spikePos + 1] = -0.99f;
        }
    }

    return testSound;
}

static int findOffsetOfSpikes (const juce::AudioBuffer<float>& buffer)
{
    auto minSpikeLevel = 5.0f;
    auto smooth = 0.975;
    auto* s = buffer.getReadPointer (0);
    constexpr int spikeDriftAllowed = 5;

    const auto originalSpikes = createSpikes();
    const auto failedMatchesAllowed = originalSpikes.size() / 5;

    juce::Array<int> spikesFound;
    spikesFound.ensureStorageAllocated (100);
    auto runningAverage = 0.0;
    int lastSpike = 0;

    for (int i = 0; i < buffer.getNumSamples() - 10; ++i)
    {
        auto samp = std::abs (s[i]);

        if (samp > runningAverage * minSpikeLevel && i > lastSpike + 20)
        {
            lastSpike = i;
            spikesFound.add (i);
        }

        runningAverage = runningAverage * smooth + (1.0 - smooth) * samp;
    }

    if (spikesFound.size() < originalSpikes.size() / 3)
        return -1;

    auto findNearestSpike = [&] (int targetOffset)
    {
        int nearestOffset = -1;
        int nearestDistance = spikeDriftAllowed + 1;

        for (auto offset : spikesFound)
        {
            auto distance = offset - targetOffset;

            if (distance < -spikeDriftAllowed)
                continue;

            if (distance > spikeDriftAllowed)
                break;

            distance = std::abs (distance);

            if (distance < nearestDistance)
            {
                nearestOffset = offset;
                nearestDistance = distance;
            }
        }

        return nearestOffset;
    };

    int bestOffset = -1;
    int64_t bestMatchQuality = 25 * 10;

    for (int offsetToTest = 0; offsetToTest < buffer.getNumSamples() - 2048; ++offsetToTest)
    {
        int64_t matchQuality = 0;
        int numFailedMatches = 0;

        for (auto referenceOffset : originalSpikes)
        {
            auto nearestSpikeDistance = findNearestSpike (referenceOffset + offsetToTest);

            if (nearestSpikeDistance < 0)
            {
                if (++numFailedMatches > failedMatchesAllowed)
                {
                    matchQuality = 0;
                    break;
                }

                continue;
            }

            matchQuality += (spikeDriftAllowed - nearestSpikeDistance)
                                * (spikeDriftAllowed - nearestSpikeDistance);
        }

        if (matchQuality > bestMatchQuality)
        {
            bestMatchQuality = matchQuality;
            bestOffset = offsetToTest;
        }
    }

    return bestOffset;
}

inline std::optional<int> findSyncDeltaSamples (const juce::AudioBuffer<float>& testSound,
                                                const juce::AudioBuffer<float>& recorded)
{
    auto referenceStart = findOffsetOfSpikes (testSound);
    jassert (referenceStart >= 0);

    auto recordedStart = findOffsetOfSpikes (recorded);

    if (recordedStart < 0)
        return {};

    return recordedStart - referenceStart;
}

struct LatencyTesterResult
{
    double sampleRate;
    int bufferSize;
    int deviceLatency;
    int adjustedNumSamples;
    TimeDuration adjustedTime;
};

inline LatencyTesterResult getLatencyTesterResult (DeviceManager& dm, int detectedOffsetNumSamples)
{
    LatencyTesterResult res;
    res.sampleRate = dm.getSampleRate();
    res.bufferSize = dm.getBlockSize();
    res.deviceLatency = dm.getRecordAdjustmentSamples();

    res.adjustedNumSamples = detectedOffsetNumSamples - res.deviceLatency;
    res.adjustedTime = TimeDuration::fromSamples (res.adjustedNumSamples, res.sampleRate);

    return res;
}


//==============================================================================
//==============================================================================
struct LatencyTester    : public juce::AudioIODeviceCallback,
                          private juce::Timer
{
    LatencyTester (juce::AudioDeviceManager& d, std::function<void (std::optional<int>)>testFinishedCallback_)
        : deviceManager (d),
          testFinishedCallback (std::move (testFinishedCallback_))
    {
        assert (testFinishedCallback);
        deviceManager.addAudioCallback (this);
    }

    ~LatencyTester() override
    {
        deviceManager.removeAudioCallback (this);
    }

    void beginTest()
    {
        startTimer (50);

        const juce::ScopedLock sl (lock);
        testSound = createSyncTestSound();
        recordedSound.clear();
        playingSampleNum = recordedSampleNum = 0;
        isRunning = true;
    }

    //==============================================================================
    /** @internal */
    bool tryToEndTest()
    {
        if (isRunning && recordedSampleNum >= recordedSound.getNumSamples())
        {
            isRunning = false;
            stopTimer();

            testFinishedCallback (findSyncDeltaSamples (testSound, recordedSound));
            return true;
        }

        return false;
    }

    /** @internal */
    void timerCallback() override
    {
        CRASH_TRACER
        tryToEndTest();
    }

    //==============================================================================
    /** @internal */
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        isRunning = false;
        sampleRate = device->getCurrentSampleRate();
        deviceInputLatency = device->getInputLatencyInSamples();
        deviceOutputLatency = device->getOutputLatencyInSamples();
        playingSampleNum = recordedSampleNum = 0;

        recordedSound.setSize (1, (int) (1.5 * sampleRate));
        recordedSound.clear();
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        const juce::ScopedLock sl (lock);

        if (isRunning)
        {
            auto recordingBuffer = recordedSound.getWritePointer (0, 0);
            auto playBuffer = testSound.getReadPointer (0, 0);

            for (int i = 0; i < numSamples; ++i)
            {
                if (recordedSampleNum < recordedSound.getNumSamples())
                {
                    float inputSamp = 0.0f;

                    for (int j = numInputChannels; --j >= 0;)
                        if (const float* const chan = inputChannelData[j])
                            inputSamp += chan[i];

                    recordingBuffer[recordedSampleNum] = inputSamp;
                }

                ++recordedSampleNum;

                const float outputSamp = (playingSampleNum < testSound.getNumSamples()) ? playBuffer [playingSampleNum] : 0;

                for (int j = numOutputChannels; --j >= 0;)
                    if (float* const chan = outputChannelData[j])
                        chan[i] = outputSamp;

                ++playingSampleNum;
            }
        }
        else
        {
            // We need to clear the output buffers, in case they're full of junk..
            for (int i = 0; i < numOutputChannels; ++i)
                if (float* const chan = outputChannelData[i])
                    juce::FloatVectorOperations::clear (chan, numSamples);
        }
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::function<void (std::optional<int>)> testFinishedCallback;

    juce::AudioBuffer<float> testSound, recordedSound;
    int playingSampleNum = 0, recordedSampleNum = -1;
    juce::CriticalSection lock;
    double sampleRate = 0;
    bool isRunning = false;
    int deviceInputLatency = 0, deviceOutputLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatencyTester)
};

} // namespace test_utilities
#endif //DOXYGEN

} // namespace tracktion::inline engine
