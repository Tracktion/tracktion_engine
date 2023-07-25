/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCHER_CLIP_PLAYBACK_HANDLE

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LauncherClipPlaybackHandleTests : public juce::UnitTest
{
public:
    LauncherClipPlaybackHandleTests()
            : juce::UnitTest ("LauncherClipPlaybackHandle", "tracktion_engine")
    {
    }

    void runTest() override
    {
        runOneShotTests();
        runLoopingTests();
    }

private:
    void runOneShotTests()
    {
        beginTest ("One shot");

        auto h = LauncherClipPlaybackHandle::forOneShot ({ 2_bp, 4_bd });
        expect (! h.getStart());
        expect (! h.getProgress (0_bp));
        expect (! h.getProgress (4_bp));
        expect (! h.getProgress (8_bp));

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 2_bp, 2.5_bp });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        beginTest ("One shot: start from 0");
        h.start (0_bp);
        expect (h.getStart() == 0_bp);
        expect (h.getProgress (0_bp).has_value());
        expect (! h.getProgress (-1_bp));
        expect (! h.getProgress (5_bp));
        expectWithinAbsoluteError (*h.getProgress (0_bp), 0.0f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (1_bp), 0.25f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (2_bp), 0.5f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (3_bp), 0.75f, 0.001f);
        expect (! h.getProgress (4_bp));

        {
            const auto s = h.timelineRangeToClipSourceRange ({ -1_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ -0.25_bp, 0.5_bd });
            expect (! s.playing1);
            expect (s.range1 == BeatRange (1.75_bp, 2.0_bp));
            expect (s.playing2);
            expect (s.range2 == BeatRange (2.0_bp, 2.25_bp));
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
            expect (s.playing1);
            expect (s.range1 == BeatRange (3.0_bp, 3.5_bp));
            expect (! s.playing2);
            expect (s.range2.isEmpty());
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 3.75_bp, 0.5_bd });
            expect (s.playing1);
            expect (s.range1 == BeatRange (5.75_bp, 6.0_bp));
            expect (! s.playing2);
            expect (s.range2 == BeatRange (6.0_bp, 6.25_bp));
        }

        beginTest ("One shot: stop");
        h.stop();
        expect (! h.getStart());
        expect (! h.getProgress (2_bp));

        {
            const auto s = h.timelineRangeToClipSourceRange ({ -1_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ -0.25_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 1.0_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 3.75_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        beginTest ("One shot: start from 2");
        h.start (2_bp);
        expect (h.getStart() == 2_bp);
        expect (! h.getProgress (0_bp));
        expect (! h.getProgress (-1_bp));
        expect (! h.getProgress (7_bp));
        expectWithinAbsoluteError (*h.getProgress (2_bp), 0.0f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (3_bp), 0.25f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (4_bp), 0.5f, 0.001f);
        expectWithinAbsoluteError (*h.getProgress (5_bp), 0.75f, 0.001f);
        expect (! h.getProgress (6_bp));

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 1_bp, 0.5_bd });
            expect (s.range1.isEmpty());
            expect (! s.playing1);
            expect (s.range2.isEmpty());
            expect (! s.playing2);
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 1.75_bp, 0.5_bd });
            expect (! s.playing1);
            expect (s.range1 == BeatRange (1.75_bp, 2.0_bp));
            expect (s.playing2);
            expect (s.range2 == BeatRange (2.0_bp, 2.25_bp));
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 3.0_bp, 0.5_bd });
            expect (s.playing1);
            expect (s.range1 == BeatRange (3.0_bp, 3.5_bp));
            expect (! s.playing2);
            expect (s.range2.isEmpty());
        }

        {
            const auto s = h.timelineRangeToClipSourceRange ({ 5.75_bp, 0.5_bd });
            expect (s.playing1);
            expect (s.range1 == BeatRange (5.75_bp, 6.0_bp));
            expect (! s.playing2);
            expect (s.range2 == BeatRange (6.0_bp, 6.25_bp));
        }
    }

    void runLoopingTests()
    {
        beginTest ("Looping");
    }
};

static LauncherClipPlaybackHandleTests launcherClipPlaybackHandleTests;

}} // namespace tracktion { inline namespace engine

 #endif
