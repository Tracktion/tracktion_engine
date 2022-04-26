/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
        for (auto ts : tracktion::graph::test_utilities::getTestSetups (*this))
        {
            runBasicTests<WaveNode> ("WaveNode", ts, true);
            runBasicTests<WaveNode> ("WaveNode", ts, false);
            runLoopedTimelineTests<WaveNode> ("WaveNode", ts);
        }

       #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
        logMessage ("WaveNodeRealTime");

        for (auto ts : tracktion::graph::test_utilities::getTestSetups (*this))
        {
            runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, true);
            runBasicTests<WaveNodeRealTime> ("WaveNodeRealTime", ts, false);
            runLoopedTimelineTests<WaveNodeRealTime> ("WaveNodeRealTime", ts);
        }
       #endif
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
    template<typename NodeType>
    void runBasicTests (juce::String nodeTypeName, test_utilities::TestSetup ts, bool playSyncedToRange)
    {
        using namespace tracktion::graph;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 5.0;
        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());
        
        tracktion::graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (toSamples (0.08_tp, ts.sampleRate));
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        if (playSyncedToRange)
            playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
        else
            playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });
        
        beginTest (nodeTypeName + " at time 0s");
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);
            
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, fileLengthSeconds }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ fileLengthSeconds, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        beginTest (nodeTypeName + " at time 0s, dragging");
        {
            // If the user is dragging the playhead doesn't move so the whole buffer will be 0.08s of the start of the clip
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
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

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.4f, 0.282f);
        }

        beginTest (nodeTypeName + " at time 1s - 4s");
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimePosition (4.0s)),
                                            TimeDuration(),
                                            TimeRange(),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }

        beginTest (nodeTypeName + " at time 1s - 4s, loop every 1s");
        {
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimePosition (4.0s)),
                                            TimeDuration(),
                                            TimeRange (0.0s, TimePosition (1.0s)),
                                            LiveClipLevel(),
                                            1.0,
                                            juce::AudioChannelSet::canonicalChannelSet (sinAudioFile.getNumChannels()),
                                            juce::AudioChannelSet::canonicalChannelSet (1),
                                            processState,
                                            EditItemID(),
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 1, 6.0);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }
    }

    template<typename NodeType>
    void runLoopedTimelineTests (juce::String nodeTypeName, test_utilities::TestSetup ts)
    {
        using namespace tracktion::graph;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 1.0;
        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());
        
        tracktion::graph::PlayHead playHead;
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        beginTest (nodeTypeName + " Loop 0s-1s");
        {
            // This test loops a 1s sin file so the output should be 5s of sin data
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (0.0s, TimeDuration::fromSeconds (fileLengthSeconds)),
                                            TimeDuration(),
                                            TimeRange(),
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
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }

        beginTest (nodeTypeName + " Loop 1s-2s");
        {
            // This test loops a 1s sin file so the output should be 5s of sin data
            auto node = makeNode<NodeType> (sinAudioFile,
                                            TimeRange (1.0s, TimeDuration::fromSeconds (fileLengthSeconds) + 1.0s),
                                            TimeDuration(),
                                            TimeRange(),
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
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, graph::timeToSample ({ 0.0, 5.0 }, ts.sampleRate), 1.0f, 0.707f);
        }
    }
};

static WaveNodeTests waveNodeTests;

#endif

}} // namespace tracktion { inline namespace engine
