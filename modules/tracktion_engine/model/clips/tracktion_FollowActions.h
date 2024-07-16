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
/** Determines the type of action to perform after a Clip has played for a set period. */
enum class FollowAction
{
    none,

    globalStop,
    globalReturnToArrangement,
    globalPlayAgain,

    trackPrevious,
    trackNext,
    trackFirst,
    trackLast,
    trackAny,
    trackOther,
    trackRoundRobin,

    currentGroupPrevious,
    currentGroupNext,
    currentGroupFirst,
    currentGroupLast,
    currentGroupAny,
    currentGroupOther,
    currentGroupRoundRobin,

    previousGroupFirst,
    previousGroupLast,
    previousGroupAny,

    nextGroupFirst,
    nextGroupLast,
    nextGroupAny,

    otherGroupFirst,
    otherGroupLast,
    otherGroupAny,
};

//==============================================================================
/** Converts a FollowAction choice to a FollowAction if possible. */
std::optional<FollowAction> followActionFromChoice (juce::String);

/** Converts a string to a FollowAction if possible. */
std::optional<FollowAction> followActionFromString (juce::String);

/** Converts a FollowAction to a string if possible. */
juce::String toString (FollowAction);

/** Creates a follow action for a Clip. */
std::function<void (MonotonicBeat)> createFollowAction (Clip&);

//==============================================================================
//==============================================================================
class FollowActions : public juce::ChangeBroadcaster
{
public:
    struct Action
    {
        juce::ValueTree state;
        juce::CachedValue<FollowAction> action;
        juce::CachedValue<double> weight;
    };

    FollowActions (juce::ValueTree, juce::UndoManager*);
    ~FollowActions() override;

    Action& addAction();

    void removeAction (Action&);

    std::span<Action*> getActions() const;

private:
    juce::ValueTree state;
    juce::UndoManager* undoManager = nullptr;
    class List;
    std::unique_ptr<List> list;
};

}


//==============================================================================
//==============================================================================
namespace juce
{
    template <>
    struct VariantConverter<tracktion::engine::FollowAction>
    {
        static tracktion::engine::FollowAction fromVar (const var& v)
        {
            using namespace tracktion::engine;
            return followActionFromString (v.toString()).value_or (FollowAction::none);
        }

        static var toVar (tracktion::engine::FollowAction v)
        {
            return toString (v);
        }
    };
}
