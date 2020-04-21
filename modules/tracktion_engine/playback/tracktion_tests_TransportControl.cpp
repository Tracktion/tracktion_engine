/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

#if TRACKTION_UNIT_TESTS

class RecordingSyncTests    : public juce::UnitTest
{
public:
    RecordingSyncTests()
        : juce::UnitTest ("RecordingSyncTests", "Tracktion:Longer") {}

    //==============================================================================
    void runTest() override
    {
        HostedAudioDeviceInterface::Parameters params;
        params.sampleRate = 44100.0;
        params.blockSize = 256;
        params.inputChannels = 16;
        params.fixedBlockSize = true;
        runSynchronisationTest (params);

        params.sampleRate = 48000.0;
        params.blockSize = 512;
        runSynchronisationTest (params);

        params.blockSize = 64;
        runSynchronisationTest (params);

        cleanUp();

        // Test reinitialisation and clean-up
        params.sampleRate = 44100.0;
        runSynchronisationTest (params);

        cleanUp();
    }

    void runSynchronisationTest (const HostedAudioDeviceInterface::Parameters& params)
    {
        using namespace std::chrono_literals;

        Engine& engine = *Engine::getEngines()[0];
        auto& deviceManager = engine.getDeviceManager();
        auto& audioIO = deviceManager.getHostedAudioDeviceInterface();

        audioIO.initialise (params);
        audioIO.prepareToPlay (params.sampleRate, params.blockSize);

        beginTest ("Test device setup");
        {
            expectEquals (deviceManager.getNumWaveInDevices(), params.inputChannels);
            expect ((deviceManager.getNumWaveOutDevices() == 1 && deviceManager.getWaveOutDevice (0)->isStereoPair())
                    || (deviceManager.getNumWaveOutDevices() == 2 && ! deviceManager.getWaveOutDevice (0)->isStereoPair()));
        }

        TempCurrentWorkingDirectory tempDir;
        auto edit = createEditWithTracksForInputs (engine, params);
        auto& transport = edit->getTransport();

        beginTest ("Test injected impulses align");
        {
            // Start recording, add an impulse after 1s then wait another 1s and stop recording
            ProcessThread processThread (audioIO, params);

            transport.stop (false, false);

            transport.record (false, false);
            auto& playhead = *transport.getCurrentPlayhead();
            processThread.waitForThreadToStart();
            waitUntilPlayheadPosition (playhead, 1.0);

            processThread.insertImpulseIntoNextBlock();
            waitUntilPlayheadPosition (playhead, 2.0);
            expect (! processThread.needsToInsertImpulse(), "Impulse not inserted");

            transport.stop (false, true);
        }

        {
            // Get recorded audio files and check the impulse is in the same place
            auto clips = getAllClipsFromTracks<WaveAudioClip> (*edit);
            expectEquals (clips.size(), getAudioTracks (*edit).size());
            auto audioFiles = getSourceFilesFromClips (clips);
            auto sampleIndicies = getSampleIndiciesOfImpulse (engine, audioFiles);

            expect (tempDir.tempDir.isDirectory(), "Output dir not created");
            expectEquals (tempDir.tempDir.getFullPathName(), File::getCurrentWorkingDirectory().getFullPathName(), "Current directory has changed");
            expectEquals (audioFiles.size(), deviceManager.getNumWaveInDevices(), "Audio files not created");

            for (const auto& f : audioFiles)
                expectGreaterOrEqual (getFileLength (engine, f), 2.0, "File length less then 2 seconds");

            expectEquals ((int) std::count_if (sampleIndicies.begin(), sampleIndicies.end(),
                                               [] (auto index) { return index != - 1; }),
                          audioFiles.size(), "Some files don't have an impulse in them");

            for (auto index : sampleIndicies)
                if (index != sampleIndicies[0])
                    expect (false, String ("Mismatch of impulse indicies (FIRST & SECOND samples, difference of DIFF)")
                            .replace ("FIRST", String (index))
                            .replace ("SECOND", String (sampleIndicies[0]))
                            .replace ("DIFF", String (index - sampleIndicies[0])));
        }
    }

    void cleanUp()
    {
        auto& deviceManager = Engine::getEngines()[0]->getDeviceManager();
        deviceManager.closeDevices();
        deviceManager.removeHostedAudioDeviceInterface();
        deviceManager.deviceManager.closeAudioDevice();
    }

    //==============================================================================
    template<class Clock, class Duration>
    static void yield_until (const std::chrono::time_point<Clock, Duration>& sleep_time)
    {
        while (Clock::now() < sleep_time)
            std::this_thread::yield();
    }
    
    void waitUntilPlayheadPosition (const PlayHead& playhead, double time)
    {
        using namespace std::chrono_literals;
        
        while (playhead.getUnloopedPosition() < time)
            std::this_thread::sleep_for (1ms);
    }

    //==============================================================================
    std::unique_ptr<Edit> createEditWithTracksForInputs (Engine& engine, const HostedAudioDeviceInterface::Parameters& params)
    {
        auto edit = std::make_unique<Edit> (Edit::Options { engine, createEmptyEdit (engine), ProjectItemID::createNewID (0) });
        auto& transport = edit->getTransport();
        transport.ensureContextAllocated();
        auto context = transport.getCurrentPlaybackContext();

        edit->ensureNumberOfAudioTracks (params.inputChannels);
        auto audioTracks = getAudioTracks (*edit);
        auto inputInstances = context->getAllInputs();
        inputInstances.removeIf ([] (auto instance) { return instance->owner.isMidi(); });

        expectEquals (inputInstances.size(), params.inputChannels);
        expectEquals (inputInstances.size(), audioTracks.size());

        for (int i = 0; i < params.inputChannels; ++i)
        {
            auto track = audioTracks.getUnchecked (i);
            auto inputInstance = inputInstances.getUnchecked (i);
            inputInstance->setTargetTrack (*track, 0, true);
            inputInstance->setRecordingEnabled (*track, true);
        }

        return edit;
    }

    //==============================================================================
    struct ProcessThread
    {
        ProcessThread (HostedAudioDeviceInterface& deviceInterface, const HostedAudioDeviceInterface::Parameters& params)
            : audioIO (deviceInterface)
        {
            silenceBuffer.setSize (params.inputChannels, params.blockSize);
            silenceBuffer.clear();
            const int msPerBlock = roundToInt ((params.blockSize / params.sampleRate) * 1000.0);

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

        AudioBuffer<float> silenceBuffer;
        MidiBuffer emptyMidiBuffer;

        std::thread processThread;
        std::atomic<bool> hasStarted { false }, shouldStop { false }, insertImpulse { false };
    };

    //==============================================================================
    struct TempCurrentWorkingDirectory
    {
        TempCurrentWorkingDirectory()
        {
            tempDir.createDirectory();
            tempDir.setAsCurrentWorkingDirectory();
        }

        ~TempCurrentWorkingDirectory()
        {
            tempDir.deleteRecursively (false);
            originalCwd.setAsCurrentWorkingDirectory();
        }

        const File originalCwd = File::getCurrentWorkingDirectory();
        const File tempDir = File::createTempFile ({});
    };

    //==============================================================================
    template<typename ClipType>
    Array<ClipType*> getAllClipsFromTracks (Edit& edit)
    {
        Array<ClipType*> clips;

        for (auto audioTrack : getAudioTracks (edit))
            for (auto clip : audioTrack->getClips())
                if (auto waveClip = dynamic_cast<ClipType*> (clip))
                    clips.add (waveClip);

        return clips;
    }

    template<typename ClipType>
    Array<File> getSourceFilesFromClips (const Array<ClipType*>& clips)
    {
        Array<File> files;

        for (auto clip : clips)
            files.add (clip->getCurrentSourceFile());

        return files;
    }

    int64_t findImpulseSampleIndex (Engine& engine, File& file)
    {
        if (auto reader = std::unique_ptr<AudioFormatReader> (tracktion_engine::AudioFileUtils::createReaderFor (engine, file)))
            return reader->searchForLevel (0, reader->lengthInSamples,
                                           0.9f, 1.1f,
                                           0);

        return -1;
    }

    std::vector<int64_t> getSampleIndiciesOfImpulse (Engine& engine, const Array<File>& files)
    {
        std::vector<int64_t> sampleIndicies;

        for (auto file : files)
            sampleIndicies.push_back (findImpulseSampleIndex (engine, file));

        return sampleIndicies;
    }
    
    double getFileLength (Engine& engine, const File& file)
    {
        if (auto reader = std::unique_ptr<AudioFormatReader> (tracktion_engine::AudioFileUtils::createReaderFor (engine, file)))
            if (reader->sampleRate > 0.0)
                return reader->lengthInSamples / reader->sampleRate;

        return 0.0;
    }
};

static RecordingSyncTests recordingSyncTests;

#endif // TRACKTION_UNIT_TESTS

}
