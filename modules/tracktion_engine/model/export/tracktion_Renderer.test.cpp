/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include "../../../3rd_party/doctest/tracktion_doctest.hpp"
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_RENDERING

TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("Renderer single audio track")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto fileLength = 5_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        auto track = getAudioTracks (*edit)[0];
        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");
        Renderer::Parameters params (*edit);
        params.destFile = destFile.getFile();
        params.time = params.time.withLength (fileLength);
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
        std::atomic<bool> callbackFinished { false };

        auto thumbnail = std::make_shared<juce::AudioThumbnail> (256,
                                                                 engine.getAudioFileFormatManager().readFormatManager,
                                                                 engine.getAudioFileManager().getAudioThumbnailCache());

        auto handle = EditRenderer::render (std::move (params),
                                            [&callbackFinished, f = destFile.getFile()] (auto res)
                                            {
                                                CHECK (res);
                                                CHECK (*res == f);
                                                callbackFinished = true;
                                            },
                                            thumbnail);

        test_utilities::runDispatchLoopUntilTrue (callbackFinished);

        CHECK (callbackFinished);
        CHECK_EQ (handle->getProgress(), 1.0f);
        auto buffer = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        CHECK_EQ (buffer->getNumSamples(), toSamples (fileLength, 44100.0));

        // N.B. The samples/length on the thumbnail are quatised to the low-res rate so aren't accurate
        CHECK (thumbnail->isFullyLoaded());
        CHECK (thumbnail->getNumSamplesFinished() >= toSamples (fileLength, 44100.0));
        CHECK (thumbnail->getTotalLength() >= fileLength.inSeconds());
    }

    TEST_CASE ("Renderer multichannel export")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& afm = engine.getAudioFileManager();

        // Create a 4-channel source with unique constant values per channel
        // so we can verify both channel count and ordering in the output.
        const choc::buffer::ChannelCount numSourceChannels = 4;
        const double sampleRate = 44100.0;
        const auto clipLength = 1_td;
        const auto numFrames = static_cast<choc::buffer::FrameCount> (toSamples (clipLength, sampleRate));

        choc::buffer::InterleavedBuffer<float> sourceBuffer (numSourceChannels, numFrames);

        for (choc::buffer::ChannelCount ch = 0; ch < numSourceChannels; ++ch)
            for (choc::buffer::FrameCount f = 0; f < numFrames; ++f)
                sourceBuffer.getSample (ch, f) = (float) (ch + 1) * 0.1f;  // ch0=0.1, ch1=0.2, ch2=0.3, ch3=0.4

        const juce::File virtualFile ("/memory/multichannel-test.wav");
        const auto key = virtualFile.getFullPathName().toStdString();
        afm.registerMemoryBuffer (key, sourceBuffer.getView(), sampleRate);

        // Helper: render an edit and return the result buffer
        auto renderEdit = [&] (Edit& edit, ChannelConfiguration channelConfig, bool mustMono = false)
            -> std::optional<juce::AudioBuffer<float>>
        {
            juce::TemporaryFile destFile (".wav");
            Renderer::Parameters params (edit);
            params.destFile = destFile.getFile();
            params.time = params.time.withLength (clipLength);
            params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
            params.canRenderInMono = false;
            params.mustRenderInMono = mustMono;

            if (! channelConfig.isEmpty())
                params.channelConfig = channelConfig;

            std::atomic<bool> callbackFinished { false };
            bool renderSucceeded = false;

            auto handle = EditRenderer::render (std::move (params),
                                                [&] (auto res)
                                                {
                                                    renderSucceeded = res.has_value();
                                                    callbackFinished = true;
                                                });

            test_utilities::runDispatchLoopUntilTrue (callbackFinished);

            if (! renderSucceeded)
                return {};

            return test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        };

        // Helper: create an edit with the multichannel clip, all channels active
        auto createEditWithClip = [&]
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];
            auto clip = insertWaveClip (*track, {}, virtualFile,
                                        { .time = { 0_tp, clipLength } },
                                        DeleteExistingClips::no);
            REQUIRE (clip != nullptr);
            clip->setActiveChannelConfiguration (ChannelConfiguration::discreteChannels ((int) numSourceChannels));
            return edit;
        };

        SUBCASE ("auto-detect from graph preserves channel count and content")
        {
            auto edit = createEditWithClip();
            auto buffer = renderEdit (*edit, {});
            REQUIRE (buffer.has_value());
            CHECK_EQ (buffer->getNumChannels(), (int) numSourceChannels);

            // Verify each channel has the expected content
            for (int ch = 0; ch < (int) numSourceChannels; ++ch)
            {
                float expected = (float) (ch + 1) * 0.1f;
                CHECK (buffer->getRMSLevel (ch, 0, buffer->getNumSamples()) == doctest::Approx (expected).epsilon (0.01));
            }
        }

        SUBCASE ("explicit channelConfig with reversed order")
        {
            auto edit = createEditWithClip();

            // Request output in reversed order: ch3, ch2, ch1, ch0
            ChannelConfiguration reversedConfig;
            reversedConfig.addChannel (ChannelIndex::createMono (3));
            reversedConfig.addChannel (ChannelIndex::createMono (2));
            reversedConfig.addChannel (ChannelIndex::createMono (1));
            reversedConfig.addChannel (ChannelIndex::createMono (0));

            auto buffer = renderEdit (*edit, reversedConfig);
            REQUIRE (buffer.has_value());
            CHECK_EQ (buffer->getNumChannels(), 4);

            // Output channel 0 should have graph channel 3's content (0.4)
            CHECK (buffer->getRMSLevel (0, 0, buffer->getNumSamples()) == doctest::Approx (0.4f).epsilon (0.01));
            // Output channel 1 should have graph channel 2's content (0.3)
            CHECK (buffer->getRMSLevel (1, 0, buffer->getNumSamples()) == doctest::Approx (0.3f).epsilon (0.01));
            // Output channel 2 should have graph channel 1's content (0.2)
            CHECK (buffer->getRMSLevel (2, 0, buffer->getNumSamples()) == doctest::Approx (0.2f).epsilon (0.01));
            // Output channel 3 should have graph channel 0's content (0.1)
            CHECK (buffer->getRMSLevel (3, 0, buffer->getNumSamples()) == doctest::Approx (0.1f).epsilon (0.01));
        }

        SUBCASE ("fewer output channels than source")
        {
            auto edit = createEditWithClip();
            auto buffer = renderEdit (*edit, ChannelConfiguration::discreteChannels (2));
            REQUIRE (buffer.has_value());
            CHECK_EQ (buffer->getNumChannels(), 2);

            // Should have first two channels' content
            CHECK (buffer->getRMSLevel (0, 0, buffer->getNumSamples()) == doctest::Approx (0.1f).epsilon (0.01));
            CHECK (buffer->getRMSLevel (1, 0, buffer->getNumSamples()) == doctest::Approx (0.2f).epsilon (0.01));
        }

        SUBCASE ("mustRenderInMono overrides multichannel")
        {
            auto edit = createEditWithClip();
            auto buffer = renderEdit (*edit, {}, true);
            REQUIRE (buffer.has_value());
            CHECK_EQ (buffer->getNumChannels(), 1);
        }

        afm.unregisterMemoryBuffer (key);
    }

    TEST_CASE ("Renderer 7.1 surround export")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& afm = engine.getAudioFileManager();

        const choc::buffer::ChannelCount numSourceChannels = 8;
        const double sampleRate = 44100.0;
        const auto clipLength = 1_td;
        const auto numFrames = static_cast<choc::buffer::FrameCount> (toSamples (clipLength, sampleRate));

        choc::buffer::InterleavedBuffer<float> sourceBuffer (numSourceChannels, numFrames);

        for (choc::buffer::ChannelCount ch = 0; ch < numSourceChannels; ++ch)
            for (choc::buffer::FrameCount f = 0; f < numFrames; ++f)
                sourceBuffer.getSample (ch, f) = (float) (ch + 1) * 0.05f;

        const juce::File virtualFile ("/memory/7.1-test.wav");
        const auto key = virtualFile.getFullPathName().toStdString();
        afm.registerMemoryBuffer (key, sourceBuffer.getView(), sampleRate);

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];
        auto clip = insertWaveClip (*track, {}, virtualFile,
                                    { .time = { 0_tp, clipLength } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);

        // Render with explicit 7.1 channel config
        juce::TemporaryFile destFile (".wav");
        Renderer::Parameters params (*edit);
        params.destFile = destFile.getFile();
        params.time = params.time.withLength (clipLength);
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
        params.channelConfig = ChannelConfiguration::surround7_1();
        params.canRenderInMono = false;

        std::atomic<bool> callbackFinished { false };
        bool renderSucceeded = false;

        auto handle = EditRenderer::render (std::move (params),
                                            [&] (auto res)
                                            {
                                                renderSucceeded = res.has_value();
                                                callbackFinished = true;
                                            });

        test_utilities::runDispatchLoopUntilTrue (callbackFinished);
        CHECK (renderSucceeded);

        auto buffer = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        REQUIRE (buffer.has_value());
        CHECK_EQ (buffer->getNumChannels(), 8);

        afm.unregisterMemoryBuffer (key);
    }

    TEST_CASE ("Renderer auto-detects channel count from edit")
    {
        // When channelConfig is empty ("From Edit"), the renderer should use
        // the graph's actual channel count, not default to stereo.
        auto& engine = *Engine::getEngines()[0];
        auto& afm = engine.getAudioFileManager();

        const choc::buffer::ChannelCount numSourceChannels = 3;
        const double sampleRate = 44100.0;
        const auto clipLength = 1_td;
        const auto numFrames = static_cast<choc::buffer::FrameCount> (toSamples (clipLength, sampleRate));

        choc::buffer::InterleavedBuffer<float> sourceBuffer (numSourceChannels, numFrames);

        for (choc::buffer::ChannelCount ch = 0; ch < numSourceChannels; ++ch)
            for (choc::buffer::FrameCount f = 0; f < numFrames; ++f)
                sourceBuffer.getSample (ch, f) = (float) (ch + 1) * 0.1f;

        const juce::File virtualFile ("/memory/3ch-auto-detect.wav");
        const auto key = virtualFile.getFullPathName().toStdString();
        afm.registerMemoryBuffer (key, sourceBuffer.getView(), sampleRate);

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];
        auto clip = insertWaveClip (*track, {}, virtualFile,
                                    { .time = { 0_tp, clipLength } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);
        clip->setActiveChannelConfiguration (ChannelConfiguration::discreteChannels ((int) numSourceChannels));

        // Use RenderOptions with empty channel config = "From Edit" auto-detect
        auto renderOptions = RenderOptions::forGeneralExporter (*edit);
        renderOptions->setChannelConfiguration ({});  // "From Edit"

        juce::TemporaryFile destFile (".wav");
        auto params = renderOptions->getRenderParameters (*edit);
        params.destFile = destFile.getFile();
        params.time = params.time.withLength (clipLength);
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();

        std::atomic<bool> callbackFinished { false };

        auto handle = EditRenderer::render (std::move (params),
                                            [&callbackFinished] (auto res)
                                            {
                                                CHECK (res);
                                                callbackFinished = true;
                                            });

        test_utilities::runDispatchLoopUntilTrue (callbackFinished);
        CHECK (callbackFinished);

        auto buffer = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        REQUIRE (buffer.has_value());
        CHECK_EQ (buffer->getNumChannels(), (int) numSourceChannels);

        // Verify per-channel content is preserved
        for (int ch = 0; ch < (int) numSourceChannels; ++ch)
        {
            float expected = (float) (ch + 1) * 0.1f;
            CHECK (buffer->getRMSLevel (ch, 0, buffer->getNumSamples()) == doctest::Approx (expected).epsilon (0.01));
        }

        afm.unregisterMemoryBuffer (key);
    }

    TEST_CASE ("Renderer memory buffer clip")
    {
        auto& engine = *Engine::getEngines()[0];
        auto& afm = engine.getAudioFileManager();

        const choc::buffer::ChannelCount numChannels = 2;
        const double sampleRate = 44100.0;
        const auto clipLength = 2_td;
        const auto numFrames = static_cast<choc::buffer::FrameCount> (toSamples (clipLength, sampleRate));

        // Fill a stereo buffer with a constant value so we can verify it survives rendering
        const float testValue = 0.5f;
        choc::buffer::InterleavedBuffer<float> buffer (numChannels, numFrames);

        for (choc::buffer::FrameCount f = 0; f < numFrames; ++f)
            for (choc::buffer::ChannelCount ch = 0; ch < numChannels; ++ch)
                buffer.getSample (ch, f) = testValue;

        // Register with a key that matches the juce::File path we'll give the clip
        const juce::File virtualFile ("/memory/test-buffer.wav");
        const auto key = virtualFile.getFullPathName().toStdString();
        afm.registerMemoryBuffer (key, buffer.getView(), sampleRate);

        // Create an edit with one audio track and insert a clip referencing the memory buffer
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];
        auto clip = insertWaveClip (*track, {}, virtualFile,
                                    { .time = { 0_tp, clipLength } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);

        // Render the edit to a file
        juce::TemporaryFile destFile (".wav");
        Renderer::Parameters params (*edit);
        params.destFile = destFile.getFile();
        params.time = params.time.withLength (clipLength);
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();

        std::atomic<bool> callbackFinished { false };
        bool renderSucceeded = false;

        auto handle = EditRenderer::render (std::move (params),
                                            [&] (auto res)
                                            {
                                                renderSucceeded = res.has_value();
                                                callbackFinished = true;
                                            });

        test_utilities::runDispatchLoopUntilTrue (callbackFinished);
        CHECK (renderSucceeded);

        // Read back the rendered file and verify samples match the source buffer
        auto rendered = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        REQUIRE (rendered.has_value());
        CHECK_EQ (rendered->getNumChannels(), static_cast<int> (numChannels));
        CHECK_EQ (rendered->getNumSamples(), static_cast<int> (numFrames));

        for (int ch = 0; ch < rendered->getNumChannels(); ++ch)
            CHECK (rendered->getRMSLevel (ch, 0, rendered->getNumSamples()) == doctest::Approx (testValue).epsilon (0.01));

        afm.unregisterMemoryBuffer (key);
    }
}

#endif


#if ENGINE_UNIT_TESTS_FREEZE

//==============================================================================
//==============================================================================
class TrackFreezeTests  : public juce::UnitTest
{
public:
    TrackFreezeTests()
        : juce::UnitTest ("Track Freeze", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];

        beginTest ("End allowance");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];

            // Create a track with a plugin with end allowance
            auto synth = dynamic_cast<FourOscPlugin*> (edit->getPluginCache().createNewPlugin (FourOscPlugin::xmlTypeName, {}).get());
            static auto organPatch = "<PLUGIN type=\"4osc\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetName=\"4OSC: Organ\" filterFreq=\"127.0\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.0\" ampSustain=\"100.0\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.0\" waveShape2=\"4\"> <MODMATRIX/> </PLUGIN>";

            if (auto e = juce::parseXML (organPatch))
                if (auto v = juce::ValueTree::fromXml (*e); v.isValid())
                    synth->restorePluginStateFromValueTree (v);

            const auto tailLength = synth->getTailLength();
            expectGreaterThan (tailLength, 0.0);

            track->pluginList.insertPlugin (*synth, 0, nullptr);

            // Insert a MIDI clip with a 1-beat note
            auto midiClip = track->insertMIDIClip ({ 0.0s, TimePosition (1.0s) }, nullptr);
            midiClip->getSequence().addNote (69, BeatPosition::fromBeats (0.0), BeatDuration::fromBeats (1.0), 127, 0, nullptr);
            const auto trackLength = track->getLengthIncludingInputTracks();
            expectWithinAbsoluteError (trackLength.inSeconds(), 1.0, 0.001);

            // Check expected end allowance
            juce::Array<EditItemID> trackIDs { track->itemID };
            juce::Array<Clip*> clips { midiClip.get() };
            const auto endAllowance = RenderOptions::findEndAllowance (*edit, &trackIDs, &clips);
            expectWithinAbsoluteError (endAllowance.inSeconds(), tailLength, 0.001);

            // Ensure freezing that track has the track length plus the end allowance
            const auto expectedTotalLength = trackLength + TimeDuration::fromSeconds (tailLength);

            {
                track->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }

            {
                track->setFrozen (false, AudioTrack::individualFreeze);

                // Create a new track and set the destination of the first one to this
                auto track2 = edit->insertNewAudioTrack ({ {},{} }, nullptr);
                track->getOutput().setOutputToTrack (track2.get());
                trackIDs = juce::Array<EditItemID> { track2->itemID };
                expectEquals (RenderOptions::findEndAllowance (*edit, &trackIDs, nullptr), TimeDuration(),
                              "End allowance of new empty track is not 0");

                // Now freeze this track and it should contain track's end allowance
                track2->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track2));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }
        }
    }
};

static TrackFreezeTests trackFreezeTests;

#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS