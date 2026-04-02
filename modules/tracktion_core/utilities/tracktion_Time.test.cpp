/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS_TIME

#include "tracktion_Time.h"

namespace tracktion::inline core {

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

} // namespace tracktion::inline core

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline core {

TEST_SUITE ("tracktion_core")
{
    TEST_CASE ("Time")
    {
        using namespace std::literals;

        SUBCASE ("Timeline Point")
        {
            {
                TimelinePoint p;
                const auto t = p.time_since_epoch();
                CHECK (t.count() == 0.0);
                CHECK (t == std::chrono::seconds (0));
                CHECK (t == 0s);
            }

            {
                TimelinePoint p { Duration (1.0) };
                const auto t = p.time_since_epoch();
                CHECK (t.count() == 1.0);
                CHECK (t == std::chrono::seconds (1));
                CHECK (t == std::chrono::milliseconds (1000));
                CHECK (t == 1s);
            }

            {
                TimelinePoint p { 1.0s };
                const auto t = p.time_since_epoch();
                CHECK (t.count() == 1.0);
                CHECK (t == std::chrono::seconds (1));
                CHECK (t == std::chrono::milliseconds (1000));
                CHECK (t == 1s);
            }

            {
                TimelinePoint p1;
                const auto p2 = p1 + Duration (1.0);
                const auto p3 = p2 + Duration (1.0);

                p1 += Duration (1.0);

                CHECK (p1 == p2);

                const auto t1 = p1.time_since_epoch();
                CHECK (t1.count() == 1.0);
                CHECK (t1 == std::chrono::seconds (1));
                CHECK (t1 == std::chrono::milliseconds (1000));
                CHECK (t1 == 1s);

                const auto t3 = p3.time_since_epoch();
                CHECK (t3.count() == 2.0);
                CHECK (t3 == std::chrono::seconds (2));
                CHECK (t3 == std::chrono::milliseconds (2000));
                CHECK (t3 == 2s);
            }

            {
                TimelinePoint p (Duration (-0.5));

                const auto t = p.time_since_epoch();
                CHECK (t.count() == -0.5);
                CHECK (t == Duration (-0.5));
                CHECK (t == std::chrono::milliseconds (-500));
                CHECK (t == -(0.5s));
                CHECK (t == -0.5s);
            }

            {
                TimelinePoint p (-0.5s);

                const auto t = p.time_since_epoch();
                CHECK (t.count() == -0.5);
                CHECK (t == Duration (-0.5));
                CHECK (t == std::chrono::milliseconds (-500));
                CHECK (t == -(0.5s));
                CHECK (t == -0.5s);
                CHECK (std::chrono::duration<double> (t).count() == -0.5);
                CHECK (std::chrono::duration_cast<std::chrono::seconds> (t).count() == 0); // std::chrono::seconds is an integer rep
                CHECK (std::chrono::duration_cast<Duration> (t).count() == -0.5);
                CHECK (std::chrono::duration_cast<std::chrono::milliseconds> (t).count() == -500);
            }
        }

        SUBCASE ("Duration")
        {
            const Duration d1 (1.0);
            const Duration d2 (0.5);
            const auto d3 = d1 + d2;

            CHECK (d3.count() == 1.5);
            CHECK (d3 == 1.5s);
            CHECK (d3 == 1500ms);
            CHECK (d3 == std::chrono::milliseconds (1500));
            CHECK (std::chrono::duration<double> (d3).count() == 1.5);
            CHECK (std::chrono::duration_cast<std::chrono::seconds> (d3).count() == 1); // std::chrono::seconds is an integer rep
            CHECK (std::chrono::duration_cast<std::chrono::milliseconds> (d3).count() == 1500);
            CHECK (std::chrono::duration_cast<std::chrono::milliseconds> (d3).count() == 1500.0);
        }

        SUBCASE ("TimePosition")
        {
            CHECK (TimePosition() == TimePosition());
            CHECK (TimePosition::fromSeconds (42.0) == TimePosition::fromSeconds (42.0));
            CHECK (TimePosition::fromSeconds (-42.0) == TimePosition::fromSeconds (-42.0));
            CHECK (TimePosition() != TimePosition::fromSeconds (42.0));
            CHECK (TimePosition::fromSeconds (-42.0) != TimePosition::fromSeconds (42.0));

            // pods
            CHECK_EQ (TimePosition().inSeconds(), 0.0);
            CHECK_EQ (TimePosition::fromSeconds (0.5).inSeconds(), 0.5);
            CHECK_EQ (TimePosition::fromSeconds (0.5f).inSeconds(), 0.5);
            CHECK_EQ (TimePosition::fromSeconds (42).inSeconds(), 42.0);
            CHECK_EQ (TimePosition::fromSeconds (42u).inSeconds(), 42.0);

            CHECK (TimePosition() == TimePosition (TimePosition()));
            CHECK (TimePosition::fromSeconds (0.5) == TimePosition (TimePosition::fromSeconds (0.5)));
            CHECK (TimePosition::fromSeconds (0.5f) == TimePosition (TimePosition::fromSeconds (0.5f)));
            CHECK (TimePosition::fromSeconds (42) == TimePosition (TimePosition::fromSeconds (42)));
            CHECK (TimePosition::fromSeconds (42u) == TimePosition (TimePosition::fromSeconds (42u)));
            CHECK (TimePosition::fromSeconds (42) != TimePosition (TimePosition::fromSeconds (24)));

            CHECK_EQ (TimePosition::fromSeconds (-0.5).inSeconds(), -0.5);
            CHECK_EQ (TimePosition::fromSeconds (-0.5f).inSeconds(), -0.5);
            CHECK_EQ (TimePosition::fromSeconds (-42).inSeconds(), -42.0);

            // Chrono
            CHECK_EQ (TimePosition (std::chrono::seconds (45)).inSeconds(), 45.0); // std::chrono::seconds is an integer rep
            CHECK_EQ (TimePosition (std::chrono::milliseconds (1000)).inSeconds(), 1.0);
            CHECK_EQ (TimePosition (std::chrono::duration<double> (3.5)).inSeconds(), 3.5);

            // Chrono literals
            CHECK_EQ (TimePosition (1h).inSeconds(), 3600.0);
            CHECK_EQ (TimePosition (1min).inSeconds(), 60.0);
            CHECK_EQ (TimePosition (45s).inSeconds(), 45.0);
            CHECK_EQ (TimePosition (1234ms).inSeconds(), 1.234);
            CHECK (TimePosition (1s) == TimePosition (1.0s));
            CHECK (TimePosition (1s) == TimePosition (1000ms));
            CHECK (TimePosition (1s) + 1s == TimePosition (1000ms) + 1000ms);
            CHECK (TimePosition (1s) + 1s == TimePosition (2000ms));
            CHECK (TimePosition (1s) - 1.0s == TimePosition (0ms));
            CHECK ((0_tp + 1.0e-5s) == TimePosition::fromSeconds (1.0e-5));

            // Samples
            {
                CHECK_EQ (TimePosition::fromSamples (44100, 44100.0).inSeconds(), 1.0);
                CHECK_EQ (TimePosition::fromSamples (22050, 44100.0).inSeconds(), 0.5);
                CHECK_EQ (TimePosition::fromSamples (-4'032'000, 96000.0).inSeconds(), -42.0);
                CHECK_EQ (TimePosition::fromSamples (-44100, 88200.0).inSeconds(), -0.5);

                CHECK_EQ (toSamples (TimePosition::fromSamples (44100, 44100.0), 44100.0), (int64_t) 44100);
                CHECK_EQ (toSamples (TimePosition::fromSamples (22050, 44100.0), 44100.0), (int64_t) 22050);
                CHECK_EQ (toSamples (TimePosition::fromSamples (-4'032'000, 96000.0), 96000.0), (int64_t) -4'032'000);
                CHECK_EQ (toSamples (TimePosition::fromSamples (-44100, 88200.0), 88200.0), (int64_t) -44100);
            }

            // Ordering
            {
                CHECK (TimePosition (1min) > TimePosition());
                CHECK (TimePosition() >= TimePosition::fromSeconds (-42.0));
                CHECK (TimePosition (-8s) > TimePosition (-42s));

                std::vector<TimePosition> v { TimePosition (2min), TimePosition (5min), TimePosition (1min) };
                std::sort (v.begin(), v.end());
                CHECK (v[0] == TimePosition (1min));
                CHECK (v[1] == TimePosition (2min));
                CHECK (v[2] == TimePosition (5min));
            }
        }

        SUBCASE ("TimeDuration")
        {
            CHECK (TimeDuration() == TimeDuration());
            CHECK (TimeDuration::fromSeconds (42.0) == TimeDuration::fromSeconds (42.0));
            CHECK (TimeDuration::fromSeconds (-42.0) == TimeDuration::fromSeconds (-42.0));
            CHECK (TimeDuration() != TimeDuration::fromSeconds (42.0));
            CHECK (TimeDuration::fromSeconds (-42.0) != TimeDuration::fromSeconds (42.0));

            CHECK (TimeDuration() == TimeDuration (TimeDuration()));
            CHECK (TimeDuration::fromSeconds (0.5) == TimeDuration (TimeDuration::fromSeconds (0.5)));
            CHECK (TimeDuration::fromSeconds (0.5f) == TimeDuration (TimeDuration::fromSeconds (0.5f)));
            CHECK (TimeDuration::fromSeconds (42) == TimeDuration (TimeDuration::fromSeconds (42)));
            CHECK (TimeDuration::fromSeconds (42u) == TimeDuration (TimeDuration::fromSeconds (42u)));
            CHECK (TimeDuration::fromSeconds (42) != TimeDuration (TimeDuration::fromSeconds (24)));

            // pods
            CHECK_EQ (TimeDuration().inSeconds(), 0.0);
            CHECK_EQ (TimeDuration::fromSeconds (0.5).inSeconds(), 0.5);
            CHECK_EQ (TimeDuration::fromSeconds (0.5f).inSeconds(), 0.5);
            CHECK_EQ (TimeDuration::fromSeconds (42).inSeconds(), 42.0);
            CHECK_EQ (TimeDuration::fromSeconds (42u).inSeconds(), 42.0);

            CHECK_EQ (TimeDuration::fromSeconds (-0.5).inSeconds(), -0.5);
            CHECK_EQ (TimeDuration::fromSeconds (-0.5f).inSeconds(), -0.5);
            CHECK_EQ (TimeDuration::fromSeconds (-42).inSeconds(), -42.0);

            // Chrono
            CHECK_EQ (TimeDuration (std::chrono::seconds (45)).inSeconds(), 45.0); // std::chrono::seconds is an integer rep
            CHECK_EQ (TimeDuration (std::chrono::milliseconds (1000)).inSeconds(), 1.0);
            CHECK_EQ (TimeDuration (std::chrono::duration<double> (3.5)).inSeconds(), 3.5);

            // Chrono literals
            CHECK_EQ (TimeDuration (1h).inSeconds(), 3600.0);
            CHECK_EQ (TimeDuration (1min).inSeconds(), 60.0);
            CHECK_EQ (TimeDuration (45s).inSeconds(), 45.0);
            CHECK_EQ (TimeDuration (1234ms).inSeconds(), 1.234);

            // Samples
            {
                CHECK_EQ (TimeDuration::fromSamples (44100, 44100.0).inSeconds(), 1.0);
                CHECK_EQ (TimeDuration::fromSamples (22050, 44100.0).inSeconds(), 0.5);
                CHECK_EQ (TimeDuration::fromSamples (-4'032'000, 96000.0).inSeconds(), -42.0);
                CHECK_EQ (TimeDuration::fromSamples (-44100, 88200.0).inSeconds(), -0.5);

                CHECK_EQ (toSamples (TimeDuration::fromSamples (44100, 44100.0), 44100.0), (int64_t) 44100);
                CHECK_EQ (toSamples (TimeDuration::fromSamples (22050, 44100.0), 44100.0), (int64_t) 22050);
                CHECK_EQ (toSamples (TimeDuration::fromSamples (-4'032'000, 96000.0), 96000.0), (int64_t) -4'032'000);
                CHECK_EQ (toSamples (TimeDuration::fromSamples (-44100, 88200.0), 88200.0), (int64_t) -44100);
            }

            // Ordering
            {
                CHECK (TimeDuration (1min) > TimeDuration());
                CHECK (TimeDuration() >= TimeDuration::fromSeconds (-42.0));
                CHECK (TimeDuration::fromSeconds (-8) > TimeDuration::fromSeconds (-42.0));

                std::vector<TimeDuration> v { TimeDuration (-2min), TimeDuration (5min), TimeDuration (1min) };
                std::sort (v.begin(), v.end());
                CHECK (v[0] == TimeDuration (-2min));
                CHECK (v[1] == TimeDuration (1min));
                CHECK (v[2] == TimeDuration (5min));
            }
        }

        SUBCASE ("Time arithmatic")
        {
            CHECK_EQ ((TimePosition (2s) - TimePosition (2s)).inSeconds(), 0.0);
            CHECK_EQ ((TimePosition (0s) - TimePosition (2s)).inSeconds(), -2.0);
            CHECK_EQ ((TimePosition (2s) - TimePosition (4s)).inSeconds(), -2.0);

            CHECK_EQ ((TimeDuration (2s) + TimeDuration (2s)).inSeconds(), 4.0);
            CHECK_EQ ((TimeDuration (2s) - TimeDuration (2s)).inSeconds(), 0.0);
            CHECK_EQ ((TimeDuration (2s) - TimeDuration (4s)).inSeconds(), -2.0);

            CHECK_EQ ((TimePosition (2s) + TimeDuration (2s)).inSeconds(), 4.0);
            CHECK_EQ ((TimePosition (2s) - TimeDuration (2s)).inSeconds(), 0.0);
            CHECK_EQ ((TimePosition (2s) - TimeDuration (4s)).inSeconds(), -2.0);

            CHECK_EQ (1_tp * 2,     2_tp);
            CHECK_EQ (-4_tp * 4,    -16_tp);
            CHECK_EQ (1_tp / 2,     0.5_tp);
            CHECK_EQ (-4_tp / 4,    -1_tp);

            CHECK_EQ (1_td * 2,     2_td);
            CHECK_EQ (-4_td * 4,    -16_td);
            CHECK_EQ (1_td / 2,     0.5_td);
            CHECK_EQ (-4_td / 4,    -1_td);

            CHECK_EQ (1_tp / 2_td,  0.5);
            CHECK_EQ (1_td / 2_td,  0.5);
        }

        SUBCASE ("BeatPosition")
        {
            CHECK (BeatPosition() == BeatPosition());
            CHECK (BeatPosition::fromBeats (42.0) == BeatPosition::fromBeats (42.0));
            CHECK (BeatPosition::fromBeats (-42.0) == BeatPosition::fromBeats (-42.0));
            CHECK (BeatPosition() != BeatPosition::fromBeats (42.0));
            CHECK (BeatPosition::fromBeats (-42.0) != BeatPosition::fromBeats (42.0));

            CHECK (BeatPosition() == BeatPosition (BeatPosition()));
            CHECK (BeatPosition::fromBeats (0.5) == BeatPosition (BeatPosition::fromBeats (0.5)));
            CHECK (BeatPosition::fromBeats (0.5f) == BeatPosition (BeatPosition::fromBeats (0.5f)));
            CHECK (BeatPosition::fromBeats (42) == BeatPosition (BeatPosition::fromBeats (42)));
            CHECK (BeatPosition::fromBeats (42u) == BeatPosition (BeatPosition::fromBeats (42u)));
            CHECK (BeatPosition::fromBeats (42) != BeatPosition (BeatPosition::fromBeats (24)));

            CHECK_EQ (BeatPosition().inBeats(), 0.0);
            CHECK_EQ (BeatPosition::fromBeats (0.5).inBeats(), 0.5);
            CHECK_EQ (BeatPosition::fromBeats (0.5f).inBeats(), 0.5);
            CHECK_EQ (BeatPosition::fromBeats (42).inBeats(), 42.0);
            CHECK_EQ (BeatPosition::fromBeats (42u).inBeats(), 42.0);

            CHECK (BeatPosition::fromBeats (0.5) == 0.5_bp);
            CHECK (BeatPosition::fromBeats (0.5f) == 0.5_bp);
            CHECK (BeatPosition::fromBeats (42) == 42_bp);
            CHECK (BeatPosition::fromBeats (42u) == 42_bp);

            // Ordering
            {
                CHECK (BeatPosition::fromBeats (1) > BeatPosition());
                CHECK (BeatPosition() >= BeatPosition::fromBeats (-42.0));
                CHECK (BeatPosition::fromBeats (-8) > BeatPosition::fromBeats (-42.0));

                std::vector<BeatPosition> v { BeatPosition::fromBeats (-2), BeatPosition::fromBeats (5), BeatPosition::fromBeats (1) };
                std::sort (v.begin(), v.end());
                CHECK (v[0] == BeatPosition::fromBeats (-2));
                CHECK (v[1] == BeatPosition::fromBeats (1));
                CHECK (v[2] == BeatPosition::fromBeats (5));
            }
        }

        SUBCASE ("BeatDuration")
        {
            CHECK (BeatDuration() == BeatDuration());
            CHECK (BeatDuration::fromBeats (42.0) == BeatDuration::fromBeats (42.0));
            CHECK (BeatDuration::fromBeats (-42.0) == BeatDuration::fromBeats (-42.0));
            CHECK (BeatDuration() != BeatDuration::fromBeats (42.0));
            CHECK (BeatDuration::fromBeats (-42.0) != BeatDuration::fromBeats (42.0));

            CHECK (BeatDuration() == BeatDuration (BeatDuration()));
            CHECK (BeatDuration::fromBeats (0.5) == BeatDuration (BeatDuration::fromBeats (0.5)));
            CHECK (BeatDuration::fromBeats (0.5f) == BeatDuration (BeatDuration::fromBeats (0.5f)));
            CHECK (BeatDuration::fromBeats (42) == BeatDuration (BeatDuration::fromBeats (42)));
            CHECK (BeatDuration::fromBeats (42u) == BeatDuration (BeatDuration::fromBeats (42u)));
            CHECK (BeatDuration::fromBeats (42) != BeatDuration (BeatDuration::fromBeats (24)));

            CHECK_EQ (BeatDuration().inBeats(), 0.0);
            CHECK_EQ (BeatDuration::fromBeats (0.5).inBeats(), 0.5);
            CHECK_EQ (BeatDuration::fromBeats (0.5f).inBeats(), 0.5);
            CHECK_EQ (BeatDuration::fromBeats (42).inBeats(), 42.0);
            CHECK_EQ (BeatDuration::fromBeats (42u).inBeats(), 42.0);
            CHECK_EQ (BeatDuration::fromBeats (-0.5).inBeats(), -0.5);
            CHECK_EQ (BeatDuration::fromBeats (-0.5f).inBeats(), -0.5);
            CHECK_EQ (BeatDuration::fromBeats (-42).inBeats(), -42.0);

            CHECK (BeatDuration::fromBeats (0.5) == 0.5_bd);
            CHECK (BeatDuration::fromBeats (0.5f) == 0.5_bd);
            CHECK (BeatDuration::fromBeats (42) == 42_bd);
            CHECK (BeatDuration::fromBeats (42u) == 42_bd);

            // Ordering
            {
                CHECK (BeatDuration::fromBeats (1) > BeatDuration());
                CHECK (BeatDuration() >= BeatDuration::fromBeats (-42.0));
                CHECK (BeatDuration::fromBeats (-8) > BeatDuration::fromBeats (-42.0));

                std::vector<BeatDuration> v { BeatDuration::fromBeats (-2), BeatDuration::fromBeats (5), BeatDuration::fromBeats (1) };
                std::sort (v.begin(), v.end());
                CHECK (v[0] == BeatDuration::fromBeats (-2));
                CHECK (v[1] == BeatDuration::fromBeats (1));
                CHECK (v[2] == BeatDuration::fromBeats (5));
            }
        }

        SUBCASE ("Beat arithmatic")
        {
            CHECK_EQ ((BeatPosition::fromBeats (2.0) - BeatPosition::fromBeats (2.0)).inBeats(), 0.0);
            CHECK_EQ ((BeatPosition::fromBeats (0.0) - BeatPosition::fromBeats (2.0)).inBeats(), -2.0);
            CHECK_EQ ((BeatPosition::fromBeats (2.0) - BeatPosition::fromBeats (4.0)).inBeats(), -2.0);

            CHECK_EQ ((BeatDuration::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            CHECK_EQ ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            CHECK_EQ ((BeatDuration::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);

            CHECK_EQ ((BeatPosition::fromBeats (2.0) + BeatDuration::fromBeats (2.0)).inBeats(), 4.0);
            CHECK_EQ ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (2.0)).inBeats(), 0.0);
            CHECK_EQ ((BeatPosition::fromBeats (2.0) - BeatDuration::fromBeats (4.0)).inBeats(), -2.0);

            CHECK_EQ ((2.0_bp + 2.0_bd).inBeats(), 4.0);
            CHECK_EQ ((2_bp - 2.0_bd).inBeats(), 0.0);
            CHECK_EQ ((2_bp - 4_bd).inBeats(), -2.0);

            CHECK_EQ (1_bp * 2,     2_bp);
            CHECK_EQ (-4_bp * 4,    -16_bp);
            CHECK_EQ (1_bp / 2,     0.5_bp);
            CHECK_EQ (-4_bp / 4,    -1_bp);

            CHECK_EQ (1_bd * 2,     2_bd);
            CHECK_EQ (-4_bd * 4,    -16_bd);
            CHECK_EQ (1_bd / 2,     0.5_bd);
            CHECK_EQ (-4_bd / 4,    -1_bd);

            CHECK_EQ (1_bp / 2_bd,  0.5);
            CHECK_EQ (1_bd / 2_bd,  0.5);
        }

        SUBCASE ("Time hashing")
        {
            const auto hash1 = std::hash<BeatPosition>() (4_bp);
            const auto hash2 = std::hash<BeatPosition>() (8_bp);
            const auto hash3 = std::hash<BeatPosition>() (12_bp);
            const auto hash4 = std::hash<BeatPosition>() (16_bp);

            CHECK_NE (hash1, hash2);
            CHECK_NE (hash2, hash3);
            CHECK_NE (hash3, hash4);
        }
    }
}

} // namespace tracktion::inline core
#endif //TRACKTION_UNIT_TESTS_TIME
