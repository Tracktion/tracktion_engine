/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_FOLLOW_ACTIONS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "tracktion_FollowActions.h"
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

namespace
{
    template <typename T1, typename T2>
    void followActionExpectInt (T1 int1, T2 int2)
    {
        CHECK_EQ (static_cast<std::int64_t> (int1), static_cast<std::int64_t> (int2));
    }
}

TEST_SUITE ("tracktion_engine")
{

TEST_CASE ("Follow Actions")
{
    using namespace test_utilities;
    auto& engine = *Engine::getEngines()[0];

    auto fileLength = 4_td;

    auto edit = createTestEdit (engine);
    auto track = getAudioTracks (*edit)[0];
    auto& clipSlotList = track->getClipSlotList();

    clipSlotList.ensureNumberOfSlots (8);
    auto slots = clipSlotList.getClipSlots();

    // Clip Positions
    {
        CHECK_EQ (slots.size(), 8);

        for (int clipIndex : { 0, 1, 3, 4, 5, 7 })
            insertWaveClip (*slots[clipIndex], {}, juce::File(), ClipPosition { { 0_tp, fileLength } }, DeleteExistingClips::no);

        for (int clipIndex : { 0, 1, 3, 4, 5, 7 })
        {
            const auto clipContex = follow_action_utils::createClipContext (*slots[clipIndex]->getClip());
            followActionExpectInt (clipContex->allSceneHandles.size(), 8);
            followActionExpectInt (clipContex->validSceneHandles.size(), 6);
            followActionExpectInt (clipContex->groups.size(), 3);
        }

        struct ContextIndicies
        {
            size_t sceneIndex = 0, groupIndex = 0, indexInGroup = 0;
        };

        for (auto ctxIndicies : std::initializer_list<ContextIndicies> {
                                    { 0, 0, 0 },
                                    { 1, 0, 1 },

                                    { 3, 1, 0 },
                                    { 4, 1, 1 },
                                    { 5, 1, 2 },

                                    { 7, 2, 0 },
                                })
        {
            const auto clipContex = follow_action_utils::createClipContext (*slots[static_cast<int> (ctxIndicies.sceneIndex)]->getClip());
            followActionExpectInt (clipContex->sceneIndex, ctxIndicies.sceneIndex);
            followActionExpectInt (clipContex->groupIndex, ctxIndicies.groupIndex);
            followActionExpectInt (clipContex->indexInGroup, ctxIndicies.indexInGroup);
        }
    }

    auto testAction = [&slots] (auto current, auto expectedQueued, auto action)
    {
        const auto ctx = follow_action_utils::createClipContext (*slots[static_cast<int> (current)]->getClip());
        auto lh = follow_action_utils::getLaunchHandle (*ctx, action);

        if (expectedQueued == -1)
        {
            CHECK (! lh);
            return;
        }

        if (const auto index = index_if (slots, [&lh] (auto clipSlot) -> bool { return clipSlot->getClip() && clipSlot->getClip()->getLaunchHandle() == lh; }))
            followActionExpectInt (*index, expectedQueued);
        else
            CHECK (false);
    };

    // Track Previous
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::trackPrevious); };
        test (0, -1);
        test (1, 0);
        test (3, 1);
        test (4, 3);
        test (5, 4);
        test (7, 5);
    }

    // Track Next
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::trackNext); };
        test (0, 1);
        test (1, 3);
        test (3, 4);
        test (4, 5);
        test (5, 7);
        test (7, -1);
    }

    // Track First
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::trackFirst); };
        test (0, 0);
        test (1, 0);
        test (3, 0);
        test (4, 0);
        test (5, 0);
        test (7, 0);
    }

    // Track Last
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::trackLast); };
        test (0, 7);
        test (1, 7);
        test (3, 7);
        test (4, 7);
        test (5, 7);
        test (7, 7);
    }

    // Track Round Robin
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::trackRoundRobin); };
        test (0, 1);
        test (1, 3);
        test (3, 4);
        test (4, 5);
        test (5, 7);
        test (7, 0);
    }

    // Current Group Previous
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::currentGroupPrevious); };
        test (0, -1);
        test (1, 0);
        test (3, -1);
        test (4, 3);
        test (5, 4);
        test (7, -1);
    }

    // Current Group Next
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::currentGroupNext); };
        test (0, 1);
        test (1, -1);
        test (3, 4);
        test (4, 5);
        test (5, -1);
        test (7, -1);
    }

    // Current Group First
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::currentGroupFirst); };
        test (0, 0);
        test (1, 0);
        test (3, 3);
        test (4, 3);
        test (5, 3);
        test (7, 7);
    }

    // Current Group Last
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::currentGroupLast); };
        test (0, 1);
        test (1, 1);
        test (3, 5);
        test (4, 5);
        test (5, 5);
        test (7, 7);
    }

    // Current Group Round Robin
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::currentGroupRoundRobin); };
        test (0, 1);
        test (1, 0);
        test (3, 4);
        test (4, 5);
        test (5, 3);
        test (7, 7);
    }

    // Previous Group First
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::previousGroupFirst); };
        test (0, -1);
        test (1, -1);
        test (3, 0);
        test (4, 0);
        test (5, 0);
        test (7, 3);
    }

    // Next Group First
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::nextGroupFirst); };
        test (0, 3);
        test (1, 3);
        test (3, 7);
        test (4, 7);
        test (5, 7);
        test (7, -1);
    }

    // Next Group Last
    {
        auto test = [&testAction] (auto current, auto expectedQueued) { testAction (current, expectedQueued, FollowAction::nextGroupLast); };
        test (0, 5);
        test (1, 5);
        test (3, 7);
        test (4, 7);
        test (5, 7);
        test (7, -1);
    }
}

} // TEST_SUITE

}

#endif
