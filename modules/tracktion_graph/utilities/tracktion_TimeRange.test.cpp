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

        runTests<TimeRange> ("TimeRange");
    }

    template<typename Type>
    Type fromRaw (double t)
    {
        return fromUnderlyingType<Type> (t);
    }


    template<typename RangeType>
    void runTests (juce::String testName)
    {
        using namespace tracktion_graph;
        using namespace std::literals;

        using PT = typename RangeType::PositionType;
        using DT = typename RangeType::DurationType;

        beginTest (testName);
        {
            {
                const auto r = RangeType();
                expectEquals (r.getStart(), PT());
                expectEquals (r.getEnd(), PT());
                expectEquals (r.getLength(), DT());
                expectEquals (r.getCentre(), PT());
                expectEquals (r.clipPosition (fromRaw<PT> (1.0)), PT());
                expectEquals (r.clipPosition (fromRaw<PT> (-1.0)), PT());

                expect (r.isEmpty());
                expect (! r.overlaps ({ fromRaw<PT> (0.0), fromRaw<PT> (2.0) }));
                expect (! r.overlaps ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
                expect (! r.contains ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
                expect (! r.contains ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
                expect (! r.containsInclusive (fromRaw<PT> (1.0)));
                expect (r.containsInclusive (fromRaw<PT> (0.0)));

                expect (r.getUnionWith ({ fromRaw<PT> (-1.0), fromRaw<PT> (1.0) })
                        == RangeType (fromRaw<PT> (-1.0), fromRaw<PT> (1.0)));
                expect (r.getIntersectionWith ({ fromRaw<PT> (-1.0), fromRaw<PT> (-1.0) })
                        == RangeType());
                expect (r.rescaled (PT(), 2.0) == RangeType());
                expect (r.constrainRange ({ fromRaw<PT> (-1.0), fromRaw<PT> (-1.0) })
                        == RangeType());
                expect (r.expanded (fromRaw<DT> (1.0))
                        == RangeType (fromRaw<PT> (-1.0), fromRaw<PT> (1.0)));
                expect (r.reduced (fromRaw<DT> (1.0))
                        == RangeType());

                expect (r.movedToStartAt (fromRaw<PT> (1.0))
                        == RangeType::emptyRange (fromRaw<PT> (1.0)));
                expect (r.movedToEndAt (fromRaw<PT> (1.0))
                        == RangeType::emptyRange (fromRaw<PT> (1.0)));
                expect (r.withStart (fromRaw<PT> (-1.0))
                        == RangeType (fromRaw<PT> (-1.0), PT()));
                expect (r.withEnd (fromRaw<PT> (1.0))
                        == RangeType (PT(), fromRaw<PT> (1.0)));
            }

            {
                const auto r = RangeType ({ PT(), fromRaw<PT> (1.0) });
                expect (RangeType (r) == r);
                expect ((RangeType() = r) == r);

                expect (RangeType (fromRaw<PT> (1.0), fromRaw<PT> (2.0))
                        == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (1.0)));
                expect (RangeType::between (fromRaw<PT> (2.0), fromRaw<PT> (1.0))
                        == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (1.0)));
                expect (RangeType::emptyRange (fromRaw<PT> (1.0))
                        == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (0.0)));
            }
        }
    }
};

static TimeRangeTests timeRangeTests;

} // namespace tracktion_graph

#endif //GRAPH_UNIT_TESTS_TIME
