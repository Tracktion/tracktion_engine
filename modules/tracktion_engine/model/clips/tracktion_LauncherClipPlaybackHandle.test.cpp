/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCHER_CLIP_PLAYBACK_HANDLE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("LauncherClipPlaybackHandle: One shot")
{
    auto h = LauncherClipPlaybackHandle::forOneShot ({ 2_bp, 4_bd });
    CHECK (! h.getStart());
    CHECK (! h.getProgress (0_bp));
    CHECK (! h.getProgress (4_bp));
    CHECK (! h.getProgress (8_bp));

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 2_bp, 2.5_bp });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    // One shot: start from 0
    h.start (0_bp);
    CHECK (h.getStart() == 0_bp);
    CHECK (h.getProgress (0_bp).has_value());
    CHECK (! h.getProgress (-1_bp));
    CHECK (! h.getProgress (5_bp));
    CHECK (std::abs (*h.getProgress (0_bp) - 0.0f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (1_bp) - 0.25f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (2_bp) - 0.5f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (3_bp) - 0.75f) <= 0.001f);
    CHECK (! h.getProgress (4_bp));

    {
        const auto s = h.timelineRangeToClipSourceRange ({ -1_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ -0.25_bp, 0.5_bd });
        CHECK (! s.playing1);
        CHECK (s.range1 == BeatRange (1.75_bp, 2.0_bp));
        CHECK (s.playing2);
        CHECK (s.range2 == BeatRange (2.0_bp, 2.25_bp));
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (3.0_bp, 3.5_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 3.75_bp, 0.5_bd });
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (5.75_bp, 6.0_bp));
        CHECK (! s.playing2);
        CHECK (s.range2 == BeatRange (6.0_bp, 6.25_bp));
    }

    // One shot: stop
    h.stop();
    CHECK (! h.getStart());
    CHECK (! h.getProgress (2_bp));

    {
        const auto s = h.timelineRangeToClipSourceRange ({ -1_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ -0.25_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 3.75_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    // One shot: start from 2
    h.start (2_bp);
    CHECK (h.getStart() == 2_bp);
    CHECK (! h.getProgress (0_bp));
    CHECK (! h.getProgress (-1_bp));
    CHECK (! h.getProgress (7_bp));
    CHECK (std::abs (*h.getProgress (2_bp) - 0.0f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (3_bp) - 0.25f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (4_bp) - 0.5f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (5_bp) - 0.75f) <= 0.001f);
    CHECK (! h.getProgress (6_bp));

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.75_bp, 0.5_bd });
        CHECK (! s.playing1);
        CHECK (s.range1 == BeatRange (1.75_bp, 2.0_bp));
        CHECK (s.playing2);
        CHECK (s.range2 == BeatRange (2.0_bp, 2.25_bp));
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 3.0_bp, 0.5_bd });
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (3.0_bp, 3.5_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 5.75_bp, 0.5_bd });
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (5.75_bp, 6.0_bp));
        CHECK (! s.playing2);
        CHECK (s.range2 == BeatRange (6.0_bp, 6.25_bp));
    }
}

TEST_CASE ("LauncherClipPlaybackHandle: Looping")
{
    // 0...1...2...3...4...5...6...7...8...9...10..11..
    //     {   [           |               |
    //     3   4   5   6   3   4   5   6   3

    auto h = LauncherClipPlaybackHandle::forLooping ({ 3_bp, 4_bd }, 1_bd);
    CHECK (! h.getStart());
    CHECK (! h.getProgress (0_bp));
    CHECK (! h.getProgress (4_bp));
    CHECK (! h.getProgress (8_bp));

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 0.75_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    // Looping: start from 2
    h.start (2_bp);
    CHECK (h.getStart() == 2_bp);
    CHECK (! h.getProgress (-1_bp));
    CHECK (! h.getProgress (0_bp));
    CHECK (! h.getProgress (1_bp));

    CHECK (std::abs (*h.getProgress (2_bp) - 0.25f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (3_bp) - 0.5f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (4_bp) - 0.75f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (5_bp) - 0.0f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (6_bp) - 0.25f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (7_bp) - 0.5f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (8_bp) - 0.75f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (9_bp) - 0.0f) <= 0.001f);
    CHECK (std::abs (*h.getProgress (10_bp) - 0.25f) <= 0.001f);

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 0.75_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
        CHECK (s.range1.isEmpty());
        CHECK (! s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 2.0_bd });
        CHECK (s.range1 == BeatRange (3_bp, 4_bp));
        CHECK (! s.playing1);
        CHECK (s.range2 == BeatRange (4_bp, 5_bp));
        CHECK (s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 3.0_bp, 1.0_bd });
        CHECK (s.range1 == BeatRange (5_bp, 6_bp));
        CHECK (s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 4.0_bp, 2.0_bd });
        CHECK (s.range1 == BeatRange (6_bp, 7_bp));
        CHECK (s.playing1);
        CHECK (s.range2 == BeatRange (3_bp, 4_bp));
        CHECK (s.playing2);
    }

    {
        const auto s = h.timelineRangeToClipSourceRange ({ 6.0_bp, 1.0_bd });
        CHECK (s.range1 == BeatRange (4_bp, 5_bp));
        CHECK (s.playing1);
        CHECK (s.range2.isEmpty());
        CHECK (! s.playing2);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

 #endif
