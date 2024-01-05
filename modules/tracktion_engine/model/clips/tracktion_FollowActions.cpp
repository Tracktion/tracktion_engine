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

//==============================================================================
std::optional<FollowAction> followActionFromString (juce::String s)
{
    return magic_enum::enum_cast<FollowAction> (s.toStdString());
}

juce::String toString (FollowAction fa)
{
    return std::string (magic_enum::enum_name (fa));
}

//==============================================================================
namespace follow_action_utils
{
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

inline std::shared_ptr<ClipContext> createClipContext (Clip& c)
{
    auto sc = makeSafeRef (c);
    auto audioTrack = dynamic_cast<AudioTrack*> (c.getTrack());

    if (! audioTrack)
        return {};

    auto ctx = std::make_shared<follow_action_utils::ClipContext>();
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

    return ctx;
}
}


inline std::function<void (MonotonicBeat)> createFollowAction (std::shared_ptr<follow_action_utils::ClipContext> ctx,
                                                               FollowAction followAction)
{
    using enum FollowAction;

    switch (followAction)
    {
        case none:
            return {};

        case globalStop:
            return {};
        case globalReturnToArrangement:
            return [ctx] (auto)
            { ctx->track->playSlotClips = true; };
        case globalPlayAgain:
            return [ctx] (auto b)
            { ctx->launchHandle->play (b); };

        case scenePrevious:
            if (ctx->sceneIndex > 0)
                return [ctx] (auto b)
                { ctx->sceneHandles[ctx->sceneIndex - 1]->play (b); };
        case sceneNext:
            if (ctx->sceneIndex < (ctx->sceneHandles.size() - 1))
                return [ctx] (auto b)
                { ctx->sceneHandles[ctx->sceneIndex + 1]->play (b); };
        case sceneFirst:
            return [ctx] (auto b)
            { ctx->sceneHandles.front()->play (b); };
        case sceneLast:
            return [ctx] (auto b)
            { ctx->sceneHandles.back()->play (b); };
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
            return [ctx] (auto b)
            { ctx->sceneHandles[(ctx->sceneIndex + 1) % ctx->sceneHandles.size()]->play (b); };

        case currentGroupPrevious:
            if (ctx->indexInGroup > 0)
                return [ctx, &group = ctx->getGroup()] (auto b)
                { group[(ctx->indexInGroup - 1)]->play (b); };
        case currentGroupNext:
            if (ctx->indexInGroup < (ctx->getGroup().size() - 1))
                return [ctx, &group = ctx->getGroup()] (auto b)
                { group[(ctx->indexInGroup + 1)]->play (b); };
        case currentGroupFirst:
            return [ctx] (auto b)
            { ctx->getGroup().front()->play (b); };
        case currentGroupLast:
            return [ctx] (auto b)
            { ctx->getGroup().back()->play (b); };
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
                return [ctx, group] (auto b)
                { group->front()->play (b); };
        case previousGroupLast:
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b)
                { group->back()->play (b); };
        case previousGroupAny:
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b)
                {
                    const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                    (*group)[index]->play (b);
                };

        case nextGroupFirst:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b)
                { group->front()->play (b); };
        case nextGroupLast:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b)
                { group->back()->play (b); };
        case nextGroupAny:
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b)
                {
                    const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                    (*group)[index]->play (b);
                };

        case otherGroupFirst:
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                { ctx->getOtherGroup()->front()->play (b); };
        case otherGroupLast:
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                { ctx->getOtherGroup()->back()->play (b); };
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

std::function<void (MonotonicBeat)> createFollowAction (Clip& c)
{
    auto followActions = c.getFollowActions();

    if (! followActions)
        return {};

    auto actions = followActions->getActions();

    if (actions.empty())
        return {};

    auto ctx = follow_action_utils::createClipContext (c);

    // Create actions with probabilities
    if (actions.size() == 1)
        return createFollowAction (ctx, followActions->getActions().front()->action.get());

    struct FollowActionContainer
    {
        juce::Range<double> probabilityRange;
        std::function<void (MonotonicBeat)> action;
    };

    std::vector<FollowActionContainer> followActionContainers;
    double maxProbability = 0.0;

    for (auto action : actions)
    {
        FollowActionContainer container;
        container.probabilityRange = { maxProbability, maxProbability + action->weight };
        container.action = createFollowAction (ctx, action->action);
        followActionContainers.push_back (std::move (container));

        maxProbability = container.probabilityRange.getEnd();
    }

    return [followActionContainers = std::move (followActionContainers), ctx, maxProbability] (MonotonicBeat beat)
              {
                  const auto randomValue = ctx->random.nextFloat() * maxProbability;

                  for (auto& actionContainer : followActionContainers)
                  {
                      if (! actionContainer.probabilityRange.contains (randomValue))
                          continue;

                      if (actionContainer.action)
                          return actionContainer.action (beat);

                      return;
                  }
              };
}


//==============================================================================
//==============================================================================
class FollowActions::List : public ValueTreeObjectList<Action>
{
public:
    List (FollowActions& fa, juce::ValueTree listState)
        : ValueTreeObjectList<Action> (std::move (listState)),
          followActions (fa)
    {
        rebuildObjects();
    }

    ~List() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::ACTION);
    }

    Action* createNewObject (const juce::ValueTree& newState) override
    {
        auto a = new Action();

        auto v = newState;
        a->action.referTo (v, IDs::type, followActions.undoManager, FollowAction::currentGroupRoundRobin);
        a->weight.referTo (v, IDs::weight, followActions.undoManager, 1.0);
        a->state = std::move (v);

        return a;
    }

    void deleteObject (Action* a) override
    {
        delete a;
    }

    void newObjectAdded (Action*) override
    {
        followActions.sendChangeMessage();
    }

    void objectRemoved (Action*) override
    {
        followActions.sendSynchronousChangeMessage();
    }

    void objectOrderChanged() override
    {
        followActions.sendChangeMessage();
    }

private:
    FollowActions& followActions;
};

//==============================================================================
FollowActions::FollowActions (juce::ValueTree v, juce::UndoManager* um)
    : state (std::move (v)), undoManager (um)
{
      list = std::make_unique<List> (*this, state);
}

FollowActions::~FollowActions()
{
}

FollowActions::Action& FollowActions::addAction()
{
    state.appendChild ({ IDs::ACTION, {} }, undoManager);
    return *getActions().back();
}

void FollowActions::removeAction (Action& actionToRemove)
{
    auto actionState = actionToRemove.action.getValueTree();
    actionState.getParent().removeChild (actionState, undoManager);
}

std::span<FollowActions::Action*> FollowActions::getActions() const
{
    return { list->begin(), list->end() };
}

}
