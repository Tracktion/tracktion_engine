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

#if TRACKTION_UNIT_TESTS

using namespace tracktion_graph;

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
            runBasicTests (ts);
    }

private:
    //==============================================================================
    //==============================================================================
    void runBasicTests (test_utilities::TestSetup ts)
    {
        using namespace tracktion_graph;
        auto& engine = *tracktion_engine::Engine::getEngines()[0];

        const double fileLengthSeconds = 5.0;
        auto sinFile = test_utilities::getSinFile<juce::WavAudioFormat> (ts.sampleRate, fileLengthSeconds);
        AudioFile sinAudioFile (engine, sinFile->getFile());
        
        tracktion_graph::PlayHead playHead;
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
                                            playHead,
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 6.0);
            
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, fileLengthSeconds }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ fileLengthSeconds, fileLengthSeconds + 1.0 }, ts.sampleRate), 0.0f, 0.0f);
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
                                            playHead,
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 6.0);

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
                                            playHead,
                                            true);
            
            // Process node writing to a wave file and ensure level is 1.0 for 5s, silent afterwards
            auto testContext = createBasicTestContext (std::move (node), ts, 1, 6.0);

            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 0.0, 1.0 }, ts.sampleRate), 0.0f, 0.0f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 1.0, 4.0 }, ts.sampleRate), 1.0f, 0.707f);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, timeToSample ({ 4.0, 5.0 }, ts.sampleRate), 0.0f, 0.0f);
        }
    }
};

static WaveNodeTests waveNodeTests;

#endif

}
