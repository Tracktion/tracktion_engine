/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_MIDICLIP

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Benchmark: MIDI Clip sequence")
{
    // Create an empty edit
    // Add a MIDI clip with some random data
    // Loop that clip for 50 hours
    // Time how long the sequence takes to build

    auto& engine = *tracktion::engine::Engine::getEngines()[0];
    Clipboard clipboard;
    auto edit = Edit::createSingleTrackEdit (engine);
    juce::Random r (42);

    const auto duration = 8.0s;
    auto c = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0.0s, TimePosition (duration) }, nullptr);
    const auto sequence = graph::test_utilities::createRandomMidiMessageSequence (c->getPosition().getLength().inSeconds(), r, { 0.031, 0.062 });

    c->getSequence().importMidiSequence (sequence, nullptr, 0s, nullptr);

    {
        const int numRepititions = juce::roundToInt (50h / duration);
        c->setNumberOfLoops (numRepititions);
    }

    {
        [[ maybe_unused ]] auto loopedSequence = c->createSequenceLooped (c->getSequence());
    }

    // Ensure this is cached for both subsequent benchmarks
    c->getSequenceLooped();

    {
        [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::seconds, false);
    }

    {
        [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::beats, false);
    }

    {
        [[ maybe_unused ]] auto playbackSeq = c->getSequenceLooped().exportToPlaybackMidiSequence (*c, MidiList::TimeBase::beatsRaw, false);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif //TRACKTION_BENCHMARKS
