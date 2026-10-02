/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <span>

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
    std::vector<std::shared_ptr<LaunchHandle>> allSceneHandles;
    std::vector<std::shared_ptr<LaunchHandle>> validSceneHandles;   // enabled clips
    std::vector<std::shared_ptr<LaunchHandle>> disabledHandles;
    std::vector<std::vector<std::shared_ptr<LaunchHandle>>> groups;
    size_t sceneIndex = 0, groupIndex = 0, indexInGroup = 0;
    juce::Random random;

    bool isDisabled (const std::shared_ptr<LaunchHandle>& h) const
    {
        return contains_v (disabledHandles, h);
    }

    /** Disabled clips can't be launched, so a follow action targeting one does nothing. */
    void play (const std::shared_ptr<LaunchHandle>& h, MonotonicBeat b) const
    {
        if (h && ! isDisabled (h))
            h->play (b);
    }

    size_t getIndexInValidHandles() const
    {
        if (auto iter = std::find (validSceneHandles.begin(), validSceneHandles.end(), launchHandle);
            iter != validSceneHandles.end())
            return static_cast<size_t> (std::distance (validSceneHandles.begin(), iter));

        assert (false);
        return 0;
    }

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

            // Disabled clips stay in the groups, so their shape doesn't change,
            // but aren't chosen by the track's actions. The clip itself is playing
            if (lh && clip->disabled.get() && clip != &c)
                ctx->disabledHandles.push_back (std::move (lh));
            else if (lh)
                ctx->validSceneHandles.emplace_back (std::move (lh));
        }
        else
        {
            allSceneHandles.push_back (nullptr);
        }
    }

    if (ctx->validSceneHandles.empty())
        return {};

    // Create groups
    ctx->sceneIndex = static_cast<size_t> (c.getClipSlot()->getIndex());

    for (auto groupView : split (allSceneHandles, nullptr))
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
    ctx->allSceneHandles = std::move (allSceneHandles);

    return ctx;
}

inline std::shared_ptr<LaunchHandle> getLaunchHandle (follow_action_utils::ClipContext& ctx,
                                                      FollowAction followAction)
{
    using enum FollowAction;

    assert (! ctx.getGroup().empty());
    assert (! ctx.validSceneHandles.empty());

    switch (followAction)
    {
        case globalReturnToArrangement: [[ fallthrough ]];
        case trackAny:                  [[ fallthrough ]];
        case trackOther:                [[ fallthrough ]];
        case currentGroupAny:           [[ fallthrough ]];
        case currentGroupOther:         [[ fallthrough ]];
        case previousGroupAny:          [[ fallthrough ]];
        case otherGroupFirst:           [[ fallthrough ]];
        case otherGroupLast:            [[ fallthrough ]];
        case nextGroupAny:              [[ fallthrough ]];
        case otherGroupAny:
            assert (false && "These actions are random can't return launch handles");
            break;

        case none:
            return {};

        case globalStop:
            return {};

        case globalPlayAgain:
            return ctx.launchHandle;

        case trackPrevious:
        {
            if (auto validIndex = ctx.getIndexInValidHandles(); validIndex > 0)
                if (auto prevHandle = ctx.validSceneHandles[validIndex - 1]; prevHandle)
                    return prevHandle;

            break;
        }
        case trackNext:
        {
            if (auto validIndex = ctx.getIndexInValidHandles(); validIndex < (ctx.validSceneHandles.size() - 1))
                if (auto nextHandle = ctx.validSceneHandles[validIndex + 1]; nextHandle)
                    return nextHandle;

            break;
        }
        case trackFirst:
        {
            return ctx.validSceneHandles.front();
        }
        case trackLast:
        {
            return ctx.validSceneHandles.back();
        }
        case trackRoundRobin:
        {
            const auto validIndex = ctx.getIndexInValidHandles();
            return ctx.validSceneHandles[(validIndex + 1) % ctx.validSceneHandles.size()];
        }
        case currentGroupPrevious:
        {
            if (ctx.indexInGroup > 0)
                return ctx.getGroup()[(ctx.indexInGroup - 1)];

            break;
        }
        case currentGroupNext:
        {
            if (ctx.indexInGroup < (ctx.getGroup().size() - 1))
                return ctx.getGroup()[(ctx.indexInGroup + 1)];

            break;
        }
        case currentGroupFirst:
        {
            return ctx.getGroup().front();
        }
        case currentGroupLast:
        {
            return ctx.getGroup().back();
        }
        case currentGroupRoundRobin:
        {
            auto &group = ctx.getGroup();
            return group[(ctx.indexInGroup + 1) % group.size()];
        }
        case previousGroupFirst:
        {
            if (auto group = ctx.getPreviousGroup())
                return group->front();

            break;
        }
        case previousGroupLast:
        {
            if (auto group = ctx.getPreviousGroup())
                return group->back();

            break;
        }
        case nextGroupFirst:
        {
            if (auto group = ctx.getNextGroup())
                return group->front();

            break;
        }
        case nextGroupLast:
        {
            if (auto group = ctx.getNextGroup())
                return group->back();

            break;
        }
    };

    return {};
}
}

inline std::function<void (MonotonicBeat)> createFollowAction (std::shared_ptr<follow_action_utils::ClipContext> ctx,
                                                               FollowAction followAction)
{
    using enum FollowAction;

    switch (followAction)
    {
        case globalReturnToArrangement:
        {
            // This runs on the audio thread, so it can't set the property itself
            return [ctx] (auto)
                   { ctx->track->returnToArrangementRequested.store (true, std::memory_order_release); };
        }
        case trackAny:
        {
            return [ctx] (auto b)
                   {
                       const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (ctx->validSceneHandles.size()) }));
                       ctx->play (ctx->validSceneHandles[index], b);
                   };
        }
        case trackOther:
        {
            if (ctx->validSceneHandles.size() > 1)
                return [ctx] (auto b)
                       {
                           for (;;)
                           {
                               const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (ctx->validSceneHandles.size()) }));

                               if (index == ctx->sceneIndex)
                                   continue;

                               ctx->play (ctx->validSceneHandles[index], b);
                               break;
                           }
                       };

            break;
        }
        case currentGroupAny:
        {
            return [ctx, &group = ctx->getGroup()] (auto b)
                   {
                       const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group.size()) }));
                       ctx->play (group[index], b);
                   };
        }
        case currentGroupOther:
        {
            if (ctx->getGroup().size() > 1)
                return [ctx, &group = ctx->getGroup()] (auto b)
                       {
                           for (;;)
                           {
                               const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group.size()) }));

                               if (index == ctx->indexInGroup)
                                   continue;

                               ctx->play (group[index], b);
                               break;
                           }
                       };

            break;
        }
        case previousGroupAny:
        {
            if (auto group = ctx->getPreviousGroup())
                return [ctx, group] (auto b)
                       {
                           const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                           ctx->play ((*group)[index], b);
                       };

            break;
        }
        case nextGroupAny:
        {
            if (auto group = ctx->getNextGroup())
                return [ctx, group] (auto b)
                       {
                           const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                           ctx->play ((*group)[index], b);
                       };

            break;
        }
        case otherGroupFirst:
        {
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                       { ctx->play (ctx->getOtherGroup()->front(), b); };

            break;
        }
        case otherGroupLast:
        {
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                       { ctx->play (ctx->getOtherGroup()->back(), b); };

            break;
        }
        case otherGroupAny:
        {
            if (ctx->groups.size() > 1)
                return [ctx] (auto b)
                       {
                           auto group = ctx->getOtherGroup();
                           const auto index = static_cast<size_t> (ctx->random.nextInt ({ 0, static_cast<int> (group->size()) }));
                           ctx->play ((*group)[index], b);
                       };

            break;
        }
        case none:                      [[ fallthrough ]];
        case globalStop:                [[ fallthrough ]];
        case globalPlayAgain:           [[ fallthrough ]];
        case trackPrevious:             [[ fallthrough ]];
        case trackNext:                 [[ fallthrough ]];
        case trackFirst:                [[ fallthrough ]];
        case trackLast:                 [[ fallthrough ]];
        case trackRoundRobin:           [[ fallthrough ]];
        case currentGroupPrevious:      [[ fallthrough ]];
        case currentGroupNext:          [[ fallthrough ]];
        case currentGroupFirst:         [[ fallthrough ]];
        case currentGroupLast:          [[ fallthrough ]];
        case currentGroupRoundRobin:    [[ fallthrough ]];
        case previousGroupFirst:        [[ fallthrough ]];
        case previousGroupLast:         [[ fallthrough ]];
        case nextGroupFirst:            [[ fallthrough ]];
        case nextGroupLast:
        {
            // All these are know at graph build time
            if (auto lh = getLaunchHandle (*ctx, followAction); lh && ! ctx->isDisabled (lh))
                return [lh] (auto b) { lh->play (b); };

            break;
        }
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

    return [actionContainers = std::move (followActionContainers), ctx, maxProbability] (MonotonicBeat beat)
              {
                  const auto randomValue = ctx->random.nextFloat() * maxProbability;

                  for (auto& actionContainer : actionContainers)
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
