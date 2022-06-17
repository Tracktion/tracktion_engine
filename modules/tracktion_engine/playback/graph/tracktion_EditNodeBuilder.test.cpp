/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if GRAPH_UNIT_TESTS_EDITNODE

using namespace tracktion::graph;

//==============================================================================
//==============================================================================
class EditNodeBuilderTests : public juce::UnitTest
{
public:
    EditNodeBuilderTests()
        : juce::UnitTest ("Edit Node Builder", "tracktion_graph")
    {
    }

    void runTest() override
    {
        tracktion::graph::test_utilities::TestSetup ts;
        ts.sampleRate = 44100.0;
        ts.blockSize = 256;

        runTrackDestinationRendering (ts, 3.0s, 2, false);
        runTrackDestinationRendering (ts, 3.0s, 2, true);

        runAuxSend (ts, 3.0s, 2, false);
        runAuxSend (ts, 3.0s, 2, true);

        runRackRendering (ts, 3.0s, 2, false);
        runRackRendering (ts, 3.0s, 2, true);

        runSubmix (ts, 3.0s, 2, false);
        runSubmix (ts, 3.0s, 2, true);

        runMuteSolo (ts, 3.0s, 2, false);
        runMuteSolo (ts, 3.0s, 2, true);

        runClipFade (ts, 3.0s, 2, false);
        runClipFade (ts, 3.0s, 2, true);
    }

private:
    //==============================================================================
    //==============================================================================
    void runRackRendering (test_utilities::TestSetup ts,
                           TimeDuration durationInSeconds,
                           int numChannels,
                           bool isMultiThreaded)
    {
        using namespace tracktion::graph;
        using namespace test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->ensureNumberOfAudioTracks (1);
            auto track = getAudioTracks (*edit)[0];

            track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            Plugin::Array plugins;
            plugins.add (track->getVolumePlugin());
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rack), 0).get());

            beginTest ("Basic Rack Creation: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                              ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }

            rackInstance->leftInputGoesTo = -1;
            rackInstance->rightOutputComesFrom = -1;

            beginTest ("Unconnected Inputs/Outputs: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                              ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }
    }

    /** Has two tracks, one with a sin clip and aux send which is muted, one with an
        aux return which is unmuted. The second track should not be audible because
        the aux source is muted.
    */
    void runAuxSend (test_utilities::TestSetup ts,
                     TimeDuration durationInSeconds,
                     int numChannels,
                     bool isMultiThreaded)
    {
        using namespace tracktion::graph;
        using namespace test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->ensureNumberOfAudioTracks (2);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            {
                auto auxSourceTrack = getAudioTracks (*edit)[0];
                auxSourceTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
                auxSourceTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);
                auxSourceTrack->setMute (true);
            }

            {
                auto auxReturnTrack = getAudioTracks (*edit)[1];
                auxReturnTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);
            }

            beginTest ("Aux Send Mute Rendering: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                              ts, numChannels, durationInSeconds.inSeconds(), true);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                auto result = testContext.processAll();

                expectAudioBuffer (*this, result->buffer, 0, 0.0f, 0.0f);
                expectAudioBuffer (*this, result->buffer, 1, 0.0f, 0.0f);
            }
        }
    }

    /** Has two tracks with a sin clip at 0.5 magnitude on each track, both sent to a third track as their destination.
        Once rendered, the resulting file should have a magnitude of 1.0.
    */
    void runTrackDestinationRendering (test_utilities::TestSetup ts,
                                       TimeDuration durationInSeconds,
                                       int numChannels,
                                       bool isMultiThreaded)
    {
        using namespace tracktion::graph;
        using namespace test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->ensureNumberOfAudioTracks (3);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
            auto destTrack = getAudioTracks (*edit)[2];

            for (int trackIndex : { 0, 1 })
            {
                auto track = getAudioTracks (*edit)[trackIndex];
                track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
                track->getVolumePlugin()->setVolumeDb (gainToDb (0.5f));
                track->getOutput().setOutputToTrack (destTrack);
            }

            beginTest ("Track Destination Rendering: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                              ts, numChannels, durationInSeconds.inSeconds(), true);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                auto result = testContext.processAll();

                expectAudioBuffer (*this, result->buffer, 0, 1.0f, 0.707f);
                expectAudioBuffer (*this, result->buffer, 1, 1.0f, 0.707f);
            }
        }
    }

    /** Creates a submix in a submix with a sin audio track and renders all the combinations of this.
        E.g.
            submix_1
                submix_2
                    audio_1

        Expects:
        submix_1, submix_2, audio_1 = 0dB
        submix_2, audio_1           = -6dB
        audio_1                     = -12dB
        submix_1, audio_1           = 0dB/NA
        submix_1, submix_2          = 0dB/NA
        submix_1                    = 0dB/NA
        submix_2                    = -6dB/NA
    */
    void runSubmix (test_utilities::TestSetup ts,
                    TimeDuration durationInSeconds,
                    int numChannels,
                    bool isMultiThreaded)
    {
        using namespace tracktion::graph;
        using namespace test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto submixTrack1 = edit->insertNewFolderTrack ({ nullptr, nullptr }, nullptr, true).get();
            submixTrack1->getVolumePlugin()->setVolumeDb (6.0f);

            auto submixTrack2 = edit->insertNewFolderTrack ({ submixTrack1, nullptr }, nullptr, true).get();
            submixTrack2->getVolumePlugin()->setVolumeDb (6.0f);

            auto audioTrack = edit->insertNewAudioTrack ({ submixTrack2, nullptr }, nullptr).get();
            audioTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
            audioTrack->getVolumePlugin()->setVolumeDb (-12.0f);

            beginTest ("Submix Rendering: " + description);
            {
                expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack1, submixTrack2, audioTrack }, 1.0f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack2, audioTrack }, 0.5f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { audioTrack }, 0.25f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack1, audioTrack }, 0.0f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack1, submixTrack2 }, 0.0f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack1 }, 1.0f);
                expectPeak (*this, *edit, { 0s, durationInSeconds }, { submixTrack2 }, 0.5f);
            }
        }
    }

    /** Runs tests solo and muting various track configurations. */
    void runMuteSolo (test_utilities::TestSetup ts,
                      TimeDuration durationInSeconds,
                      int numChannels,
                      bool isMultiThreaded)
    {
        using namespace tracktion::graph;
        using namespace test_utilities;
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        beginTest ("Basic Solo/Mute: " + description);
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // No tracks solo/muted
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 muted
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Track 2 solo
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Track 1 solo
            audioTrack1->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 & 2 solo
            audioTrack1->setSolo (true);
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
        }

        beginTest ("Basic solo isolate: " + description);
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // No tracks solo/muted
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 solo isolate
            audioTrack1->setSoloIsolate (true);
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 2 solo (track 1 should still be audible)
            audioTrack2->setSolo (true);
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
        }

        beginTest ("Track destination solo/mute: " + description);
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // Set track1 to output to track2
            getTrackOutput (*audioTrack1)->setOutputToTrack (dynamic_cast<AudioTrack*> (audioTrack2));
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Set vol of track 1 to -6dB
            audioTrack1->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack1->getVolumePlugin()->setVolumeDb (0.0f);

            // Set track 1 volume to -6dB  (output should be -6dB)
            audioTrack1->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack1->getVolumePlugin()->setVolumeDb (0.0f);

            // Set vol of track 2 to -6dB
            audioTrack2->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack2->getVolumePlugin()->setVolumeDb (0.0f);

            // Solo track 1 (which implicitly solos track 2)
            audioTrack1->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Solo track 2 (which implicitly solos track 1)
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Solo track 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 1 & 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Mute track 2 (output should be silent)
            audioTrack2->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 2, mute track 2 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack2->setMute (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
        }

        beginTest ("Submix solo/mute: " + description);
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto submixTop = edit->insertNewFolderTrack ({{}}, nullptr, true).get();
            auto submixMid = edit->insertNewFolderTrack ({ submixTop, nullptr }, nullptr, true).get();

            auto audioTrack = edit->insertNewAudioTrack ({ submixMid, nullptr }, nullptr).get();
            audioTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            // All tracks
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Soloing any should pass audio
            submixTop->setSolo (true);
            submixMid->setSolo (true);
            audioTrack->setSolo (true);
            expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Soloing any should stop audio
            for (auto t : std::array<Track*, 3> { submixTop, submixMid, audioTrack })
            {
                t->setMute (true);
                expectPeakAndResetMuteSolo (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
            }

            // Soloing and muting any/all should stop audio
            submixTop->setSolo (true);
            submixMid->setSolo (true);
            audioTrack->setSolo (true);
            submixTop->setMute (true);
            submixMid->setMute (true);
            audioTrack->setMute (true);
            expectPeak (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
        }
    }

    void runClipFade (test_utilities::TestSetup ts,
                      TimeDuration durationInSeconds,
                      int numChannels,
                      bool isMultiThreaded)
    {
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");

        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
        auto clip = getAudioTracks (*edit)[0]->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
        clip->setFadeInType (AudioFadeCurve::linear);
        clip->setFadeOutType (AudioFadeCurve::linear);

        beginTest ("No fade");
        {
            expectRMS (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f);
        }

        beginTest ("Fade in");
        {
            clip->setFadeIn (durationInSeconds);
            expectRMS (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
            clip->setFadeIn (0_td);
        }

        beginTest ("Fade out");
        {
            clip->setFadeOut (durationInSeconds);
            expectRMS (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
            clip->setFadeOut (0_td);
        }

        beginTest ("Fade in and out");
        {
            clip->setFadeIn (durationInSeconds / 2);
            clip->setFadeOut (durationInSeconds / 2);
            expectRMS (*this, *edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
        }
    }

    //==============================================================================
    //==============================================================================
    static std::unique_ptr<tracktion::graph::Node> createNode (Edit& edit, ProcessState& processState,
                                                              double sampleRate, int blockSize)
    {
        CreateNodeParams params { processState };
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true; // Required for audio files to be read
        return createNodeForEdit (edit, params);
    }

    //==============================================================================
    static void expectPeak (juce::UnitTest& ut, Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (ut, Renderer::measureStatistics ("", edit, tr, getTracksMask (tracks), blockSize));
        ut.expect (juce::isWithin (stats.peak, expectedPeak, 0.01f), juce::String ("Expected peak: ") + juce::String (expectedPeak, 4));
    }

    static void expectRMS (juce::UnitTest& ut, Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedRMS)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (ut, Renderer::measureStatistics ("", edit, tr, getTracksMask (tracks), blockSize));
        ut.expect (juce::isWithin (stats.average, expectedRMS, 0.01f), juce::String ("Expected RMS: ") + juce::String (expectedRMS, 4));
    }

    static void expectPeakAndResetMuteSolo (juce::UnitTest& ut, Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        expectPeak (ut, edit, tr, tracks, expectedPeak);

        for (auto t : tracks)
        {
            t->setMute (false);
            t->setSolo (false);
            t->setSoloIsolate (false);
        }
    }

    static Renderer::Statistics logStats (juce::UnitTest& ut, Renderer::Statistics stats)
    {
        ut.logMessage ("Stats: peak " + juce::String (stats.peak) + ", avg " + juce::String (stats.average) + ", duration " + juce::String (stats.audioDuration));
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
};

static EditNodeBuilderTests editNodeBuilderTests;

#endif

}} // namespace tracktion { inline namespace engine
