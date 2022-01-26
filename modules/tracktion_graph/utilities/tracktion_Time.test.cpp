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
    }

private:

};

static TimeTests timeTests;

} // namespace tracktion_graph

#endif //GRAPH_UNIT_TESTS_TIME
