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
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
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

        SUBCASE ("getMainBusInputChannelConfiguration returns main bus inputs only")
        {
            // Should return 2 (main bus stereo), not 3 (total including sidechain)
            CHECK_EQ (externalPlugin->getMainBusInputChannelConfiguration().getNumChannels(), 2);
        }

        SUBCASE ("getMainBusOutputChannelConfiguration returns main bus outputs only")
        {
            // Should return 2 (main bus stereo), not 3 (total including SC monitor)
            CHECK_EQ (externalPlugin->getMainBusOutputChannelConfiguration().getNumChannels(), 2);
        }

        SUBCASE ("canSidechain still detects the sidechain bus")
        {
            CHECK (externalPlugin->canSidechain());
        }

        // Restore previous state
        pm.knownPluginList.removeType (pluginDesc);
        pm.createPluginInstance = prevCallback;
    }
}

#endif // ENGINE_UNIT_TESTS_EXTERNALPLUGIN

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
