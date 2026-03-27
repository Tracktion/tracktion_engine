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

static PatchBayPlugin* insertPatchBay (Track& track, int index = 0)
{
    auto plugin = track.edit.getPluginCache().createNewPlugin (createValueTree (IDs::PLUGIN, IDs::type, PatchBayPlugin::xmlTypeName));

    if (plugin != nullptr)
        track.pluginList.insertPlugin (plugin, index, nullptr);

    return dynamic_cast<PatchBayPlugin*> (plugin.get());
}

#if ENGINE_UNIT_TESTS_PATCHBAY
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("PatchBay: getNumOutputChannelsGivenInputs adapts to input count")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto pb = insertPatchBay (*track);
        REQUIRE (pb != nullptr);

        // New plugin has default 2 wires: ch0->ch0, ch1->ch1
        CHECK (pb->getNumWires() == 2);

        // Mono input should give mono output (ch1 wire has no valid source)
        CHECK (pb->getNumOutputChannelsGivenInputs (1) == 1);

        // Stereo input should give stereo output
        CHECK (pb->getNumOutputChannelsGivenInputs (2) == 2);

        // Quad input should pass through (no wires beyond ch1)
        CHECK (pb->getNumOutputChannelsGivenInputs (4) == 4);
    }

    TEST_CASE ("PatchBay: explicit multichannel wires expand output")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto pb = insertPatchBay (*track);
        REQUIRE (pb != nullptr);

        // Add a wire from ch0 -> ch3
        pb->makeConnection (0, 3, 0.0f, nullptr);

        // Mono input: ch0->ch0 and ch0->ch3 are both valid, so need 4 output channels
        CHECK (pb->getNumOutputChannelsGivenInputs (1) == 4);

        // Stereo input: ch0->ch0, ch1->ch1, ch0->ch3 all valid, still need 4
        CHECK (pb->getNumOutputChannelsGivenInputs (2) == 4);
    }

    TEST_CASE ("PatchBay: mono audio passthrough")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto fileLength = 2_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds(), 1, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        // Render without PatchBay to get baseline channel count
        auto baselineRender = test_utilities::renderToAudioBuffer (*edit);
        auto baselineChannels = baselineRender.buffer.getNumChannels();

        insertPatchBay (*track);

        auto render = test_utilities::renderToAudioBuffer (*edit);

        // PatchBay should not increase the channel count
        CHECK (render.buffer.getNumChannels() <= baselineChannels);

        // Audio should pass through
        CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.5f);
    }

    TEST_CASE ("PatchBay: getChannelNames reports mono inputs for mono track")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 1, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_td } },
                        DeleteExistingClips::no);

        auto pb = insertPatchBay (*track);
        REQUIRE (pb != nullptr);

        juce::StringArray ins, outs;
        pb->getChannelNames (&ins, &outs);

        // Input is mono (from clip)
        CHECK (ins.size() == 1);

        // Output may be >= 1 depending on downstream (output device is typically stereo)
        CHECK (outs.size() >= 1);
    }

    TEST_CASE ("PatchBay: getChannelNames reports stereo for stereo track")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_td } },
                        DeleteExistingClips::no);

        auto pb = insertPatchBay (*track);
        REQUIRE (pb != nullptr);

        juce::StringArray ins, outs;
        pb->getChannelNames (&ins, &outs);

        CHECK (ins.size() == 2);
        CHECK (outs.size() == 2);
    }

    TEST_CASE ("PatchBay: getChannelNames reports different in/out when downstream expects more channels")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_td } },
                        DeleteExistingClips::no);

        // Insert PatchBay first
        auto pb = insertPatchBay (*track, 0);
        REQUIRE (pb != nullptr);

        // Insert a RackInstance after PatchBay with 4 input channels
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);

        rackType->addInput (-1, "Input 3");
        rackType->addInput (-1, "Input 4");

        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 1).get());
        REQUIRE (rackInstance != nullptr);
        rackInstance->setNumInputChannels (4);

        juce::StringArray ins, outs;
        pb->getChannelNames (&ins, &outs);

        // Upstream is stereo
        CHECK (ins.size() == 2);

        // Downstream rack expects 4 channels
        CHECK (outs.size() == 4);
    }

    TEST_CASE ("PatchBay: getChannelNames updates when moved in chain")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 1.0, 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, 1_td } },
                        DeleteExistingClips::no);

        // Insert a 4-input RackInstance at position 0
        VolumeAndPanPlugin::Ptr volPan (dynamic_cast<VolumeAndPanPlugin*> (edit->getPluginCache().getOrCreatePluginFor (VolumeAndPanPlugin::create()).get()));
        Plugin::Array plugins;
        plugins.add (volPan);
        auto rackType = RackType::createTypeToWrapPlugins (plugins, *edit);
        rackType->addInput (-1, "Input 3");
        rackType->addInput (-1, "Input 4");
        auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rackType), 0).get());
        REQUIRE (rackInstance != nullptr);
        rackInstance->setNumInputChannels (4);
        rackInstance->setNumOutputChannels (4);

        // Insert PatchBay BEFORE the rack (position 0)
        auto pb = insertPatchBay (*track, 0);
        REQUIRE (pb != nullptr);

        {
            juce::StringArray ins, outs;
            pb->getChannelNames (&ins, &outs);
            CHECK (ins.size() == 2);   // upstream is stereo clip
            CHECK (outs.size() == 4);  // downstream rack expects 4
        }

        struct ChangeListener : public SelectableListener
        {
            bool notified = false;
            void selectableObjectChanged (Selectable*) override { notified = true; }
            void selectableObjectAboutToBeDeleted (Selectable*) override {}
        } changeListener;

        pb->addSelectableListener (&changeListener);

        // Move PatchBay AFTER the rack by moving its ValueTree
        auto parentTree = pb->state.getParent();
        int pbIndex = parentTree.indexOf (pb->state);
        int rackIndex = parentTree.indexOf (rackInstance->state);
        parentTree.moveChild (pbIndex, rackIndex, nullptr);

        // Pump the message manager so async change notifications fire
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

        CHECK (changeListener.notified);

        pb->removeSelectableListener (&changeListener);

        {
            juce::StringArray ins, outs;
            pb->getChannelNames (&ins, &outs);
            CHECK (ins.size() == 4);   // now after 4-output rack, input is 4
        }
    }

    TEST_CASE ("PatchBay: legacy stereo wires still work")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto fileLength = 2_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds(), 2, 220.0f);

        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        insertPatchBay (*track);

        auto render = test_utilities::renderToAudioBuffer (*edit);

        // Stereo clip should remain stereo
        CHECK (render.buffer.getNumChannels() == 2);

        // Both channels should have audio
        CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 0) > 0.5f);
        CHECK (test_utilities::getRMSLevel (render, { 0_tp, fileLength }, 1) > 0.5f);
    }
}
#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
