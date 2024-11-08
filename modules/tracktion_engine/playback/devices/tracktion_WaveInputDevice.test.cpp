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
#include <tracktion_engine/testing/tracktion_RoundTripLatency.h>

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_WAVE_INPUT_DEVICE
    TEST_SUITE ("tracktion_engine")
    {
        TEST_CASE ("WaveInputDevice: No device latency")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 1, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            tc.ensureContextAllocated();

            CHECK_EQ(engine.getDeviceManager().getWaveInputDevices()[0]->getRecordAdjustment(), 0_td);

            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 1);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());

            auto& destTrack = *getAudioTracks (*edit)[0];
            auto destAssignment = edit->getCurrentPlaybackContext()->getAllInputs()[0]->setTarget (destTrack.itemID, true, nullptr);
            (*destAssignment)->recordEnabled = true;
            edit->dispatchPendingUpdatesSynchronously();

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            player.process (squareBuffer);

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);

            CHECK_EQ (squareBuffer.getNumSamples(), recordedFileBuffer.getNumSamples());
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileBuffer, squareBuffer,
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Device input latency")
        {
            constexpr int inputLatencyNumSamples = 1000;
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512,
                                                           .inputChannels = 1, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {},
                                                           .inputLatencyNumSamples = inputLatencyNumSamples });

            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            tc.ensureContextAllocated();

            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 1);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());
            AudioFile af (engine, squareFile->getFile());

            auto& destTrack = *getAudioTracks (*edit)[0];
            auto destAssignment = edit->getCurrentPlaybackContext()->getAllInputs()[0]->setTarget (destTrack.itemID, true, nullptr);
            (*destAssignment)->recordEnabled = true;
            edit->dispatchPendingUpdatesSynchronously();
            CHECK_EQ(engine.getDeviceManager().getWaveInputDevices()[0]->getRecordAdjustment(), 0_td);

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            player.process (squareBuffer);

            // Flush the output as the first 1000 samples will be discarded by the recording
            // This wouldn't normally happen, you'd just end up with the last 1000 samples missing from the recording
            player.process (inputLatencyNumSamples);

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);

            CHECK_EQ (squareBuffer.getNumSamples(), recordedFileBuffer.getNumSamples());
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileBuffer, squareBuffer,
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Device input & output latency, loop-back")
        {
            using namespace choc::buffer;
            constexpr int inputLatencyNumSamples = 1000;
            constexpr int outputLatencyNumSamples = 250;
            constexpr int blockSize = 512;
            constexpr auto blockNumFrames = static_cast<FrameCount> (blockSize);
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = blockSize,
                                                           .inputChannels = 1, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {},
                                                           .inputLatencyNumSamples = inputLatencyNumSamples,
                                                           .outputLatencyNumSamples = outputLatencyNumSamples });

            auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            tc.ensureContextAllocated();

            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 1);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());
            AudioFile af (engine, squareFile->getFile());
            auto& sourceTrack = *getAudioTracks (*edit)[0];

            auto clip = insertWaveClip (sourceTrack, {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);

            auto& destTrack = *getAudioTracks (*edit)[1];
            auto destAssignment = edit->getCurrentPlaybackContext()->getAllInputs()[0]->setTarget (destTrack.itemID, true, nullptr);
            (*destAssignment)->recordEnabled = true;
            (*destAssignment)->input.owner.setMonitorMode (InputDevice::MonitorMode::off);
            edit->dispatchPendingUpdatesSynchronously();

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            {
                const auto numFramesToProcess = toBufferView (squareBuffer).getNumFrames()
                                                  + static_cast<FrameCount> (inputLatencyNumSamples + outputLatencyNumSamples);
                processLoopedBack (player, numFramesToProcess, { af });
            }

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);
            auto recordedFileView = toBufferView (recordedFileBuffer);

            CHECK_EQ (squareBuffer.getNumSamples(), recordedFileBuffer.getNumSamples());

            // These will be offset by a block which is unavoidable.
            // We just need to make sure we account for this when testing
            // for round-trip latency with the loop-back test
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileView.fromFrame (blockNumFrames),
                                                           toBufferView (squareBuffer).getStart (recordedFileView.getNumFrames()- blockNumFrames),
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Device input & output latency, loop-back corrected")
        {
            using namespace choc::buffer;
            constexpr int inputLatencyNumSamples = 1000;
            constexpr int outputLatencyNumSamples = 250;
            constexpr int blockSize = 512;
            auto& engine = *Engine::getEngines()[0];
            auto& dm = engine.getDeviceManager();
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = blockSize,
                                                           .inputChannels = 1, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {},
                                                           .inputLatencyNumSamples = inputLatencyNumSamples,
                                                           .outputLatencyNumSamples = outputLatencyNumSamples });

            // Detect latency to apply
            auto latencyTesterResult = [&]
            {
                std::optional<int> foundOffset;
                test_utilities::LatencyTester tester (dm.deviceManager,
                                                      [&foundOffset] (auto result)
                                                      { foundOffset = result; });

                // N.B. For testing purposes, we need to remove the DeviceManager from the callback as
                // the HostedAudioDevice has a single buffer that will be cleared by the device manager,
                // breaking the loop-back
                dm.closeDevices();
                tester.beginTest();

                {
                    const auto numFramesToProcess = toSamples (3_td, player.getParams().sampleRate);
                    processLoopedBack (player, numFramesToProcess);
                }

                CHECK (tester.tryToEndTest());
                CHECK (foundOffset);
                const auto res = test_utilities::getLatencyTesterResult (dm, *foundOffset);

                // Restart the device to add it back to the EnginePlayer's callbacks
                dm.initialise();
                dm.dispatchPendingUpdates();
                dm.getWaveInputDevices()[0]->setRecordAdjustment (res.adjustedTime);

                return res;
            }();

            // Setup the Edit to record
            auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            tc.ensureContextAllocated();

            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 1);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());
            AudioFile af (engine, squareFile->getFile());
            auto& sourceTrack = *getAudioTracks (*edit)[0];

            auto clip = insertWaveClip (sourceTrack, {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);

            auto& destTrack = *getAudioTracks (*edit)[1];
            auto destAssignment = edit->getCurrentPlaybackContext()->getAllInputs()[0]->setTarget (destTrack.itemID, true, nullptr);
            (*destAssignment)->recordEnabled = true;
            (*destAssignment)->input.owner.setMonitorMode (InputDevice::MonitorMode::off);
            edit->dispatchPendingUpdatesSynchronously();

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            {
                const auto numFramesToProcess = toBufferView (squareBuffer).getNumFrames()
                                                  + static_cast<FrameCount> (latencyTesterResult.deviceLatency + latencyTesterResult.adjustedNumSamples);
                processLoopedBack (player, numFramesToProcess, { af });
            }

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);
            auto recordedFileView = toBufferView (recordedFileBuffer);

            CHECK_EQ (squareBuffer.getNumSamples(), recordedFileBuffer.getNumSamples());
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileView,
                                                           toBufferView (squareBuffer),
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Track device")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 2);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());

            AudioFile af (engine, squareFile->getFile());
            auto& sourceTrack = *getAudioTracks (*edit)[0];

            auto clip = insertWaveClip (sourceTrack, {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);

            auto& destTrack = *getAudioTracks (*edit)[1];
            auto destAssignment = assignTrackAsInput (destTrack, sourceTrack, InputDevice::DeviceType::trackWaveDevice);
            destAssignment->recordEnabled = true;
            edit->dispatchPendingUpdatesSynchronously();

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            test_utilities::waitForFileToBeMapped (af);

            player.process (static_cast<int> (af.getLengthInSamples()));

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);

            CHECK_EQ (af.getLengthInSamples(), recordedFileBuffer.getNumSamples());
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileBuffer, squareBuffer,
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Track device with latency plugin")
        {
            constexpr float pluginLatencySeconds = 0.1f;
            auto& engine = *Engine::getEngines()[0];
            engine.getPluginManager().createBuiltInType<LatencyPlugin>();
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 2);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());

            AudioFile af (engine, squareFile->getFile());
            auto& sourceTrack = *getAudioTracks (*edit)[0];
            insertNewPlugin<LatencyPlugin> (sourceTrack, 0)->latencyTimeSeconds = pluginLatencySeconds;

            auto clip = insertWaveClip (sourceTrack, {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);

            auto& destTrack = *getAudioTracks (*edit)[1];
            auto destAssignment = assignTrackAsInput (destTrack, sourceTrack, InputDevice::DeviceType::trackWaveDevice);
            destAssignment->recordEnabled = true;
            edit->dispatchPendingUpdatesSynchronously();

            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            test_utilities::waitForFileToBeMapped (af);

            player.process (static_cast<int> (af.getLengthInSamples()));
            player.process (timeToSample (pluginLatencySeconds, 44100.0));

            tc.stop (false, true);

            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);

            CHECK_EQ (af.getLengthInSamples(), recordedFileBuffer.getNumSamples());
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileBuffer, squareBuffer,
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }

        TEST_CASE ("WaveInputDevice: Track device with insert plugin, loop-back")
        {
            using namespace choc::buffer;
            constexpr int inputLatencyNumSamples = 1000;
            constexpr int outputLatencyNumSamples = 250;
            constexpr int blockSize = 512;
            constexpr auto blockNumFrames = static_cast<FrameCount> (blockSize);
            auto& engine = *Engine::getEngines()[0];
            engine.getPluginManager().createBuiltInType<InsertPlugin>();
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = blockSize,
                                                           .inputChannels = 2, .outputChannels = 2,
                                                           .inputNames = {}, .outputNames = {},
                                                           .inputLatencyNumSamples = inputLatencyNumSamples,
                                                           .outputLatencyNumSamples = outputLatencyNumSamples });

            // Create an Edit with 2 tracks
            auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            tc.ensureContextAllocated();

            // Create a square file to be used as a clip on track 1
            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0, 1);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());
            AudioFile af (engine, squareFile->getFile());

            auto& sourceTrack = *getAudioTracks (*edit)[0];
            auto clip = insertWaveClip (sourceTrack, {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);

            // Add an InsertPlugin on track 1 and asign the second set of mono channels to it
            auto& dm = engine.getDeviceManager();
            dm.setDeviceInChannelStereo (0, false);
            dm.setDeviceOutChannelStereo (0, false);
            auto waveOutput = dm.getWaveOutputDevices();
            auto waveInputs = dm.getWaveInputDevices();
            auto insertPlugin = insertNewPlugin<InsertPlugin> (sourceTrack, 0);
            insertPlugin->outputDevice = waveOutput[1]->getName();
            insertPlugin->inputDevice = waveInputs[1]->getName();

            // Route the output of track 1 to track 2, disable monitoring and record from it
            auto& destTrack = *getAudioTracks (*edit)[1];
            auto destAssignment = assignTrackAsInput (destTrack, sourceTrack, InputDevice::DeviceType::trackWaveDevice);
            destAssignment->recordEnabled = true;
            destAssignment->input.owner.setMonitorMode (InputDevice::MonitorMode::off);
            edit->dispatchPendingUpdatesSynchronously();

            // Start recording and loop back the audio
            test_utilities::TempCurrentWorkingDirectory tempDir;
            tc.record (false);

            {
                const auto numFramesToProcess = toBufferView (squareBuffer).getNumFrames()
                                                  + static_cast<FrameCount> (inputLatencyNumSamples + outputLatencyNumSamples);
                processLoopedBack (player, numFramesToProcess, { af });
            }

            tc.stop (false, true);

            // Test the output is in sync
            auto recordedClip = dynamic_cast<WaveAudioClip*> (destTrack.getClips()[0]);
            CHECK(recordedClip);
            auto recordedFile = recordedClip->getSourceFileReference().getFile();
            auto recordedFileBuffer = *engine::test_utilities::loadFileInToBuffer (engine, recordedFile);
            auto recordedFileView = toBufferView (recordedFileBuffer).getChannelRange ({ .start = 0, .end = 1u });

            CHECK_EQ (squareBuffer.getNumSamples(), recordedFileBuffer.getNumSamples());

            // These will be offset by a block which is unavoidable.
            // We just need to make sure we account for this when testing
            // for round-trip latency with the loop-back test
            CHECK (graph::test_utilities::buffersAreEqual (recordedFileView.fromFrame (blockNumFrames),
                                                           toBufferView (squareBuffer).getStart (recordedFileView.getNumFrames()- blockNumFrames),
                                                           juce::Decibels::decibelsToGain (-99.0f)));
        }
    }
#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
