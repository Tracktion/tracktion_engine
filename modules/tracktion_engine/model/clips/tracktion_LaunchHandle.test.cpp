/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LAUNCH_HANDLE

#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
class LaunchHandleTests : public juce::UnitTest
{
public:
    LaunchHandleTests()
        : juce::UnitTest ("LaunchHandle", "tracktion_engine")
    {
    }

    void runTest() override
    {
        runBasicLauchHandleTests();
        runQuantisedLauchHandleTests();
        legatoLauchHandleTests();
    }

private:
    void runBasicLauchHandleTests()
    {
        beginTest ("Non-quantised launching");

        {
            LaunchHandle h;
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! h.getQueuedStatus());

            h.play ({});
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

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
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());
                expect (getBeatRange (syncRange) == BeatRange (0_bp, 0.5_bp));
                expect (getMonotonicBeatRange (syncRange).v == BeatRange (0_bp, 0.5_bp));

                expect (! s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (0_bp, 0.5_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }

            {
                auto s = h.advance (advanceSync (0.5_bd));
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());

                expect (! s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (0.5_bp, 1.0_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }

            h.stop ({});
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

            {
                auto s = h.advance (advanceSync (0.5_bd));
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (! h.getQueuedStatus());

                expect (! s.isSplit);
                expect (! s.playing1);
                expect (s.range1 == BeatRange (1.0_bp, 1.5_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }
        }
    }

    void runQuantisedLauchHandleTests()
    {
        beginTest ("Quantised launching");

        {
            LaunchHandle h;
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! h.getQueuedStatus());

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
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

            {
                auto s = advanceHandle (0.5_bd);
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());

                expect (s.isSplit);
                expect (! s.playing1);
                expect (s.range1 == BeatRange (0_bp, 0.25_bp));
                expect (s.playing2);
                expect (s.range2 == BeatRange (0.25_bp, 0.5_bp));
            }

            {
                auto s = advanceHandle (0.5_bd);
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());

                expect (! s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (0.5_bp, 1.0_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }

            h.stop (MonotonicBeat { 1.25_bp });
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

            {
                auto s = advanceHandle (0.5_bd);
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (! h.getQueuedStatus());

                expect (s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (1.0_bp, 1.25_bp));
                expect (! s.playing2);
                expect (s.range2 == BeatRange (1.25_bp, 1.5_bp));
            }
        }
    }

    void legatoLauchHandleTests()
    {
        beginTest ("Legato launching");
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
            expect (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! sourceHandle.getQueuedStatus());

            expect (destHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! destHandle.getQueuedStatus());

            // Start source
            {
                sourceHandle.play (MonotonicBeat { 1_bp });
                expect (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (sourceHandle.getQueuedStatus() == LaunchHandle::QueueState::playQueued);
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
                expect (sourceHandle.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (! sourceHandle.getQueuedStatus());

                expect (s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (3_bp, 4_bp));
                expect (s.playStartTime1 == 1_bp);
                expect (! s.playing2);
                expect (s.range2 == BeatRange (4_bp, 5_bp));
                expect (! s.playStartTime2);
            }

            // Advance dest the same 2 bd
            {
                auto s = destHandle.advance (syncRange);
                expect (destHandle.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! destHandle.getQueuedStatus());

                expect (s.isSplit);
                expect (! s.playing1);
                expect (s.range1 == BeatRange (3_bp, 4_bp));
                expect (! s.playStartTime1);
                expect (s.playing2);
                expect (s.range2 == BeatRange (4_bp, 5_bp));
                expect (s.playStartTime2 == 1_bp);

                expect (destHandle.getPlayedRange()->getStart() == 1_bp);
                expect (destHandle.getPlayedMonotonicRange()->v.getStart() == 1_bp);
            }
        }
    }
};

static LaunchHandleTests launchHandleTests;

} // namespace tracktion::namespace engine

#endif
