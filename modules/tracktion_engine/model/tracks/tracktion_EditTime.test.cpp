/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

namespace tracktion_engine
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
        using namespace tracktion_graph;
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

        beginTest ("EditTime");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto& ts = edit->tempoSequence;

            // Tempo starts at 60
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 60.0);

            {
                auto tp = TimePosition::fromSeconds (0.0);
                auto et = EditTime (tp, ts);
                expectEquals (et.toTime().inSeconds(), 0.0);
                expectEquals (et.toBeats().inBeats(), 0.0);

                expectEquals ((et + TimeDuration::fromSeconds (4.0)).toTime().inSeconds(), 4.0);
                expectEquals ((et + 4s).toTime().inSeconds(), 4.0);
                expectEquals ((et + 4s).toBeats().inBeats(), 4.0);

                expectEquals ((et - TimeDuration::fromSeconds (4.0)).toTime().inSeconds(), -4.0);
                expectEquals ((et - 4s).toTime().inSeconds(), -4.0);
                expectEquals ((et - 4s).toBeats().inBeats(), -4.0);
            }

            {
                auto tp = TimePosition::fromSeconds (8.0);
                auto et = EditTime (tp, ts);
                expectEquals (et.toTime().inSeconds(), 8.0);
                expectEquals (et.toBeats().inBeats(), 8.0);

                expectEquals ((et + 4s).toTime().inSeconds(), 12.0);
                expectEquals ((et + 4s).toBeats().inBeats(), 12.0);
            }

            {
                auto bp = BeatPosition::fromBeats (8.0);
                auto et = EditTime (bp, ts);
                expectEquals (et.toTime().inSeconds(), 8.0);
                expectEquals (et.toBeats().inBeats(), 8.0);

                expectEquals ((et + 4s).toTime().inSeconds(), 12.0);
                expectEquals ((et + 4s).toBeats().inBeats(), 12.0);

                expectEquals ((et - 4s).toTime().inSeconds(), 4.0);
                expectEquals ((et - 4s).toBeats().inBeats(), 4.0);
            }

            // Change tempo to 120
            ts.getTempo (0)->setBpm (120.0);
            expectEquals (ts.getNumTempos(), 1);
            expectEquals (ts.getTempo (0)->getBpm(), 120.0);

            {
                auto tp = TimePosition::fromSeconds (0.0);
                auto et = EditTime (tp, ts);
                expectEquals (et.toTime().inSeconds(), 0.0);
                expectEquals (et.toBeats().inBeats(), 0.0);

                expectEquals ((et + 4s).toTime().inSeconds(), 4.0);
                expectEquals ((et + 4s).toBeats().inBeats(), 8.0);

                expectEquals ((et - 4s).toTime().inSeconds(), -4.0);
                expectEquals ((et - 4s).toBeats().inBeats(), -8.0);
            }

            {
                auto tp = TimePosition::fromSeconds (8.0);
                auto et = EditTime (tp, ts);
                expectEquals (et.toTime().inSeconds(), 8.0);
                expectEquals (et.toBeats().inBeats(), 16.0);

                expectEquals ((et + 4s).toTime().inSeconds(), 12.0);
                expectEquals ((et + 4s).toBeats().inBeats(), 24.0);

                expectEquals ((et - 4s).toTime().inSeconds(), 4.0);
                expectEquals ((et - 4s).toBeats().inBeats(), 8.0);
            }
        }
    }
};

static EditTimeTests editTimeTests;

} // namespace tracktion_engine

#endif // TRACKTION_UNIT_TESTS
