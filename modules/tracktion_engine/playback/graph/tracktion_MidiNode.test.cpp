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

#if GRAPH_UNIT_TESTS_MIDINODE

//==============================================================================
//==============================================================================
class MidiNodeTests : public juce::UnitTest
{
public:
    MidiNodeTests()
        : juce::UnitTest ("MidiNode", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto setup : tracktion::graph::test_utilities::getTestSetups (*this))
        {
            logMessage (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N"));

            runMidiTests (setup, true);
            runMidiTests (setup, false);
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
        testProcess.setPlayHead (&processState.playHeadState.playHead);
        
        return testProcess.processAll();
    }

    //==============================================================================
    //==============================================================================
    void runMidiTests (tracktion::graph::test_utilities::TestSetup ts, bool playSyncedToRange)
    {
        using namespace tracktion::graph;
        
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        
        // Avoid creating events at the end of the duration as they'll get lost after latency is applied
        const auto masterSequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, ts.random);
        
        tracktion::graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (timeToSample (0.08, ts.sampleRate));
        tracktion::graph::PlayHeadState playHeadState (playHead);
        ProcessState processState (playHeadState);

        if (playSyncedToRange)
            playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
        else
            playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });        
        
        beginTest ("Basic MIDI");
        {
            auto sequence = masterSequence;
            auto node = std::make_unique<tracktion::engine::MidiNode> (std::vector<juce::MidiMessageSequence> ({ sequence }),
                                                                       MidiList::TimeBase::seconds,
                                                                       juce::Range<int>::withStartAndLength (1, 1),
                                                                       false,
                                                                       juce::Range<double> (0.0, duration),
                                                                       LiveClipLevel(),
                                                                       processState,
                                                                       EditItemID());

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 0, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Offset MIDI");
        {
            const auto editTimeRange = juce::Range<double>::withStartAndLength (1.0, duration);
            auto node = std::make_unique<tracktion::engine::MidiNode> (std::vector<juce::MidiMessageSequence> ({ masterSequence }),
                                                                       MidiList::TimeBase::seconds,
                                                                       juce::Range<int>::withStartAndLength (1, 1),
                                                                       false,
                                                                       editTimeRange,
                                                                       LiveClipLevel(),
                                                                       processState,
                                                                       EditItemID());

            auto testContext = createTracktionTestContext (processState, std::move (node), ts, 0, editTimeRange.getEnd());

            juce::MidiMessageSequence expectedSequence;
            expectedSequence.addSequence (masterSequence,
                                          1.0,
                                          editTimeRange.getStart(),
                                          editTimeRange.getEnd());

            expectGreaterThan (expectedSequence.getNumEvents(), 0);
            expectEquals (expectedSequence.getNumEvents(), masterSequence.getNumEvents());
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, expectedSequence);
        }
    }
};

static MidiNodeTests midiNodeTests;

#endif //TRACKTION_UNIT_TESTS

}} // namespace tracktion { inline namespace engine
