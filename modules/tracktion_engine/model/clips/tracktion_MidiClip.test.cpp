/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS

#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class MidiClipBenchmarks  : public juce::UnitTest
{
public:
    MidiClipBenchmarks()
        : juce::UnitTest ("MidiClip", "tracktion_benchmarks")
    {}

    void runTest() override
    {
        runLoopedMidiClipBenchmark();
    }

private:
    BenchmarkDescription getDescription (std::string bmName)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = bmName;
        
        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
                 bmCategory, bmName, bmDescription };
    }

    void runLoopedMidiClipBenchmark()
    {
        // Create an empty edit
        // Add a MIDI clip with some random data
        // Loop that clip for 50 hours
        // Time how long the sequence takes to build

        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit (engine);
        juce::Random r (42);

        beginTest ("Benchmark: MIDI Clip sequence");
        {
            const auto duration = 8.0s;
            auto c = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0.0s, TimePosition (duration) }, nullptr);
            const auto sequence = test_utilities::createRandomMidiMessageSequence (c->getPosition().getLength().inSeconds(), r, { 0.031, 0.062 });

            {
                ScopedBenchmark sb (getDescription ("Add sequence to clip"));
                c->getSequence().importMidiSequence (sequence, nullptr, 0s, nullptr);
            }

            {
                ScopedBenchmark sb (getDescription ("Loop sequence for 50 hours"));
                const int numRepititions = juce::roundToInt (50h / duration);
                c->setNumberOfLoops (numRepititions);
            }

            {
                ScopedBenchmark sb (getDescription ("Calculate looped sequence"));
                [[ maybe_unused ]] auto loopedSequence = c->createSequenceLooped (c->getSequence());
            }

            // Ensure this is cached for both subsequent benchmarks
            c->getSequenceLooped();

            {
                ScopedBenchmark sb (getDescription ("Calculate looped sequence in seconds"));
                [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::seconds, false);
            }

            {
                ScopedBenchmark sb (getDescription ("Calculate looped sequence in beats"));
                [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::beats, false);
            }

            {
                ScopedBenchmark sb (getDescription ("Calculate looped sequence in beats raw"));
                [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::beatsRaw, false);
            }
        }
    }
};

static MidiClipBenchmarks midiClipBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_BENCHMARKS
