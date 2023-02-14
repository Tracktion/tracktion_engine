/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS_TIME

#include "tracktion_TimeRange.h"

namespace tracktion { inline namespace core
{

//==============================================================================
//==============================================================================
class TimeRangeTests    : public juce::UnitTest
{
public:
    TimeRangeTests()
        : juce::UnitTest ("TimeRange", "tracktion_core")
    {
    }
    
    void runTest() override
    {
        using namespace std::literals;
        
        runTests<TimeRange> ("TimeRange");
        runTests<BeatRange> ("BeatRange");
        runHashingTests();
    }

    template<typename Type>
    Type fromRaw (double t)
    {
        return fromUnderlyingType<Type> (t);
    }

    template<typename RangeType>
    void runTests (juce::StringRef testName)
    {
        using namespace std::literals;

        using PT = typename RangeType::Position;
        using DT = typename RangeType::Duration;

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

    void runHashingTests()
    {
        beginTest ("TimeRange hashing");
        {
            {
                const auto hash1 = std::hash<BeatRange>() (BeatRange (8_bp, 16_bp));
                const auto hash2 = std::hash<BeatRange>() (BeatRange (4_bp, 12_bp));
                expectNotEquals (hash1, hash2);
            }

            {
                std::size_t hash1 = 0, hash2 = 0;
                hash_combine (hash1, BeatRange (8_bp, 16_bp));
                hash_combine (hash2, BeatRange (4_bp, 12_bp));
                expectNotEquals (hash1, hash2);
            }
        }
    }
};

static TimeRangeTests timeRangeTests;

}} // namespace tracktion

#endif //TRACKTION_UNIT_TESTS_TIME
