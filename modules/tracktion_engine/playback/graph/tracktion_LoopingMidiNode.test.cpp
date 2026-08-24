/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LOOPINGMIDINODE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

//==============================================================================
namespace looping_midi_test_helpers
{
    static juce::MidiMessageSequence getSeqFromFile (juce::File f)
    {
        juce::MidiFile midiFile;

        {
            juce::FileInputStream in (f);

            if (! in.openedOk() || ! midiFile.readFrom (in))
                return {};
        }

        midiFile.convertTimestampTicksToSeconds();

        // Track 0 contains meta events from the tempo track etc.
        return *midiFile.getTrack (1);
    }

    static juce::MidiMessageSequence renderMidiClip (MidiClip& mc, graph::test_utilities::TestSetup ts,
                                                     TimeRange rangeToRender)
    {
        auto renderOpts = RenderOptions::forClipRender ({ &mc }, true);
        renderOpts->setFormat (RenderOptions::midi);
        renderOpts->setIncludePlugins (false);
        auto params = renderOpts->getRenderParameters (mc);
        params.sampleRateForAudio = ts.sampleRate;
        params.blockSizeForAudio = ts.blockSize;
        params.time = rangeToRender;

        juce::TemporaryFile t1;
        params.destFile = t1.getFile();
        const auto seqFile = Renderer::renderToFile ("proxy", params);

        return getSeqFromFile (seqFile);
    }

    static void testMidiClip (MidiClip& mc, graph::test_utilities::TestSetup ts)
    {
        auto renderOpts = RenderOptions::forClipRender ({ &mc }, true);
        renderOpts->setFormat (RenderOptions::midi);
        renderOpts->setIncludePlugins (false);
        auto params = renderOpts->getRenderParameters (mc);
        params.sampleRateForAudio = ts.sampleRate;
        params.blockSizeForAudio = ts.blockSize;

        juce::TemporaryFile t1, t2;

        mc.setUsesProxy (true);
        CHECK (mc.canUseProxy());
        params.destFile = t1.getFile();
        const auto seqWithProxyFile = Renderer::renderToFile ("proxy", params);

        mc.setUsesProxy (false);
        CHECK (! mc.canUseProxy());
        params.destFile = t2.getFile();
        const auto seqWithoutProxyFile = Renderer::renderToFile ("non-proxy", params);

        graph::test_utilities::expectMidiMessageSequence (graph::test_utilities::stripMetaEvents (getSeqFromFile (seqWithoutProxyFile)),
                                                          graph::test_utilities::stripMetaEvents (getSeqFromFile (seqWithProxyFile)));
    }

    struct BytesAndTimeStamp
    {
        uint8_t bytes[3];
        double timestamp;
    };

    static void runSequenceClippingTest (std::vector<BytesAndTimeStamp> data, juce::Range<double> clipRange, size_t numEventsExpected)
    {
        choc::midi::Sequence seq;

        for (auto d : data)
            seq.events.push_back ({ d.timestamp, choc::midi::ShortMessage (d.bytes[0], d.bytes[1], d.bytes[2]) });

        std::vector<std::pair<size_t, size_t>> noteOffMap;
        MidiHelpers::createNoteOffMap (noteOffMap, seq);
        MidiHelpers::clipSequenceToRange (seq, clipRange, noteOffMap);

        CHECK_EQ (seq.events.size(), numEventsExpected);

        bool allEventsWithinRange = true;

        for (auto e : seq)
            if (! clipRange.contains (e.timeStamp))
                allEventsWithinRange = false;

        CHECK_MESSAGE (allEventsWithinRange, "Not all events within the expected range");
    }
} // namespace looping_midi_test_helpers

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("LoopingMidiNode")
{
    using namespace looping_midi_test_helpers;

    for (auto setup : tracktion::graph::test_utilities::getTestSetups())
    {
        MESSAGE (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                    .replace ("SR", juce::String (setup.sampleRate))
                    .replace ("BS", juce::String (setup.blockSize))
                    .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N").toStdString());

        // runMidiTests
        {
            auto ts = setup;

            // Setup
            auto& engine = *tracktion::engine::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit (engine);

            auto r = ts.random;
            const auto duration = edit->tempoSequence.toTime ({ 4, 0_bd });
            const auto seq = graph::test_utilities::createRandomMidiMessageSequence (duration.inSeconds(), r);

            auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, duration }, nullptr);
            CHECK (mc->canUseProxy());
            mc->getSequence().importMidiSequence (seq, nullptr, 0_tp, nullptr);
            mc->setNumberOfLoops (1);
            const auto loopStart    = 1.0 + r.nextDouble() * 3.0;
            const auto loopEnd      = 16.0 - (1.0 + r.nextDouble() * 3.0);
            mc->setLoopRangeBeats ({ BeatPosition::fromBeats (loopStart), BeatPosition::fromBeats (loopEnd) });

            mc->setEnd (30_tp, true);

            // Basic sequence - Looped sequence
            testMidiClip (*mc, ts);

            // Quantised sequence
            QuantisationType quantisation;
            quantisation.setType ("1/16");
            mc->setQuantisation (quantisation);
            testMidiClip (*mc, ts);

            // Grooved sequence
            auto& gtm = engine.getGrooveTemplateManager();
            CHECK (gtm.getNumTemplates() > 0);
            mc->setGrooveTemplate (gtm.getTemplateName (0));
            mc->setGrooveStrength (1.0f);
            testMidiClip (*mc, ts);
        }

        // runStuckNotesTests (usesProxy=true, numLoopIterations=1)
        for (auto [usesProxy, numLoopIterations] : std::initializer_list<std::pair<bool, int>> { { true, 1 }, { false, 1 }, { true, 2 }, { false, 2 } })
        {
            auto ts = setup;

            // Stuck notes
            auto& engine = *tracktion::engine::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit (engine);
            auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, 0_tp }, nullptr);
            auto& sequence = mc->getSequence();

            for (int i = 0; i < 8; ++i)
                sequence.addNote (40 + i, BeatPosition::fromBeats (i), 1_bd, 127, 0, nullptr);

            mc->setUsesProxy (usesProxy);
            mc->setEnd (edit->tempoSequence.toTime (sequence.getLastBeatNumber()), true);

            if (numLoopIterations > 0)
                mc->setNumberOfLoops (numLoopIterations);

            std::vector<juce::MidiMessage> noteEvents;
            const auto renderedSequence = renderMidiClip (*mc, ts, { 0_tp, 100_tp });

            for (auto meh : renderedSequence)
            {
                if (! meh->message.isNoteOnOrOff())
                    continue;

                if (meh->message.isNoteOn())
                {
                    const auto duration = meh->noteOffObject->message.getTimeStamp() - meh->message.getTimeStamp();
                    CHECK_EQ (duration, 0.5); // duration of 1 beat
                }

                noteEvents.push_back (meh->message);
            }

            // Check we have the correct number of notes
            CHECK_EQ ((int) noteEvents.size(), 16 * numLoopIterations);

            // Check the last note ends before the end of the clip
            CHECK_LE (noteEvents.back().getTimeStamp(), mc->getPosition().getEnd().inSeconds());
        }

        // runOffsetTests
        {
            auto ts = setup;

            // MIDI clip with offset
            auto& engine = *tracktion::engine::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit (engine);
            auto& tempoSeq = edit->tempoSequence;
            auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ tempoSeq.toTime (tempo::BarsAndBeats { 1 }),
                                                                   tempoSeq.toTime (tempo::BarsAndBeats { 2 }) },
                                                                 nullptr);
            mc->setOffset (toDuration (tempoSeq.toTime (1.5_bp)));

            auto& sequence = mc->getSequence();
            sequence.addNote (49, 0_bp, 1_bd, 127, 0, nullptr);
            sequence.addNote (50, 1_bp, 1_bd, 127, 0, nullptr);

            auto seq = graph::test_utilities::stripNonNoteOnOffMessages (renderMidiClip (*mc, ts, { 0_tp, mc->getPosition().getEnd() }));
            CHECK_EQ (seq.getNumEvents(), 2);

            testMidiClip (*mc, ts);
        }
    }

    // runProgramChangeTests
    for (bool sendBankSelect : { false, true })
    {
        const auto seqXml = R"(
        <SEQUENCE ver="1" channelNumber="1">
          <CONTROL b="0.0" type="4097" val="6784"/>
          <CONTROL b="0.0" type="7" val="12800"/>
          <CONTROL b="0.0" type="39" val="5376"/>
          <CONTROL b="0.0" type="10" val="8192"/>
          <CONTROL b="0.0" type="42" val="0"/>
          <NOTE p="46" b="0.0" l="0.7139999999999986" v="100" c="0"/>
          <NOTE p="47" b="1.186" l="0.7139999999999986" v="100" c="0"/>
          <NOTE p="50" b="1.899999999999999" l="0.7680000000000007" v="90" c="0"/>
          <NOTE p="53" b="2.667999999999999" l="1.536000000000001" v="110" c="0"/>
          <NOTE p="58" b="3.204000000000001" l="0.7699999999999996" v="95" c="0"/>
          <NOTE p="57" b="3.974" l="0.7519999999999989" v="100" c="0"/>
          <NOTE p="56" b="4.726" l="0.75" v="100" c="0"/>
          <NOTE p="57" b="5.476" l="0.7760000000000034" v="102" c="0"/>
          <NOTE p="53" b="6.252" l="1.043999999999997" v="105" c="0"/>
        </SEQUENCE>)";

        // Program/bank changes
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);
        auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, 0_tp }, nullptr);
        mc->setSendingBankChanges (sendBankSelect);

        auto& sequence = mc->getSequence();
        sequence.copyFrom (MidiList (juce::ValueTree::fromXml (seqXml), nullptr), nullptr);
        mc->setEnd (edit->tempoSequence.toTime (sequence.getLastBeatNumber()), true);

        for (auto timeBase : { MidiList::TimeBase::seconds, MidiList::TimeBase::beats, MidiList::TimeBase::beatsRaw })
        {
            bool hasFoundNoteEvents = false;
            int numNonNoteEventsAferNoteEvents = 0, numCCEvents = 0, numPCEvents = 0, numNoteOnEvents = 0, numNoteOffEvents = 0;

            const auto midiMessageSequence = sequence.exportToPlaybackMidiSequence (*mc, timeBase, false);

            for (auto meh : midiMessageSequence)
            {
                if (meh->message.isNoteOnOrOff())
                    hasFoundNoteEvents = true;

                if (meh->message.isController())
                {
                    ++numCCEvents;

                    if (hasFoundNoteEvents)
                        ++numNonNoteEventsAferNoteEvents;
                }
                else if (meh->message.isProgramChange())
                {
                    ++numPCEvents;

                    if (hasFoundNoteEvents)
                        ++numNonNoteEventsAferNoteEvents;
                }
                else if (meh->message.isNoteOn())
                {
                    ++numNoteOnEvents;
                }
                else if (meh->message.isNoteOff())
                {
                    ++numNoteOffEvents;
                }
            }

            CHECK_EQ (numNoteOnEvents, numNoteOffEvents);
            CHECK_EQ (numNoteOnEvents, 9);
            CHECK_EQ (numCCEvents, 4 + (sendBankSelect ? 2 : 0));
            CHECK_EQ (numPCEvents, 1);
            CHECK_EQ (numNonNoteEventsAferNoteEvents, 0);

            // Check round-trip import of sequence
            {
                MidiList importedList;
                importedList.setMidiChannel (MidiChannel (1));
                importedList.importMidiSequence (midiMessageSequence, nullptr, 0_tp, nullptr);

                CHECK_EQ (importedList.getNotes().size(), sequence.getNotes().size());
                CHECK_EQ (importedList.getControllerEvents().size(), sequence.getControllerEvents().size() + (sendBankSelect ? 2 : 0));
                CHECK_EQ (importedList.getSysexEvents().size(), sequence.getSysexEvents().size());
            }
        }
    }

    // runSequenceClippingTests
    // Clipping sequence to range
    {
        looping_midi_test_helpers::BytesAndTimeStamp data[] = {
            { { 0x90, 0x4e, 0x7c }, 12.0 },
            { { 0x90, 0x42, 0x7c }, 12.0 },
            { { 0x90, 0x49, 0x7c }, 12.0 },
            { { 0x80, 0x42, 0x00 }, 12.4354 },
            { { 0x80, 0x49, 0x00 }, 12.4578 },
            { { 0x80, 0x4e, 0x00 }, 12.4764 },
            { { 0x90, 0x4e, 0x7c }, 14.5 },
            { { 0x90, 0x49, 0x7f }, 14.5 },
            { { 0x90, 0x42, 0x7c }, 14.5 },
            { { 0x80, 0x4e, 0x00 }, 14.9335 },
            { { 0x80, 0x42, 0x00 }, 14.9519 },
            { { 0x80, 0x49, 0x00 }, 14.9752 },
            { { 0x90, 0x42, 0x7b }, 15.5 },
            { { 0x80, 0x42, 0x00 }, 15.9744 },
            { { 0x90, 0x4e, 0x7b }, 16.0 },
            { { 0x90, 0x49, 0x7b }, 16.0 },
            { { 0x80, 0x49, 0x00 }, 17.0 },
            { { 0x80, 0x4e, 0x00 }, 17.0312 },
            { { 0x90, 0x42, 0x7b }, 20.0 },
            { { 0x80, 0x42, 0x00 }, 20.3751 },
            { { 0x90, 0x49, 0x7b }, 22.0 },
            { { 0x90, 0x42, 0x7a }, 22.0 },
            { { 0x90, 0x4e, 0x7b }, 22.5 },
            { { 0x80, 0x42, 0x00 }, 22.5101 },
            { { 0x80, 0x49, 0x00 }, 22.5207 },
            { { 0x80, 0x4e, 0x00 }, 23.0279 },
            { { 0x90, 0x4e, 0x79 }, 24.0 },
            { { 0x90, 0x42, 0x7a }, 24.0 },
            { { 0x90, 0x49, 0x7a }, 24.0 },
            { { 0x80, 0x49, 0x00 }, 24.1976 },
            { { 0x80, 0x42, 0x00 }, 24.2033 },
            { { 0x80, 0x4e, 0x00 }, 24.2284 } };

        runSequenceClippingTest ({ std::begin (data), std::end (data) },
                                 { 12.0, 24.0 }, static_cast<size_t> (26));
    }

    {
        looping_midi_test_helpers::BytesAndTimeStamp data[] = {
            { { 0x90, 0x4e, 0x7c }, 0.0 },
            { { 0x90, 0x42, 0x7c }, 0.0 },
            { { 0x90, 0x49, 0x7c }, 0.0 },
            { { 0x80, 0x42, 0x00 }, 0.4354 },
            { { 0x80, 0x49, 0x00 }, 0.4578 },
            { { 0x80, 0x4e, 0x00 }, 0.4764 },
            { { 0x90, 0x4e, 0x7c }, 2.5 },
            { { 0x90, 0x49, 0x7f }, 2.5 },
            { { 0x90, 0x42, 0x7c }, 2.5 },
            { { 0x80, 0x4e, 0x00 }, 2.9335 },
            { { 0x80, 0x42, 0x00 }, 2.9519 },
            { { 0x80, 0x49, 0x00 }, 2.9752 },
            { { 0x90, 0x42, 0x7b }, 3.5 },
            { { 0x80, 0x42, 0x00 }, 3.9744 },
            { { 0x90, 0x4e, 0x7b }, 4.0 },
            { { 0x90, 0x49, 0x7b }, 4.0 },
            { { 0x80, 0x49, 0x00 }, 5.0 },
            { { 0x80, 0x4e, 0x00 }, 5.0312 },
            { { 0x90, 0x42, 0x7b }, 8.0 },
            { { 0x80, 0x42, 0x00 }, 8.3751 },
            { { 0x90, 0x49, 0x7b }, 10.0 },
            { { 0x90, 0x42, 0x7a }, 10.0 },
            { { 0x90, 0x4e, 0x7b }, 10.5 },
            { { 0x80, 0x42, 0x00 }, 10.5101 },
            { { 0x80, 0x49, 0x00 }, 10.5207 },
            { { 0x80, 0x4e, 0x00 }, 11.0279 },
            { { 0x90, 0x4e, 0x79 }, 12.0 },
            { { 0x90, 0x42, 0x7a }, 12.0 },
            { { 0x90, 0x49, 0x7a }, 12.0 },
            { { 0x80, 0x49, 0x00 }, 12.1976 },
            { { 0x80, 0x42, 0x00 }, 12.2033 },
            { { 0x80, 0x4e, 0x00 }, 12.2284 } };

        runSequenceClippingTest ({ std::begin (data), std::end (data) },
                                 { 0.0, 12.0 }, static_cast<size_t> (26));
    }
}

TEST_CASE ("LoopingMidiNode two-byte messages in note-off map")
{
    // A 2-byte message (e.g. program change or channel pressure) whose data byte
    // matches a sounding note's pitch and channel used to be probed with
    // choc::midi::Message::isNoteOff(), which reads the non-existent velocity
    // byte and asserts
    choc::midi::Sequence seq;

    const uint8_t noteOn[]          = { 0x92, 53, 100 };
    const uint8_t programChange[]   = { 0xc2, 53 };
    const uint8_t channelPressure[] = { 0xd2, 53 };
    const uint8_t noteOff[]         = { 0x82, 53, 0 };

    seq.events.push_back ({ 0.0,  choc::midi::LongMessage (noteOn, sizeof (noteOn)) });
    seq.events.push_back ({ 0.25, choc::midi::LongMessage (programChange, sizeof (programChange)) });
    seq.events.push_back ({ 0.5,  choc::midi::LongMessage (channelPressure, sizeof (channelPressure)) });
    seq.events.push_back ({ 1.0,  choc::midi::LongMessage (noteOff, sizeof (noteOff)) });

    std::vector<std::pair<size_t, size_t>> noteOffMap;
    MidiHelpers::createNoteOffMap (noteOffMap, seq);

    // The note-on must map to the real note-off, not the program change
    REQUIRE_EQ (noteOffMap.size(), static_cast<size_t> (1));
    CHECK_EQ (noteOffMap[0].first, static_cast<size_t> (0));
    CHECK_EQ (noteOffMap[0].second, static_cast<size_t> (3));

    // The rest of the sequence-caching pipeline also probes these messages
    QuantisationType quantisation;
    quantisation.setType ("1/16");
    quantisation.setIsQuantisingNoteOffs (true);
    MidiHelpers::applyQuantisationToSequence (quantisation, true, seq, noteOffMap);

    auto& gtm = tracktion::engine::Engine::getEngines()[0]->getGrooveTemplateManager();
    REQUIRE (gtm.getNumTemplates() > 0);
    MidiHelpers::applyGrooveToSequence (*gtm.getTemplate (0), 1.0f, seq);

    seq.sortEvents();
    MidiHelpers::createNoteOffMap (noteOffMap, seq);
    MidiHelpers::clipSequenceToRange (seq, { 0.0, 4.0 }, noteOffMap);

    CHECK_EQ (seq.events.size(), static_cast<size_t> (4));
}

TEST_CASE ("LoopingMidiNode program change colliding with note pitch")
{
    // End-to-end version of the above: a mid-clip program change whose program
    // number matches an earlier note's pitch used to assert when the clip's
    // playback sequence was cached
    using namespace looping_midi_test_helpers;

    auto& engine = *tracktion::engine::Engine::getEngines()[0];
    auto edit = Edit::createSingleTrackEdit (engine);
    auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, 0_tp }, nullptr);

    auto& sequence = mc->getSequence();
    sequence.addNote (53, 0_bp, 2_bd, 100, 0, nullptr);
    sequence.addControllerEvent (1_bp, MidiControllerEvent::programChangeType, 53 << 7, nullptr);

    mc->setUsesProxy (false);
    mc->setEnd (edit->tempoSequence.toTime (4_bp), true);
    mc->setNumberOfLoops (1);

    const auto ts = tracktion::graph::test_utilities::getTestSetups()[0];
    const auto rendered = renderMidiClip (*mc, ts, { 0_tp, mc->getPosition().getEnd() });

    int numNoteOns = 0, numNoteOffs = 0, numProgramChanges = 0;

    for (auto meh : rendered)
    {
        if (meh->message.isNoteOn())            ++numNoteOns;
        else if (meh->message.isNoteOff())      ++numNoteOffs;
        else if (meh->message.isProgramChange()) ++numProgramChanges;
    }

    CHECK_EQ (numNoteOns, 1);
    CHECK_EQ (numNoteOffs, 1);
    CHECK_EQ (numProgramChanges, 1);
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif //ENGINE_UNIT_TESTS_LOOPINGMIDINODE
