/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

#if GRAPH_UNIT_TESTS_EDITNODE

using namespace tracktion_graph;

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
        tracktion_graph::test_utilities::TestSetup ts;
        ts.sampleRate = 44100.0;
        ts.blockSize = 256;
        
        runRackRendering (ts, 3.0, 2, true);
        runRackRendering (ts, 3.0, 2, false);
    }

private:
    //==============================================================================
    //==============================================================================
    void runRackRendering (test_utilities::TestSetup ts,
                           double durationInSeconds,
                           int numChannels,
                           bool isMultiThreaded)
    {
        using namespace tracktion_graph;
        using namespace test_utilities;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (isMultiThreaded ? ", MT" : ", ST");
        
        tracktion_graph::PlayHead playHead;
        tracktion_graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };

        {
            auto sinFile = tracktion_graph::test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, durationInSeconds, 2, 220.0f);

            auto edit = Edit::createSingleTrackEdit (engine);
            edit->ensureNumberOfAudioTracks (1);
            auto track = getAudioTracks (*edit)[0];

            track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { 0.0, durationInSeconds } }, false);
            
            Plugin::Array plugins;
            plugins.add (track->getVolumePlugin());
            auto rack = RackType::createTypeToWrapPlugins (plugins, *edit);
            auto rackInstance = dynamic_cast<RackInstance*> (track->pluginList.insertPlugin (RackInstance::create (*rack), 0).get());

            beginTest ("Basic Rack Creation: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize),
                                                              ts, numChannels, durationInSeconds, false);
                
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
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize),
                                                              ts, numChannels, durationInSeconds, false);
                
                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);
                
                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }
    }
    
    //==============================================================================
    //==============================================================================
    static std::unique_ptr<tracktion_graph::Node> createNode (Edit& edit, ProcessState& processState,
                                                              double sampleRate, int blockSize)
    {
        CreateNodeParams params { processState };
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true; // Required for audio files to be read
        return createNodeForEdit (edit, params);
    }
};

static EditNodeBuilderTests editNodeBuilderTests;

#endif

} // namespace tracktion_engine
