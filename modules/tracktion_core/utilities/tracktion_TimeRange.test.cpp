/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_TIME

#include "tracktion_TimeRange.h"

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline core {

namespace
{
    template<typename Type>
    Type fromRaw (double t)
    {
        return fromUnderlyingType<Type> (t);
    }

    template<typename RangeType>
    void runRangeTests()
    {
        using namespace std::literals;

        using PT = typename RangeType::Position;
        using DT = typename RangeType::Duration;

        {
            const auto r = RangeType();
            CHECK_EQ (r.getStart(), PT());
            CHECK_EQ (r.getEnd(), PT());
            CHECK_EQ (r.getLength(), DT());
            CHECK_EQ (r.getCentre(), PT());
            CHECK_EQ (r.clipPosition (fromRaw<PT> (1.0)), PT());
            CHECK_EQ (r.clipPosition (fromRaw<PT> (-1.0)), PT());

            CHECK (r.isEmpty());
            CHECK (! r.overlaps ({ fromRaw<PT> (0.0), fromRaw<PT> (2.0) }));
            CHECK (! r.overlaps ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
            CHECK (! r.contains ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
            CHECK (! r.contains ({ fromRaw<PT> (1.0), fromRaw<PT> (2.0) }));
            CHECK (! r.containsInclusive (fromRaw<PT> (1.0)));
            CHECK (r.containsInclusive (fromRaw<PT> (0.0)));

            CHECK (r.getUnionWith ({ fromRaw<PT> (-1.0), fromRaw<PT> (1.0) })
                    == RangeType (fromRaw<PT> (-1.0), fromRaw<PT> (1.0)));
            CHECK (r.getIntersectionWith ({ fromRaw<PT> (-1.0), fromRaw<PT> (-1.0) })
                    == RangeType());
            CHECK (r.rescaled (PT(), 2.0) == RangeType());
            CHECK (r.constrainRange ({ fromRaw<PT> (-1.0), fromRaw<PT> (-1.0) })
                    == RangeType());
            CHECK (r.expanded (fromRaw<DT> (1.0))
                    == RangeType (fromRaw<PT> (-1.0), fromRaw<PT> (1.0)));
            CHECK (r.reduced (fromRaw<DT> (1.0))
                    == RangeType());

            CHECK (r.movedToStartAt (fromRaw<PT> (1.0))
                    == RangeType::emptyRange (fromRaw<PT> (1.0)));
            CHECK (r.movedToEndAt (fromRaw<PT> (1.0))
                    == RangeType::emptyRange (fromRaw<PT> (1.0)));
            CHECK (r.withStart (fromRaw<PT> (-1.0))
                    == RangeType (fromRaw<PT> (-1.0), PT()));
            CHECK (r.withEnd (fromRaw<PT> (1.0))
                    == RangeType (PT(), fromRaw<PT> (1.0)));
        }

        {
            const auto r = RangeType ({ PT(), fromRaw<PT> (1.0) });
            CHECK (RangeType (r) == r);
            CHECK ((RangeType() = r) == r);

            CHECK (RangeType (fromRaw<PT> (1.0), fromRaw<PT> (2.0))
                    == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (1.0)));
            CHECK (RangeType::between (fromRaw<PT> (2.0), fromRaw<PT> (1.0))
                    == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (1.0)));
            CHECK (RangeType::emptyRange (fromRaw<PT> (1.0))
                    == RangeType (fromRaw<PT> (1.0), fromRaw<DT> (0.0)));
        }
    }
}

TEST_SUITE ("tracktion_core")
{
    TEST_CASE ("TimeRange")
    {
        SUBCASE ("TimeRange")
        {
            runRangeTests<TimeRange>();
        }

        SUBCASE ("BeatRange")
        {
            runRangeTests<BeatRange>();
        }

        SUBCASE ("TimeRange hashing")
        {
            {
                const auto hash1 = std::hash<BeatRange>() (BeatRange (8_bp, 16_bp));
                const auto hash2 = std::hash<BeatRange>() (BeatRange (4_bp, 12_bp));
                CHECK_NE (hash1, hash2);
            }

            {
                std::size_t hash1 = 0, hash2 = 0;
                hash_combine (hash1, BeatRange (8_bp, 16_bp));
                hash_combine (hash2, BeatRange (4_bp, 12_bp));
                CHECK_NE (hash1, hash2);
            }
        }
    }
}

} // namespace tracktion::inline core
#endif //TRACKTION_UNIT_TESTS_TIME
