/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCH_HANDLE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("LaunchHandle: Edit position jumps")
{
    // Blocks of 0.5 beats; the Edit beat can jump, the monotonic beat can't
    SyncRange syncRange;
    auto advanceSync = [&syncRange] (BeatDuration duration, std::optional<BeatPosition> jumpTo = {})
                       {
                           auto start = syncRange.end;

                           if (jumpTo)
                               start.beat = *jumpTo;

                           auto end = start;
                           end.monotonicBeat.v = end.monotonicBeat.v + duration;
                           end.beat = end.beat + duration;
                           syncRange = SyncRange { start, end };

                           return syncRange;
                       };

    SUBCASE ("A looping handle keeps its phase when the playhead jumps back past its start")
    {
        LaunchHandle h;
        const auto loopLength = 4_bd;
        h.advance (advanceSync (1_bd), loopLength);  // 0-1
        h.play ({});
        h.advance (advanceSync (1_bd), loopLength);  // starts at 1

        for (int i = 0; i < 4; ++i)
            h.advance (advanceSync (1_bd), loopLength); // to 6

        // Jump back to 0.5: the start moves back a whole loop to -3
        auto s = h.advance (advanceSync (0.5_bd, 0.5_bp), loopLength);
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (s.playing1);
        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (-3.0));

        auto played = h.getPlayedRange();
        REQUIRE (played);
        CHECK (played->getStart().inBeats() == doctest::Approx (-3.0));
        CHECK (played->getEnd().inBeats() == doctest::Approx (1.0));

        // The monotonic range is unaffected, so timed stops and follow actions are too
        auto monotonic = h.getPlayedMonotonicRange();
        REQUIRE (monotonic);
        CHECK (monotonic->v.getStart().inBeats() == doctest::Approx (1.0));
        CHECK (monotonic->v.getLength().inBeats() == doctest::Approx (5.5));

        // As is the length last played, which a performance recording takes as
        // the recorded length - it handles an arrangement loop wrap itself
        h.stop ({});
        h.advance (advanceSync (0.5_bd), loopLength);
        auto last = h.getLastPlayedRange();
        REQUIRE (last);
        CHECK (last->getLength().inBeats() == doctest::Approx (5.5));
    }

    SUBCASE ("A forward jump keeps the start, and the played range ends at the playhead")
    {
        LaunchHandle h;
        h.play ({});
        h.advance (advanceSync (1_bd), 4_bd);           // starts at 0
        auto s = h.advance (advanceSync (1_bd, 10_bp), 4_bd);

        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (0.0));

        auto played = h.getPlayedRange();
        REQUIRE (played);
        CHECK (played->getEnd().inBeats() == doctest::Approx (11.0));
    }

    SUBCASE ("A one-shot handle stops when the playhead jumps")
    {
        LaunchHandle h;
        h.play ({});
        h.advance (advanceSync (1_bd));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

        auto s = h.advance (advanceSync (1_bd, 0_bp));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! s.playing1);
        CHECK (! h.getPlayedRange());

        auto last = h.getLastPlayedRange();
        REQUIRE (last);
        CHECK (last->getLength().inBeats() == doctest::Approx (1.0));
    }

    SUBCASE ("Continuous blocks aren't a jump")
    {
        LaunchHandle h;
        h.play ({});

        for (int i = 0; i < 8; ++i)
            h.advance (advanceSync (0.5_bd));

        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        auto played = h.getPlayedRange();
        REQUIRE (played);
        CHECK (played->getLength().inBeats() == doctest::Approx (4.0));
    }
}

TEST_CASE ("LaunchHandle: Non-quantised launching")
{
    LaunchHandle h;
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (! h.getQueuedStatus());

    h.play ({});
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

    SyncRange syncRange;
    auto advanceSync = [&syncRange] (auto duration)
                       {
                           auto newEnd = syncRange.end;
                           newEnd.monotonicBeat.v = newEnd.monotonicBeat.v + duration;
                           newEnd.beat = newEnd.beat + duration;
                           syncRange = SyncRange { syncRange.end, newEnd };

                           return syncRange;
                       };

    {
        auto s = h.advance (advanceSync (0.5_bd));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! h.getQueuedStatus());
        CHECK (getBeatRange (syncRange) == BeatRange (0_bp, 0.5_bp));
        CHECK (getMonotonicBeatRange (syncRange).v == BeatRange (0_bp, 0.5_bp));

        CHECK (! s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (0_bp, 0.5_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }

    {
        auto s = h.advance (advanceSync (0.5_bd));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! h.getQueuedStatus());

        CHECK (! s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (0.5_bp, 1.0_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }

    h.stop ({});
    CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

    {
        auto s = h.advance (advanceSync (0.5_bd));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! h.getQueuedStatus());

        CHECK (! s.isSplit);
        CHECK (! s.playing1);
        CHECK (s.range1 == BeatRange (1.0_bp, 1.5_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }
}

TEST_CASE ("LaunchHandle: Quantised launching")
{
    LaunchHandle h;
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (! h.getQueuedStatus());

    SyncRange syncRange;
    auto advanceHandle = [&h, &syncRange] (auto duration)
    {
        auto newEnd = syncRange.end;
        newEnd.monotonicBeat.v = newEnd.monotonicBeat.v + duration;
        newEnd.beat = newEnd.beat + duration;
        syncRange = SyncRange { syncRange.end, newEnd };

        return h.advance (syncRange);
    };

    h.play (MonotonicBeat { 0.25_bp });
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

    {
        auto s = advanceHandle (0.5_bd);
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! h.getQueuedStatus());

        CHECK (s.isSplit);
        CHECK (! s.playing1);
        CHECK (s.range1 == BeatRange (0_bp, 0.25_bp));
        CHECK (s.playing2);
        CHECK (s.range2 == BeatRange (0.25_bp, 0.5_bp));
    }

    {
        auto s = advanceHandle (0.5_bd);
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! h.getQueuedStatus());

        CHECK (! s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (0.5_bp, 1.0_bp));
        CHECK (! s.playing2);
        CHECK (s.range2.isEmpty());
    }

    h.stop (MonotonicBeat { 1.25_bp });
    CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
    CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

    {
        auto s = advanceHandle (0.5_bd);
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! h.getQueuedStatus());

        CHECK (s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (1.0_bp, 1.25_bp));
        CHECK (! s.playing2);
        CHECK (s.range2 == BeatRange (1.25_bp, 1.5_bp));
    }
}

TEST_CASE ("LaunchHandle: Legato launching")
{
    LaunchHandle sourceHandle, destHandle;

    SyncRange syncRange;
    auto advancePlayhead = [&] (auto duration)
    {
        auto newEnd = syncRange.end;
        newEnd.monotonicBeat.v = newEnd.monotonicBeat.v + duration;
        newEnd.beat = newEnd.beat + duration;
        syncRange = SyncRange { syncRange.end, newEnd };
    };


    // Init status
    CHECK (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (! sourceHandle.getQueuedStatus());

    CHECK (destHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    CHECK (! destHandle.getQueuedStatus());

    // Start source
    {
        sourceHandle.play (MonotonicBeat { 1_bp });
        CHECK (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (sourceHandle.getQueuedStatus() == LaunchHandle::QueueState::playQueued);
    }

    // Advance timeline
    {
        advancePlayhead (3_bd);
        sourceHandle.advance (syncRange);
        destHandle.advance (syncRange);
    }

    // Switch at 4 bp
    {
        const auto switchBeat = MonotonicBeat { 4_bp };
        destHandle.playSynced (sourceHandle, switchBeat);
        sourceHandle.stop (switchBeat);
    }

    // Advance source another 2 bd
    {
        advancePlayhead (2_bd);
        auto s = sourceHandle.advance (syncRange);
        CHECK (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! sourceHandle.getQueuedStatus());

        CHECK (s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (3_bp, 4_bp));
        CHECK (s.playStartTime1 == 1_bp);
        CHECK (! s.playing2);
        CHECK (s.range2 == BeatRange (4_bp, 5_bp));
        CHECK (! s.playStartTime2);
    }

    // Advance dest the same 2 bd
    {
        auto s = destHandle.advance (syncRange);
        CHECK (destHandle.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! destHandle.getQueuedStatus());

        CHECK (s.isSplit);
        CHECK (! s.playing1);
        CHECK (s.range1 == BeatRange (3_bp, 4_bp));
        CHECK (! s.playStartTime1);
        CHECK (s.playing2);
        CHECK (s.range2 == BeatRange (4_bp, 5_bp));
        CHECK (s.playStartTime2 == 1_bp);

        CHECK (destHandle.getPlayedRange()->getStart() == 1_bp);
        CHECK (destHandle.getPlayedMonotonicRange()->v.getStart() == 1_bp);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif
