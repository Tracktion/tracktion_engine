/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_WAVE_OUTPUT_DEVICE
    TEST_SUITE ("tracktion_engine")
    {
        TEST_CASE ("WaveOutputDevice: reverseChannels swaps device indices on stereo device")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                                                           .inputNames = {}, .outputNames = {} });

            auto& dm = engine.getDeviceManager();
            auto outputDevices = dm.getWaveOutputDevices();
            REQUIRE (outputDevices.size() > 0);

            auto device = outputDevices[0];
            REQUIRE (device->getChannels().getNumChannels() == 2);

            // Before reversal: channels should be consecutive [0, 1]
            CHECK (device->getChannels()[0].indexInDevice == 0);
            CHECK (device->getChannels()[1].indexInDevice == 1);
            CHECK (! device->isReversed());

            // After reversal: device indices should be swapped [1, 0]
            device->reverseChannels (true);
            CHECK (device->isReversed());
            CHECK (device->getChannels()[0].indexInDevice == 1);
            CHECK (device->getChannels()[1].indexInDevice == 0);

            // Channel types should be preserved
            CHECK (device->getChannels()[0].channel == juce::AudioChannelSet::left);
            CHECK (device->getChannels()[1].channel == juce::AudioChannelSet::right);

            // Un-reversing should restore original order
            device->reverseChannels (false);
            CHECK (! device->isReversed());
            CHECK (device->getChannels()[0].indexInDevice == 0);
            CHECK (device->getChannels()[1].indexInDevice == 1);
        }

        TEST_CASE ("WaveOutputDevice: reverseChannels is idempotent")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                                                           .inputNames = {}, .outputNames = {} });

            auto& dm = engine.getDeviceManager();
            auto device = dm.getWaveOutputDevices()[0];

            device->reverseChannels (true);
            CHECK (device->getChannels()[0].indexInDevice == 1);
            CHECK (device->getChannels()[1].indexInDevice == 0);

            // Calling reverseChannels(true) again should not double-reverse
            device->reverseChannels (true);
            CHECK (device->getChannels()[0].indexInDevice == 1);
            CHECK (device->getChannels()[1].indexInDevice == 0);

            device->reverseChannels (false);
        }

        TEST_CASE ("WaveOutputDevice: reverseChannels on 4-channel device")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 4,
                                                           .inputNames = {}, .outputNames = {} });

            auto& dm = engine.getDeviceManager();
            dm.setAllWaveOutputsToNumChannels (4);
            auto outputDevices = dm.getWaveOutputDevices();
            REQUIRE (outputDevices.size() > 0);

            auto device = outputDevices[0];
            REQUIRE (device->getChannels().getNumChannels() == 4);

            // Before reversal
            CHECK (device->getChannels()[0].indexInDevice == 0);
            CHECK (device->getChannels()[1].indexInDevice == 1);
            CHECK (device->getChannels()[2].indexInDevice == 2);
            CHECK (device->getChannels()[3].indexInDevice == 3);

            // After reversal: all device indices reversed
            device->reverseChannels (true);
            CHECK (device->getChannels()[0].indexInDevice == 3);
            CHECK (device->getChannels()[1].indexInDevice == 2);
            CHECK (device->getChannels()[2].indexInDevice == 1);
            CHECK (device->getChannels()[3].indexInDevice == 0);

            device->reverseChannels (false);
        }

        TEST_CASE ("WaveOutputDevice: reversed stereo output routes audio to swapped channels")
        {
            using namespace graph::test_utilities;

            auto& engine = *Engine::getEngines()[0];

            // Create a stereo sin file: both channels have sine (mono clip will be duplicated to stereo)
            auto sinFile = getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 220.0f);
            AudioFile af (engine, sinFile->getFile());

            // -- Normal (non-reversed) playback --
            {
                auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);

                auto& track = *getAudioTracks (*edit)[0];
                insertWaveClip (track, {}, sinFile->getFile(),
                                ClipPosition {{ 0_tp, TimePosition::fromSeconds (1.0) }}, DeleteExistingClips::no);

                auto player = test_utilities::createEnginePlayer (*edit,
                    { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                      .inputNames = {}, .outputNames = {} },
                    { af });

                // Ensure output is not reversed
                auto device = engine.getDeviceManager().getWaveOutputDevices()[0];
                device->reverseChannels (false);

                auto buffer = test_utilities::process (*player, TimeDuration::fromSeconds (1.0));
                auto bufferView = toBufferView (buffer);

                // Both channels should have audio (mono clip duplicated to stereo)
                auto rmsL = getRMS (bufferView.getChannel (0));
                auto rmsR = getRMS (bufferView.getChannel (1));
                CHECK_GT (rmsL, 0.1f);
                CHECK_GT (rmsR, 0.1f);
            }

            // -- Reversed playback: Create a stereo file with signal only on L --
            choc::buffer::ChannelArrayBuffer<float> stereoBuffer (choc::buffer::Size::create (2u, 44100u));

            for (choc::buffer::FrameCount frame = 0; frame < 44100u; ++frame)
            {
                stereoBuffer.getSample (0, frame) = std::sin (static_cast<float> (frame) * juce::MathConstants<float>::twoPi * 220.0f / 44100.0f);
                stereoBuffer.getSample (1, frame) = 0.0f;
            }

            auto stereoFile = writeToTemporaryFile<juce::WavAudioFormat> (stereoBuffer, 44100.0);
            AudioFile stereoAf (engine, stereoFile->getFile());

            // Normal playback: ch0 should have sine, ch1 should be silent
            {
                auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);

                auto& track = *getAudioTracks (*edit)[0];
                insertWaveClip (track, {}, stereoFile->getFile(),
                                ClipPosition {{ 0_tp, TimePosition::fromSeconds (1.0) }}, DeleteExistingClips::no);

                auto player = test_utilities::createEnginePlayer (*edit,
                    { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                      .inputNames = {}, .outputNames = {} },
                    { stereoAf });

                auto device = engine.getDeviceManager().getWaveOutputDevices()[0];
                device->reverseChannels (false);

                auto buffer = test_utilities::process (*player, TimeDuration::fromSeconds (1.0));
                auto bufferView = toBufferView (buffer);

                auto rmsNormalL = getRMS (bufferView.getChannel (0));
                auto rmsNormalR = getRMS (bufferView.getChannel (1));
                CHECK_GT (rmsNormalL, 0.3f);
                CHECK_LT (rmsNormalR, 0.01f);
            }

            // Reversed playback: ch0 should be silent, ch1 should have sine
            // Important: reverse the device BEFORE the graph is built
            {
                test_utilities::EnginePlayer rawPlayer (engine,
                    { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                      .inputNames = {}, .outputNames = {} });

                auto device = engine.getDeviceManager().getWaveOutputDevices()[0];
                device->reverseChannels (true);

                auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);

                auto& track = *getAudioTracks (*edit)[0];
                insertWaveClip (track, {}, stereoFile->getFile(),
                                ClipPosition {{ 0_tp, TimePosition::fromSeconds (1.0) }}, DeleteExistingClips::no);

                edit->dispatchPendingUpdatesSynchronously();
                edit->getTransport().ensureContextAllocated();
                test_utilities::waitForFileToBeMapped (stereoAf);
                edit->getTransport().play (false);

                auto buffer = test_utilities::process (rawPlayer, TimeDuration::fromSeconds (1.0));
                auto bufferView = toBufferView (buffer);

                auto rmsReversedL = getRMS (bufferView.getChannel (0));
                auto rmsReversedR = getRMS (bufferView.getChannel (1));

                // After reversal: L channel (sine) should now be on device channel 1,
                // R channel (silence) should now be on device channel 0
                CHECK_LT (rmsReversedL, 0.01f);
                CHECK_GT (rmsReversedR, 0.3f);

                device->reverseChannels (false);
            }
        }

        TEST_CASE ("WaveOutputDevice: reverseChannels on mono device is a no-op")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto& dm = engine.getDeviceManager();
            auto outputDevices = dm.getWaveOutputDevices();
            REQUIRE (outputDevices.size() > 0);

            auto device = outputDevices[0];
            REQUIRE (device->getChannels().getNumChannels() == 1);

            auto originalIndex = device->getChannels()[0].indexInDevice;
            device->reverseChannels (true);
            CHECK (device->getChannels()[0].indexInDevice == originalIndex);
            CHECK (! device->isReversed()); // mono device should reject reversal
        }

        TEST_CASE ("WaveOutputDevice: waveOutputs iteration is safe during device list rebuild")
        {
            // Verifies that waveOutputs and activeOutputChannels are protected by
            // contextLock during the audio callback. These arrays are iterated on the
            // audio thread while handleAsyncUpdate can swap and destroy them on the
            // message thread. Without the lock held, this is a use-after-free.
            auto& engine = *Engine::getEngines()[0];
            auto& dm = engine.getDeviceManager();
            auto& audioIO = dm.getHostedAudioDeviceInterface();

            HostedAudioDeviceInterface::Parameters params { .sampleRate = 44100.0, .blockSize = 512,
                                                            .inputChannels = 0, .outputChannels = 2,
                                                            .inputNames = {}, .outputNames = {} };

            audioIO.initialise (params);
            audioIO.prepareToPlay (params.sampleRate, params.blockSize);
            dm.dispatchPendingUpdates();

            // Capture pointers to the current WaveOutputDevice objects
            auto devicesBefore = dm.getWaveOutputDevices();
            REQUIRE (! devicesBefore.empty());
            auto firstDeviceBefore = devicesBefore[0];

            // Trigger a device list rebuild — this creates new objects and destroys old ones
            dm.setAllWaveOutputsToNumChannels (4);
            dm.dispatchPendingUpdates();

            auto devicesAfter = dm.getWaveOutputDevices();
            REQUIRE (! devicesAfter.empty());

            // The old device pointers are now dangling — new objects were created
            CHECK (firstDeviceBefore != devicesAfter[0]);

            // Now test the concurrent scenario: audio processing + device swaps
            std::atomic<bool> shouldStop { false };
            std::atomic<bool> hasStarted { false };
            std::atomic<int> blocksProcessed { 0 };

            juce::AudioBuffer<float> silenceBuffer (params.outputChannels, params.blockSize);
            silenceBuffer.clear();
            juce::MidiBuffer emptyMidi;

            std::thread audioThread ([&]
            {
                hasStarted = true;

                while (! shouldStop.load (std::memory_order_relaxed))
                {
                    audioIO.processBlock (silenceBuffer, emptyMidi);
                    blocksProcessed.fetch_add (1, std::memory_order_relaxed);
                    std::this_thread::yield();
                }
            });

            while (! hasStarted)
                std::this_thread::yield();

            // Repeatedly trigger device list rebuilds while audio is processing.
            // Under TSan/ASan, this will detect the race if waveOutputs iteration
            // is not protected by the contextLock.
            for (int i = 0; i < 5000; ++i)
            {
                dm.setAllWaveOutputsToNumChannels ((i % 2 == 0) ? 4u : 2u);
                dm.dispatchPendingUpdates();
            }

            shouldStop = true;
            audioThread.join();

            CHECK (blocksProcessed.load() > 0);

            // Restore original state
            dm.setAllWaveOutputsToNumChannels (2);
            dm.dispatchPendingUpdates();

            dm.deviceManager.closeAudioDevice();
            dm.removeHostedAudioDeviceInterface();
        }
    }
#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
