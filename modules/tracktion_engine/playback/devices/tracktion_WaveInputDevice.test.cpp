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
        TEST_CASE ("WaveInputDevice")
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
    }
#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
