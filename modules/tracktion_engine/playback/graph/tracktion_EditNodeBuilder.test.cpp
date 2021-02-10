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
        
        runTrackDestinationRendering (ts, 3.0, 2, true);
        runTrackDestinationRendering (ts, 3.0, 2, false);

        runAuxSend (ts, 3.0, 2, true);
        runAuxSend (ts, 3.0, 2, false);

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
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
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
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                              ts, numChannels, durationInSeconds, false);
                
                if (! isMultiThreaded)
                    testContext.getNodePlayer().setNumThreads (0);
                
                testContext.setPlayHead (&playHeadState.playHead);
                playHeadState.playHead.playSyncedToRange ({});
                testContext.processAll();
            }
        }
    }

    /** Has two tracks, one with a sin clip and aux send which is muted, one with an
        aux return which is unmuted. The second track should be audible even though
        the aux source is muted.
    */
    void runAuxSend (test_utilities::TestSetup ts,
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
            edit->ensureNumberOfAudioTracks (2);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
            
            {
                auto auxSourceTrack = getAudioTracks (*edit)[0];
                auxSourceTrack->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { 0.0, durationInSeconds } }, false);
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
                                                              ts, numChannels, durationInSeconds, true);
                
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

    /** Has two tracks with a sin clip at 0.5 magnitude on each track, both sent to a third track as their destination.
        Once rendered, the resulting file should have a magnitude of 1.0.
    */
    void runTrackDestinationRendering (test_utilities::TestSetup ts,
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
            edit->ensureNumberOfAudioTracks (3);
            edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
            auto destTrack = getAudioTracks (*edit)[2];
            
            for (int trackIndex : { 0, 1 })
            {
                auto track = getAudioTracks (*edit)[trackIndex];
                track->insertWaveClip ({}, sinFile->getFile(), ClipPosition { { 0.0, durationInSeconds } }, false);
                track->getVolumePlugin()->setVolumeDb (gainToDb (0.5f));
                track->getOutput().setOutputToTrack (destTrack);
            }
            
            beginTest ("Track Destination Rendering: " + description);
            {
                auto node = createNode (*edit, processState, ts.sampleRate, ts.blockSize);
                TestProcess<TracktionNodePlayer> testContext (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                     getPoolCreatorFunction (ThreadPoolStrategy::hybrid)),
                                                              ts, numChannels, durationInSeconds, true);
                
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
