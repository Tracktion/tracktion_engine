/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ENGINE_UNIT_TESTS_LOOPINGMIDINODE

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LoopingMidiNodeTests : public juce::UnitTest
{
public:
    LoopingMidiNodeTests()
        : juce::UnitTest ("LoopingMidiNodeTests", "tracktion_engine")
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

            runMidiTests (setup);

            runStuckNotesTests (setup, true, 1);
            runStuckNotesTests (setup, false, 1);

            runStuckNotesTests (setup, true, 2);
            runStuckNotesTests (setup, false, 2);

            runOffsetTests (setup);
        }

        runProgramChangeTests (false);
        runProgramChangeTests (true);
    }

private:
    //==============================================================================
    //==============================================================================
    void runMidiTests (test_utilities::TestSetup ts)
    {
        beginTest ("Setup");

        // Create a random sequence, loop it, render it and check its the same when proxy is on/off
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);

        auto r = ts.random;
        const auto duration = edit->tempoSequence.toTime ({ 4, 0_bd });
        const auto seq = test_utilities::createRandomMidiMessageSequence (duration.inSeconds(), r);

        // Add sequnce and loop randomly in the first and last of the 4 bars (16 beats)
        auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, duration }, nullptr);
        expect (mc->canUseProxy());
        mc->getSequence().importMidiSequence (seq, nullptr, 0_tp, nullptr);
        mc->setNumberOfLoops (1);
        const auto loopStart    = 1.0 + r.nextDouble() * 3.0;
        const auto loopEnd      = 16.0 - (1.0 + r.nextDouble() * 3.0);
        mc->setLoopRangeBeats ({ BeatPosition::fromBeats (loopStart), BeatPosition::fromBeats (loopEnd) });

        // Loop for 30s
        mc->setEnd (30_tp, true);

        // Basic sequence
        beginTest ("Looped sequence");
        testMidiClip (*mc, ts);

        // Quantised sequence
        beginTest ("Quantised sequence");
        QuantisationType quantisation;
        quantisation.setType ("1/16");
        mc->setQuantisation (quantisation);
        testMidiClip (*mc, ts);

        // Grooved sequence
        beginTest ("Grooved sequence");
        auto& gtm = engine.getGrooveTemplateManager();
        expect (gtm.getNumTemplates() > 0);
        mc->setGrooveTemplate (gtm.getTemplateName (0));
        mc->setGrooveStrength (1.0f);
        testMidiClip (*mc, ts);
    }

    void runStuckNotesTests (test_utilities::TestSetup ts, bool usesProxy, int numLoopIterations)
    {
        beginTest ("Stuck notes");

        // Create an empty edit
        // Add a MIDI clip with 8 notes 1 beat each
        // Render that clip and ensure there is no note after beat 8

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
                expectEquals (duration, 0.5); // duration of 1 beat
            }

            noteEvents.push_back (meh->message);
        }

        // Check we have the correct number of notes
        expectEquals ((int) noteEvents.size(), 16 * numLoopIterations);

        // Check the last note ends before the end of the clip
        expectLessOrEqual (noteEvents.back().getTimeStamp(), mc->getPosition().getEnd().inSeconds());
    }

    void runProgramChangeTests (bool sendBankSelect)
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

        beginTest ("Program/bank changes");

        // Create a clip with the above sequence
        // Render the output and ensure the CC messages are before the note events

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

            expectEquals (numNoteOnEvents, numNoteOffEvents);
            expectEquals (numNoteOnEvents, 9);
            expectEquals (numCCEvents, 4 + (sendBankSelect ? 2 : 0));
            expectEquals (numPCEvents, 1);
            expectEquals (numNonNoteEventsAferNoteEvents, 0);

            // Check round-trip import of sequence
            {
                MidiList importedList;
                importedList.setMidiChannel (MidiChannel (1));
                importedList.importMidiSequence (midiMessageSequence, nullptr, 0_tp, nullptr);

                expectEquals (importedList.getNotes().size(), sequence.getNotes().size());
                expectEquals (importedList.getControllerEvents().size(), sequence.getControllerEvents().size() + (sendBankSelect ? 2 : 0));
                expectEquals (importedList.getSysexEvents().size(), sequence.getSysexEvents().size());
            }
        }
    }

    void runOffsetTests (test_utilities::TestSetup ts)
    {
        beginTest ("MIDI clip with offset");

        // - Create a MIDI clip at bar 2 of 1 bar
        // - Add two notes, one beat each
        // - Set the offset to 1.5 beats (so only half of one note should be visible)
        // - Render the track which should only contain a single note

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

        auto seq = test_utilities::stripNonNoteOnOffMessages (renderMidiClip (*mc, ts, { 0_tp, mc->getPosition().getEnd() }));
        expectEquals (seq.getNumEvents(), 2);

        testMidiClip (*mc, ts);
    }

    void testMidiClip (MidiClip& mc, test_utilities::TestSetup ts)
    {
        auto renderOpts = RenderOptions::forClipRender ({ &mc }, true);
        renderOpts->setFormat (RenderOptions::midi);
        renderOpts->setIncludePlugins (false);
        auto params = renderOpts->getRenderParameters (mc);
        params.sampleRateForAudio = ts.sampleRate;
        params.blockSizeForAudio = ts.blockSize;

        juce::TemporaryFile t1, t2;

        mc.setUsesProxy (true);
        expect (mc.canUseProxy());
        params.destFile = t1.getFile();
        const auto seqWithProxyFile = Renderer::renderToFile ("proxy", params);

        mc.setUsesProxy (false);
        expect (! mc.canUseProxy());
        params.destFile = t2.getFile();
        const auto seqWithoutProxyFile = Renderer::renderToFile ("non-proxy", params);

        test_utilities::expectMidiMessageSequence (*this,
                                                   test_utilities::stripMetaEvents (getSeqFromFile (seqWithoutProxyFile)),
                                                   test_utilities::stripMetaEvents (getSeqFromFile (seqWithProxyFile)));
    }

    juce::MidiMessageSequence renderMidiClip (MidiClip& mc, test_utilities::TestSetup ts,
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
};

static LoopingMidiNodeTests loopingMidiNodeTests;

}} // namespace tracktion { inline namespace engine

#endif //ENGINE_UNIT_TESTS_LOOPINGMIDINODE
