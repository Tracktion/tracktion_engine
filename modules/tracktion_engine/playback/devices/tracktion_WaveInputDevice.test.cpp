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

        TEST_CASE ("WaveInputDevice: Device round-trip latency")
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
                FrameCount startFrame = 0;
                ChannelArrayBuffer<float> scratchBuffer (Size::create (1, blockSize));
                scratchBuffer.clear();

                for (;;)
                {
                    const auto numFramesLeft = numFramesToProcess - startFrame;

                    if (numFramesLeft == 0)
                        break;

                    const auto numFramesThisTime = std::min (scratchBuffer.getNumFrames(), numFramesLeft);

                    auto blockInput = scratchBuffer.getStart (numFramesThisTime);
                    auto bufferInput = toAudioBuffer (blockInput);
                    auto bufferOutput = player.process (bufferInput);
                    copyIntersectionAndClearOutside (scratchBuffer, toBufferView (bufferOutput));

                    startFrame += numFramesThisTime;
                }
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

        TEST_CASE ("WaveInputDevice: Track Device")
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

        TEST_CASE ("WaveInputDevice: Track Device with latency plugin")
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
    }
#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
