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

#if GRAPH_UNIT_TESTS_WAVENODE

//==============================================================================
//==============================================================================
class WaveNodeTests : public juce::UnitTest
{
public:
    WaveNodeTests()
        : juce::UnitTest ("WaveNode", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto ts : tracktion_graph::test_utilities::getTestSetups (*this))
        {
            runBasicTests (ts, true);
            runBasicTests (ts, false);
            runLoopedTimelineTests (ts);
        }
    }

private:
    //==============================================================================
    static std::shared_ptr<test_utilities::TestContext> createTracktionTestContext (ProcessState& processState, std::unique_ptr<Node> node,
                                                                                    test_utilities::TestSetup ts, int numChannels, double durationInSeconds)
    {
        test_utilities::TestProcess<TracktionNodePlayer> testProcess (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                             getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                                      ts, numChannels, durationInSeconds, true);
        return testProcess.processAll();
    }
    
    //==============================================================================
    //==============================================================================
    void runBasicTests (test_utilities::TestSetup ts, bool playSyncedToRange)
    {
        using namespace tracktion_graph;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 5.0;
        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());
        
        tracktion_graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (timeToSample (0.08, ts.sampleRate));
        tracktion_graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        if (playSyncedToRange)
            playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
        else
            playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });
        
        beginTest ("WaveNode at time 0s");
        {
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (0.0, fileLengthSeconds),
                                            0.0,
                                            EditTimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);
            
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, fileLengthSeconds }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ fileLengthSeconds, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        beginTest ("WaveNode at time 0s, dragging");
        {
            // If the user is dragging the playhead doesn't move so the whole buffer will be 0.08s of the start of the clip
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (0.0, fileLengthSeconds),
                                            0.0,
                                            EditTimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            playHead.setUserIsDragging (true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            playHead.setUserIsDragging (false);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.4f, 0.282f);
        }

        beginTest ("WaveNode at time 1s - 4s");
        {
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (1.0, 4.0),
                                            0.0,
                                            EditTimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        beginTest ("WaveNode at time 1s - 4s, loop every 1s");
        {
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (1.0, 4.0),
                                            0.0,
                                            EditTimeRange (0.0, 1.0),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }
    }

    void runLoopedTimelineTests (test_utilities::TestSetup ts)
    {
        using namespace tracktion_graph;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 1.0;
        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());
        
        tracktion_graph::PlayHead playHead;
        tracktion_graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        beginTest ("Loop 0s-1s");
        {
            // This test loops a 1s sin file so the output should be 5s of sin data
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (0.0, fileLengthSeconds),
                                            0.0,
                                            EditTimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);

            // Loop playback between 0s & 1s on the timeline
            playHead.play ({ 0, timeToSample (1.0, ts.sampleRate) }, true);

            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }

        beginTest ("Loop 1s-2s");
        {
            // This test loops a 1s sin file so the output should be 5s of sin data
            auto node = makeNode<WaveNode> (sinAudioFile,
                                            EditTimeRange (1.0, 1.0 + fileLengthSeconds),
                                            0.0,
                                            EditTimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);

            // Loop playback between 0s & 1s on the timeline
            playHead.setReferenceSampleRange ({ 0, ts.blockSize });
            playHead.play ({ timeToSample (1.0, ts.sampleRate), timeToSample (2.0, ts.sampleRate) }, true);

            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }
    }
};

static WaveNodeTests waveNodeTests;

#endif

}
