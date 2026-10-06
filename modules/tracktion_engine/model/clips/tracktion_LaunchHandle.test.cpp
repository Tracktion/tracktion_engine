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

    SUBCASE ("A playing handle carries on from where it was when the playhead jumps back")
    {
        LaunchHandle h;
        h.advance (advanceSync (1_bd));     // 0-1
        h.play ({});
        h.advance (advanceSync (1_bd));     // starts at 1

        for (int i = 0; i < 4; ++i)
            h.advance (advanceSync (1_bd)); // to 6, 5 beats in

        // Jump back to 0.5: still 5 beats in, so it started at -4.5
        auto s = h.advance (advanceSync (0.5_bd, 0.5_bp));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (s.playing1);
        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (-4.5));

        auto played = h.getPlayedRange();
        REQUIRE (played);
        CHECK (played->getStart().inBeats() == doctest::Approx (-4.5));
        CHECK (played->getEnd().inBeats() == doctest::Approx (1.0));

        // The monotonic range is unaffected, so timed stops and follow actions are too
        auto monotonic = h.getPlayedMonotonicRange();
        REQUIRE (monotonic);
        CHECK (monotonic->v.getStart().inBeats() == doctest::Approx (1.0));
        CHECK (monotonic->v.getLength().inBeats() == doctest::Approx (5.5));

        // As is the length last played, which a performance recording takes as
        // the recorded length - it handles an arrangement loop wrap itself
        h.stop ({});
        h.advance (advanceSync (0.5_bd));
        auto last = h.getLastPlayedRange();
        REQUIRE (last);
        CHECK (last->getLength().inBeats() == doctest::Approx (5.5));
    }

    SUBCASE ("A repeating handle keeps repeating on its own grid after the playhead jumps")
    {
        // As the Repeat trigger mode: retrigger every beat
        for (auto jumpTo : { 10_bp, 0.25_bp, 2.75_bp })
        {
            CAPTURE (jumpTo.inBeats());
            syncRange = {};
            LaunchHandle h;
            h.setLooping (1_bd);
            h.advance (advanceSync (0.5_bd));   // 0-0.5
            h.play ({});
            h.advance (advanceSync (0.5_bd));   // starts at 0.5

            int numRepeats = 0;

            for (int i = 0; i < 6; ++i)         // to 4, repeating at 1.5, 2.5 and 3.5
                if (h.advance (advanceSync (0.5_bd)).isSplit)
                    ++numRepeats;

            CHECK (numRepeats == 3);

            // Jump, then play for 1.5 beats: it repeats once, a beat after the last
            // repeat, as if there was no jump
            numRepeats = 0;

            for (int i = 0; i < 3; ++i)
                if (h.advance (advanceSync (0.5_bd, i == 0 ? std::optional (jumpTo) : std::nullopt)).isSplit)
                    ++numRepeats;

            CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
            CHECK (numRepeats == 1);
        }
    }

    SUBCASE ("A forward jump keeps its position, and the played range ends at the playhead")
    {
        LaunchHandle h;
        h.play ({});
        h.advance (advanceSync (1_bd));     // starts at 0
        auto s = h.advance (advanceSync (1_bd, 10_bp));

        // 1 beat in at 10, so it started at 9
        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (9.0));

        auto played = h.getPlayedRange();
        REQUIRE (played);
        CHECK (played->getEnd().inBeats() == doctest::Approx (11.0));
        CHECK (played->getLength().inBeats() == doctest::Approx (2.0));
    }

    SUBCASE ("A one-shot handle carries on when the playhead jumps")
    {
        LaunchHandle h;
        h.play ({});
        h.advance (advanceSync (1_bd));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

        auto s = h.advance (advanceSync (1_bd, 0_bp));
        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (s.playing1);
        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (-1.0));
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

TEST_CASE ("LaunchHandle: Stopping a queued launch")
{
    SyncRange syncRange;
    auto advanceSync = [&syncRange] (BeatDuration duration)
                       {
                           auto end = syncRange.end;
                           end.monotonicBeat.v = end.monotonicBeat.v + duration;
                           end.beat = end.beat + duration;
                           syncRange = SyncRange { syncRange.end, end };
                           return syncRange;
                       };

    SUBCASE ("Stopping a stopped handle cancels its queued play")
    {
        LaunchHandle h;
        h.play (MonotonicBeat { 4_bp });
        REQUIRE (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

        h.stop ({});
        CHECK (! h.getQueuedStatus());

        for (int i = 0; i < 16; ++i)
            h.advance (advanceSync (0.5_bd));

        CHECK (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
    }

    SUBCASE ("Stopping a playing handle queues a stop")
    {
        LaunchHandle h;
        h.play ({});
        h.advance (advanceSync (0.5_bd));
        REQUIRE (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

        h.stop (MonotonicBeat { 2_bp });
        CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
    }

    SUBCASE ("The end of a timed play stops it, unless a play is queued")
    {
        LaunchHandle h;
        CHECK (! h.stopAtEndOfPlay (MonotonicBeat { 1_bp })); // not playing

        h.play ({});
        h.advance (advanceSync (0.5_bd));

        // Nothing queued: stops, and its follow action should run
        CHECK (h.stopAtEndOfPlay (MonotonicBeat { 1_bp }));
        CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);

        // A stop already queued: stops here instead, without a follow action
        CHECK (! h.stopAtEndOfPlay (MonotonicBeat { 0.75_bp }));
        CHECK (h.getQueuedEventPosition()->v.inBeats() == doctest::Approx (0.75));

        // A relaunch queued: that carries on instead
        h.play (MonotonicBeat { 4_bp });
        CHECK (! h.stopAtEndOfPlay (MonotonicBeat { 1_bp }));
        CHECK (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);
    }

    SUBCASE ("A launch first seen after its position plays in phase from the first block")
    {
        LaunchHandle h;
        h.advance (advanceSync (1_bd));     // 0-1
        h.advance (advanceSync (1_bd));     // 1-2
        h.play (MonotonicBeat { 1_bp });    // already passed

        auto s = h.advance (advanceSync (0.5_bd)); // 2-2.5
        CHECK (s.playing1);
        REQUIRE (s.playStartTime1);
        CHECK (s.playStartTime1->inBeats() == doctest::Approx (1.0));

        // The next block carries on from the same start, so nothing jumps
        auto next = h.advance (advanceSync (0.5_bd));
        REQUIRE (next.playStartTime1);
        CHECK (next.playStartTime1->inBeats() == doctest::Approx (1.0));
    }

    SUBCASE ("A stop always cancels a queued play whilst the audio thread advances")
    {
        // The audio thread holds the queue's lock whilst it advances. A stop made
        // then used to see nothing queued and leave the play to start later
        LaunchHandle h;
        std::atomic<bool> running { true }, paused { false }, pauseRequested { false };

        std::thread audioThread ([&]
                                 {
                                     SyncRange range;

                                     while (running)
                                     {
                                         if (pauseRequested)
                                         {
                                             paused = true;

                                             while (pauseRequested && running)
                                                 std::this_thread::yield();

                                             paused = false;
                                             continue;
                                         }

                                         auto end = range.end;
                                         end.monotonicBeat.v = end.monotonicBeat.v + 0.001_bd;
                                         end.beat = end.beat + 0.001_bd;
                                         range = SyncRange { range.end, end };
                                         h.advance (range);
                                     }
                                 });

        int numMissed = 0;

        // A race, so it's tried many times
        for (int i = 0; i < 20000; ++i)
        {
            h.play (MonotonicBeat { 1.0e6_bp }); // never reached
            h.stop ({});

            // Pause the audio thread so the queue can be read reliably
            pauseRequested = true;

            while (! paused)
                std::this_thread::yield();

            if (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued)
                ++numMissed;

            pauseRequested = false;
        }

        running = false;
        audioThread.join();

        CHECK (numMissed == 0);
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

TEST_CASE ("LaunchHandle: Legato launch edge cases")
{
    LaunchHandle sourceHandle, destHandle;

    SyncRange syncRange;
    auto advancePlayhead = [&] (BeatDuration duration)
    {
        auto newEnd = syncRange.end;
        newEnd.monotonicBeat.v = newEnd.monotonicBeat.v + duration;
        newEnd.beat = newEnd.beat + duration;
        syncRange = SyncRange { syncRange.end, newEnd };
        return syncRange;
    };

    // The source plays from beat 1
    sourceHandle.play (MonotonicBeat { 1_bp });
    sourceHandle.advance (advancePlayhead (3_bd));  // 0-3
    destHandle.advance (syncRange);
    REQUIRE (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::playing);

    SUBCASE ("A legato launch first seen after its position plays in phase from the first block")
    {
        // e.g. an unquantised launch, which has passed by the time the audio thread sees it
        const auto switchBeat = MonotonicBeat { 2_bp };
        destHandle.playSynced (sourceHandle, switchBeat);
        sourceHandle.stop (switchBeat);

        auto s = destHandle.advance (advancePlayhead (2_bd));   // 3-5
        CHECK (destHandle.getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (! s.isSplit);
        CHECK (s.playing1);
        CHECK (s.range1 == BeatRange (3_bp, 5_bp));
        CHECK (s.playStartTime1 == 1_bp);

        CHECK (destHandle.getPlayedRange() == BeatRange (1_bp, 5_bp));
        REQUIRE (destHandle.getPlayedMonotonicRange());
        CHECK (destHandle.getPlayedMonotonicRange()->v == BeatRange (1_bp, 5_bp));

        // And carries on from the same start
        s = destHandle.advance (advancePlayhead (1_bd));        // 5-6
        CHECK (s.playing1);
        CHECK (s.playStartTime1 == 1_bp);
        CHECK (destHandle.getPlayedRange() == BeatRange (1_bp, 6_bp));
    }

    SUBCASE ("A relaunch after a cancelled legato launch doesn't sync to the old clip")
    {
        destHandle.playSynced (sourceHandle, MonotonicBeat { 8_bp });
        destHandle.stop ({});
        CHECK (! destHandle.getQueuedStatus());

        destHandle.play (MonotonicBeat { 4_bp });
        auto s = destHandle.advance (advancePlayhead (2_bd));   // 3-5
        CHECK (s.isSplit);
        CHECK (s.playing2);
        CHECK (s.playStartTime2 == 4_bp);
        REQUIRE (destHandle.getPlayedRange());
        CHECK (destHandle.getPlayedRange()->getStart() == 4_bp);
    }

    SUBCASE ("A launch replacing a queued legato launch doesn't sync to the old clip")
    {
        destHandle.playSynced (sourceHandle, MonotonicBeat { 8_bp });
        destHandle.play (MonotonicBeat { 4_bp });

        auto s = destHandle.advance (advancePlayhead (2_bd));   // 3-5
        CHECK (s.isSplit);
        CHECK (s.playing2);
        CHECK (s.playStartTime2 == 4_bp);
        REQUIRE (destHandle.getPlayedRange());
        CHECK (destHandle.getPlayedRange()->getStart() == 4_bp);
    }
}

} // TEST_SUITE

} // namespace tracktion::inline engine

#endif
