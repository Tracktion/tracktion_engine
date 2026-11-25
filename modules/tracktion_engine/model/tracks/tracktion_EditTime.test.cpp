/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDIT_TIME

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("EditTime")
    {
        using namespace std::literals;
        auto& engine = *Engine::getEngines()[0];

        // Beat/Time conversion
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 60.0);

            CHECK_EQ (toBeats (TimePosition::fromSeconds (0.0), ts).inBeats(), 0.0);
            CHECK_EQ (toBeats (TimePosition::fromSeconds (8.0), ts).inBeats(), 8.0);

            CHECK_EQ (toTime (BeatPosition::fromBeats (0.0), ts).inSeconds(), 0.0);
            CHECK_EQ (toTime (BeatPosition::fromBeats (8.0), ts).inSeconds(), 8.0);

            // Change tempo to 120
            ts.getTempo (0)->setBpm (120.0);
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 120.0);

            CHECK_EQ (toBeats (TimePosition::fromSeconds (0.0), ts).inBeats(), 0.0);
            CHECK_EQ (toBeats (TimePosition::fromSeconds (8.0), ts).inBeats(), 16.0);

            CHECK_EQ (toTime (BeatPosition::fromBeats (0.0), ts).inSeconds(), 0.0);
            CHECK_EQ (toTime (BeatPosition::fromBeats (16.0), ts).inSeconds(), 8.0);
        }

        // Beat/Time Range conversion
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 60.0);

            const TimeRange originalTimeRange (TimePosition::fromSeconds (8.0), TimePosition::fromSeconds (16.0));
            const auto beatRange = toBeats (originalTimeRange, ts);

            CHECK_EQ (beatRange.getStart().inBeats(), 8.0);
            CHECK_EQ (beatRange.getEnd().inBeats(), 16.0);
            CHECK_EQ (beatRange.getLength().inBeats(), 8.0);
            CHECK_EQ (beatRange.getCentre().inBeats(), 12.0);
            CHECK (! beatRange.isEmpty());

            const auto timeRange = toTime (beatRange, ts);
            CHECK_EQ (timeRange.getStart().inSeconds(), 8.0);
            CHECK_EQ (timeRange.getEnd().inSeconds(), 16.0);
            CHECK_EQ (timeRange.getLength().inSeconds(), 8.0);
            CHECK_EQ (timeRange.getCentre().inSeconds(), 12.0);
            CHECK (! timeRange.isEmpty());
            CHECK (timeRange == originalTimeRange);
        }

        // EditTime
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 60.0);

            {
                auto et = EditTime (TimePosition::fromSeconds (0.0));
                CHECK_EQ (toTime (et, ts).inSeconds(), 0.0);
                CHECK_EQ (toBeats (et, ts).inBeats(), 0.0);
                CHECK_EQ ((toTime (et, ts) + 4s).inSeconds(), 4.0);
                CHECK_EQ (toBeats ((toTime (et, ts) + 4s), ts).inBeats(), 4.0);

                CHECK_EQ ((toTime (et, ts) - TimeDuration::fromSeconds (4.0)).inSeconds(), -4.0);
                CHECK_EQ ((toTime (et, ts) - 4s).inSeconds(), -4.0);
            }

            {
                auto et = EditTime (TimePosition::fromSeconds (8.0));
                CHECK_EQ (toTime (et, ts).inSeconds(), 8.0);
                CHECK_EQ (toBeats (et, ts).inBeats(), 8.0);

                CHECK_EQ ((toTime (et, ts) + 4s).inSeconds(), 12.0);
            }

            {
                auto et = EditTime (BeatPosition::fromBeats (8.0));
                CHECK_EQ (toTime (et, ts).inSeconds(), 8.0);
                CHECK_EQ (toBeats (et, ts).inBeats(), 8.0);

                CHECK_EQ ((toTime (et, ts) + 4s).inSeconds(), 12.0);
                CHECK_EQ (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 12.0);

                CHECK_EQ ((toTime (et, ts) - 4s).inSeconds(), 4.0);
                CHECK_EQ (toBeats (toTime (et, ts) - 4s, ts).inBeats(), 4.0);
            }

            // Change tempo to 120
            ts.getTempo (0)->setBpm (120.0);
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 120.0);

            {
                auto et = EditTime (TimePosition::fromSeconds (0.0));
                CHECK_EQ (toTime (et, ts).inSeconds(), 0.0);
                CHECK_EQ (toBeats (et, ts).inBeats(), 0.0);

                CHECK_EQ ((toTime (et, ts) + 4s).inSeconds(), 4.0);
                CHECK_EQ (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 8.0);

                CHECK_EQ ((toTime (et, ts) - 4s).inSeconds(), -4.0);
                CHECK_EQ (toBeats (toTime (et, ts) - 4s, ts).inBeats(), -8.0);
            }

            {
                auto et = EditTime (TimePosition::fromSeconds (8.0));
                CHECK_EQ (toTime (et, ts).inSeconds(), 8.0);
                CHECK_EQ (toBeats (et, ts).inBeats(), 16.0);

                CHECK_EQ ((toTime (et, ts) + 4s).inSeconds(), 12.0);
                CHECK_EQ (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 24.0);

                CHECK_EQ ((toTime (et, ts) - 4s).inSeconds(), 4.0);
                CHECK_EQ (toBeats (toTime (et, ts) - 4s, ts).inBeats(), 8.0);
            }
        }

        // EditTimeRange
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            CHECK_EQ (ts.getNumTempos(), 1);
            CHECK_EQ (ts.getTempo (0)->getBpm(), 60.0);

            const auto originalTimeRange = TimeRange (TimePosition::fromSeconds (8.0), TimePosition::fromSeconds (16.0));
            const auto originalBeatRange = toBeats (originalTimeRange, ts);

            {
                EditTimeRange etr (originalTimeRange);
                CHECK (toTime (etr, ts) == originalTimeRange);
                CHECK (toBeats (etr, ts) == originalBeatRange);
            }

            {
                EditTimeRange etr (originalBeatRange);
                CHECK (toTime (etr, ts) == originalTimeRange);
                CHECK (toBeats (etr, ts) == originalBeatRange);
            }
        }
    }

    TEST_CASE ("Clip offset in beats matches content beats")
    {
        using namespace std::literals;
        auto& engine = *Engine::getEngines()[0];

        auto checkOffset = [] (Clip& c)
        {
            CHECK (c.getOffsetInBeats().inBeats() == doctest::Approx ((c.getStartBeat() - c.getContentStartBeat()).inBeats()));

            // The clip end, measured from the content start, must equal length + offset
            const auto endInContentBeats = c.getContentBeatAtTime (c.getPosition().getEnd());
            CHECK (endInContentBeats.inBeats() == doctest::Approx ((c.getLengthInBeats() + c.getOffsetInBeats()).inBeats()));
        };

        // 4/4: a beat is a quarter note, so 2s at 60bpm is 2 beats
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto clip = getAudioTracks (*edit)[0]->insertMIDIClip (TimeRange (4_tp, 8_tp), nullptr);
            clip->setOffset (2s);

            CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (2.0));
            checkOffset (*clip);
        }

        // 8/8: a beat is an eighth note, so 2s at 60bpm is 4 beats (issue waveform_beta#1316)
        {
            auto edit = test_utilities::createTestEdit (engine);
            edit->tempoSequence.getTimeSig (0)->setStringTimeSig ("8/8");
            auto clip = getAudioTracks (*edit)[0]->insertMIDIClip (TimeRange (4_tp, 8_tp), nullptr);
            clip->setOffset (2s);

            CHECK (clip->getStartBeat().inBeats() == doctest::Approx (8.0));
            CHECK (clip->getContentStartBeat().inBeats() == doctest::Approx (4.0));
            CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (4.0));
            checkOffset (*clip);
        }

        // Reporter's edit: 11/8 then 8/8 at beat 132, 85bpm, clip with a large offset
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;
            ts.getTempo (0)->setBpm (85.0);
            ts.getTimeSig (0)->setStringTimeSig ("11/8");
            ts.insertTimeSig (BeatPosition::fromBeats (132.0))->setStringTimeSig ("8/8");

            auto clip = getAudioTracks (*edit)[0]->insertMIDIClip ({ TimePosition::fromSeconds (122.8235294117647),
                                                                     TimeDuration::fromSeconds (13.23764705882353) }, nullptr);
            clip->setOffset (TimeDuration::fromSeconds (27.88235294117647));

            // A note at content beat 79 sits at the clip start, so it can be lengthened to the clip end
            CHECK (clip->getOffsetInBeats().inBeats() == doctest::Approx (79.0));
            const auto maxLen = clip->getLengthInBeats() + clip->getOffsetInBeats() - 79_bd;
            CHECK (maxLen.inBeats() == doctest::Approx (37.5).epsilon (0.001));
            checkOffset (*clip);
        }

        // The offset spans a tempo change: the offset is the beat distance to the content start
        {
            auto edit = test_utilities::createTestEdit (engine);
            edit->tempoSequence.insertTempo (BeatPosition::fromBeats (2.0), 120.0, 0.0f);
            auto clip = getAudioTracks (*edit)[0]->insertMIDIClip (TimeRange (4_tp, 8_tp), nullptr);
            clip->setOffset (3s);

            checkOffset (*clip);
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
