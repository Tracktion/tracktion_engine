/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#include "tracktion_RoundTripLatency.h"

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LATENCY
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "tracktion_EnginePlayer.h"

///@internal
namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Latency offset detection")
    {
        using namespace choc::buffer;
        juce::AudioBuffer<float> testSound = test_utilities::createSyncTestSound();

        constexpr int diffNumSamples = 1234;
        juce::AudioBuffer<float> recorded (testSound.getNumChannels(), testSound.getNumSamples() + diffNumSamples);
        recorded.clear();
        choc::buffer::copy (toBufferView (recorded).fromFrame (static_cast<FrameCount> (diffNumSamples)),
                            toBufferView (testSound));

        auto detectedOffset = test_utilities::findSyncDeltaSamples (testSound, recorded);
        CHECK(detectedOffset);
        CHECK_EQ(*detectedOffset, diffNumSamples);
    }

    TEST_CASE("LatencyTester: No device latency")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 1, .outputChannels = 1,
                                                       .inputNames = {}, .outputNames = {} });

        std::optional<int> foundOffset;
        test_utilities::LatencyTester tester (engine.getDeviceManager().deviceManager,
                                              [&foundOffset] (auto result)
                                              { foundOffset = result; });

        // N.B. For testing purposes, we need to remove the DeviceManager from the callback as
        // the HostedAudioDevice has a single buffer that will be cleared by the device manager,
        // breaking the loop-back
        engine.getDeviceManager().closeDevices();
        tester.beginTest();

        {
            const auto numFramesToProcess = toSamples (3_td, player.getParams().sampleRate);
            processLoopedBack (player, numFramesToProcess);
        }

        CHECK(tester.tryToEndTest());
        CHECK(foundOffset);

        // N.B. We're expecting a block size offset here
        CHECK_EQ(*foundOffset, player.getParams().blockSize);

        auto res = test_utilities::getLatencyTesterResult (engine.getDeviceManager(), *foundOffset);
        CHECK_EQ(res.adjustedNumSamples, player.getParams().blockSize);
    }

    TEST_CASE("LatencyTester: Device latency")
    {
        constexpr int inputLatencyNumSamples = 1000;
        constexpr int outputLatencyNumSamples = 250;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 1, .outputChannels = 1,
                                                       .inputNames = {}, .outputNames = {},
                                                       .inputLatencyNumSamples = inputLatencyNumSamples,
                                                       .outputLatencyNumSamples = outputLatencyNumSamples });

        std::optional<int> foundOffset;
        test_utilities::LatencyTester tester (engine.getDeviceManager().deviceManager,
                                              [&foundOffset] (auto result)
                                              { foundOffset = result; });

        // N.B. For testing purposes, we need to remove the DeviceManager from the callback as
        // the HostedAudioDevice has a single buffer that will be cleared by the device manager,
        // breaking the loop-back
        engine.getDeviceManager().closeDevices();
        tester.beginTest();

        {
            const auto numFramesToProcess = toSamples (3_td, player.getParams().sampleRate);
            processLoopedBack (player, numFramesToProcess);
        }

        CHECK(tester.tryToEndTest());
        CHECK(foundOffset);

        // N.B. We're expecting a block size offset here
        CHECK_EQ(*foundOffset, player.getParams().blockSize + inputLatencyNumSamples + outputLatencyNumSamples);

        auto res = test_utilities::getLatencyTesterResult (engine.getDeviceManager(), *foundOffset);
        CHECK_EQ(res.deviceLatency, inputLatencyNumSamples + outputLatencyNumSamples);
        CHECK_EQ(res.adjustedNumSamples, player.getParams().blockSize);
    }
}


} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
