/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
/** Converts an integer sample number to a time in seconds. */
template<typename IntType>
constexpr double sampleToTime (IntType samplePosition, double sampleRate)
{
    return samplePosition / sampleRate;
}

/** Converts a time in seconds to a sample number. */
constexpr int64_t timeToSample (double timeInSeconds, double sampleRate)
{
    return static_cast<int64_t> ((timeInSeconds * sampleRate) + 0.5);
}

/** Converts an integer sample range to a time range in seconds. */
template<typename IntType>
constexpr juce::Range<double> sampleToTime (juce::Range<IntType> sampleRange, double sampleRate)
{
    return { sampleToTime (sampleRange.getStart(), sampleRate),
             sampleToTime (sampleRange.getEnd(), sampleRate) };
}

/** Converts a time range in seconds to a range of sample numbers. */
constexpr juce::Range<int64_t> timeToSample (juce::Range<double> timeInSeconds, double sampleRate)
{
    return { timeToSample (timeInSeconds.getStart(), sampleRate),
             timeToSample (timeInSeconds.getEnd(), sampleRate) };
}

/** Converts a time range in seconds to a range of sample numbers. */
template<typename RangeType>
constexpr juce::Range<int64_t> timeToSample (RangeType timeInSeconds, double sampleRate)
{
    return { timeToSample (timeInSeconds.getStart(), sampleRate),
             timeToSample (timeInSeconds.getEnd(), sampleRate) };
}

//==============================================================================
//==============================================================================
/** Attempts to find a Node of a given type with a specified ID.
    This uses the sortedNodes vector so should be relatively quick (much quicker
    than traversing from the root node.
*/
template<typename NodeType, typename Predicate>
NodeType* findNode (NodeGraph& nodeGraph, Predicate pred)
{
    auto found = std::find_if (nodeGraph.sortedNodes.begin(),
                               nodeGraph.sortedNodes.end(),
                               [&pred] (auto nodeAndID)
                               {
                                   if (auto foundType = dynamic_cast<NodeType*> (nodeAndID.node))
                                       if (pred (*foundType))
                                           return true;

                                   return false;
                               });

    if (found != nodeGraph.sortedNodes.end())
        return dynamic_cast<NodeType*> (found->node);

    return nullptr;
}

/** Attempts to find a Node of a given type with a specified ID.
    This uses the sortedNodes vector so should be relatively quick (much quicker
    than traversing from the root node.
*/
template<typename NodeType>
NodeType* findNodeWithID (NodeGraph& nodeGraph, size_t nodeIDToLookFor)
{
    auto found = std::find_if (nodeGraph.sortedNodes.begin(),
                               nodeGraph.sortedNodes.end(),
                               [nodeIDToLookFor] (auto nodeAndID)
                               {
                                   return nodeAndID.id == nodeIDToLookFor
                                       && dynamic_cast<NodeType*> (nodeAndID.node) != nullptr;
                               });

    if (found != nodeGraph.sortedNodes.end())
        return dynamic_cast<NodeType*> (found->node);

    return nullptr;
}

/** Attempts to find a Node of a given type with a specified ID.
    This uses the sortedNodes vector so should be relatively quick (much quicker
    than traversing from the root node.
*/
template<typename NodeType>
NodeType* findNodeWithIDIfNonZero (NodeGraph* nodeGraph, size_t nodeIDToLookFor)
{
    return (nodeGraph == nullptr || nodeIDToLookFor == 0)
                ? nullptr
                : findNodeWithID<NodeType> (*nodeGraph, nodeIDToLookFor);
}

}}
