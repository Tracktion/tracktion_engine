/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if GRAPH_UNIT_TESTS_TIME

#include "tracktion_TimeRange.h"

namespace tracktion_graph
{

juce::String& operator<< (juce::String& s, TimeDuration d)
{
    return s << juce::String (d.inSeconds());
}

juce::String& operator<< (juce::String& s, TimePosition p)
{
    return s << juce::String (p.inSeconds());
}

//==============================================================================
//==============================================================================
class TimeRangeTests    : public juce::UnitTest
{
public:
    TimeRangeTests()
        : juce::UnitTest ("TimeRange", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        using namespace tracktion_graph;
        using namespace std::literals;
        
        beginTest ("TimeRange");
        {
            {
                const auto r = TimeRange();
                expectEquals (r.getStart(), TimePosition());
                expectEquals (r.getEnd(), TimePosition());
                expectEquals (r.getLength(), TimeDuration());
                expectEquals (r.getCentre(), TimePosition());
                expectEquals (r.clipPosition (TimePosition (1s)), TimePosition());
                expectEquals (r.clipPosition (TimePosition (-1s)), TimePosition());

                expect (r.isEmpty());
                expect (! r.overlaps ({ TimePosition (0s), TimePosition (2s) }));
                expect (! r.overlaps ({ TimePosition (1s), TimePosition (2s) }));
                expect (! r.contains ({ TimePosition (1s), TimePosition (2s) }));
                expect (! r.contains ({ TimePosition (1s), TimePosition (2s) }));
                expect (! r.containsInclusive (TimePosition (1s)));
                expect (r.containsInclusive (TimePosition (0s)));

                expect (r.getUnionWith ({ TimePosition (-1s), TimePosition (1s) }) == TimeRange (TimePosition (-1s), TimePosition (1s)));
                expect (r.getIntersectionWith ({ TimePosition (-1s), TimePosition (1s) }) == TimeRange());
                expect (r.rescaled (TimePosition(), 2.0) == TimeRange());
                expect (r.constrainRange ({ TimePosition (-1s), TimePosition (1s) }) == TimeRange());
                expect (r.expanded (TimeDuration (1s)) == TimeRange (TimePosition (-1s), TimePosition (1s)));
                expect (r.reduced (TimeDuration (1s)) == TimeRange());

                expect (r.movedToStartAt (TimePosition (1s)) == TimeRange::emptyRange (TimePosition (1s)));
                expect (r.movedToEndAt (TimePosition (1s)) == TimeRange::emptyRange (TimePosition (1s)));
                expect (r.withStart (TimePosition (-1s)) == TimeRange (TimePosition (-1s), TimePosition()));
                expect (r.withEnd (TimePosition (1s)) == TimeRange (TimePosition(), TimePosition (1s)));
            }

            {
                const auto r = TimeRange ({ TimePosition(), TimePosition (1s) });
                expect (TimeRange (r) == r);
                expect ((TimeRange() = r) == r);

                expect (TimeRange (TimePosition (1s), TimePosition (2s))
                        == TimeRange (TimePosition (1s), TimeDuration (1s)));
                expect (TimeRange::between (TimePosition (2s), TimePosition (1s))
                        == TimeRange (TimePosition (1s), TimeDuration (1s)));
                expect (TimeRange::emptyRange (TimePosition (1s))
                        == TimeRange (TimePosition (1s), TimeDuration (0s)));
            }
        }

        beginTest ("BeatRange");
        {

        }
    }
};

static TimeRangeTests timeRangeTests;

} // namespace tracktion_graph

#endif //GRAPH_UNIT_TESTS_TIME
