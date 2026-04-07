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
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS
