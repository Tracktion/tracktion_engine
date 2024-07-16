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

namespace tracktion { inline namespace engine
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
};

static LaunchHandleTests launchHandleTests;

}} // namespace tracktion { inline namespace engine

#endif
