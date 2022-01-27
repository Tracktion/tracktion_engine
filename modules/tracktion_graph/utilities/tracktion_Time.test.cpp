/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_TIME

#include "tracktion_Time.h"

namespace tracktion_graph
{

struct TimelineClock
{
    typedef std::chrono::duration<double, std::chrono::seconds::period> duration;
    typedef duration::rep                                               rep;
    typedef duration::period                                            period;
    typedef std::chrono::time_point<TimelineClock>                      time_point;
    static const bool is_steady =                                       false;

    static time_point now() noexcept
    {
        return {};
    }
};

using TimelinePoint = std::chrono::time_point<TimelineClock>;
using Duration = TimelinePoint::duration;


//==============================================================================
//==============================================================================
class TimeTests : public juce::UnitTest
{
public:
    TimeTests()
        : juce::UnitTest ("Time", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        using namespace tracktion_graph;
        using namespace std::literals;
        
        beginTest ("Timeline Point");
        {
            {
                TimelinePoint p;
                const auto t = p.time_since_epoch();
                expect (t.count() == 0.0);
                expect (t == std::chrono::seconds (0));
                expect (t == 0s);
            }

            {
                TimelinePoint p { Duration (1.0) };
                const auto t = p.time_since_epoch();
                expect (t.count() == 1.0);
                expect (t == std::chrono::seconds (1));
                expect (t == std::chrono::milliseconds (1000));
                expect (t == 1s);
            }

            {
                TimelinePoint p { 1.0s };
                const auto t = p.time_since_epoch();
                expect (t.count() == 1.0);
                expect (t == std::chrono::seconds (1));
                expect (t == std::chrono::milliseconds (1000));
                expect (t == 1s);
            }

            {
                TimelinePoint p1;
                const auto p2 = p1 + Duration (1.0);
                const auto p3 = p2 + Duration (1.0);
                
                p1 += Duration (1.0);

                expect (p1 == p2);

                const auto t1 = p1.time_since_epoch();
                expect (t1.count() == 1.0);
                expect (t1 == std::chrono::seconds (1));
                expect (t1 == std::chrono::milliseconds (1000));
                expect (t1 == 1s);

                const auto t3 = p3.time_since_epoch();
                expect (t3.count() == 2.0);
                expect (t3 == std::chrono::seconds (2));
                expect (t3 == std::chrono::milliseconds (2000));
                expect (t3 == 2s);
            }
            
            {
                TimelinePoint p (Duration (-0.5));

                const auto t = p.time_since_epoch();
                expect (t.count() == -0.5);
                expect (t == Duration (-0.5));
                expect (t == std::chrono::milliseconds (-500));
                expect (t == -(0.5s));
                expect (t == -0.5s);
            }

            {
                TimelinePoint p (-0.5s);

                const auto t = p.time_since_epoch();
                expect (t.count() == -0.5);
                expect (t == Duration (-0.5));
                expect (t == std::chrono::milliseconds (-500));
                expect (t == -(0.5s));
                expect (t == -0.5s);
                expect (std::chrono::duration<double> (t).count() == -0.5);
                expect (std::chrono::duration_cast<std::chrono::seconds> (t).count() == 0); // std::chrono::seconds is an integer rep
                expect (std::chrono::duration_cast<Duration> (t).count() == -0.5);
                expect (std::chrono::duration_cast<std::chrono::milliseconds> (t).count() == -500);
            }
        }
        
        beginTest ("Duration");
        {
            const Duration d1 (1.0);
            const Duration d2 (0.5);
            const auto d3 = d1 + d2;
            
            expect (d3.count() == 1.5);
            expect (d3 == 1.5s);
            expect (d3 == 1500ms);
            expect (d3 == std::chrono::milliseconds (1500));
            expect (std::chrono::duration<double> (d3).count() == 1.5);
            expect (std::chrono::duration_cast<std::chrono::seconds> (d3).count() == 1); // std::chrono::seconds is an integer rep
            expect (std::chrono::duration_cast<std::chrono::milliseconds> (d3).count() == 1500);
            expect (std::chrono::duration_cast<std::chrono::milliseconds> (d3).count() == 1500.0);
        }

        beginTest ("TimePosition");
        {
            expect (TimePosition() == TimePosition());
            expect (TimePosition::fromSeconds (42.0) == TimePosition::fromSeconds (42.0));
            expect (TimePosition::fromSeconds (-42.0) == TimePosition::fromSeconds (-42.0));
            expect (TimePosition() != TimePosition::fromSeconds (42.0));
            expect (TimePosition::fromSeconds (-42.0) != TimePosition::fromSeconds (42.0));

            // pods
            expectEquals (TimePosition().inSeconds(), 0.0);
            expectEquals (TimePosition::fromSeconds (0.5).inSeconds(), 0.5);
            expectEquals (TimePosition::fromSeconds (0.5f).inSeconds(), 0.5);
            expectEquals (TimePosition::fromSeconds (42).inSeconds(), 42.0);
            expectEquals (TimePosition::fromSeconds (42u).inSeconds(), 42.0);

            expect (TimePosition() == TimePosition (TimePosition()));
            expect (TimePosition::fromSeconds (0.5) == TimePosition (TimePosition::fromSeconds (0.5)));
            expect (TimePosition::fromSeconds (0.5f) == TimePosition (TimePosition::fromSeconds (0.5f)));
            expect (TimePosition::fromSeconds (42) == TimePosition (TimePosition::fromSeconds (42)));
            expect (TimePosition::fromSeconds (42u) == TimePosition (TimePosition::fromSeconds (42u)));
            expect (TimePosition::fromSeconds (42) != TimePosition (TimePosition::fromSeconds (24)));

            expectEquals (TimePosition::fromSeconds (-0.5).inSeconds(), -0.5);
            expectEquals (TimePosition::fromSeconds (-0.5f).inSeconds(), -0.5);
            expectEquals (TimePosition::fromSeconds (-42).inSeconds(), -42.0);

            // Chrono
            expectEquals (TimePosition (std::chrono::seconds (45)).inSeconds(), 45.0); // std::chrono::seconds is an integer rep
            expectEquals (TimePosition (std::chrono::milliseconds (1000)).inSeconds(), 1.0);
            expectEquals (TimePosition (std::chrono::duration<double> (3.5)).inSeconds(), 3.5);

            // Chrono literals
            expectEquals (TimePosition (1h).inSeconds(), 3600.0);
            expectEquals (TimePosition (1min).inSeconds(), 60.0);
            expectEquals (TimePosition (45s).inSeconds(), 45.0);
            expectEquals (TimePosition (1234ms).inSeconds(), 1.234);

            // Samples
            {
                expectEquals (TimePosition::fromSamples (44100, 44100.0).inSeconds(), 1.0);
                expectEquals (TimePosition::fromSamples (22050, 44100.0).inSeconds(), 0.5);
                expectEquals (TimePosition::fromSamples (-4'032'000, 96000.0).inSeconds(), -42.0);
                expectEquals (TimePosition::fromSamples (-44100, 88200.0).inSeconds(), -0.5);

                expectEquals (toSamples (TimePosition::fromSamples (44100, 44100.0), 44100.0), (int64_t) 44100);
                expectEquals (toSamples (TimePosition::fromSamples (22050, 44100.0), 44100.0), (int64_t) 22050);
                expectEquals (toSamples (TimePosition::fromSamples (-4'032'000, 96000.0), 96000.0), (int64_t) -4'032'000);
                expectEquals (toSamples (TimePosition::fromSamples (-44100, 88200.0), 88200.0), (int64_t) -44100);
            }
        }

        beginTest ("TimeDuration");
        {
            expect (TimeDuration() == TimeDuration());
            expect (TimeDuration::fromSeconds (42.0) == TimeDuration::fromSeconds (42.0));
            expect (TimeDuration::fromSeconds (-42.0) == TimeDuration::fromSeconds (-42.0));
            expect (TimeDuration() != TimeDuration::fromSeconds (42.0));
            expect (TimeDuration::fromSeconds (-42.0) != TimeDuration::fromSeconds (42.0));

            expect (TimeDuration() == TimeDuration (TimeDuration()));
            expect (TimeDuration::fromSeconds (0.5) == TimeDuration (TimeDuration::fromSeconds (0.5)));
            expect (TimeDuration::fromSeconds (0.5f) == TimeDuration (TimeDuration::fromSeconds (0.5f)));
            expect (TimeDuration::fromSeconds (42) == TimeDuration (TimeDuration::fromSeconds (42)));
            expect (TimeDuration::fromSeconds (42u) == TimeDuration (TimeDuration::fromSeconds (42u)));
            expect (TimeDuration::fromSeconds (42) != TimeDuration (TimeDuration::fromSeconds (24)));

            // pods
            expectEquals (TimeDuration().inSeconds(), 0.0);
            expectEquals (TimeDuration::fromSeconds (0.5).inSeconds(), 0.5);
            expectEquals (TimeDuration::fromSeconds (0.5f).inSeconds(), 0.5);
            expectEquals (TimeDuration::fromSeconds (42).inSeconds(), 42.0);
            expectEquals (TimeDuration::fromSeconds (42u).inSeconds(), 42.0);

            expectEquals (TimeDuration::fromSeconds (-0.5).inSeconds(), -0.5);
            expectEquals (TimeDuration::fromSeconds (-0.5f).inSeconds(), -0.5);
            expectEquals (TimeDuration::fromSeconds (-42).inSeconds(), -42.0);

            // Chrono
            expectEquals (TimeDuration (std::chrono::seconds (45)).inSeconds(), 45.0); // std::chrono::seconds is an integer rep
            expectEquals (TimeDuration (std::chrono::milliseconds (1000)).inSeconds(), 1.0);
            expectEquals (TimeDuration (std::chrono::duration<double> (3.5)).inSeconds(), 3.5);

            // Chrono literals
            expectEquals (TimeDuration (1h).inSeconds(), 3600.0);
            expectEquals (TimeDuration (1min).inSeconds(), 60.0);
            expectEquals (TimeDuration (45s).inSeconds(), 45.0);
            expectEquals (TimeDuration (1234ms).inSeconds(), 1.234);

            // Samples
            {
                expectEquals (TimeDuration::fromSamples (44100, 44100.0).inSeconds(), 1.0);
                expectEquals (TimeDuration::fromSamples (22050, 44100.0).inSeconds(), 0.5);
                expectEquals (TimeDuration::fromSamples (-4'032'000, 96000.0).inSeconds(), -42.0);
                expectEquals (TimeDuration::fromSamples (-44100, 88200.0).inSeconds(), -0.5);

                expectEquals (toSamples (TimeDuration::fromSamples (44100, 44100.0), 44100.0), (int64_t) 44100);
                expectEquals (toSamples (TimeDuration::fromSamples (22050, 44100.0), 44100.0), (int64_t) 22050);
                expectEquals (toSamples (TimeDuration::fromSamples (-4'032'000, 96000.0), 96000.0), (int64_t) -4'032'000);
                expectEquals (toSamples (TimeDuration::fromSamples (-44100, 88200.0), 88200.0), (int64_t) -44100);
            }
        }

        beginTest ("Time addition/subtraction");
        {
            expectEquals ((TimeDuration (2s) + TimeDuration (2s)).inSeconds(), 4.0);
            expectEquals ((TimeDuration (2s) - TimeDuration (2s)).inSeconds(), 0.0);
            expectEquals ((TimeDuration (2s) - TimeDuration (4s)).inSeconds(), -2.0);

            expectEquals ((TimePosition (2s) + TimeDuration (2s)).inSeconds(), 4.0);
            expectEquals ((TimePosition (2s) - TimeDuration (2s)).inSeconds(), 0.0);
            expectEquals ((TimePosition (2s) - TimeDuration (4s)).inSeconds(), -2.0);
        }

        beginTest ("BeatPosition");
        {
            expect (BeatPosition() == BeatPosition());
            expect (BeatPosition::fromBeats (42.0) == BeatPosition::fromBeats (42.0));
            expect (BeatPosition::fromBeats (-42.0) == BeatPosition::fromBeats (-42.0));
            expect (BeatPosition() != BeatPosition::fromBeats (42.0));
            expect (BeatPosition::fromBeats (-42.0) != BeatPosition::fromBeats (42.0));

            expect (BeatPosition() == BeatPosition (BeatPosition()));
            expect (BeatPosition::fromBeats (0.5) == BeatPosition (BeatPosition::fromBeats (0.5)));
            expect (BeatPosition::fromBeats (0.5f) == BeatPosition (BeatPosition::fromBeats (0.5f)));
            expect (BeatPosition::fromBeats (42) == BeatPosition (BeatPosition::fromBeats (42)));
            expect (BeatPosition::fromBeats (42u) == BeatPosition (BeatPosition::fromBeats (42u)));
            expect (BeatPosition::fromBeats (42) != BeatPosition (BeatPosition::fromBeats (24)));

            expectEquals (BeatPosition().inBeats(), 0.0);
            expectEquals (BeatPosition::fromBeats (0.5).inBeats(), 0.5);
            expectEquals (BeatPosition::fromBeats (0.5f).inBeats(), 0.5);
            expectEquals (BeatPosition::fromBeats (42).inBeats(), 42.0);
            expectEquals (BeatPosition::fromBeats (42u).inBeats(), 42.0);
        }

        beginTest ("BeatDuration");
        {
            expect (BeatDuration() == BeatDuration());
            expect (BeatDuration::fromBeats (42.0) == BeatDuration::fromBeats (42.0));
            expect (BeatDuration::fromBeats (-42.0) == BeatDuration::fromBeats (-42.0));
            expect (BeatDuration() != BeatDuration::fromBeats (42.0));
            expect (BeatDuration::fromBeats (-42.0) != BeatDuration::fromBeats (42.0));

            expect (BeatDuration() == BeatDuration (BeatDuration()));
            expect (BeatDuration::fromBeats (0.5) == BeatDuration (BeatDuration::fromBeats (0.5)));
            expect (BeatDuration::fromBeats (0.5f) == BeatDuration (BeatDuration::fromBeats (0.5f)));
            expect (BeatDuration::fromBeats (42) == BeatDuration (BeatDuration::fromBeats (42)));
            expect (BeatDuration::fromBeats (42u) == BeatDuration (BeatDuration::fromBeats (42u)));
            expect (BeatDuration::fromBeats (42) != BeatDuration (BeatDuration::fromBeats (24)));

            expectEquals (BeatDuration().inBeats(), 0.0);
            expectEquals (BeatDuration::fromBeats (0.5).inBeats(), 0.5);
            expectEquals (BeatDuration::fromBeats (0.5f).inBeats(), 0.5);
            expectEquals (BeatDuration::fromBeats (42).inBeats(), 42.0);
            expectEquals (BeatDuration::fromBeats (42u).inBeats(), 42.0);
            expectEquals (BeatDuration::fromBeats (-0.5).inBeats(), -0.5);
            expectEquals (BeatDuration::fromBeats (-0.5f).inBeats(), -0.5);
            expectEquals (BeatDuration::fromBeats (-42).inBeats(), -42.0);
        }

        beginTest ("Beat addition/subtraction");
        {
            expectEquals ((BeatDuration::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            expectEquals ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            expectEquals ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);

            expectEquals ((BeatPosition::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            expectEquals ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            expectEquals ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);
        }

        beginTest ("Beat addition/subtraction");
        {
            expectEquals ((BeatDuration::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            expectEquals ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            expectEquals ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);

            expectEquals ((BeatPosition::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            expectEquals ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            expectEquals ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);
        }
    }
};

static TimeTests timeTests;

} // namespace tracktion_graph

#endif //GRAPH_UNIT_TESTS_TIME
