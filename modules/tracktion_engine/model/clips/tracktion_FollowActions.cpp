/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <span>
#include "../../../3rd_party/nanorange/tracktion_nanorange.hpp"

namespace tracktion::inline engine
{

std::optional<FollowAction> followActionFromString (juce::String s)
{
    return magic_enum::enum_cast<FollowAction> (s.toStdString());
}

juce::String toString (FollowAction fa)
{
    return std::string (magic_enum::enum_name (fa));
}

std::function<void (MonotonicBeat)> createFollowAction (Clip& c)
{
    using enum FollowAction;
    auto sc = makeSafeRef (c);
    auto audioTrack = dynamic_cast<AudioTrack*> (c.getTrack());

    if (! audioTrack)
        return {};

    struct ClipContext
    {
        Clip::Ptr clip;
        AudioTrack::Ptr track;
        std::shared_ptr<LaunchHandle> launchHandle;
        std::vector<std::shared_ptr<LaunchHandle>> sceneHandles;
        std::vector<std::vector<std::shared_ptr<LaunchHandle>>> groups;
        size_t sceneIndex = 0, groupIndex = 0, indexInGroup = 0;
        juce::Random random;

        std::vector<std::shared_ptr<LaunchHandle>>& getGroup()
        {
            return groups[groupIndex];
        }

        std::vector<std::shared_ptr<LaunchHandle>>* getPreviousGroup()
        {
            return groupIndex == 0 ? nullptr : &groups[groupIndex - 1];
        }

        std::vector<std::shared_ptr<LaunchHandle>>* getNextGroup()
        {
            return groupIndex == (groups.size() - 1) ? nullptr : &groups[groupIndex + 1];
        }

        std::vector<std::shared_ptr<LaunchHandle>>* getOtherGroup()
        {
            if (groups.size() == 1)
                return nullptr;

            for (;;)
            {
                const auto index = static_cast<size_t> (random.nextInt ({ 0, static_cast<int> (groups.size()) }));

                if (index != groupIndex)
                    return &groups[static_cast<size_t> (index)];
            }
        }
    };

    auto ctx = std::make_shared<ClipContext>();
    ctx->clip = c;
    ctx->track = audioTrack;
    ctx->launchHandle = c.getLaunchHandle();

    std::vector<std::shared_ptr<LaunchHandle>> allSceneHandles;

    for (auto cs : audioTrack->getClipSlotList().getClipSlots())
    {
        if (auto clip = cs->getClip())
        {
            auto lh = clip->getLaunchHandle();
            allSceneHandles.push_back (lh);

            if (lh)
                ctx->sceneHandles.emplace_back (std::move (lh));
        }
        else
        {
            allSceneHandles.push_back (nullptr);
        }
    }

    if (ctx->sceneHandles.empty())
        return {};

    // Create groups
    ctx->sceneIndex = static_cast<size_t> (c.getClipSlot()->getIndex());

    for (auto groupView : nano::split_view (allSceneHandles, { nullptr }))
    {
        std::vector<std::shared_ptr<LaunchHandle>> group;

        for (auto lh : groupView)
        {
            if (ctx->launchHandle == lh)
            {
                ctx->groupIndex = ctx->groups.size();
                ctx->indexInGroup = group.size();
            }

            group.push_back (std::move (lh));
        }

        if (group.empty())
            continue;

        assert (! contains_v (group, nullptr));
        ctx->groups.push_back (std::move (group));
    }

    assert (! ctx->groups.empty());

    switch (c.followAction.get())
    {
        case none:
            return {};

        case globalStop:
            return {};
        case globalReturnToArrangement:
            return [ctx] (auto) { ctx->track->playSlotClips = true; };
        case globalplayAgain:
            return [ctx] (auto b) { ctx->launchHandle->play (b); };

        case scenePrevious:
            if (ctx->sceneIndex > 0)
                return [ctx] (auto b) { ctx->sceneHandles[ctx->sceneIndex - 1]->play (b); };
        case sceneNext:
            if (ctx->sceneIndex < (ctx->sceneHandles.size() - 1))
                return [ctx] (auto b) { ctx->sceneHandles[ctx->sceneIndex + 1]->play (b); };
        case sceneFirst:
                return [ctx] (auto b) { ctx->sceneHandles.front()->play (b); };
        case sceneLast:
                return [ctx] (auto b) { ctx->sceneHandles.back()->play (b); };
        case sceneAny:
            return [ctx] (auto b)
                   {
                       const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (ctx->sceneHandles.size()) }));
                       ctx->sceneHandles[index]->play (b);
                   };
        case sceneOther:
            if (ctx->sceneHandles.size() > 1)
                return [ctx] (auto b)
                       {
                           for (;;)
                           {
                               const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (ctx->sceneHandles.size()) }));

                               if (index == ctx->sceneIndex)
                                   continue;

                               ctx->sceneHandles[index]->play (b);
                           }
                       };
        case sceneRoundRobin:
            return [ctx] (auto b) { ctx->sceneHandles[(ctx->sceneIndex + 1) % ctx->sceneHandles.size()]->play (b); };

        case currentGroupPrevious:
            if (ctx->indexInGroup > 0)
                return [ctx, &group = ctx->getGroup()] (auto b) { group[(ctx->indexInGroup - 1)]->play (b); };
        case currentGroupNext:
            if (ctx->indexInGroup < (ctx->getGroup().size() - 1))
                return [ctx, &group = ctx->getGroup()] (auto b) { group[(ctx->indexInGroup + 1)]->play (b); };
        case currentGroupFirst:
                return [ctx] (auto b) { ctx->getGroup().front()->play (b); };
        case currentGroupLast:
                return [ctx] (auto b) { ctx->getGroup().back()->play (b); };
        case currentGroupAny:
            return [ctx, &group = ctx->getGroup()] (auto b)
                   {
                       const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group.size()) }));
                       group[index]->play (b);
                   };
        case currentGroupOther:
            if (ctx->getGroup().size() > 1)
                return [ctx, &group = ctx->getGroup()] (auto b)
                       {
                           for (;;)
                           {
                               const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group.size()) }));

                               if (index == ctx->indexInGroup)
                                   continue;

                               group[index]->play (b);
                           }
                       };
        case currentGroupRoundRobin:
            return [ctx, &group = ctx->getGroup()] (auto b)
                   {
                       group[(ctx->indexInGroup + 1) % group.size()]->play (b);
                   };

        case previousGroupFirst:
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b) { group->front()->play (b); };
        case previousGroupLast:
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b) { group->back()->play (b); };
        case previousGroupAny:
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b)
                       {
                           const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                           (*group)[index]->play (b);
                       };

        case nextGroupFirst:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b) { group->front()->play (b); };
        case nextGroupLast:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b) { group->back()->play (b); };
        case nextGroupAny:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b)
                {
                    const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                    (*group)[index]->play (b);
                };

        case otherGroupFirst:
            if (ctx->groups.size() > 1)
                return [ctx] (auto b) { ctx->getOtherGroup()->front()->play (b); };
        case otherGroupLast:
            if (ctx->groups.size() > 1)
                return [ctx] (auto b) { ctx->getOtherGroup()->back()->play (b); };
        case otherGroupAny:
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                {
                    auto group = ctx->getOtherGroup();
                    const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                    (*group)[index]->play (b);
                };
    };

    return {};
}

}
