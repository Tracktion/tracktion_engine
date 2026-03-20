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

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_RACKINSTANCE
TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("Rack instance wet/dry param automation")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto um = &edit->getUndoManager();
        auto track = getAudioTracks (*edit)[0];

        // Sin file must outlive everything!
        auto fileLength = 5_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        // Rack
        VolumeAndPanPlugin::Ptr volPanPlugin (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));

        Plugin::Array plugins;
        plugins.add (volPanPlugin);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        auto rackInstancePlugin = track->pluginList.insertPlugin (RackInstance::create (*rackType), 0);
        auto rackInstance = dynamic_cast<RackInstance*> (rackInstancePlugin.get());

        auto wetGain = rackInstance->wetGain;
        auto& wetCurve = wetGain->getCurve();
        wetCurve.addPoint (2.5_tp, 1.0f, 0.0, um);
        wetCurve.addPoint (2.5_tp, 0.0f, 0.0, um);
        CHECK(wetGain->isAutomationActive());
        CHECK(rackInstance->isAutomationNeeded());

        CHECK (getValueAt (*wetGain, 2_tp) == doctest::Approx (1.0f));
        CHECK (getValueAt (*wetGain, 3_tp) == doctest::Approx (0.0f));

        // Render clip
        {
            insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                            DeleteExistingClips::no);

            auto render = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));
            CHECK (test_utilities::getRMSLevel (render, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f));
        }

        // Internal plugin automation
        {
            rackInstance->wetGain->getCurve().clear (um);
            rackInstance->wetGain->setParameter (1.0f, juce::dontSendNotification);

            auto volParam = volPanPlugin->volParam;
            auto& volCurve = volParam->getCurve();

            auto firstVal = getValueAt (*volParam, 0_tp);
            CHECK (firstVal == doctest::Approx (decibelsToVolumeFaderPosition (0.0f)));
            volCurve.addPoint (2.5_tp, firstVal, 0.0, um);
            volCurve.addPoint (2.5_tp, 0.0f, 0.0, um);

            auto render = test_utilities::renderToAudioBuffer (*edit);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, 2_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));
            CHECK (test_utilities::getRMSLevel (render, { 3_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f));
        }
    }

    TEST_CASE ("Rack instance: default stereo channel mappings")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        CHECK (rackInstance->getNumChannelMappings() == 2);
        CHECK (rackInstance->getInputMapping (0) == 1);
        CHECK (rackInstance->getInputMapping (1) == 2);
        CHECK (rackInstance->getOutputMapping (0) == 1);
        CHECK (rackInstance->getOutputMapping (1) == 2);
        CHECK (rackInstance->getInputGainParam (0) != nullptr);
        CHECK (rackInstance->getInputGainParam (1) != nullptr);
        CHECK (rackInstance->getOutputGainParam (0) != nullptr);
        CHECK (rackInstance->getOutputGainParam (1) != nullptr);
    }

    TEST_CASE ("Rack instance: set and get mappings")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        rackInstance->setInputMapping (0, 3);
        rackInstance->setOutputMapping (1, 4);

        CHECK (rackInstance->getInputMapping (0) == 3);
        CHECK (rackInstance->getOutputMapping (1) == 4);

        // Disconnect a channel
        rackInstance->setInputMapping (1, -1);
        CHECK (rackInstance->getInputMapping (1) == -1);
    }

    TEST_CASE ("Rack instance: add and remove channel mappings")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        // Add extra rack I/O pins
        rackType->addInput (-1, "Input 3");
        rackType->addInput (-1, "Input 4");
        rackType->addOutput (-1, "Output 3");
        rackType->addOutput (-1, "Output 4");

        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        CHECK (rackInstance->getNumChannelMappings() == 2);

        // Add channels
        rackInstance->addChannelMapping();
        CHECK (rackInstance->getNumChannelMappings() == 3);
        CHECK (rackInstance->getInputMapping (2) == -1);  // new channel defaults to disconnected
        CHECK (rackInstance->getOutputMapping (2) == -1);
        CHECK (rackInstance->getInputGainParam (2) != nullptr);
        CHECK (rackInstance->getOutputGainParam (2) != nullptr);

        rackInstance->addChannelMapping();
        CHECK (rackInstance->getNumChannelMappings() == 4);

        // Set mappings on new channels
        rackInstance->setInputMapping (2, 3);
        rackInstance->setInputMapping (3, 4);
        rackInstance->setOutputMapping (2, 3);
        rackInstance->setOutputMapping (3, 4);

        CHECK (rackInstance->getInputMapping (2) == 3);
        CHECK (rackInstance->getInputMapping (3) == 4);
        CHECK (rackInstance->getOutputMapping (2) == 3);
        CHECK (rackInstance->getOutputMapping (3) == 4);

        // Remove last channel
        rackInstance->removeLastChannelMapping();
        CHECK (rackInstance->getNumChannelMappings() == 3);

        // Can't remove below 1 channel
        rackInstance->removeLastChannelMapping();
        rackInstance->removeLastChannelMapping();
        CHECK (rackInstance->getNumChannelMappings() == 1);
        rackInstance->removeLastChannelMapping();
        CHECK (rackInstance->getNumChannelMappings() == 1);
    }

    TEST_CASE ("Rack instance: getNumOutputChannelsGivenInputs passthrough")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        // Default: numOutputChannels == 2 (from default stereo mappings)
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (2) == 2);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (4) == 4);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (8) == 8);

        // With numOutputChannels = 4, output should be at least 4
        rackInstance->setNumOutputChannels (4);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (2) == 4);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (4) == 4);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (8) == 8);
    }

    TEST_CASE ("Rack instance: separate input/output channel counts")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        // Default: both match CHANNELMAP count (2)
        CHECK (rackInstance->getNumInputChannels() == 2);
        CHECK (rackInstance->getNumOutputChannels() == 2);
        CHECK (rackInstance->getNumChannelMappings() == 2);

        // Set output channels to 3
        rackInstance->setNumOutputChannels (3);
        CHECK (rackInstance->getNumOutputChannels() == 3);
        CHECK (rackInstance->getNumInputChannels() == 2);
        CHECK (rackInstance->getNumChannelMappings() >= 3);
        CHECK (rackInstance->getNumOutputChannelsGivenInputs (2) == 3);

        // Set input channels to 4 — CHANNELMAP should grow
        rackInstance->setNumInputChannels (4);
        CHECK (rackInstance->getNumInputChannels() == 4);
        CHECK (rackInstance->getNumChannelMappings() >= 4);

        // Minimum is 1
        rackInstance->setNumInputChannels (0);
        CHECK (rackInstance->getNumInputChannels() == 1);
        rackInstance->setNumOutputChannels (-5);
        CHECK (rackInstance->getNumOutputChannels() == 1);
    }

    TEST_CASE ("Rack instance: linked input/output levels")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        rackInstance->addChannelMapping();
        rackInstance->addChannelMapping();

        // Test linked inputs
        rackInstance->linkInputs = true;
        rackInstance->setInputLevel (0, -6.0f);

        CHECK (rackInstance->getInputGainParam (0)->getCurrentValue() == doctest::Approx (-6.0f));
        CHECK (rackInstance->getInputGainParam (1)->getCurrentValue() == doctest::Approx (-6.0f));
        CHECK (rackInstance->getInputGainParam (2)->getCurrentValue() == doctest::Approx (-6.0f));
        CHECK (rackInstance->getInputGainParam (3)->getCurrentValue() == doctest::Approx (-6.0f));

        // Unlinked
        rackInstance->linkInputs = false;
        rackInstance->setInputLevel (1, -12.0f);

        CHECK (rackInstance->getInputGainParam (0)->getCurrentValue() == doctest::Approx (-6.0f));
        CHECK (rackInstance->getInputGainParam (1)->getCurrentValue() == doctest::Approx (-12.0f));
        CHECK (rackInstance->getInputGainParam (2)->getCurrentValue() == doctest::Approx (-6.0f));
    }

    TEST_CASE ("Rack instance: legacy stereo format migration")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        // Create a rack type
        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        // Create a normal RackInstance, then modify its state to simulate legacy format
        auto legacyState = RackInstance::create (*rackType);
        legacyState.setProperty (IDs::leftTo, 1, nullptr);
        legacyState.setProperty (IDs::rightTo, 2, nullptr);
        legacyState.setProperty (IDs::leftFrom, 2, nullptr);
        legacyState.setProperty (IDs::rightFrom, 1, nullptr);
        legacyState.setProperty (IDs::leftInDb, -3.0f, nullptr);
        legacyState.setProperty (IDs::rightInDb, -6.0f, nullptr);
        legacyState.setProperty (IDs::leftOutDb, -1.0f, nullptr);
        legacyState.setProperty (IDs::rightOutDb, -2.0f, nullptr);

        auto plugin = track->pluginList.insertPlugin (legacyState, 0);
        auto rackInstance = dynamic_cast<RackInstance*> (plugin.get());
        REQUIRE (rackInstance != nullptr);

        // Verify migration happened correctly
        CHECK (rackInstance->getNumChannelMappings() == 2);
        CHECK (rackInstance->getInputMapping (0) == 1);
        CHECK (rackInstance->getInputMapping (1) == 2);
        CHECK (rackInstance->getOutputMapping (0) == 2);
        CHECK (rackInstance->getOutputMapping (1) == 1);

        // Check gain values were migrated
        CHECK (rackInstance->getInputGainParam (0)->getCurrentValue() == doctest::Approx (-3.0f));
        CHECK (rackInstance->getInputGainParam (1)->getCurrentValue() == doctest::Approx (-6.0f));
        CHECK (rackInstance->getOutputGainParam (0)->getCurrentValue() == doctest::Approx (-1.0f));
        CHECK (rackInstance->getOutputGainParam (1)->getCurrentValue() == doctest::Approx (-2.0f));

        // Verify old properties were removed
        CHECK (! rackInstance->state.hasProperty (IDs::leftTo));
        CHECK (! rackInstance->state.hasProperty (IDs::rightTo));
        CHECK (! rackInstance->state.hasProperty (IDs::leftFrom));
        CHECK (! rackInstance->state.hasProperty (IDs::rightFrom));
    }

    TEST_CASE ("Rack instance: 4-channel audio routing")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto fileLength = 2_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds(), 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        // Create rack with a volume plugin
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        // Add extra I/O pins
        rackType->addInput (-1, "Input 3");
        rackType->addInput (-1, "Input 4");
        rackType->addOutput (-1, "Output 3");
        rackType->addOutput (-1, "Output 4");

        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        // Add 2 more channel mappings for 4-channel
        rackInstance->addChannelMapping();
        rackInstance->addChannelMapping();

        CHECK (rackInstance->getNumChannelMappings() == 4);

        // Map all 4 channels
        rackInstance->setInputMapping (2, 3);
        rackInstance->setInputMapping (3, 4);
        rackInstance->setOutputMapping (2, 3);
        rackInstance->setOutputMapping (3, 4);

        // Verify it renders without crashing
        auto render = test_utilities::renderToAudioBuffer (*edit);
        CHECK (render.buffer.getNumSamples() > 0);

        // First two channels should have audio (from the stereo sin file)
        CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.0f);
        CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) > 0.0f);
    }

    TEST_CASE ("Rack instance: rackMacro moveAutomation")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto um = &edit->getUndoManager();
        auto track = getAudioTracks (*edit)[0];

        // Create a rack with a VolumeAndPan plugin
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        // Add a macro parameter to the rack type
        auto macro = rackType->getMacroParameterListForWriting().createMacroParameter();
        REQUIRE (macro != nullptr);

        // Add automation points to the macro curve in the range 1s-3s
        auto& macroCurve = macro->getCurve();
        macroCurve.addPoint (1_tp, 0.2f, 0.0, um);
        macroCurve.addPoint (2_tp, 0.8f, 0.0, um);
        macroCurve.addPoint (3_tp, 0.4f, 0.0, um);
        CHECK (macroCurve.getNumPoints() == 3);

        // Insert a RackInstance on the track
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());
        REQUIRE (rackInstance != nullptr);

        // Build a TrackAutomationSection covering the 1s-3s range
        TrackAutomationSection section;
        section.position = { 1_tp, 3_tp };
        section.src = track;
        section.dst = track;

        juce::Array<TrackAutomationSection> sections;
        sections.add (section);

        // Move the automation by +2s
        moveAutomation (sections, 2_td, false);

        // The macro automation points should now be in 3s-5s range
        CHECK (macroCurve.getNumPoints() >= 2);

        bool foundPointNear3s = false;
        bool foundPointNear4s = false;
        bool foundPointNear5s = false;

        for (int i = 0; i < macroCurve.getNumPoints(); ++i)
        {
            auto t = macroCurve.getPointTime (i);

            if (std::abs ((t - 3_tp).inSeconds()) < 0.01)
                foundPointNear3s = true;

            if (std::abs ((t - 4_tp).inSeconds()) < 0.01)
                foundPointNear4s = true;

            if (std::abs ((t - 5_tp).inSeconds()) < 0.01)
                foundPointNear5s = true;
        }

        CHECK (foundPointNear3s);
        CHECK (foundPointNear4s);
        CHECK (foundPointNear5s);
    }

    TEST_CASE ("PluginNode: minimum channel count with maxNumChannels")
    {
        // When maxNumChannels > 0, PluginNode should guarantee at least that many channels
        // even if the input node reports 0 channels. This prevents crashes in plugins that
        // unconditionally access channel data.
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        // Create a VolumeAndPanPlugin (pass-through, returns numInputs from getNumOutputChannelsGivenInputs)
        auto volPan = edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create());

        graph::PlayHead playHead;
        PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        // Input node with 0 channels
        auto pluginNode = tracktion::graph::makeNode<PluginNode> (
            tracktion::graph::makeNode<tracktion::graph::SilentNode> (0),
            volPan,
            44100.0, 512,
            nullptr, processState,
            false, false,
            2);  // maxNumChannels = 2

        auto props = pluginNode->getNodeProperties();
        CHECK (props.numberOfChannels >= 2);
    }

    TEST_CASE ("Rack instance: setInputMappingByName and setOutputMappingByName")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        Plugin::Array plugins;
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());

        // Disconnect via name
        rackInstance->setInputMappingByName (0, rackInstance->getNoPinName());
        CHECK (rackInstance->getInputMapping (0) == -1);

        // Reconnect via name - getInputChoices(false) gives names without number prefix
        auto inputChoices = rackInstance->getInputChoices (false);

        if (inputChoices.size() > 1)
        {
            rackInstance->setInputMappingByName (0, inputChoices[1]);
            CHECK (rackInstance->getInputMapping (0) == 1);
        }
    }
}
#endif

} // namespace::inline namespace engine

#endif //TRACKTION_UNIT_TESTS
