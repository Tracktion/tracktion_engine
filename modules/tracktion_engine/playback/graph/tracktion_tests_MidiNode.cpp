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
class MidiNodeTests : public juce::UnitTest
{
public:
    MidiNodeTests()
        : juce::UnitTest ("MidiNode", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto setup : test_utilities::getTestSetups (*this))
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
    //==============================================================================
    void runMidiTests (test_utilities::TestSetup ts, bool playSyncedToRange)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        
        // Avoid creating events at the end of the duration as they'll get lost after latency is applied
        const auto masterSequence = test_utilities::createRandomMidiMessageSequence (duration - 0.5, ts.random);
        
        tracktion_graph::PlayHead playHead;
        playHead.setScrubbingBlockLength (timeToSample (0.08, ts.sampleRate));
        tracktion_graph::PlayHeadState playHeadState (playHead);
        
        if (playSyncedToRange)
            playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
        else
            playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });        
        
        beginTest ("Basic MIDI");
        {
            auto sequence = masterSequence;
            auto node = std::make_unique<tracktion_engine::MidiNode> (sequence,
                                                                      juce::Range<int>::withStartAndLength (1, 1),
                                                                      false,
                                                                      EditTimeRange (0.0, duration),
                                                                      LiveClipLevel(),
                                                                      playHeadState,
                                                                      EditItemID());
            
            auto testContext = createBasicTestContext (std::move (node), playHeadState, ts, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Offset MIDI");
        {
            auto node = std::make_unique<tracktion_engine::MidiNode> (masterSequence,
                                                                      juce::Range<int>::withStartAndLength (1, 1),
                                                                      false,
                                                                      EditTimeRange (1.0, duration),
                                                                      LiveClipLevel(),
                                                                      playHeadState,
                                                                      EditItemID());
            
            auto testContext = createBasicTestContext (std::move (node), playHeadState, ts, 1, duration + 1.0);

            auto expectedSequence = masterSequence;
            expectedSequence.addTimeToMessages (1.0);

            expectGreaterThan (expectedSequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, expectedSequence);
        }
    }
};

static MidiNodeTests midiNodeTests;

#endif //TRACKTION_UNIT_TESTS

}
