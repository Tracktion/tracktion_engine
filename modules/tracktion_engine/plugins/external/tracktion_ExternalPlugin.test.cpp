/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include "../../playback/graph/tracktion_EditNodeBuilder.h"
#include "../../playback/graph/tracktion_PluginNode.h"
#include "../../playback/graph/tracktion_TracktionNodePlayer.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_EXTERNALPLUGIN

//==============================================================================
/** A minimal AudioPluginInstance with sidechain input and output buses for testing.
    Stereo main I/O + mono sidechain input + mono sidechain output.
    Total: 3 input channels, 3 output channels. Main bus: 2 in, 2 out.
*/
class SidechainTestPlugin  : public juce::AudioPluginInstance
{
public:
    SidechainTestPlugin()
        : juce::AudioPluginInstance (BusesProperties()
                                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                                        .withInput  ("Sidechain", juce::AudioChannelSet::mono(),   true)
                                        .withOutput ("SC Monitor", juce::AudioChannelSet::mono(),  true))
    {
    }

    void fillInPluginDescription (juce::PluginDescription& d) const override
    {
        d.name = "SidechainTestPlugin";
        d.pluginFormatName = "Test";
        d.fileOrIdentifier = "SidechainTestPlugin";
        d.descriptiveName = "Test plugin with sidechain";
        d.numInputChannels = getTotalNumInputChannels();
        d.numOutputChannels = getTotalNumOutputChannels();
        d.isInstrument = false;
    }

    const juce::String getName() const override { return "SidechainTestPlugin"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    using juce::AudioProcessor::processBlock;

    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override
    {
        // Recorded so a test can check which signal arrived on which input channel.
        // Only read these once processing has finished
        numInputChannelsSeen = std::max (numInputChannelsSeen, b.getNumChannels());

        for (int ch = 0; ch < std::min (b.getNumChannels(), maxRecordedChannels); ++ch)
            inputPeaks[ch] = std::max (inputPeaks[ch], b.getMagnitude (ch, 0, b.getNumSamples()));
    }

    static constexpr int maxRecordedChannels = 8;
    float inputPeaks[maxRecordedChannels] = {};
    int numInputChannelsSeen = 0;
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ExternalPlugin: sidechain does not inflate track channels")
    {
        // Verify the test plugin has the expected bus layout
        SidechainTestPlugin testProc;

        // Total channels include sidechain buses
        CHECK_EQ (testProc.getTotalNumInputChannels(), 3);   // stereo main + mono sidechain
        CHECK_EQ (testProc.getTotalNumOutputChannels(), 3);  // stereo main + mono SC monitor

        // Main bus channels exclude sidechain
        CHECK_EQ (testProc.getMainBusNumInputChannels(), 2);
        CHECK_EQ (testProc.getMainBusNumOutputChannels(), 2);

        // Set up the engine with a custom createPluginInstance callback
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getPluginManager();
        auto prevCallback = pm.createPluginInstance;

        pm.createPluginInstance =
            [&] (const juce::PluginDescription& d, double, int, juce::String&)
            -> std::unique_ptr<juce::AudioPluginInstance>
            {
                if (d.name == "SidechainTestPlugin")
                    return std::make_unique<SidechainTestPlugin>();

                return nullptr;
            };

        // Register the test plugin in the known plugin list so ExternalPlugin can find it
        auto pluginDesc = testProc.getPluginDescription();
        pm.knownPluginList.addType (pluginDesc);

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        // Create ExternalPlugin wrapping our test processor
        auto pluginState = ExternalPlugin::create (engine, pluginDesc);
        auto pluginRef = track->pluginList.insertPlugin (pluginState, 0);
        auto externalPlugin = dynamic_cast<ExternalPlugin*> (pluginRef.get());
        REQUIRE (externalPlugin != nullptr);

        // Fully initialise so the AudioPluginInstance is created
        externalPlugin->initialiseFully();
        REQUIRE (externalPlugin->getAudioPluginInstance() != nullptr);

        SUBCASE ("getNumOutputChannelsGivenInputs returns main bus outputs only")
        {
            // Should return 2 (main bus stereo), not 3 (total including SC monitor)
            CHECK_EQ (externalPlugin->getNumOutputChannelsGivenInputs (2), 2);
        }

        SUBCASE ("getBusses() reports all input buses including sidechain")
        {
            auto busses = externalPlugin->getBusses();
            CHECK_EQ (busses.inputs.size(), 2u);                              // main + sidechain
            CHECK_EQ (busses.inputs.front().getNumChannels(), 2);             // main stereo
        }

        SUBCASE ("getBusses() reports all output buses including SC monitor")
        {
            auto busses = externalPlugin->getBusses();
            CHECK_EQ (busses.outputs.size(), 2u);                             // main + SC monitor
            CHECK_EQ (busses.outputs.front().getNumChannels(), 2);            // main stereo
        }

        SUBCASE ("canSidechain still detects the sidechain bus")
        {
            CHECK (externalPlugin->canSidechain());
        }

        // Restore previous state
        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }

    //==============================================================================
    TEST_CASE ("PluginNode: sidechain channels do not propagate to downstream nodes")
    {
        // When a plugin with a sidechain is on a track, downstream nodes (e.g. a
        // level meter inserted after the plugin) must see only the plugin's
        // main-bus output channels — not main + sidechain.
        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getPluginManager();
        auto prevCallback = pm.createPluginInstance;

        SidechainTestPlugin testProc;

        pm.createPluginInstance =
            [&] (const juce::PluginDescription& d, double, int, juce::String&)
            -> std::unique_ptr<juce::AudioPluginInstance>
            {
                if (d.name == "SidechainTestPlugin")
                    return std::make_unique<SidechainTestPlugin>();

                return nullptr;
            };

        auto pluginDesc = testProc.getPluginDescription();
        pm.knownPluginList.addType (pluginDesc);

        auto edit = test_utilities::createTestEdit (engine);
        edit->ensureNumberOfAudioTracks (2);
        auto pluginTrack = getAudioTracks (*edit)[0];
        auto sidechainSourceTrack = getAudioTracks (*edit)[1];

        auto pluginState = ExternalPlugin::create (engine, pluginDesc);
        auto pluginRef = pluginTrack->pluginList.insertPlugin (pluginState, 0);
        auto externalPlugin = dynamic_cast<ExternalPlugin*> (pluginRef.get());
        REQUIRE (externalPlugin != nullptr);

        externalPlugin->initialiseFully();
        REQUIRE (externalPlugin->getAudioPluginInstance() != nullptr);

        // Connect the sidechain source so the plugin reports it has a valid sidechain
        externalPlugin->setSidechainSourceID (sidechainSourceTrack->itemID);
        REQUIRE (externalPlugin->getSidechainSourceID().isValid());

        // Build the playback graph
        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, edit->tempoSequence };
        CreateNodeParams params { processState };
        params.sampleRate = 44100.0;
        params.blockSize = 256;
        params.forRendering = true;

        auto rootNode = createNodeForEdit (*edit, params);
        REQUIRE (rootNode != nullptr);

        // Find the PluginNode wrapping our SidechainTestPlugin, then locate the
        // node directly downstream of it (its parent in the graph). Downstream
        // consumers see that parent's channel count — that is what the bug
        // reported as 3 (main + sidechain) instead of 2 (main only).
        PluginNode* matchingPluginNode = nullptr;
        tracktion::graph::Node* downstreamOfPlugin = nullptr;

        tracktion::graph::visitNodes (*rootNode,
                                      [&] (tracktion::graph::Node& n)
                                      {
                                          if (auto pn = dynamic_cast<PluginNode*> (&n))
                                              if (&pn->getPlugin() == externalPlugin)
                                                  matchingPluginNode = pn;
                                      },
                                      true);

        REQUIRE (matchingPluginNode != nullptr);

        tracktion::graph::visitNodes (*rootNode,
                                      [&] (tracktion::graph::Node& n)
                                      {
                                          for (auto* in : n.getDirectInputNodes())
                                              if (in == matchingPluginNode)
                                                  downstreamOfPlugin = &n;
                                      },
                                      true);

        REQUIRE (downstreamOfPlugin != nullptr);

        // The plugin's main output bus is stereo; the sidechain monitor bus
        // (mono) must NOT appear in the channel count downstream.
        CHECK_EQ (downstreamOfPlugin->getNodeProperties().numberOfChannels, 2);

        // Restore previous state
        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }

    //==============================================================================
    TEST_CASE ("PluginNode: a mono track feeding a sidechained plugin doesn't leak into the sidechain")
    {
        // A mono track into a stereo-main plugin needs a mono->stereo conversion, which
        // duplicates the source channel across both destinations. That has to happen
        // before the sidechain is summed in - run afterwards it converts the combined
        // buffer instead, overwriting the sidechain channel with a copy of the track.
        // Both orderings produce the same channel count, so only the content shows it.
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr double durationInSeconds = 0.5;

        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getPluginManager();
        auto prevCallback = pm.createPluginInstance;

        SidechainTestPlugin testProc;
        SidechainTestPlugin* createdInstance = nullptr;

        pm.createPluginInstance =
            [&] (const juce::PluginDescription& d, double, int, juce::String&)
            -> std::unique_ptr<juce::AudioPluginInstance>
            {
                if (d.name != "SidechainTestPlugin")
                    return nullptr;

                auto instance = std::make_unique<SidechainTestPlugin>();
                createdInstance = instance.get();

                return instance;
            };

        auto pluginDesc = testProc.getPluginDescription();
        pm.knownPluginList.addType (pluginDesc);

        auto edit = test_utilities::createTestEdit (engine, 2);
        auto pluginTrack = getAudioTracks (*edit)[0];
        auto sidechainSourceTrack = getAudioTracks (*edit)[1];

        // A track's channel count comes from its clips, so a mono clip makes the track
        // node mono and forces the conversion this test is about
        auto monoFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationInSeconds, 1, 220.0f);
        insertWaveClip (*pluginTrack, {}, monoFile->getFile(),
                        { .time = { 0_tp, TimePosition::fromSeconds (durationInSeconds) } },
                        DeleteExistingClips::no);
        REQUIRE_EQ (pluginTrack->getChannelConfiguration().getNumChannels(), 1);

        // The sidechain source needs audio of its own: a silent one can't tell a
        // preserved sidechain channel apart from one the conversion dropped
        auto sidechainFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationInSeconds, 2, 440.0f);
        insertWaveClip (*sidechainSourceTrack, {}, sidechainFile->getFile(),
                        { .time = { 0_tp, TimePosition::fromSeconds (durationInSeconds) } },
                        DeleteExistingClips::no);

        auto pluginState = ExternalPlugin::create (engine, pluginDesc);
        auto pluginRef = pluginTrack->pluginList.insertPlugin (pluginState, 0);
        auto externalPlugin = dynamic_cast<ExternalPlugin*> (pluginRef.get());
        REQUIRE (externalPlugin != nullptr);

        externalPlugin->initialiseFully();
        REQUIRE (externalPlugin->getAudioPluginInstance() != nullptr);
        REQUIRE (createdInstance != nullptr);

        // setSidechainSourceID on its own leaves the plugin with no wires, which makes
        // createSidechainInputNodeForPlugin bail out. Going through the by-name setter
        // is what the sidechain editor does, and it fills in the default routing
        externalPlugin->setSidechainSourceByName ("2. " + sidechainSourceTrack->getName());
        REQUIRE (externalPlugin->getSidechainSourceID().isValid());
        REQUIRE (externalPlugin->getNumWires() > 0);

        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState, edit->tempoSequence };
        CreateNodeParams params { processState };
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true;

        auto rootNode = createNodeForEdit (*edit, params);
        REQUIRE (rootNode != nullptr);

        graph::test_utilities::TestSetup ts;
        ts.sampleRate = sampleRate;
        ts.blockSize = blockSize;

        graph::test_utilities::TestProcess<TracktionNodePlayer> testContext (
            std::make_unique<TracktionNodePlayer> (std::move (rootNode), processState,
                                                   sampleRate, blockSize,
                                                   tracktion::graph::getPoolCreatorFunction (tracktion::graph::ThreadPoolStrategy::realTime)),
            ts, 2, durationInSeconds, true);

        testContext.getNodePlayer().setNumThreads (0);
        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});
        testContext.processAll();

        // The default routing wires the track to plugin channels 0 and 1 and the
        // sidechain source to channel 2
        REQUIRE_EQ (createdInstance->numInputChannelsSeen, 3);

        // The mono track, duplicated across the stereo main bus
        CHECK (createdInstance->inputPeaks[0] > 0.1f);
        CHECK (createdInstance->inputPeaks[1] > 0.1f);

        // The sidechain source. Converting after the sidechain is summed in remaps only
        // the track's own channel onto the main bus and drops this one, so it reads 0
        CHECK (createdInstance->inputPeaks[2] > 0.1f);

        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }

    //==============================================================================
    TEST_CASE ("ExternalPlugin: restoring layout with more buses than plugin supports")
    {
        // SidechainTestPlugin has fixed bus count (no addBus support).
        // Simulates a saved edit where the plugin previously had extra buses
        // (e.g. plugin version changed, or format changed between VST3/AU).
        SidechainTestPlugin testProc;

        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getPluginManager();
        auto prevCallback = pm.createPluginInstance;

        pm.createPluginInstance =
            [&] (const juce::PluginDescription& d, double, int, juce::String&)
            -> std::unique_ptr<juce::AudioPluginInstance>
            {
                if (d.name == "SidechainTestPlugin")
                    return std::make_unique<SidechainTestPlugin>();

                return nullptr;
            };

        auto pluginDesc = testProc.getPluginDescription();
        pm.knownPluginList.addType (pluginDesc);

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto pluginState = ExternalPlugin::create (engine, pluginDesc);

        // Fabricate a saved layout with 4 input buses — the plugin only has 2.
        // This happens when an edit was saved with a plugin version that supported
        // more buses, or when the plugin format changed.
        juce::ValueTree layoutTree (IDs::LAYOUT);

        juce::ValueTree inputs (IDs::INPUTS);

        for (int i = 0; i < 4; ++i)
            inputs.addChild (createValueTree (IDs::BUS, IDs::index, i, IDs::layout, "stereo"), -1, nullptr);

        juce::ValueTree outputs (IDs::OUTPUTS);

        for (int i = 0; i < 4; ++i)
            outputs.addChild (createValueTree (IDs::BUS, IDs::index, i, IDs::layout, "stereo"), -1, nullptr);

        layoutTree.addChild (inputs, -1, nullptr);
        layoutTree.addChild (outputs, -1, nullptr);

        juce::MemoryBlock mb;
        {
            juce::MemoryOutputStream os (mb, false);
            layoutTree.writeToStream (os);
        }

        pluginState.setProperty (IDs::layout, mb, nullptr);

        // Without the fix, readBusLayout populates targetBuses for bus indices
        // that addBus failed to create, causing a size mismatch in setBusesLayout.
        auto pluginRef = track->pluginList.insertPlugin (pluginState, 0);
        auto externalPlugin = dynamic_cast<ExternalPlugin*> (pluginRef.get());
        REQUIRE (externalPlugin != nullptr);

        externalPlugin->initialiseFully();
        CHECK (externalPlugin->getAudioPluginInstance() != nullptr);

        // Plugin should keep its original bus count since addBus isn't supported
        if (auto pi = externalPlugin->getAudioPluginInstance())
        {
            CHECK_EQ (pi->getBusCount (true), 2);
            CHECK_EQ (pi->getBusCount (false), 2);
        }

        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }

    TEST_CASE ("ExternalPlugin: restoring layout with fewer buses than plugin default")
    {
        // SidechainTestPlugin defaults to 2 input + 2 output buses.
        // Saved state only lists 1 of each — the plugin can't remove the extras
        // because canRemoveBus returns false by default.
        SidechainTestPlugin testProc;
        CHECK_EQ (testProc.getBusCount (true), 2);
        CHECK_EQ (testProc.getBusCount (false), 2);

        auto& engine = *Engine::getEngines()[0];
        auto& pm = engine.getPluginManager();
        auto prevCallback = pm.createPluginInstance;

        pm.createPluginInstance =
            [&] (const juce::PluginDescription& d, double, int, juce::String&)
            -> std::unique_ptr<juce::AudioPluginInstance>
            {
                if (d.name == "SidechainTestPlugin")
                    return std::make_unique<SidechainTestPlugin>();

                return nullptr;
            };

        auto pluginDesc = testProc.getPluginDescription();
        pm.knownPluginList.addType (pluginDesc);

        auto edit = test_utilities::createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        auto pluginState = ExternalPlugin::create (engine, pluginDesc);

        // Saved layout with only 1 input + 1 output bus (plugin defaults to 2 each)
        juce::ValueTree layoutTree (IDs::LAYOUT);

        juce::ValueTree inputs (IDs::INPUTS);
        inputs.addChild (createValueTree (IDs::BUS, IDs::index, 0, IDs::layout, "stereo"), -1, nullptr);

        juce::ValueTree outputs (IDs::OUTPUTS);
        outputs.addChild (createValueTree (IDs::BUS, IDs::index, 0, IDs::layout, "stereo"), -1, nullptr);

        layoutTree.addChild (inputs, -1, nullptr);
        layoutTree.addChild (outputs, -1, nullptr);

        juce::MemoryBlock mb;
        {
            juce::MemoryOutputStream os (mb, false);
            layoutTree.writeToStream (os);
        }

        pluginState.setProperty (IDs::layout, mb, nullptr);

        // Without the fix, targetBuses has 1 entry but plugin has 2 buses → assertion
        auto pluginRef = track->pluginList.insertPlugin (pluginState, 0);
        auto externalPlugin = dynamic_cast<ExternalPlugin*> (pluginRef.get());
        REQUIRE (externalPlugin != nullptr);

        externalPlugin->initialiseFully();
        CHECK (externalPlugin->getAudioPluginInstance() != nullptr);

        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }
}

#endif // ENGINE_UNIT_TESTS_EXTERNALPLUGIN

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
