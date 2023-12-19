/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine
{

//==============================================================================
/** Determines the type of action to perform after a Clip has played for a set period. */
enum class FollowAction
{
    none,

    globalStop,
    globalReturnToArrangement,
    globalplayAgain,

    scenePrevious,
    sceneNext,
    sceneFirst,
    sceneLast,
    sceneAny,
    sceneOther,
    sceneRoundRobin,

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
