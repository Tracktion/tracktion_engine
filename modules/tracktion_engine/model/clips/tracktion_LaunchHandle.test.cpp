/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
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
        auto& engine = *Engine::getEngines()[0];
        runBasicLauchHandleTests (engine);
        runQuantisedLauchHandleTests (engine);
    }

private:
    void runBasicLauchHandleTests (Engine& engine)
    {
        beginTest ("Non-quantised launching");

        {
            LaunchHandle h (4_bd);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! h.getQueuedStatus());
            expect (! h.getProgress());

            h.play ({});
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

            {
                auto s = h.update ({ 0_bp, 0.5_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());
                expectWithinAbsoluteError (*h.getProgress(), 0.125f, 0.001f);

                expect (! s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (0_bp, 0.5_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }

            {
                auto s = h.update ({ 0.5_bp, 1.0_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());
                expectWithinAbsoluteError (*h.getProgress(), 0.25f, 0.001f);

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
                auto s = h.update ({ 1.0_bp, 1.5_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (! h.getQueuedStatus());
                expect (! h.getProgress());

                expect (! s.isSplit);
                expect (! s.playing1);
                expect (s.range1 == BeatRange (1.0_bp, 1.5_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }
        }
    }

    void runQuantisedLauchHandleTests (Engine& engine)
    {
        beginTest ("Quantised launching");

        {
            LaunchHandle h (4_bd);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (! h.getQueuedStatus());
            expect (! h.getProgress());

            h.play (0.25_bp);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::playQueued);

            {
                auto s = h.update ({ 0_bp, 0.5_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());
                expectWithinAbsoluteError (*h.getProgress(), 0.0625f, 0.001f);

                expect (s.isSplit);
                expect (! s.playing1);
                expect (s.range1 == BeatRange (0_bp, 0.25_bp));
                expect (s.playing2);
                expect (s.range2 == BeatRange (0.25_bp, 0.5_bp));
            }

            {
                auto s = h.update ({ 0.5_bp, 1.0_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);
                expect (! h.getQueuedStatus());
                expectWithinAbsoluteError (*h.getProgress(), 0.1875f, 0.001f);

                expect (! s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (0.5_bp, 1.0_bp));
                expect (! s.playing2);
                expect (s.range2.isEmpty());
            }

            h.stop (1.25_bp);
            expect (h.getQueuedStatus() == LaunchHandle::QueueState::stopQueued);
            expect (h.getPlayingStatus() == LaunchHandle::PlayState::playing);

            {
                auto s = h.update ({ 1.0_bp, 1.5_bp });
                expect (h.getPlayingStatus() == LaunchHandle::PlayState::stopped);
                expect (! h.getQueuedStatus());
                expect (! h.getProgress());

                expect (s.isSplit);
                expect (s.playing1);
                expect (s.range1 == BeatRange (1.0_bp, 1.25_bp));
                expect (! s.playing2);
                expect (s.range2 == BeatRange (1.25_bp, 1.5_bp));
            }
        }
    }
};

static LaunchHandleTests clipSlotTests;

}} // namespace tracktion { inline namespace engine

#endif
