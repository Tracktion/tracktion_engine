/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPEFFECTS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ClipEffects: MakeMono renders both output channels")
    {
        auto& engine = *Engine::getEngines()[0];

        // A project-backed edit is required so insertWaveClip creates a ProjectItem
        // reference for the source. Without it, getWaveInfo() falls through to
        // getAudioFile().getInfo() (which reads the already-rendered mono proxy) and
        // getActiveChannelConfiguration() accidentally returns mono — masking the bug.
        // With a project item, getWaveInfo() always returns the original stereo info
        // even after the proxy is rendered, so the mismatch is exposed.
        auto tempDir = juce::File::createTempFile ({});
        tempDir.createDirectory();
        auto projectFile = tempDir.getChildFile ("test.tracktion");

        auto& pm = engine.getProjectManager();
        ProjectManager::TempProject tp (pm, projectFile, true);
        REQUIRE (tp.project != nullptr);

        auto editItem = tp.project->createNewEdit();
        REQUIRE (editItem != nullptr);

        auto edit = createEmptyEdit (engine, editItem->getSourceFile());
        REQUIRE (edit != nullptr);
        edit->setProjectItemRef (editItem->getProjectItemRef());

        edit->ensureNumberOfAudioTracks (1);
        edit->tempoSequence.getTempo (0)->setBpm (60.0);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0);

        auto track = getAudioTracks (*edit)[0];

        // Short clip to keep render fast
        auto stereoFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 0.1, 2);
        auto clip = insertWaveClip (*track, {}, stereoFile->getFile(),
                                    { .time = { 0_tp, 0.1_tp } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);

        // Enable clip effects and add MakeMono
        clip->enableEffects (true, false);
        auto effectsState = clip->state.getChildWithName (IDs::EFFECTS);
        REQUIRE (effectsState.isValid());

        auto makeMonoState = createValueTree (IDs::EFFECT,
                                              IDs::type, juce::VariantConverter<ClipEffect::EffectType>::toVar (ClipEffect::EffectType::makeMono));
        effectsState.addChild (makeMonoState, -1, nullptr);

        // Determine the expected proxy path and trigger the render job directly
        auto proxyFile = RenderManager::getAudioFileForHash (engine, edit->getTempDirectory (false), clip->getHash());
        auto job = clip->getRenderJob (proxyFile);
        REQUIRE (job != nullptr);

        // Process one tick so the 'started' message submits the job to the thread pool
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1);

        // Spin until the job completes — fast for a 0.1s clip, no fixed delay
        while (engine.getRenderManager().getNumJobs() > 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);

        REQUIRE (proxyFile.getFile().existsAsFile());

        // sourceMediaChanged() calls updateSourceFile() when needsRender() is true,
        // which points the clip's current source at the rendered proxy
        clip->sourceMediaChanged();

        auto result = test_utilities::renderToAudioBuffer (*edit);
        REQUIRE (result.buffer.getNumChannels() == 2);

        auto leftRMS  = result.buffer.getRMSLevel (0, 0, result.buffer.getNumSamples());
        auto rightRMS = result.buffer.getRMSLevel (1, 0, result.buffer.getNumSamples());

        // Both channels must carry the mono mix — with the node-builder bug,
        // only left has signal because WaveNode reads a stereo config from a mono file
        CHECK (leftRMS  > 0.01f);
        CHECK (rightRMS > 0.01f);

        tempDir.deleteRecursively (false);
    }

    TEST_CASE ("ClipEffects: copy paste remaps plugin IDs")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        edit->ensureNumberOfAudioTracks (2);
        auto tracks = getAudioTracks (*edit);
        REQUIRE (tracks.size() >= 2);

        auto track0 = tracks[0];
        auto track1 = tracks[1];

        // Create a sin wave file and insert clip on track 0
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0);
        auto clip = insertWaveClip (*track0, {}, sinFile->getFile(),
                                    { .time = { 0_tp, 1_tp } },
                                    DeleteExistingClips::no);
        REQUIRE (clip != nullptr);

        // Enable clip effects
        clip->enableEffects (true, false);
        REQUIRE (clip->getClipEffects() != nullptr);

        // Create a ReverbPlugin and build the EFFECT ValueTree
        auto reverbPlugin = edit->getPluginCache().createNewPlugin (ReverbPlugin::xmlTypeName, {});
        REQUIRE (reverbPlugin != nullptr);
        reverbPlugin->setProcessingEnabled (false);
        reverbPlugin->flushPluginStateToValueTree();

        auto effectState = createValueTree (IDs::EFFECT,
                                            IDs::type, juce::VariantConverter<ClipEffect::EffectType>::toVar (ClipEffect::EffectType::filter));
        effectState.addChild (reverbPlugin->state, -1, nullptr);

        auto effectsState = clip->state.getChildWithName (IDs::EFFECTS);
        REQUIRE (effectsState.isValid());
        effectsState.addChild (effectState, -1, nullptr);

        // Get the PluginEffect's plugin back from the clip
        auto clipEffects = clip->getClipEffects();
        REQUIRE (clipEffects->size() == 1);

        auto pluginEffect = dynamic_cast<PluginEffect*> ((*clipEffects)[0]);
        REQUIRE (pluginEffect != nullptr);
        REQUIRE (pluginEffect->plugin != nullptr);

        // Add automation ramp to roomSizeParam
        auto reverbFromEffect = dynamic_cast<ReverbPlugin*> (pluginEffect->plugin.get());
        REQUIRE (reverbFromEffect != nullptr);
        REQUIRE (reverbFromEffect->roomSizeParam != nullptr);

        auto& curve = reverbFromEffect->roomSizeParam->getCurve();
        curve.addPoint (0_tp, 0.1f, 0.0f, nullptr);
        curve.addPoint (1_tp, 0.9f, 0.0f, nullptr);
        CHECK (curve.getNumPoints() == 2);

        // Record original IDs
        auto originalClipID = clip->itemID;
        auto originalPluginID = pluginEffect->plugin->itemID;
        REQUIRE (originalClipID.isValid());
        REQUIRE (originalPluginID.isValid());

        // Copy + remap + paste to track1
        auto newClipState = clip->state.createCopy();
        EditItemID::remapIDs (newClipState, nullptr, *edit);
        track1->state.appendChild (newClipState, nullptr);

        // Find the new clip
        auto newClipID = EditItemID::fromProperty (newClipState, IDs::id);
        REQUIRE (newClipID.isValid());

        auto newClipPtr = track1->findClipForID (newClipID);
        REQUIRE (newClipPtr != nullptr);

        auto newAudioClip = dynamic_cast<AudioClipBase*> (newClipPtr);
        REQUIRE (newAudioClip != nullptr);

        // Get the new clip's PluginEffect
        auto newClipEffects = newAudioClip->getClipEffects();
        REQUIRE (newClipEffects != nullptr);
        REQUIRE (newClipEffects->size() == 1);

        auto newPluginEffect = dynamic_cast<PluginEffect*> ((*newClipEffects)[0]);
        REQUIRE (newPluginEffect != nullptr);
        REQUIRE (newPluginEffect->plugin != nullptr);

        // Assert IDs are different
        CHECK (newClipID != originalClipID);
        CHECK (newPluginEffect->plugin->itemID != originalPluginID);

        // Assert plugin type is still reverb
        auto newReverb = dynamic_cast<ReverbPlugin*> (newPluginEffect->plugin.get());
        REQUIRE (newReverb != nullptr);

        // Assert automation curve is preserved
        REQUIRE (newReverb->roomSizeParam != nullptr);
        auto& newCurve = newReverb->roomSizeParam->getCurve();
        CHECK (newCurve.getNumPoints() == 2);

        if (newCurve.getNumPoints() == 2)
        {
            CHECK (newCurve.getPointValue (0) == doctest::Approx (0.1f));
            CHECK (newCurve.getPointValue (1) == doctest::Approx (0.9f));
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPEFFECTS
