/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             TestRunner
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This simply runs the unit tests within Tracktion Engine.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          TRACKTION_UNIT_TESTS=1

  type:             Component
  mainClass:        TestRunner

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include <thread>

template<class Clock, class Duration>
void yield_until (const std::chrono::time_point<Clock, Duration>& sleep_time)
{
    while (Clock::now() < sleep_time)
        std::this_thread::yield();
}

template <class Rep, class Period>
void yield_for (const std::chrono::duration<Rep, Period>& sleep_duration)
{
    yield_until (std::chrono::steady_clock::now() + sleep_duration);
}

int64_t findImpulseSampleIndex (File& file)
{
    if (auto reader = std::unique_ptr<AudioFormatReader> (tracktion_engine::AudioFileUtils::createReaderFor (file)))
        return reader->searchForLevel (0, reader->lengthInSamples,
                                       0.9f, 1.1f,
                                       0);

    return -1;
}

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

private:
    const File originalCwd = File::getCurrentWorkingDirectory();
    const File tempDir = File::createTempFile ({});
};

namespace tracktion_engine
{

#if TRACKTION_UNIT_TESTS

class RecordingSyncTests    : public juce::UnitTest
{
public:
    RecordingSyncTests()
        : juce::UnitTest ("RecordingSyncTests ", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        using namespace std::chrono_literals;

        Engine& engine = Engine::getInstance();
        auto& deviceManager = engine.getDeviceManager();
        auto& audioIO = deviceManager.getHostedAudioDeviceInterface();

        HostedAudioDeviceInterface::Parameters params;
        params.sampleRate = 44100.0;
        params.blockSize = 256;
        params.inputChannels = 16;
        params.fixedBlockSize = true;
        audioIO.initialise (params);
        audioIO.prepareToPlay (params.sampleRate, params.blockSize);

        beginTest ("Test device setup");
        {
            expectEquals (deviceManager.getNumWaveInDevices(), params.inputChannels);
            expectEquals (deviceManager.getNumWaveOutDevices(), 1);
        }

        TempCurrentWorkingDirectory tempDir;

        auto edit = std::make_unique<Edit> (Edit::Options { engine, createEmptyEdit(), ProjectItemID::createNewID (0) });
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
            auto inputInstance = inputInstances.getUnchecked (i);
            inputInstance->setTargetTrack (audioTracks.getUnchecked (i), 0);
            inputInstance->setRecordingEnabled (true);
        }

        AudioBuffer<float> silenceBuffer (params.inputChannels, params.blockSize);
        silenceBuffer.clear();
        MidiBuffer emptyMidiBuffer;
        const int msPerBlock = roundToInt ((params.blockSize / params.sampleRate) * 1000.0);
        std::atomic<bool> shouldStop { false }, insertImpulse { false };

        std::thread processThread ([&]
                                   {
                                       while (! shouldStop.load())
                                       {
                                           auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds (msPerBlock);

                                           if (insertImpulse.exchange (false))
                                               for (int c = silenceBuffer.getNumChannels(); --c >= 0;)
                                                   silenceBuffer.setSample (c, 0, 1.0f);

                                           audioIO.processBlock (silenceBuffer, emptyMidiBuffer);
                                           silenceBuffer.clear();

                                           yield_until (endTime);
                                       }
                                   });

        // Start recording, add an impulse after 1s then wait another 1s and stop recording
        transport.stop (false, false);
        transport.record (false, false);
        yield_for (1s);
        insertImpulse.store (true);
        yield_for (1s);
        transport.stop (false, true);

        shouldStop.store (true);
        processThread.join();

        // Get recorded audio files and check the impulse is in the same place
        Array<WaveAudioClip*> clips;

        for (auto audioTrack : audioTracks)
            for (auto clip : audioTrack->getClips())
                if (auto waveClip = dynamic_cast<WaveAudioClip*> (clip))
                    clips.add (waveClip);

        expectEquals (clips.size(), audioTracks.size());

        Array<File> audioFiles;

        for (auto clip : clips)
            audioFiles.add (clip->getCurrentSourceFile());

        // Find indicies of recorded samples
        std::vector<int64_t> sampleIndicies;

        for (auto audioFile : audioFiles)
            sampleIndicies.push_back (findImpulseSampleIndex (audioFile));

        expectEquals ((int) std::count_if (sampleIndicies.begin(), sampleIndicies.end(), [] (auto index) { return index != -1; }),
                      audioFiles.size(), "Some files don't have an impulse in them");

        for (auto index : sampleIndicies)
            if (index != sampleIndicies[0])
                expect (false, String ("Mismatch of impulse indicies (FIRST & SECOND samples, difference of DIFF)")
                                   .replace ("FIRST", String (index))
                                   .replace ("SECOND", String (sampleIndicies[0]))
                                   .replace ("DIFF", String (index - sampleIndicies[0])));
    }
};

static RecordingSyncTests recordingSyncTests;

#endif // TRACKTION_UNIT_TESTS

}


//==============================================================================
class TestRunner  : public Component
{
public:
    //==============================================================================
    TestRunner()
    {
        setSize (600, 400);

        UnitTestRunner testRunner;
        testRunner.setAssertOnFailure (false);
        testRunner.runTestsInCategory ("Tracktion");

        int numFailues = 0;

        for (int i = 0; i <= testRunner.getNumResults(); ++i)
            if (auto result = testRunner.getResult (i))
                numFailues += result->failures;

        if (numFailues > 0)
            JUCEApplication::getInstance()->setApplicationReturnValue (1);

        JUCEApplication::getInstance()->quit();        
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

private:
    //==============================================================================
    tracktion_engine::Engine engine { ProjectInfo::projectName };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestRunner)
};
