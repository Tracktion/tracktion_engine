/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_BEAT_CONVERSIONS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>

/*  Clip beats (note positions, loop ranges, offsets in beats) are in the unit
    TempoSequence::toBeats() uses: the time signature's beat when
    EngineBehaviour::lengthOfOneBeatDependsOnTimeSignature() is true (the default),
    so an eighth note in 6/8 and a half note in 3/2. These tests check every
    seconds <-> beats conversion on clips against toBeats(), in time signatures
    where that beat is not a quarter note.
*/

namespace tracktion::inline engine
{

namespace clip_beat_conversion_tests
{
    struct Config
    {
        const char* timeSig;
        double bpm;
        const char* secondTimeSig = nullptr; ///< If set, starts at bar 2, before 2s, so the clips sit after it
    };

    // 4/4 must behave as before; the others each have a non-quarter-note beat
    inline const Config configs[] = {
        { "4/4", 60.0 },  { "4/4", 137.5 },
        { "8/8", 60.0 },  { "8/8", 85.0 },
        { "6/8", 120.0 }, { "3/2", 90.0 },
        { "7/16", 72.0 }, { "11/8", 85.0 },
        { "4/4", 150.0, "6/8" }, { "6/8", 120.0, "4/4" },
        { "11/8", 170.0, "8/8" }, { "3/2", 200.0, "7/16" },
    };

    inline std::unique_ptr<Edit> createEdit (Engine& engine, const Config& config)
    {
        auto edit = test_utilities::createTestEdit (engine);
        auto& ts = edit->tempoSequence;
        ts.getTempo (0)->setBpm (config.bpm);
        ts.getTimeSig (0)->setStringTimeSig (config.timeSig);

        if (config.secondTimeSig != nullptr)
        {
            ts.insertTimeSig (BeatPosition::fromBeats (ts.getTimeSig (0)->numerator.get()))->setStringTimeSig (config.secondTimeSig);
            jassert (ts.getTimeSig (1) != nullptr && ts.toTime (ts.getTimeSig (1)->getStartBeat()) < 2_tp);
        }

        return edit;
    }

    /** The number of toBeats() beats in a span of seconds, starting at a time. */
    inline double beatsIn (const TempoSequence& ts, TimePosition start, TimeDuration length)
    {
        return (ts.toBeats (start + length) - ts.toBeats (start)).inBeats();
    }

    /** The content beat of a looped clip at an Edit time, as playback sees it. */
    inline double loopedContentBeatAt (const Clip& c, TimePosition t)
    {
        const auto loopLen = c.getLoopLengthBeats().inBeats();
        const auto fromStart = (c.edit.tempoSequence.toBeats (t) - c.getStartBeat()).inBeats() + c.getOffsetInBeats().inBeats();
        return c.getLoopStartBeats().inBeats() + std::fmod (fromStart, loopLen);
    }

    inline juce::String describe (const Config& config)
    {
        return juce::String (config.timeSig) + (config.secondTimeSig != nullptr ? juce::String (" then ") + config.secondTimeSig : juce::String())
                + " at " + juce::String (config.bpm) + " bpm";
    }

    inline Clip::Ptr insertClip (Edit& edit, TrackItem::Type type, TimeRange time)
    {
        return getAudioTracks (edit)[0]->insertNewClip (type, time, nullptr);
    }

    /** Step clips only loop in a launcher slot, so put them in one. */
    inline Clip::Ptr insertLoopableClip (Edit& edit, TrackItem::Type type, TimeRange time)
    {
        if (type != TrackItem::Type::step)
            return insertClip (edit, type, time);

        auto track = getAudioTracks (edit)[0];
        track->getClipSlotList().ensureNumberOfSlots (1);
        edit.getSceneList().ensureNumberOfScenes (1);
        auto slot = track->getClipSlotList().getClipSlots()[0];
        return insertNewClip (*slot, type, time);
    }
}

TEST_SUITE ("tracktion_engine")
{
    using namespace clip_beat_conversion_tests;

    TEST_CASE ("Clip beat conversions: quarter notes and time signature beats per second")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            CAPTURE (describe (config));
            auto edit = createEdit (engine, config);
            auto& ts = edit->tempoSequence;

            for (auto t : { 0_tp, 1_tp, 7.5_tp })
            {
                // N.B. looked up by beat: getTimeSigAt (TimePosition) reads cached start times that
                // can be stale straight after a time signature is inserted
                const auto denominator = ts.getTimeSigAt (ts.toBeats (t)).denominator.get();

                CHECK (ts.getQuarterNotesPerSecondAt (t) == doctest::Approx (config.bpm / 60.0));
                CHECK (ts.getTimeSigBeatsPerSecondAt (t) == doctest::Approx (config.bpm / 60.0 * denominator / 4.0));

                // The time signature rate is the rate toBeats() counts at
                CHECK (ts.getTimeSigBeatsPerSecondAt (t) == doctest::Approx (beatsIn (ts, t, 0.25s) / 0.25));
            }

            // The deprecated function keeps its old results
            JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
            CHECK (ts.getBeatsPerSecondAt (1_tp) == doctest::Approx (ts.getQuarterNotesPerSecondAt (1_tp)));
            CHECK (ts.getBeatsPerSecondAt (1_tp, true) == doctest::Approx (ts.getTimeSigBeatsPerSecondAt (1_tp)));
            JUCE_END_IGNORE_DEPRECATION_WARNINGS
        }

        // Through a tempo curve and across time signature changes, the two keep the time signature's ratio
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;
            ts.getTimeSig (0)->setStringTimeSig ("6/8");
            ts.getTempo (0)->setCurve (0.5f);
            ts.insertTempo (12_bp, 180.0, 0.0f);
            ts.insertTimeSig (24_bp)->setStringTimeSig ("3/2");

            for (auto b : { 1_bp, 6_bp, 11_bp, 18_bp })
            {
                const auto t = ts.toTime (b);
                CHECK (ts.getTimeSigBeatsPerSecondAt (t) == doctest::Approx (ts.getQuarterNotesPerSecondAt (t) * 8.0 / 4.0));
            }

            const auto t = ts.toTime (30_bp);
            CHECK (ts.getTimeSigBeatsPerSecondAt (t) == doctest::Approx (ts.getQuarterNotesPerSecondAt (t) * 2.0 / 4.0));
            CHECK (ts.getQuarterNotesPerSecondAt (t) == doctest::Approx (3.0));
        }
    }

    TEST_CASE ("Clip beat conversions: offset in beats is the distance to the content start")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            CAPTURE (describe (config));
            auto edit = createEdit (engine, config);
            auto& ts = edit->tempoSequence;

            for (auto type : { TrackItem::Type::midi, TrackItem::Type::step })
            {
                auto clip = insertClip (*edit, type, TimeRange (4_tp, 12_tp));
                REQUIRE (clip != nullptr);
                clip->setOffset (TimeDuration::fromSeconds (1.5));

                CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (beatsIn (ts, 2.5_tp, 1.5s)));
                CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx ((clip->getStartBeat() - clip->getContentStartBeat()).inBeats()));

                clip->removeFromParent();
            }
        }
    }

    TEST_CASE ("Clip beat conversions: MIDI and step clip loop ranges set from seconds")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            for (auto type : { TrackItem::Type::midi, TrackItem::Type::step })
            {
                CAPTURE (describe (config));
                CAPTURE (type == TrackItem::Type::midi ? "midi" : "step");
                auto edit = createEdit (engine, config);
                auto& ts = edit->tempoSequence;

                auto clip = insertLoopableClip (*edit, type, TimeRange (2_tp, 10_tp));
                REQUIRE (clip != nullptr);
                const auto s = clip->getPosition().getStart();

                const TimeRange loopRange (TimePosition::fromSeconds (0.5), TimePosition::fromSeconds (2.0));
                clip->setLoopRange (loopRange);
                REQUIRE (clip->isLooping());

                // Measured from the clip start, in the same beats as the notes
                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (beatsIn (ts, s, 0.5s)));
                CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (beatsIn (ts, s + 0.5s, 1.5s)));

                // Reading it back gives the same seconds
                CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.5));
                CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (1.5));

                // Setting the reported range is a no-op, however many times it is done
                const auto startBeats = clip->getLoopStartBeats().inBeats();
                const auto lengthBeats = clip->getLoopLengthBeats().inBeats();

                for (int i = 0; i < 3; ++i)
                {
                    clip->setLoopRange (TimeRange (clip->getLoopStart(), clip->getLoopLength()));
                    CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (startBeats));
                    CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (lengthBeats));
                }
            }
        }
    }

    TEST_CASE ("Clip beat conversions: looping a MIDI or step clip with an offset keeps its content in place")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            for (auto type : { TrackItem::Type::midi, TrackItem::Type::step })
            {
                CAPTURE (describe (config));
                CAPTURE (type == TrackItem::Type::midi ? "midi" : "step");
                auto edit = createEdit (engine, config);
                auto& ts = edit->tempoSequence;

                auto clip = insertLoopableClip (*edit, type, TimeRange (4_tp, 8_tp));
                REQUIRE (clip != nullptr);
                clip->setOffset (1.5s);
                const auto s = clip->getPosition().getStart();

                const auto offsetBeats = clip->getOffsetInBeats().inBeats();
                const auto lengthBeats = clip->getLengthInBeats().inBeats();
                const auto startBeat = clip->getStartBeat();

                clip->setNumberOfLoops (3);
                REQUIRE (clip->isLooping());

                // The loop starts at the content beat that was at the clip start
                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (offsetBeats));
                CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (lengthBeats));
                CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (0.0));
                CHECK (clip->getStartBeat().inBeats() == doctest::Approx (startBeat.inBeats()));
                CHECK (clip->getPosition().getEnd().inSeconds() == doctest::Approx (ts.toTime (startBeat + BeatDuration::fromBeats (lengthBeats * 3)).inSeconds()));

                // ...so the first loop plays what the unlooped clip played
                CHECK (loopedContentBeatAt (*clip, s) == doctest::Approx (offsetBeats));
                CHECK (loopedContentBeatAt (*clip, s + 1s) == doctest::Approx (offsetBeats + beatsIn (ts, s, 1s)));

                clip->disableLooping();
                CHECK (! clip->isLooping());
                CHECK (clip->getPosition().getOffset().inSeconds() == doctest::Approx (1.5));
                CHECK (clip->getPosition().getLength().inSeconds() == doctest::Approx (4.0));
                CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (offsetBeats));
            }
        }
    }

    TEST_CASE ("Clip beat conversions: splitting a looped MIDI clip keeps the content at the split")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            CAPTURE (describe (config));
            auto edit = createEdit (engine, config);

            auto clip = insertClip (*edit, TrackItem::Type::midi, TimeRange (2_tp, 4_tp));
            REQUIRE (clip != nullptr);
            clip->setNumberOfLoops (6);
            REQUIRE (clip->isLooping());

            // Split well past the first loop so the second half's offset is more than a loop
            // and gets wrapped back into the loop
            const auto splitTime = TimePosition::fromSeconds (7.3);
            const auto expectedBeat = loopedContentBeatAt (*clip, splitTime);

            auto second = split (*clip, splitTime);
            REQUIRE (second != nullptr);
            REQUIRE (second->isLooping());

            CHECK (second->getOffsetInBeats() < second->getLoopLengthBeats());
            CHECK (second->getOffsetInBeats() >= 0_bd);
            CHECK (loopedContentBeatAt (*second, splitTime) == doctest::Approx (expectedBeat));
            CHECK (loopedContentBeatAt (*second, splitTime + 0.4s) == doctest::Approx (loopedContentBeatAt (*clip, splitTime - 0.0001s) + beatsIn (edit->tempoSequence, splitTime, 0.4s)).epsilon (0.001));
        }
    }

    TEST_CASE ("Clip beat conversions: copy and paste keeps a MIDI clip's offset in beats")
    {
        auto& engine = *Engine::getEngines()[0];

        auto copyClip = [] (Clip& c, Clipboard::Clips& clips)
        {
            SelectableList selected;
            selected.add (&c);
            clips.addSelectedClips (selected, Edit::getMaximumEditTimeRange(), Clipboard::Clips::AutomationLocked::no);
        };

        auto pasteAt = [] (const Clipboard::Clips& clips, Edit& dest, TimePosition time) -> Clip*
        {
            auto track = getAudioTracks (dest)[0];
            const auto numBefore = track->getClips().size();

            EditInsertPoint insertPoint (dest);
            insertPoint.setNextInsertPoint (time, juce::ReferenceCountedObjectPtr<Track> (track));

            Clipboard::ContentType::EditPastingOptions opts (dest, insertPoint);
            opts.silent = true;
            opts.startTrack = track;
            opts.startTime = time;

            if (! clips.pasteIntoEdit (opts) || track->getClips().size() != numBefore + 1)
                return nullptr;

            for (auto c : track->getClips())
                if (std::abs (c->getPosition().getStart().inSeconds() - time.inSeconds()) < 0.0001)
                    return c;

            return nullptr;
        };

        for (auto& source : configs)
        {
            CAPTURE (describe (source));
            auto edit = createEdit (engine, source);

            auto clip = insertClip (*edit, TrackItem::Type::midi, TimeRange (4_tp, 8_tp));
            REQUIRE (clip != nullptr);
            clip->setOffset (1.5s);
            const auto offsetBeats = clip->getOffsetInBeats().inBeats();
            Clipboard::Clips clips;
            copyClip (*clip, clips);

            // Into the same Edit
            {
                auto pasted = pasteAt (clips, *edit, 20_tp);
                REQUIRE (pasted != nullptr);
                CHECK (pasted->getOffsetInBeats().inBeats() == doctest::Approx (offsetBeats));
                CHECK (pasted->getPosition().getOffset().inSeconds() == doctest::Approx (1.5));
            }

            // Into an Edit in each other time signature: the notes stay where they were in the clip
            for (auto& dest : configs)
            {
                CAPTURE (describe (dest));
                auto destEdit = createEdit (engine, dest);
                auto pasted = pasteAt (clips, *destEdit, 4_tp);
                REQUIRE (pasted != nullptr);
                CHECK (pasted->getOffsetInBeats().inBeats() == doctest::Approx (offsetBeats));
                CHECK (pasted->getLengthInBeats().inBeats() == doctest::Approx (clip->getLengthInBeats().inBeats()));
            }
        }
    }

    TEST_CASE ("Clip beat conversions: inserting space moves later tempo, time signature and pitch changes by that time")
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto& config : configs)
        {
            // These add their own time signature change, so use one time signature to start with
            if (config.secondTimeSig != nullptr)
                continue;

            CAPTURE (describe (config));
            auto edit = createEdit (engine, config);
            auto& ts = edit->tempoSequence;
            const auto numerator = ts.getTimeSig (0)->numerator.get();

            // A tempo change, a time signature change (on a bar line) and a pitch change, all later
            // than the insertion point
            const auto changeBeat = BeatPosition::fromBeats (numerator * 4.0);
            ts.insertTempo (changeBeat, config.bpm * 1.5, 0.0f);
            ts.insertTimeSig (changeBeat)->setStringTimeSig ("5/4");
            edit->pitchSequence.insertPitch (changeBeat, 64);

            REQUIRE (ts.getNumTempos() == 2);
            REQUIRE (ts.getNumTimeSigs() == 2);
            REQUIRE (edit->pitchSequence.getNumPitches() == 2);

            const auto changeTime = ts.toTime (changeBeat);
            const auto insertTime = ts.toTime (BeatPosition::fromBeats (numerator * 1.0));
            REQUIRE (insertTime < changeTime);

            const auto space = 2.5s;
            ts.insertSpaceIntoSequence (insertTime, space, false);
            edit->pitchSequence.insertSpaceIntoSequence (insertTime, space, false);

            CHECK (ts.toTime (ts.getTempo (1)->getStartBeat()).inSeconds() == doctest::Approx ((changeTime + space).inSeconds()));
            CHECK (ts.toTime (ts.getTimeSig (1)->getStartBeat()).inSeconds() == doctest::Approx ((changeTime + space).inSeconds()));
            CHECK (ts.toTime (edit->pitchSequence.getPitch (1)->getStartBeatNumber()).inSeconds() == doctest::Approx ((changeTime + space).inSeconds()));

            // Earlier settings don't move
            CHECK (ts.getTempo (0)->getStartBeat() == 0_bp);
            CHECK (ts.getTimeSig (0)->getStartBeat() == 0_bp);
        }
    }

    TEST_CASE ("Clip beat conversions: auto-tempo audio clip loops use the notes' beats")
    {
        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 4.0, 2);

        for (auto& config : configs)
        {
            CAPTURE (describe (config));
            auto edit = createEdit (engine, config);
            auto& ts = edit->tempoSequence;
            auto track = getAudioTracks (*edit)[0];

            auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { .time = TimeRange (2_tp, 4_tp) }, DeleteExistingClips::no);
            REQUIRE (clip != nullptr);

            SUBCASE ("time-based loop reported in beats")
            {
                clip->setLoopRange (TimeRange (TimePosition::fromSeconds (0.25), TimePosition::fromSeconds (1.0)));
                REQUIRE (clip->isLooping());
                REQUIRE (! clip->beatBasedLooping());

                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (beatsIn (ts, 2_tp, 0.25s)));
                CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (beatsIn (ts, 2.25_tp, 0.75s)));
            }

            SUBCASE ("beat-based loop set from seconds")
            {
                clip->setAutoTempo (true);
                clip->setLoopRange (TimeRange (TimePosition::fromSeconds (0.25), TimePosition::fromSeconds (1.0)));
                REQUIRE (clip->beatBasedLooping());

                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (beatsIn (ts, 2_tp, 0.25s)));
                CHECK (clip->getLoopLengthBeats().inBeats() == doctest::Approx (beatsIn (ts, 2.25_tp, 0.75s)));
                CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.25));
                CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (0.75));
                CHECK (clip->getLoopRange().getEnd().inSeconds() == doctest::Approx (1.0));
            }

            SUBCASE ("toggling auto tempo keeps the loop in seconds and beats")
            {
                clip->setLoopRange (TimeRange (TimePosition::fromSeconds (0.25), TimePosition::fromSeconds (1.0)));
                clip->setAutoTempo (true);
                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (beatsIn (ts, 2_tp, 0.25s)));
                CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.25));

                clip->setAutoTempo (false);
                CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.25));
                CHECK (clip->getLoopLength().inSeconds() == doctest::Approx (0.75));
            }

            SUBCASE ("looping with an offset starts the loop at the offset")
            {
                clip->setAutoTempo (true);
                clip->setOffset (0.5s);
                const auto offsetBeats = clip->getOffsetInBeats().inBeats();

                clip->setNumberOfLoops (2);
                REQUIRE (clip->beatBasedLooping());
                CHECK (clip->getLoopStartBeats().inBeats() == doctest::Approx (offsetBeats));

                // Audio loops report seconds at the single tempo at the clip start (see the
                // AudioClipBase looping tests), so this only matches the offset when it doesn't
                // cross a time signature change
                CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (offsetBeats / ts.getTimeSigBeatsPerSecondAt (clip->getPosition().getStart())));

                if (config.secondTimeSig == nullptr)
                    CHECK (clip->getLoopStart().inSeconds() == doctest::Approx (0.5));
            }
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_BEAT_CONVERSIONS
