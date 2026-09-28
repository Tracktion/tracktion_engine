/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_EDITNODE

#include "../../utilities/tracktion_TestUtilities.h"
#include "../../testing/tracktion_EnginePlayer.h"
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

using namespace tracktion::graph;

//==============================================================================
namespace editnode_test_helpers
{
    static std::unique_ptr<tracktion::graph::Node> createNode (Edit& edit, ProcessState& processState,
                                                               double sampleRate, int blockSize)
    {
        CreateNodeParams params { processState };
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true; // Required for audio files to be read
        return createNodeForEdit (edit, params);
    }

    static Renderer::Statistics logStats (Renderer::Statistics stats)
    {
        MESSAGE (("Stats: peak " + juce::String (stats.peak) + ", avg " + juce::String (stats.average) + ", duration " + juce::String (stats.audioDuration)).toStdString());
        return stats;
    }

    static void expectPeak (Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("", edit, tr, toBitSet (tracks), blockSize));
        CHECK_MESSAGE (juce::isWithin (stats.peak, expectedPeak, 0.01f),
                       (juce::String ("Expected peak: ") + juce::String (expectedPeak, 4)).toStdString());
    }

    static void expectRMS (Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedRMS)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("", edit, tr, toBitSet (tracks), blockSize));
        CHECK_MESSAGE (juce::isWithin (stats.average, expectedRMS, 0.01f),
                       (juce::String ("Expected RMS: ") + juce::String (expectedRMS, 4)).toStdString());
    }

    static void expectPeakAndResetMuteSolo (Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        expectPeak (edit, tr, tracks, expectedPeak);

        for (auto t : tracks)
        {
            t->setMute (false);
            t->setSolo (false);
            t->setSoloIsolate (false);
        }
    }
} // namespace editnode_test_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Edit Node Builder")
{
    using namespace tracktion::graph::test_utilities;
    using namespace editnode_test_helpers;

    tracktion::graph::test_utilities::TestSetup ts;
    ts.sampleRate = 44100.0;
    ts.blockSize = 256;

    auto runTrackDestinationRendering = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
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

            // Track Destination Rendering
            {
                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), true);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                auto result = testContext.processAll();

                expectAudioBuffer (result->buffer, 0, 1.0f, 0.707f);
                expectAudioBuffer (result->buffer, 1, 1.0f, 0.707f);
            }
        }
    };

    auto runAuxSend = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
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

            // Aux Send Mute Rendering
            {
                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                              ts, numChannels, durationInSeconds.inSeconds(), true);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                auto result = testContext.processAll();

                expectAudioBuffer (result->buffer, 0, 0.0f, 0.0f);
                expectAudioBuffer (result->buffer, 1, 0.0f, 0.0f);
            }
        }
    };

    auto runRackRendering = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        {
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->ensureNumberOfAudioTracks (1);
            auto track = getAudioTracks (*edit)[0];

            track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            Plugin::Array plugins;
            plugins.add (track->getVolumePlugin());
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rack), 0).get());

            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, edit->tempoSequence };

            // Basic Rack Creation
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }

            rackInstance->setInputMapping (0, -1);
            rackInstance->setOutputMapping (1, -1);

            // Unconnected Inputs/Outputs
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }
    };

    auto runMonoRackRendering = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        {
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 1, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->ensureNumberOfAudioTracks (1);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
            auto track = getAudioTracks (*edit)[0];

            track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            Plugin::Array plugins;
            plugins.add (track->getVolumePlugin());
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            track->pluginList.insertPlugin (RackInstance::create (*rack), 0);

            tracktion::graph::PlayHead playHead;
            tracktion::graph::PlayHeadState playHeadState { playHead };
            ProcessState processState { playHeadState, edit->tempoSequence };

            auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
            graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (
                std::make_unique<TracktionNodePlayer> (std::move (node), processState,
                                                       ts.sampleRate, ts.blockSize,
                                                       getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                ts, numChannels, durationInSeconds.inSeconds(), true);

            if (! isMultiThreaded)
                testContext.getNodePlayer().setNumThreads (0);

            testContext.setPlayHead (&playHeadState.playHead);
            playHeadState.playHead.playSyncedToRange ({});
            auto result = testContext.processAll();

            // Mono signal must be present on both channels after passing through the Rack
            expectAudioBuffer (result->buffer, 0, 1.0f, 0.707f);
            expectAudioBuffer (result->buffer, 1, 1.0f, 0.707f);
        }
    };

    auto runSubmix = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        auto& engine = *engine::Engine::getEngines()[0];
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());

        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = engine::test_utilities::createTestEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto submixTrack1 = edit->insertNewFolderTrack ({ nullptr, nullptr }, nullptr, true).get();
            submixTrack1->getVolumePlugin()->setVolumeDb (6.0f);

            auto submixTrack2 = edit->insertNewFolderTrack ({ submixTrack1, nullptr }, nullptr, true).get();
            submixTrack2->getVolumePlugin()->setVolumeDb (6.0f);

            auto audioTrack = edit->insertNewAudioTrack ({ submixTrack2, nullptr }, nullptr).get();
            audioTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
            audioTrack->getVolumePlugin()->setVolumeDb (-12.0f);

            // Submix Rendering
            {
                expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack1, submixTrack2, audioTrack }, 1.0f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack2, audioTrack }, 0.5f);
                expectPeak (*edit, { 0s, durationInSeconds }, { audioTrack }, 0.25f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack1, audioTrack }, 0.0f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack1, submixTrack2 }, 0.0f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack1 }, 1.0f);
                expectPeak (*edit, { 0s, durationInSeconds }, { submixTrack2 }, 0.5f);
            }
        }
    };

    auto runMuteSolo = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());

        // Basic Solo/Mute
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // No tracks solo/muted
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 muted
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Track 2 solo
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Track 1 solo
            audioTrack1->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 & 2 solo
            audioTrack1->setSolo (true);
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
        }

        // Basic solo isolate
        {
            auto sinFile = getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // No tracks solo/muted
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 1 solo isolate
            audioTrack1->setSoloIsolate (true);
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Track 2 solo (track 1 should still be audible)
            audioTrack2->setSolo (true);
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);
        }

        // Track destination solo/mute
        {
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto audioTrack1 = getAudioTracks (*edit)[0];
            audioTrack1->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            auto audioTrack2 = edit->insertNewAudioTrack ({{}}, nullptr).get();

            // Set track1 to output to track2
            getTrackOutput (*audioTrack1)->setOutputToTrack (dynamic_cast<AudioTrack*> (audioTrack2));
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Set vol of track 1 to -6dB
            audioTrack1->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack1->getVolumePlugin()->setVolumeDb (0.0f);

            // Set track 1 volume to -6dB  (output should be -6dB)
            audioTrack1->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack1->getVolumePlugin()->setVolumeDb (0.0f);

            // Set vol of track 2 to -6dB
            audioTrack2->getVolumePlugin()->setVolumeDb (-6.0f);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.5f);
            audioTrack2->getVolumePlugin()->setVolumeDb (0.0f);

            // Solo track 1 (which implicitly solos track 2)
            audioTrack1->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Solo track 2 (which implicitly solos track 1)
            audioTrack2->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Solo track 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 1 & 2, mute track 1 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack1->setSolo (true);
            audioTrack1->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Mute track 2 (output should be silent)
            audioTrack2->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);

            // Solo track 2, mute track 2 (output should be silent)
            audioTrack2->setSolo (true);
            audioTrack2->setMute (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
        }

        // Submix solo/mute
        {
            auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            auto submixTop = edit->insertNewFolderTrack ({{}}, nullptr, true).get();
            auto submixMid = edit->insertNewFolderTrack ({ submixTop, nullptr }, nullptr, true).get();

            auto audioTrack = edit->insertNewAudioTrack ({ submixMid, nullptr }, nullptr).get();
            audioTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);

            // All tracks
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Soloing any should pass audio
            submixTop->setSolo (true);
            submixMid->setSolo (true);
            audioTrack->setSolo (true);
            expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 1.0f);

            // Soloing any should stop audio
            for (auto t : std::array<Track*, 3> { submixTop, submixMid, audioTrack })
            {
                t->setMute (true);
                expectPeakAndResetMuteSolo (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
            }

            // Soloing and muting any/all should stop audio
            submixTop->setSolo (true);
            submixMid->setSolo (true);
            audioTrack->setSolo (true);
            submixTop->setMute (true);
            submixMid->setMute (true);
            audioTrack->setMute (true);
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.0f);
        }
    };

    auto runClipFade = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), numChannels, 220.0f);

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
        auto clip = getAudioTracks (*edit)[0]->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
        clip->setFadeInType (AudioFadeCurve::linear);
        clip->setFadeOutType (AudioFadeCurve::linear);

        // No fade
        expectRMS (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f);

        // Fade in
        {
            clip->setFadeIn (durationInSeconds);
            expectRMS (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
            clip->setFadeIn (0_td);
        }

        // Fade out
        {
            clip->setFadeOut (durationInSeconds);
            expectRMS (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
            clip->setFadeOut (0_td);
        }

        // Fade in and out
        {
            clip->setFadeIn (durationInSeconds / 2);
            clip->setFadeOut (durationInSeconds / 2);
            expectRMS (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 0.707f / 2.0f);
        }
    };

    auto runBusTrack = [&] (TimeDuration durationInSeconds, int numChannels, bool isMultiThreaded)
    {
        MESSAGE ((graph::test_utilities::getDescription (ts) + juce::String (isMultiThreaded ? ", MT" : ", ST")).toStdString());
        auto& engine = *tracktion::engine::Engine::getEngines()[0];

        // --- Test A: Aux send/return signal flow through bus track ---
        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->ensureNumberOfAudioTracks (2);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            // Track 1: source with sin clip (peak=1.0) + aux send at unity gain
            auto sourceTrack = getAudioTracks (*edit)[0];
            sourceTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
            sourceTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);

            // Track 2: bus track (output=none) with aux return
            auto busTrack = getAudioTracks (*edit)[1];
            busTrack->getOutput().setOutputToNone();
            busTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);

            // Bus track aux return receives signal
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 2.0f);

            // Bus track graph builds with no output destination
            {
                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                CHECK_MESSAGE (node != nullptr, "Bus track graph should build successfully");

                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }

            // Bus track output set later
            {
                busTrack->getOutput().setOutputToDefaultDevice (false);

                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                CHECK_MESSAGE (node != nullptr, "Bus track graph should build after setting output");

                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }

        // --- Test B: Rack instance on bus track ---
        {
            auto sinFile = tracktion::graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds.inSeconds(), 2, 220.0f);

            auto edit = test_utilities::createTestEdit (engine);
            edit->ensureNumberOfAudioTracks (2);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

            // Track 1: source with sin clip + aux send
            auto sourceTrack = getAudioTracks (*edit)[0];
            sourceTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { {}, durationInSeconds } }, false);
            sourceTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);

            // Track 2: bus track (output=none) with a Rack containing an aux return
            auto busTrack = getAudioTracks (*edit)[1];
            busTrack->getOutput().setOutputToNone();

            // Insert an aux return, then wrap it in a Rack
            busTrack->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);
            Plugin::Array pluginsToWrap;
            pluginsToWrap.add (busTrack->pluginList.getPlugins().getFirst());
            RackType::createTypeToWrapPlugins (pluginsToWrap, *edit);

            // Bus track with Rack - aux return receives signal
            expectPeak (*edit, { 0s, durationInSeconds }, getAllTracks (*edit), 2.0f);

            // Bus track with Rack - graph builds with no output
            {
                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                CHECK_MESSAGE (node != nullptr, "Bus track with Rack should build successfully");

                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }

            // Bus track with Rack - output set later
            {
                busTrack->getOutput().setOutputToDefaultDevice (false);

                tracktion::graph::PlayHead playHead;
                tracktion::graph::PlayHeadState playHeadState { playHead };
                ProcessState processState { playHeadState, edit->tempoSequence };

                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                CHECK_MESSAGE (node != nullptr, "Bus track with Rack should build after setting output");

                graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                            getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                                                     ts, numChannels, durationInSeconds.inSeconds(), false);

                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);

                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }
    };

    runTrackDestinationRendering (3.0s, 2, false);
    runTrackDestinationRendering (3.0s, 2, true);

    runAuxSend (3.0s, 2, false);
    runAuxSend (3.0s, 2, true);

    runRackRendering (3.0s, 2, false);
    runRackRendering (3.0s, 2, true);

    runMonoRackRendering (3.0s, 2, false);
    runMonoRackRendering (3.0s, 2, true);

    runSubmix (3.0s, 2, false);
    runSubmix (3.0s, 2, true);

    runMuteSolo (3.0s, 2, false);
    runMuteSolo (3.0s, 2, true);

    runClipFade (3.0s, 2, false);
    runClipFade (3.0s, 2, true);

    runBusTrack (3.0s, 2, false);
    runBusTrack (3.0s, 2, true);
}

TEST_CASE ("Plugin on empty track does not crash with 0-channel buffer")
{
    using namespace tracktion::graph::test_utilities;
    using namespace editnode_test_helpers;

    tracktion::graph::test_utilities::TestSetup ts;
    ts.sampleRate = 44100.0;
    ts.blockSize = 256;

    auto& engine = *tracktion::engine::Engine::getEngines()[0];

    auto renderEmptyTrackWithPlugin = [&] (const juce::String& pluginTypeName)
    {
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];
        REQUIRE (track->getClips().isEmpty());

        track->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (pluginTypeName, {}), 0, nullptr);

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, edit->tempoSequence };

        auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
        graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (
            std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                   getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
            ts, 2, 1.0, true);

        testContext.getNodePlayer().setNumThreads (0);
        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});
        auto result = testContext.processAll();

        // Empty track with no clips should render as silence without crashing
        expectAudioBuffer (result->buffer, 0, 0.0f, 0.0f);
        expectAudioBuffer (result->buffer, 1, 0.0f, 0.0f);
    };

    // Every internal plugin declared with {} inputs in getBusses() — plus one
    // declared-stereo plugin as a control — exercised in an audio chain on a
    // track with no clips. None of these should crash or assert.
    const juce::StringArray pluginTypeNames
    {
        AuxSendPlugin::xmlTypeName,
        AuxReturnPlugin::xmlTypeName,
        VCAPlugin::xmlTypeName,
        FreezePointPlugin::xmlTypeName,
        InsertPlugin::xmlTypeName,
        TextPlugin::xmlTypeName,
        ChannelMapperPlugin::xmlTypeName,
        LevelMeterPlugin::xmlTypeName,
        MidiPatchBayPlugin::xmlTypeName,
        MidiModifierPlugin::xmlTypeName,
        ReverbPlugin::xmlTypeName,               // control: declared stereo
        VolumeAndPanPlugin::xmlTypeName,         // control: declared stereo
    };

    for (auto& name : pluginTypeNames)
    {
        SUBCASE (name.toRawUTF8())
        {
            renderEmptyTrackWithPlugin (name);
        }
    }
}

TEST_CASE ("Generator plugin Node: SilentNode stays silent with node memory sharing enabled")
{
    using namespace tracktion::graph::test_utilities;
    using namespace editnode_test_helpers;

    tracktion::graph::test_utilities::TestSetup ts;
    ts.sampleRate = 44100.0;
    ts.blockSize = 256;

    auto& engine = *tracktion::engine::Engine::getEngines()[0];
    engine.getPluginManager().createBuiltInType<ToneGeneratorPlugin>();
    auto edit = test_utilities::createTestEdit (engine);

    auto plugin = edit->getPluginCache().createNewPlugin (ToneGeneratorPlugin::xmlTypeName, {});
    REQUIRE (plugin != nullptr);

    tracktion::graph::PlayHead playHead;
    tracktion::graph::PlayHeadState playHeadState { playHead };
    ProcessState processState { playHeadState, edit->tempoSequence };

    CreateNodeParams params { processState };
    params.sampleRate = ts.sampleRate;
    params.blockSize = ts.blockSize;

    // Mirrors the browser plugin panel: a generator plugin fed by a SilentNode,
    // summed into the output (here another SilentNode stands in for the edit output)
    auto node = createGeneratorPluginNode (plugin, params, makeNode<SilentNode> (2));

    // Collect the SilentNodes so their buffers can be inspected after processing
    std::vector<Node*> silentNodes;
    std::function<void (Node&)> collectSilentNodes = [&] (Node& n)
    {
        if (dynamic_cast<SilentNode*> (&n) != nullptr)
            silentNodes.push_back (&n);

        for (auto in : n.getDirectInputNodes())
            collectSilentNodes (*in);
    };
    collectSilentNodes (*node);
    REQUIRE (silentNodes.size() == 2); // one feeding the plugin, one standing in for the edit output

    for (auto silent : silentNodes)
        CHECK (! silent->canShareOutputBuffer());

    // N.B. Memory sharing must be enabled before the Node is set so it's active when the graph is prepared
    auto player = std::make_unique<TracktionNodePlayer> (processState, getPoolCreatorFunction (ThreadPoolStrategy::hybrid));
    player->enableNodeMemorySharing (true);
    player->setNode (std::move (node), ts.sampleRate, ts.blockSize);

    graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (std::move (player), ts, 2, 1.0, true);
    testContext.getNodePlayer().setNumThreads (0);
    testContext.setPlayHead (&playHeadState.playHead);
    playHeadState.playHead.playSyncedToRange ({});
    auto result = testContext.processAll();

    // Sanity check the generator actually produced audio
    CHECK (result->buffer.getMagnitude (0, 0, result->buffer.getNumSamples()) > 0.5f);

    // A SilentNode's buffer is only cleared once, in prepareToPlay. If a downstream Node
    // had adopted it and processed in place (see ShareOutputBuffer), it would now contain
    // the generator's output and feed it back as the plugin's "silent" input
    for (auto silent : silentNodes)
    {
        auto silentBuffer = toAudioBuffer (silent->getProcessedOutput().audio);
        CHECK (silentBuffer.getMagnitude (0, silentBuffer.getNumSamples()) == 0.0f);
    }
}

//==============================================================================
namespace routing_test_helpers
{
    /** Wraps a VolumeAndPanPlugin in a Rack and connects it to numOutputs rack outputs,
        as wrapping a multi-out instrument does.
    */
    inline RackType::Ptr createWrapperRack (Edit& edit, int numOutputs)
    {
        auto volPan = edit.getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create());
        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, edit);

        while (rackType->getOutputNames().size() < numOutputs + 1) // +1 for the MIDI output
        {
            const auto pin = rackType->getOutputNames().size();
            rackType->addOutput (-1, "Output " + juce::String (pin));
            rackType->addConnection (volPan->itemID, 1 + (pin - 1) % 2, {}, pin);
        }

        return rackType;
    }

    inline RackInstance* insertRackInstance (AudioTrack& track, RackType& rackType)
    {
        return dynamic_cast<RackInstance*> (track.pluginList.insertPlugin (RackInstance::create (rackType), 0).get());
    }

    inline CompressorPlugin* insertSidechainedCompressor (AudioTrack& track, AudioTrack& source)
    {
        auto plugin = track.edit.getPluginCache().createNewPlugin (CompressorPlugin::xmlTypeName, {});
        track.pluginList.insertPlugin (plugin, 0, nullptr);
        auto comp = dynamic_cast<CompressorPlugin*> (plugin.get());
        REQUIRE (comp != nullptr);
        comp->useSidechainTrigger = true;
        comp->setSidechainSourceID (source.itemID);
        comp->guessSidechainRouting();
        comp->setThreshold (0.05f);
        comp->setRatio (0.1f);
        return comp;
    }

    /** Plays the Edit through the live device graph (as the app does) and returns the RMS of
        the second half of channel 0, so any file cache warm-up at the start is ignored.
    */
    inline float getDevicePlaybackRMS (test_utilities::EnginePlayer& player, Edit& edit, TimeDuration length)
    {
        auto& tc = edit.getTransport();
        tc.freePlaybackContext();
        tc.setPosition (0_tp);
        tc.play (false);

        auto buffer = player.process (toSamples (length, player.getParams().sampleRate));
        tc.stop (false, true);

        const auto half = buffer.getNumSamples() / 2;
        return buffer.getRMSLevel (0, half, buffer.getNumSamples() - half);
    }
}

TEST_CASE ("Edit Node Builder: routing doesn't widen a stereo render")
{
    // Racks, sends, sidechains and bus tracks are all processed off to the side of the
    // track's own signal path, so none of them should change the channel count of the
    // Edit's output. Renders size their files from this, so a wider graph means a
    // wider file full of silent channels
    using namespace routing_test_helpers;
    auto& engine = *Engine::getEngines()[0];

    auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 2, 220.0f);
    auto edit = test_utilities::createTestEdit (engine, 2);
    auto tracks = getAudioTracks (*edit);
    insertWaveClip (*tracks[0], {}, sinFile->getFile(), { .time = { 0_tp, 1_tp } }, DeleteExistingClips::no);

    SUBCASE ("No routing")                              {}
    SUBCASE ("Wrapped rack with 2 connected outputs")   { insertRackInstance (*tracks[0], *createWrapperRack (*edit, 2)); }
    SUBCASE ("Wrapped rack with 64 connected outputs")  { insertRackInstance (*tracks[0], *createWrapperRack (*edit, 64)); }

    SUBCASE ("Two instances of one rack type")
    {
        auto rackType = createWrapperRack (*edit, 2);
        insertRackInstance (*tracks[0], *rackType);
        insertRackInstance (*tracks[1], *rackType);
    }

    SUBCASE ("Aux send and return")
    {
        tracks[0]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);
        tracks[1]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);
    }

    SUBCASE ("Sidechain source")
    {
        insertSidechainedCompressor (*tracks[1], *tracks[0]);
    }

    SUBCASE ("Bus track")
    {
        tracks[0]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);
        tracks[1]->getOutput().setOutputToNone();
        tracks[1]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);
    }

    {
        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, edit->tempoSequence };

        auto node = editnode_test_helpers::createNode (*edit, processState, 44100.0, 512);
        REQUIRE (node != nullptr);
        CHECK_EQ (node->getNodeProperties().numberOfChannels, 2);
    }

    auto render = test_utilities::renderToAudioBuffer (*edit);
    CHECK_EQ (render.buffer.getNumChannels(), 2);
    CHECK_GT (test_utilities::getRMSLevel (render, { 0_tp, 1_tp }, 0), 0.1f);

    engine.getAudioFileManager().releaseAllFiles();
    edit->getTempDirectory (false).deleteRecursively();
}

TEST_CASE ("Edit Node Builder: live device graph routing")
{
    // The live graph groups tracks by output device and sinks tracks with no output,
    // which the render graph used by most tests doesn't do
    using namespace routing_test_helpers;
    auto& engine = *Engine::getEngines()[0];
    test_utilities::EnginePlayer player (engine, { .sampleRate = 44100.0, .blockSize = 512, .inputChannels = 0, .outputChannels = 2,
                                                   .inputNames = {}, .outputNames = {} });

    auto sin220 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0, 2, 220.0f);
    auto sin330 = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 2.0, 2, 330.0f);
    auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
    edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
    auto tracks = getAudioTracks (*edit);
    auto sourceClip = insertWaveClip (*tracks[0], {}, sin220->getFile(), { .time = { 0_tp, 2_tp } }, DeleteExistingClips::no);
    const auto length = 2_td;

    SUBCASE ("Track with a rack plays")
    {
        insertRackInstance (*tracks[0], *createWrapperRack (*edit, 2));
        CHECK_EQ (getDevicePlaybackRMS (player, *edit, length), doctest::Approx (0.707f).epsilon (0.05));
    }

    SUBCASE ("Track with a 64-output rack plays")
    {
        insertRackInstance (*tracks[0], *createWrapperRack (*edit, 64));
        CHECK_EQ (getDevicePlaybackRMS (player, *edit, length), doctest::Approx (0.707f).epsilon (0.05));
    }

    SUBCASE ("Bus track feeds an aux return on another track")
    {
        // Track 1 has no output, so it's only heard through its aux send
        tracks[0]->getOutput().setOutputToNone();
        tracks[0]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxSendPlugin::xmlTypeName, {}), 0, nullptr);
        tracks[1]->pluginList.insertPlugin (edit->getPluginCache().createNewPlugin (AuxReturnPlugin::xmlTypeName, {}), 0, nullptr);

        CHECK_EQ (getDevicePlaybackRMS (player, *edit, length), doctest::Approx (0.707f).epsilon (0.05));
    }

    SUBCASE ("Bus track keys a sidechain")
    {
        // Track 1 has no output, so it's only heard through the compressor it keys
        tracks[0]->getOutput().setOutputToNone();
        insertWaveClip (*tracks[1], {}, sin330->getFile(), { .time = { 0_tp, 2_tp } }, DeleteExistingClips::no);
        insertSidechainedCompressor (*tracks[1], *tracks[0]);

        sourceClip->setMuted (true);
        const auto unkeyedRMS = getDevicePlaybackRMS (player, *edit, length);
        REQUIRE_GT (unkeyedRMS, 0.1f);

        sourceClip->setMuted (false);
        const auto keyedRMS = getDevicePlaybackRMS (player, *edit, length);
        CHECK_MESSAGE (keyedRMS < unkeyedRMS * 0.7f,
                       ("Keyed RMS " + juce::String (keyedRMS, 4) + " vs unkeyed RMS " + juce::String (unkeyedRMS, 4)).toStdString());
    }

    sourceClip = nullptr;
    edit = nullptr;
    engine.getAudioFileManager().releaseAllFiles();
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif
