/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS
#include <ranges>
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>

namespace tracktion::inline engine {

#if ENGINE_UNIT_TESTS_PLAYBACK
    TEST_SUITE ("tracktion_engine")
    {
        TEST_CASE ("Playback")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            auto squareFile = graph::test_utilities::getSquareFile<juce::WavAudioFormat> (44100.0, 5.0);
            auto squareBuffer = *engine::test_utilities::loadFileInToBuffer (engine, squareFile->getFile());

            AudioFile af (engine, squareFile->getFile());
            auto clip = insertWaveClip (*getAudioTracks (*edit)[0], {}, af.getFile(), { { 0_tp, 5_tp } }, DeleteExistingClips::no);
            clip->setUsesProxy (false);
            tc.play (false);

            test_utilities::waitForFileToBeMapped (af);

            player.process (static_cast<int> (af.getLengthInSamples()));
            auto output = player.getOutput();

            CHECK_EQ (output.getNumFrames(), af.getLengthInSamples());

            CHECK (graph::test_utilities::buffersAreEqual (output, toBufferView (squareBuffer), 0.01f));
        }

        TEST_CASE ("Transport position set whilst stopped isn't quantised to samples")
        {
            auto& engine = *Engine::getEngines()[0];
            test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 1,
                                                           .inputNames = {}, .outputNames = {} });

            auto edit = engine::test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();
            edit->tempoSequence.getTempo (0)->setBpm (74.0);
            tc.ensureContextAllocated();

            // Bar 9 at 74bpm in 4/4 is beat 32 = 25.945945... seconds, which doesn't
            // fall on a whole sample at 44.1kHz
            const auto barTime = edit->tempoSequence.toTime (tempo::BarsAndBeats { .bars = 8 });
            tc.setPosition (barTime);

            // Process audio blocks so the playhead applies the posted position, and pump
            // the message loop past the transport's 200ms drag-guard so its timer syncs
            // the position back from the sample-quantised playhead
            for (int i = 0; i < 25; ++i)
            {
                player.process (512);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            }

            CHECK_EQ (tc.getPosition().inSeconds(), barTime.inSeconds());

            // The position should still read as the start of bar 9 (9|1|000, not 8|4|959)
            // when displayed the way the timecode readout does
            const auto nudge = TimeDuration::fromSeconds (0.05 / 96000.0);
            const auto barsBeats = edit->tempoSequence.toBarsAndBeats (tc.getPosition() + nudge);
            CHECK_EQ (barsBeats.bars, 8);
            CHECK_EQ (barsBeats.getWholeBeats(), 0);
            CHECK_EQ ((int) (barsBeats.getFractionalBeats().inBeats() * Edit::ticksPerQuarterNote), 0);
        }

        TEST_CASE ("Playback context can't be allocated whilst rendering")
        {
            auto& engine = *Engine::getEngines()[0];
            auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
            auto& tc = edit->getTransport();

            tc.ensureContextAllocated();
            REQUIRE (tc.getCurrentPlaybackContext() != nullptr);

            // Re-allocates in response to playbackContextChanged, the way an app-level
            // listener might, so freeing the context for a render mustn't resurrect it
            struct ContextReallocator : TransportControl::Listener
            {
                explicit ContextReallocator (TransportControl& t) : transport (t) {}

                void playbackContextChanged() override      { transport.ensureContextAllocated(); }

                TransportControl& transport;
                const ScopedListener transportListener { transport, *this };
            };

            ContextReallocator reallocator { tc };

            {
                const Edit::ScopedRenderStatus srs (*edit, true);
                CHECK (edit->isRendering());
                CHECK (tc.getCurrentPlaybackContext() == nullptr);

                tc.ensureContextAllocated();
                CHECK (tc.getCurrentPlaybackContext() == nullptr);
            }

            // The context was active before the render, so it should be restored afterwards
            CHECK (! edit->isRendering());
            CHECK (tc.getCurrentPlaybackContext() != nullptr);
        }
    }
#endif

#if ENGINE_UNIT_TESTS_RECORDING

//==============================================================================
template<class Clock, class Duration>
static void yield_until (const std::chrono::time_point<Clock, Duration>& sleep_time)
{
    while (Clock::now() < sleep_time)
        std::this_thread::yield();
}

static void waitUntilPlayheadPosition (const EditPlaybackContext& epc, TimePosition time)
{
    using namespace std::chrono_literals;

    while (epc.getUnloopedPosition() < time)
        std::this_thread::sleep_for (1ms);
}

struct ProcessThread
{
    ProcessThread (HostedAudioDeviceInterface& deviceInterface, const HostedAudioDeviceInterface::Parameters& params)
        : audioIO (deviceInterface)
    {
        silenceBuffer.setSize (params.inputChannels, params.blockSize);
        silenceBuffer.clear();
        auto msPerBlock = juce::roundToInt ((params.blockSize / params.sampleRate) * 1000.0);

        processThread = std::thread ([&, msPerBlock]
                                     {
                                         hasStarted = true;

                                         while (! shouldStop.load())
                                         {
                                             auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds (msPerBlock);

                                             if (insertImpulse.exchange (false))
                                                 for (int c = silenceBuffer.getNumChannels(); --c >= 0;)
                                                     silenceBuffer.setSample (c, 0, 1.0f);

                                             audioIO.processBlock (silenceBuffer, emptyMidiBuffer);
                                             silenceBuffer.clear();
                                             emptyMidiBuffer.clear();

                                             yield_until (endTime);
                                         }
                                     });
    }

    ~ProcessThread()
    {
        shouldStop.store (true);
        processThread.join();
    }

    void waitForThreadToStart()
    {
        while (! hasStarted)
            std::this_thread::yield();
    }

    void insertImpulseIntoNextBlock()
    {
        insertImpulse.store (true);
    }

    bool needsToInsertImpulse() const
    {
        return insertImpulse;
    }

private:
    HostedAudioDeviceInterface& audioIO;
    HostedAudioDeviceInterface::Parameters parameters;

    juce::AudioBuffer<float> silenceBuffer;
    juce::MidiBuffer emptyMidiBuffer;

    std::thread processThread;
    std::atomic<bool> hasStarted { false }, shouldStop { false }, insertImpulse { false };
};

template<typename ClipType>
static juce::Array<ClipType*> getAllClipsFromTracks (Edit& edit)
{
    juce::Array<ClipType*> clips;

    for (auto audioTrack : getAudioTracks (edit))
        for (auto clip : audioTrack->getClips())
            if (auto waveClip = dynamic_cast<ClipType*> (clip))
                clips.add (waveClip);

    return clips;
}

template<typename ClipType>
static juce::Array<juce::File> getSourceFilesFromClips (const juce::Array<ClipType*>& clips)
{
    juce::Array<juce::File> files;

    for (auto clip : clips)
        files.add (clip->getCurrentSourceFile());

    return files;
}

static int64_t findImpulseSampleIndex (Engine& engine, juce::File& file)
{
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (tracktion::engine::AudioFileUtils::createReaderFor (engine, file)))
        return reader->searchForLevel (0, reader->lengthInSamples,
                                       0.9f, 1.1f,
                                       0);

    return -1;
}

static std::vector<int64_t> getSampleIndiciesOfImpulse (Engine& engine, const juce::Array<juce::File>& files)
{
    std::vector<int64_t> sampleIndicies;

    for (auto file : files)
        sampleIndicies.push_back (findImpulseSampleIndex (engine, file));

    return sampleIndicies;
}

static double getFileLength (Engine& engine, const juce::File& file)
{
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (tracktion::engine::AudioFileUtils::createReaderFor (engine, file)))
        if (reader->sampleRate > 0.0)
            return static_cast<double> (reader->lengthInSamples) / reader->sampleRate;

    return 0.0;
}

static std::unique_ptr<Edit> createEditWithTracksForInputs (Engine& engine, const HostedAudioDeviceInterface::Parameters& params)
{
    auto edit = Edit::createSingleTrackEdit (engine);
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();
    auto context = transport.getCurrentPlaybackContext();

    edit->ensureNumberOfAudioTracks (params.inputChannels);
    auto audioTracks = getAudioTracks (*edit);
    auto inputInstances = context->getAllInputs();
    inputInstances.removeIf ([] (auto instance) { return instance->owner.isMidi(); });

    CHECK_EQ (inputInstances.size(), params.inputChannels);
    CHECK_EQ (inputInstances.size(), audioTracks.size());

    for (int i = 0; i < params.inputChannels; ++i)
    {
        auto track = audioTracks.getUnchecked (i);
        auto inputInstance = inputInstances.getUnchecked (i);
        [[ maybe_unused ]] auto res = inputInstance->setTarget (track->itemID, true, nullptr, 0);
        inputInstance->setRecordingEnabled (track->itemID, true);
    }

    return edit;
}

static void cleanUpRecordingSync()
{
    auto& deviceManager = Engine::getEngines()[0]->getDeviceManager();
    deviceManager.closeDevices();
    deviceManager.removeHostedAudioDeviceInterface();
    deviceManager.deviceManager.closeAudioDevice();
}

static void runSynchronisationTest (const HostedAudioDeviceInterface::Parameters& params)
{
    using namespace std::chrono_literals;

    Engine& engine = *Engine::getEngines()[0];
    auto& deviceManager = engine.getDeviceManager();
    auto& audioIO = deviceManager.getHostedAudioDeviceInterface();

    audioIO.initialise (params);
    audioIO.prepareToPlay (params.sampleRate, params.blockSize);
    deviceManager.dispatchPendingUpdates();
    std::ranges::for_each (deviceManager.getWaveInputDevices(),
                           [] (auto wi) { wi->setEnabled (true); });

    // Test device setup
    {
        CHECK_EQ (deviceManager.getNumWaveInDevices(), params.inputChannels);
        auto outDevCondition = (deviceManager.getNumWaveOutDevices() == 1 && deviceManager.getWaveOutDevice (0)->getChannels().getNumChannels() == 2)
                || (deviceManager.getNumWaveOutDevices() == 2 && deviceManager.getWaveOutDevice (0)->getChannels().getNumChannels() != 2);
        CHECK (outDevCondition);
    }

    test_utilities::TempCurrentWorkingDirectory tempDir;
    auto edit = createEditWithTracksForInputs (engine, params);
    auto& transport = edit->getTransport();

    // Test injected impulses align
    {
        using namespace std::chrono_literals;
        // Start recording, add an impulse after 1s then wait another 1s and stop recording
        ProcessThread processThread (audioIO, params);

        transport.stop (false, false);

        transport.record (false, false);
        auto& epc = *transport.getCurrentPlaybackContext();
        processThread.waitForThreadToStart();
        waitUntilPlayheadPosition (epc, 1.0s);

        processThread.insertImpulseIntoNextBlock();
        waitUntilPlayheadPosition (epc, 2.0s);
        CHECK_MESSAGE (! processThread.needsToInsertImpulse(), "Impulse not inserted");

        transport.stop (false, true);
    }

    {
        // Get recorded audio files and check the impulse is in the same place
        auto clips = getAllClipsFromTracks<WaveAudioClip> (*edit);
        CHECK_EQ (clips.size(), getAudioTracks (*edit).size());
        auto audioFiles = getSourceFilesFromClips (clips);
        auto sampleIndicies = getSampleIndiciesOfImpulse (engine, audioFiles);

        auto tempDirCondition = tempDir.tempDir.exists() && tempDir.tempDir.isDirectory();
        CHECK_MESSAGE (tempDirCondition, "Output dir not created");
        CHECK_MESSAGE (tempDir.tempDir.hasWriteAccess(), "Output dir is read only");
        CHECK_EQ (tempDir.tempDir.getFullPathName(), juce::File::getCurrentWorkingDirectory().getFullPathName());
        CHECK_EQ (audioFiles.size(), deviceManager.getNumWaveInDevices());

        for (const auto& f : audioFiles)
        {
            CHECK_MESSAGE (f.existsAsFile(), juce::String ("File doesn't exist FILE").replace ("FILE", f.getFullPathName()).toStdString());
            CHECK_GE (getFileLength (engine, f), 2.0);
        }

        CHECK_EQ ((int) std::count_if (sampleIndicies.begin(), sampleIndicies.end(),
                                           [] (auto index) { return index != -1; }),
                      audioFiles.size());

        for ([[maybe_unused]] int fileIndex = 0; auto index : sampleIndicies)
        {
            CHECK_GE (index, (int64_t) 0);

            if (index != sampleIndicies[0])
                CHECK_MESSAGE (false, juce::String ("Mismatch of impulse indicies (FIRST & SECOND samples, difference of DIFF)")
                                .replace ("FIRST", juce::String (index))
                                .replace ("SECOND", juce::String (sampleIndicies[0]))
                                .replace ("DIFF", juce::String (index - sampleIndicies[0])).toStdString());

            ++fileIndex;
        }
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("RecordingSyncTests")
    {
        HostedAudioDeviceInterface::Parameters params;
        params.sampleRate = 44100.0;
        params.blockSize = 256;
        params.inputChannels = 16;
        runSynchronisationTest (params);

        params.sampleRate = 48000.0;
        params.blockSize = 512;
        runSynchronisationTest (params);

        params.blockSize = 64;
        runSynchronisationTest (params);

        cleanUpRecordingSync();

        // Test reinitialisation and clean-up
        params.sampleRate = 44100.0;
        runSynchronisationTest (params);

        cleanUpRecordingSync();
    }
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Playback")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto& tc = edit->getTransport();

        struct EditAutoSaver : TransportControl::Listener
        {
            Edit& edit;
            bool autoSaveCalled = false;

            explicit EditAutoSaver (Edit& e)
                : edit (e)
            {}

            void autoSaveNow() override
            {
                autoSaveCalled = true;
            }

            const ScopedListener transportListener { edit.getTransport(), *this };
        };

        EditAutoSaver autoSaver { *edit };

        CHECK_FALSE (autoSaver.autoSaveCalled);

        tc.record (false, true);
        CHECK (tc.isPlaying());
        CHECK (tc.isRecording());

        tc.stop (false, false);
        CHECK (! tc.isPlaying());
        CHECK (! tc.isRecording());

        CHECK (autoSaver.autoSaveCalled);
    }
}
#endif // ENGINE_UNIT_TESTS_RECORDING

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
