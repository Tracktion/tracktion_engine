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
