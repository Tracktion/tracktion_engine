/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_MIDINODE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

//==============================================================================
namespace midinode_test_helpers
{
    static std::shared_ptr<graph::test_utilities::TestContext> createTracktionTestContext (ProcessState& processState, std::unique_ptr<Node> node,
                                                                                          graph::test_utilities::TestSetup ts, int numChannels, double durationInSeconds)
    {
        graph::test_utilities::TestProcess<TracktionNodePlayer> testProcess (std::make_unique<TracktionNodePlayer> (std::move (node), processState, ts.sampleRate, ts.blockSize,
                                                                                                                    getPoolCreatorFunction (ThreadPoolStrategy::realTime)),
                                                                             ts, numChannels, durationInSeconds, true);
        testProcess.setPlayHead (&processState.playHeadState.playHead);

        return testProcess.processAll();
    }
} // namespace midinode_test_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("MidiNode")
{
    using namespace midinode_test_helpers;

    for (auto setup : tracktion::graph::test_utilities::getTestSetups())
    {
        MESSAGE (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                    .replace ("SR", juce::String (setup.sampleRate))
                    .replace ("BS", juce::String (setup.blockSize))
                    .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N").toStdString());

        for (bool playSyncedToRange : { true, false })
        {
            auto ts = setup;
            using namespace tracktion::graph::test_utilities;

            const double sampleRate = 44100.0;
            const double duration = 5.0;

            // Avoid creating events at the end of the duration as they'll get lost after latency is applied
            const auto masterSequence = createRandomMidiMessageSequence (duration - 0.5, ts.random);

            tracktion::graph::PlayHead playHead;
            playHead.setScrubbingBlockLength (timeToSample (0.08, ts.sampleRate));
            tracktion::graph::PlayHeadState playHeadState (playHead);
            ProcessState processState (playHeadState);

            if (playSyncedToRange)
                playHead.play ({ 0, std::numeric_limits<int64_t>::max() }, false);
            else
                playHead.playSyncedToRange ({ 0, std::numeric_limits<int64_t>::max() });

            // Basic MIDI
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

                CHECK_GT (sequence.getNumEvents(), 0);
                expectMidiBuffer (testContext->midi, sampleRate, sequence);
            }

            // Offset MIDI
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

                CHECK_GT (expectedSequence.getNumEvents(), 0);
                CHECK_EQ (expectedSequence.getNumEvents(), masterSequence.getNumEvents());
                expectMidiBuffer (testContext->midi, sampleRate, expectedSequence);
            }
        }
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS
