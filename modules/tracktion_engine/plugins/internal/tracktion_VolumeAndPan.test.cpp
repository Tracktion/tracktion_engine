/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_VOLPANPLUGIN

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("VolumeAndPan: declares stereo minimum input but passes more channels through")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto vp = track->getVolumePlugin();
        REQUIRE (vp != nullptr);

        // The plugin declares a minimum of 2 input channels so the graph widens
        // mono upstream via a ChannelRemappingNode.
        CHECK (vp->getBusses().inputs.front().getNumChannels() == 2);

        // Output channel count tracks the input for any channel count >= declared min.
        CHECK (vp->getNumOutputChannelsGivenInputs (2) == 2);
        CHECK (vp->getNumOutputChannelsGivenInputs (4) == 4);
        CHECK (vp->getNumOutputChannelsGivenInputs (6) == 6);
        CHECK (vp->getNumOutputChannelsGivenInputs (8) == 8);
    }

    TEST_CASE ("VolumeAndPan: mono clip is widened so panning works")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        const auto fileLength = 2_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds(), 1, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        auto vp = track->getVolumePlugin();
        REQUIRE (vp != nullptr);

        // Pan hard-left: ch0 should have audio, ch1 should be silent.
        vp->setPan (-1.0f);
        {
            auto render = test_utilities::renderToAudioBuffer (*edit);
            REQUIRE (render.buffer.getNumChannels() == 2);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.1f);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) < 0.01f);
        }

        // Pan hard-right: ch1 should have audio, ch0 should be silent.
        vp->setPan (1.0f);
        {
            auto render = test_utilities::renderToAudioBuffer (*edit);
            REQUIRE (render.buffer.getNumChannels() == 2);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) < 0.01f);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) > 0.1f);
        }

        // Centre pan: mono source should be duplicated into both channels.
        vp->setPan (0.0f);
        {
            auto render = test_utilities::renderToAudioBuffer (*edit);
            REQUIRE (render.buffer.getNumChannels() == 2);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.1f);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) > 0.1f);
        }
    }

    TEST_CASE ("VolumeAndPan: stereo clip preserves both channels and pans correctly")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        const auto fileLength = 2_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds(), 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        auto vp = track->getVolumePlugin();
        REQUIRE (vp != nullptr);

        // Centre pan on a stereo source: both channels should remain audible.
        vp->setPan (0.0f);
        {
            auto render = test_utilities::renderToAudioBuffer (*edit);
            REQUIRE (render.buffer.getNumChannels() == 2);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.1f);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) > 0.1f);
        }

        // Hard-left pan on a stereo source: right channel should be silenced.
        vp->setPan (-1.0f);
        {
            auto render = test_utilities::renderToAudioBuffer (*edit);
            REQUIRE (render.buffer.getNumChannels() == 2);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.1f);
            CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) < 0.01f);
        }
    }

    TEST_CASE ("VolumeAndPan: multichannel input is passed through unchanged")
    {
        // Wrap a VolumeAndPanPlugin inside a 4-channel Rack so the plugin graph
        // actually exercises N>2 channels (the master bus is stereo, so we can't
        // test this via a normal render).
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        REQUIRE (volPan != nullptr);

        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        REQUIRE (rackType != nullptr);

        rackType->addInput (-1, "Input 3");
        rackType->addInput (-1, "Input 4");
        rackType->addOutput (-1, "Output 3");
        rackType->addOutput (-1, "Output 4");

        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());
        REQUIRE (rackInstance != nullptr);
        rackInstance->setNumInputChannels (4);
        rackInstance->setNumOutputChannels (4);

        // Through-rack the plugin should still report pass-through for 4 channels.
        CHECK (volPan->getNumOutputChannelsGivenInputs (4) == 4);
        CHECK (volPan->getBusses().inputs.front().getNumChannels() == 2);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
