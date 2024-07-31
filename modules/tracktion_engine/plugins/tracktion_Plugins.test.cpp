/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include "../../3rd_party/doctest/tracktion_doctest.hpp"
#include "../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../testing/tracktion_EnginePlayer.h"

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_PDC

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("PitchShiftPlugin Latency")
    {
        HostedAudioDeviceInterface::Parameters p;

        auto& engine = *Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();
        auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);

        const auto duration = 3_td;
        const auto numFrames = toSamples (duration, p.sampleRate);

        const auto transientPos = 1_tp;
        const auto transientFile = graph::test_utilities::getTransientFile<juce::WavAudioFormat> (p.sampleRate, duration, transientPos, 0.5f);
        const auto af = AudioFile (engine, transientFile->getFile());

        auto track1 = getAudioTracks (*edit)[0];
        insertWaveClip (*track1, {}, transientFile->getFile(), { { 0_tp, duration } }, DeleteExistingClips::no);

        auto track2 = getAudioTracks (*edit)[1];
        insertWaveClip (*track2, {}, transientFile->getFile(), { { 0_tp, duration } }, DeleteExistingClips::no);

        auto testTransient = [&] (auto plugin, bool shouldCheck)
        {
            auto player = test_utilities::createEnginePlayer (*edit, p, { af });
            player->process (numFrames);
            auto output = player->getOutput();
            const auto expectedTransientSample = toSamples (transientPos + TimeDuration::fromSeconds (plugin->getLatencySeconds()), p.sampleRate);

            auto f = graph::test_utilities::writeToTemporaryFile<juce::WavAudioFormat> (output, p.sampleRate, 0);
            auto transient = graph::test_utilities::findFirstNonZeroSample (output.getChannel (0));

            if (shouldCheck)
            {
                CHECK (transient);

                if (transient)
                {
                    CHECK_LT (std::abs (transient->first - expectedTransientSample), 5); // 5 sample tolerance
                    CHECK_GT (transient->first, 0.5f);
                }
            }
            else
            {
                MESSAGE (std::to_string (transient.has_value()));

                if (transient)
                {
                    MESSAGE ("Transient diff frames: " << std::to_string (std::abs (transient->first - expectedTransientSample))); // 5 sample tolerance
                    MESSAGE (std::to_string (transient->first > 0.5f));
                }
            }
        };

        [[ maybe_unused ]] auto insertAndTest = [&] (auto mode, bool shouldCheck)
        {
            auto pitchShiftPlugin = insertNewPlugin<PitchShiftPlugin> (*track1);
            pitchShiftPlugin->mode = mode;
            testTransient (pitchShiftPlugin, shouldCheck);
        };

        SUBCASE ("LatencyPlugin")
        {
            auto latencyPlugin = insertNewPlugin<LatencyPlugin> (*track1);
            latencyPlugin->latencyTimeSeconds = 0.25f;
            testTransient (latencyPlugin, true);
        }

       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        SUBCASE ("soundtouchNormal")
        {
            insertAndTest (TimeStretcher::soundtouchNormal, false);
        }

        SUBCASE ("soundtouchBetter")
        {
           insertAndTest (TimeStretcher::soundtouchBetter, false);
        }
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        SUBCASE ("rubberbandPercussive")
        {
            insertAndTest (TimeStretcher::rubberbandPercussive, false);
        }

        SUBCASE ("rubberbandMelodic")
        {
            insertAndTest (TimeStretcher::rubberbandMelodic, false);
        }
       #endif
    }
}

//==============================================================================
//==============================================================================
class PDCTests  : public juce::UnitTest
{
public:
    PDCTests()
        : juce::UnitTest ("PDC", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto sinFile = getSinFile<juce::WavAudioFormat> (44100.0);

        auto& engine = *Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();

        beginTest ("No latency");
        {
            expectEquals (sinFile->getFile().getFileExtension(), juce::String (".wav"));
            auto edit = engine::test_utilities::createTestEdit (engine, 2);
            auto track1 = getAudioTracks (*edit)[0];
            auto track2 = getAudioTracks (*edit)[1];
            expect (track1 != track2);

            // Add sin file
            {
                AudioFile af (engine, sinFile->getFile());
                expect (af.isValid());
                expect (af.getLength() == 1.0);

                for (auto t : { track1, track2 })
                {
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ TimePosition(), TimePosition::fromSeconds (af.getLength()) }}, false);
                    expectEquals (clip->getPosition().getStart(), TimePosition());
                    expectEquals (clip->getPosition().getEnd(), TimePosition::fromSeconds (1.0));
                }
            }

            expectPeak (*edit, { 0.0s, TimePosition (1.0s) }, { track1, track2 }, 2.0f);

            // Set end to 0.5s for volume off
            track1->getClips()[0]->setPosition ({{ TimePosition(), TimePosition::fromSeconds (0.5) }});
            expectPeak (*edit, { 0.0s, TimePosition (0.5s) }, { track1, track2 }, 2.0f);
            expectPeak (*edit, { 0.5s, TimePosition (1.0s) }, { track1, track2 }, 1.0f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }

        beginTest ("Source file with different sample rate");
        {
            auto sinFile96Ogg = getSinFile<juce::OggVorbisAudioFormat> (96000.0);
            expectEquals (sinFile96Ogg->getFile().getFileExtension(), juce::String (".ogg"));
            auto edit = engine::test_utilities::createTestEdit (engine, 2);
            auto track1 = getAudioTracks (*edit)[0];

            // Add sin file
            {
                AudioFile af (engine, sinFile96Ogg->getFile());
                expect (af.isValid());
                expect (af.getLength() == 1.0);

                for (auto t : { track1 })
                {
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ TimePosition(), TimePosition::fromSeconds (af.getLength()) }}, false);
                    expectEquals (clip->getPosition().getStart(), TimePosition());
                    expectEquals (clip->getPosition().getEnd(), TimePosition::fromSeconds (1.0));
                }
            }

            expectPeak (*edit, { 0.0s, TimePosition (1.0s) }, { track1 }, 1.0f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    void expectPeak (Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("PDC Tests", edit, tr, getTracksMask (tracks), blockSize));
        expect (juce::isWithin (stats.peak, expectedPeak, 0.001f), juce::String ("Expected peak: ") + juce::String (expectedPeak, 4));
    }

    Renderer::Statistics logStats (Renderer::Statistics stats)
    {
        logMessage ("Stats: peak " + juce::String (stats.peak) + ", avg " + juce::String (stats.average)
                     + ", duration " + juce::String (stats.audioDuration));
        return stats;
    }

    static juce::BigInteger getTracksMask (const juce::Array<Track*>& tracks)
    {
        juce::BigInteger tracksMask;

        for (auto t : tracks)
            tracksMask.setBit (t->getIndexInEditTrackList());

        jassert (tracksMask.countNumberOfSetBits() == tracks.size());
        return tracksMask;
    }

    //==============================================================================
    template<typename AudioFormatType>
    std::unique_ptr<juce::TemporaryFile> getSinFile (double sampleRate)
    {
        // Create a 1s sin buffer
        juce::AudioBuffer<float> buffer (1, (int) sampleRate);
        juce::dsp::Oscillator<float> osc ([] (float in) { return std::sin (in); });
        osc.setFrequency (220.0);
        osc.prepare ({ double (sampleRate), uint32_t (sampleRate), 1 });

        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);

        // Then write it to a temp file
        AudioFormatType format;
        auto f = std::make_unique<juce::TemporaryFile> (format.getFileExtensions()[0]);

        if (auto fileStream = f->getFile().createOutputStream())
        {
            const int numQualityOptions = format.getQualityOptions().size();
            const int qualityOptionIndex = numQualityOptions == 0 ? 0 : (numQualityOptions / 2);
            const int bitDepth = format.getPossibleBitDepths().contains (16) ? 16 : 32;

            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (AudioFormatType().createWriterFor (fileStream.get(),
                                                                                                           sampleRate, 1, bitDepth,
                                                                                                           {}, qualityOptionIndex)))
            {
                fileStream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
        }

        return f;
    }
};

static PDCTests pdcTests;

#endif

#if ENGINE_UNIT_TESTS_MODIFIERS

//==============================================================================
//==============================================================================
class ModifiedParameterValuesTests  : public juce::UnitTest
{
public:
    ModifiedParameterValuesTests()
        : juce::UnitTest ("Modified Parameter Values", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];

        const juce::TemporaryFile tempFile (".tracktionedit");
        const auto editFile = tempFile.getFile();

        const float macroParameterValue = 0.3f;
        const float modifierValue = 0.5f;
        const float modifierOffset = 0.2f;

        // 1. create a new edit and add rack with a macro parameter
        {
            editFile.deleteFile();

            auto edit = createEmptyEdit (engine, editFile);
            auto rackType = edit->getRackList().addNewRack();
            auto volumePlugin = edit->getPluginCache().createNewPlugin (VolumeAndPanPlugin::xmlTypeName, {});

            // Add plugin to rack
            rackType->addPlugin (volumePlugin, {}, true);

            // Add macro parameter
            const auto macroParameter = rackType->getMacroParameterListForWriting().createMacroParameter();
            macroParameter->setNormalisedParameter (macroParameterValue, juce::NotificationType::sendNotification);

            auto volumeAndPan = dynamic_cast<VolumeAndPanPlugin*> (volumePlugin.get());
            auto volParam = volumeAndPan->volParam;
            volParam->setNormalisedParameter (0.0f, juce::NotificationType::sendNotification);
            volParam->addModifier (*macroParameter, modifierValue, modifierOffset, 0.5f);

            // Run the dispatch loop so the ValueTree properties attached to the parameter are updated. (Otherwise the assertions below will fail)
            // This happens implicitly in the real application.
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1000);

            expectWithinAbsoluteError (volParam->getCurrentValue(),
                                       0.35f,
                                       0.001f);

            expectWithinAbsoluteError (volParam->getCurrentBaseValue(),
                                       volParam->valueRange.convertFrom0to1 (0.0f),
                                       0.001f);
            expectWithinAbsoluteError (volParam->getCurrentValue(),
                                       volParam->valueRange.convertFrom0to1 (modifierOffset + modifierValue * macroParameterValue),
                                       0.001f);
            expectWithinAbsoluteError (volParam->getCurrentModifierValue(),
                                       volParam->valueRange.convertFrom0to1 (modifierOffset + modifierValue * macroParameterValue) - volParam->getCurrentBaseValue(),
                                       0.001f);

            // volume="0.35" is saved in the plugin state xml
            EditFileOperations (*edit).save (true, true, false);
        }

        // 2. Load previously saved edit and check the parameter value
        {
            auto edit = loadEditFromFile (engine, editFile);
            auto rackType = edit->getRackList().getRackType (0);
            auto volumeAndPan = dynamic_cast<VolumeAndPanPlugin*> (rackType->getPlugins().getFirst());
            auto volParam = volumeAndPan->volParam;

            expectWithinAbsoluteError (volParam->getCurrentBaseValue(),
                                       volParam->valueRange.convertFrom0to1 (0.0f),
                                       0.001f);
            expectWithinAbsoluteError (volParam->getCurrentValue(),
                                       volParam->valueRange.convertFrom0to1(modifierOffset + modifierValue * macroParameterValue),
                                       0.001f);
            expectWithinAbsoluteError (volParam->getCurrentModifierValue(),
                                       volParam->valueRange.convertFrom0to1 (modifierOffset + modifierValue * macroParameterValue) - volParam->getCurrentBaseValue(),
                                       0.001f);

            expectWithinAbsoluteError (volParam->getCurrentValue(), 0.35f, 0.001f);
        }
    }

private:
};

static ModifiedParameterValuesTests modifiedParameterValuesTests;

#endif


#if ENGINE_UNIT_TESTS_AUTOMATION
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Plugin automation")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = engine::test_utilities::createTestEdit (engine, 1);
        auto volParam = getAudioTracks(*edit)[0]->getVolumePlugin()->volParam;
        auto& volCurve = volParam->getCurve();

        // Add point with curve=1 to give a square shape
        volCurve.addPoint (0_tp, 1.0f, 1.0f);
        CHECK_EQ (volCurve.getValueAt (0_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (5_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (10_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (15_tp), 1.0f);

        volCurve.addPoint (10_tp, 0.0f, 1.0f);
        CHECK_EQ (volCurve.getValueAt (0_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (5_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (9.9_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (10_tp), 1.0f);
        CHECK_EQ (volCurve.getValueAt (10.1_tp), 0.0f);
        CHECK_EQ (volCurve.getValueAt (15_tp), 0.0f);

        // Now check the same with an automation iterator
        {
            AutomationIterator iter (*volParam);
            auto setAndGet = [&iter] (auto t) { iter.setPosition (t); return iter.getCurrentValue(); };
            CHECK_EQ (setAndGet (0_tp), 1.0f);
            CHECK_EQ (setAndGet (5_tp), 1.0f);
            CHECK_EQ (setAndGet (9.9_tp), 1.0f);
            CHECK_EQ (setAndGet (10_tp), 1.0f);
            CHECK_EQ (setAndGet (10.1_tp), 0.0f);
            CHECK_EQ (setAndGet (15_tp), 0.0f);
        }
    }
}
#endif

#if ENGINE_UNIT_TESTS_AUX_SEND
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("Aux send initialising")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = engine::test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        auto& tc = edit->getTransport();
        auto player = test_utilities::createEnginePlayer (*edit, {});

        auto audioTracks = getTracksOfType<AudioTrack> (*edit, false);
        auto send = insertNewPlugin<AuxSendPlugin> (*audioTracks[0]);
        CHECK (send->baseClassNeedsInitialising());
        edit->dispatchPendingUpdatesSynchronously();

        tc.ensureContextAllocated();
        CHECK (send->isOwnedBy (*audioTracks[0]));
        CHECK (! send->baseClassNeedsInitialising());

        send->removeFromParent();
        audioTracks[1]->pluginList.insertPlugin (send, 0, {});
        tc.ensureContextAllocated (true);
        CHECK (send->isOwnedBy (*audioTracks[1]));
        CHECK (! send->baseClassNeedsInitialising());
    }
}
#endif

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS