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

#if TRACKTION_GRAPH_PERFORMANCE_TESTS

using namespace tracktion_graph;

//==============================================================================
//==============================================================================
class PerformanceTests : public juce::UnitTest
{
public:
    PerformanceTests()
        : juce::UnitTest ("Performance Tests", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        tracktion_graph::test_utilities::TestSetup ts;
        ts.sampleRate = 44100.0;
        ts.blockSize = 512;
        
        runWaveRendering (ts, 30.0, 2, 20, 12, true);
        runWaveRendering (ts, 30.0, 2, 20, 12, false);
    }

private:
    //==============================================================================
    //==============================================================================
    void runWaveRendering (test_utilities::TestSetup ts,
                           double durationInSeconds,
                           int numChannels,
                           int numTracks,
                           int numFilesPerTrack,
                           bool useSingleFile)
    {
        // Create Edit with 20 tracks
        // Create 12 5s files per track
        // Render the whole thing
        using namespace tracktion_graph;
        using namespace test_utilities;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];
        const auto description = test_utilities::getDescription (ts)
                                    + juce::String (useSingleFile ? ", single file" : ", multiple files");
        
        tracktion_graph::PlayHead playHead;
        tracktion_graph::PlayHeadState playHeadState { playHead };
        
        //===
        beginTest ("Wave Edit - creation: " + description);
        const double durationOfFile = durationInSeconds / numFilesPerTrack;
        auto context = createTestContext (engine, numTracks, numFilesPerTrack, durationOfFile, ts.sampleRate, ts.random, useSingleFile);
        expect (context.edit != nullptr);
        expect (useSingleFile || (context.files.size() == size_t (numTracks * numFilesPerTrack)));
        expectWithinAbsoluteError (context.edit->getLength(), durationInSeconds, 0.01);

        //===
        beginTest ("Wave - building: " + description);
        auto node = createNode (*context.edit, playHeadState, ts.sampleRate, ts.blockSize);
        expect (node != nullptr);

        //===
        beginTest ("Wave - preparing: " + description);
        TestProcess<NodePlayer> testContext (std::make_unique<NodePlayer> (std::move (node), &playHeadState),
                                             ts, numChannels, context.edit->getLength(), false);
        testContext.setPlayHead (&playHeadState.playHead);
        playHeadState.playHead.playSyncedToRange ({});
        expect (true);

        beginTest ("Wave - memory use: " + description);
        const auto nodes = tracktion_graph::getNodes (testContext.getNode(), tracktion_graph::VertexOrdering::postordering);
        std::cout << "Num nodes: " << nodes.size() << "\n";
        std::cout << juce::File::descriptionOfSizeInBytes ((int64_t) test_utilities::getMemoryUsage (nodes, ts.blockSize)) << "\n";
        expect (true);

        beginTest ("Wave - rendering: " + description);
        auto result = testContext.processAll();
        expect (true);

        beginTest ("Wave - destroying: " + description);
        result.reset();
        expect (true);
        
        beginTest ("Wave - cleanup: " + description);
        // This is deliberately empty as RAII will take care of cleanup
        expect (true);
    }
    
    //==============================================================================
    //==============================================================================
    struct EditTestContext
    {
        std::unique_ptr<Edit> edit;
        std::vector<std::unique_ptr<juce::TemporaryFile>> files;
    };
    
    static EditTestContext createTestContext (Engine& engine, int numTracks, int numFilesPerTrack, double durationOfFile, double sampleRate, juce::Random& r, bool useSingleFile)
    {
        auto edit = Edit::createSingleTrackEdit (engine);
        std::vector<std::unique_ptr<juce::TemporaryFile>> files;

        edit->ensureNumberOfAudioTracks (numTracks);
        
        if (useSingleFile)
            files.push_back (tracktion_graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationOfFile, 2, 220.0f));

        for (auto t : getAudioTracks (*edit))
        {
            for (int i = 0; i < numFilesPerTrack; ++i)
            {
                if (! useSingleFile)
                {
                    const float frequency = (float) r.nextInt ({ 110, 880 });
                    auto file = tracktion_graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, durationOfFile, 2, frequency);
                    files.push_back (std::move (file));
                }

                auto& file = files.back();
                const auto timeRange = EditTimeRange::withStartAndLength (i * durationOfFile, durationOfFile);
                auto waveClip = t->insertWaveClip (file->getFile().getFileName(), file->getFile(),
                                                   {{ timeRange }}, false);
                waveClip->setGainDB (gainToDb (1.0f / numTracks));
            }
        }
                
        return { std::move (edit), std::move (files) };
    }
    
    static std::unique_ptr<tracktion_graph::Node> createNode (Edit& edit, tracktion_graph::PlayHeadState& playHeadState,
                                                              double sampleRate, int blockSize)
    {
        CreateNodeParams params;
        params.sampleRate = sampleRate;
        params.blockSize = blockSize;
        params.forRendering = true; // Required for audio files to be read
        return createNodeForEdit (edit, playHeadState, params).node;
    }
};

static PerformanceTests performanceTests;

#endif

} // namespace tracktion_engine
