/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class EditTimeTests : public juce::UnitTest
{
public:
    EditTimeTests()
        : juce::UnitTest ("EditTime", "tracktion_graph")
    {
    }

    void runTest() override
    {
        using namespace std::literals;
        auto& engine = *Engine::getEngines()[0];

        beginTest ("Beat/Time conversion");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 60.0);

            expectEquals (toBeats (TimePosition::fromSeconds (0.0), ts).inBeats(), 0.0);
            expectEquals (toBeats (TimePosition::fromSeconds (8.0), ts).inBeats(), 8.0);

            expectEquals (toTime (BeatPosition::fromBeats (0.0), ts).inSeconds(), 0.0);
            expectEquals (toTime (BeatPosition::fromBeats (8.0), ts).inSeconds(), 8.0);

            // Change tempo to 120
            ts.getTempo (0)->setBpm (120.0);
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 120.0);

            expectEquals (toBeats (TimePosition::fromSeconds (0.0), ts).inBeats(), 0.0);
            expectEquals (toBeats (TimePosition::fromSeconds (8.0), ts).inBeats(), 16.0);

            expectEquals (toTime (BeatPosition::fromBeats (0.0), ts).inSeconds(), 0.0);
            expectEquals (toTime (BeatPosition::fromBeats (16.0), ts).inSeconds(), 8.0);
        }

        beginTest ("Beat/Time Range conversion");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 60.0);

            const TimeRange originalTimeRange (TimePosition::fromSeconds (8.0), TimePosition::fromSeconds (16.0));
            const auto beatRange = toBeats (originalTimeRange, ts);

            expectEquals (beatRange.getStart().inBeats(), 8.0);
            expectEquals (beatRange.getEnd().inBeats(), 16.0);
            expectEquals (beatRange.getLength().inBeats(), 8.0);
            expectEquals (beatRange.getCentre().inBeats(), 12.0);
            expect (! beatRange.isEmpty());

            const auto timeRange = toTime (beatRange, ts);
            expectEquals (timeRange.getStart().inSeconds(), 8.0);
            expectEquals (timeRange.getEnd().inSeconds(), 16.0);
            expectEquals (timeRange.getLength().inSeconds(), 8.0);
            expectEquals (timeRange.getCentre().inSeconds(), 12.0);
            expect (! timeRange.isEmpty());
            expect (timeRange == originalTimeRange);
        }

        beginTest ("EditTime");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 60.0);

            {
                auto et = EditTime (TimePosition::fromSeconds (0.0));
                expectEquals (toTime (et, ts).inSeconds(), 0.0);
                expectEquals (toBeats (et, ts).inBeats(), 0.0);
                expectEquals ((toTime (et, ts) + 4s).inSeconds(), 4.0);
                expectEquals (toBeats ((toTime (et, ts) + 4s), ts).inBeats(), 4.0);

                expectEquals ((toTime (et, ts) - TimeDuration::fromSeconds (4.0)).inSeconds(), -4.0);
                expectEquals ((toTime (et, ts) - 4s).inSeconds(), -4.0);
            }

            {
                auto et = EditTime (TimePosition::fromSeconds (8.0));
                expectEquals (toTime (et, ts).inSeconds(), 8.0);
                expectEquals (toBeats (et, ts).inBeats(), 8.0);

                expectEquals ((toTime (et, ts) + 4s).inSeconds(), 12.0);
            }

            {
                auto et = EditTime (BeatPosition::fromBeats (8.0));
                expectEquals (toTime (et, ts).inSeconds(), 8.0);
                expectEquals (toBeats (et, ts).inBeats(), 8.0);

                expectEquals ((toTime (et, ts) + 4s).inSeconds(), 12.0);
                expectEquals (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 12.0);

                expectEquals ((toTime (et, ts) - 4s).inSeconds(), 4.0);
                expectEquals (toBeats (toTime (et, ts) - 4s, ts).inBeats(), 4.0);
            }

            // Change tempo to 120
            ts.getTempo (0)->setBpm (120.0);
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 120.0);

            {
                auto et = EditTime (TimePosition::fromSeconds (0.0));
                expectEquals (toTime (et, ts).inSeconds(), 0.0);
                expectEquals (toBeats (et, ts).inBeats(), 0.0);

                expectEquals ((toTime (et, ts) + 4s).inSeconds(), 4.0);
                expectEquals (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 8.0);

                expectEquals ((toTime (et, ts) - 4s).inSeconds(), -4.0);
                expectEquals (toBeats (toTime (et, ts) - 4s, ts).inBeats(), -8.0);
            }

            {
                auto et = EditTime (TimePosition::fromSeconds (8.0));
                expectEquals (toTime (et, ts).inSeconds(), 8.0);
                expectEquals (toBeats (et, ts).inBeats(), 16.0);

                expectEquals ((toTime (et, ts) + 4s).inSeconds(), 12.0);
                expectEquals (toBeats (toTime (et, ts) + 4s, ts).inBeats(), 24.0);

                expectEquals ((toTime (et, ts) - 4s).inSeconds(), 4.0);
                expectEquals (toBeats (toTime (et, ts) - 4s, ts).inBeats(), 8.0);
            }
        }

        beginTest ("EditTimeRange");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 60.0);

            const auto originalTimeRange = TimeRange (TimePosition::fromSeconds (8.0), TimePosition::fromSeconds (16.0));
            const auto originalBeatRange = toBeats (originalTimeRange, ts);

            {
                EditTimeRange etr (originalTimeRange);
                expect (toTime (etr, ts) == originalTimeRange);
                expect (toBeats (etr, ts) == originalBeatRange);
            }

            {
                EditTimeRange etr (originalBeatRange);
                expect (toTime (etr, ts) == originalTimeRange);
                expect (toBeats (etr, ts) == originalBeatRange);
            }
        }
    }
};

static EditTimeTests editTimeTests;

}} // namespace tracktion { inline namespace engine

#endif // TRACKTION_UNIT_TESTS
