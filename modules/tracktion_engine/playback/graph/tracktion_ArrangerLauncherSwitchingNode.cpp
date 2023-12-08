/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_ArrangerLauncherSwitchingNode.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
ArrangerLauncherSwitchingNode::ArrangerLauncherSwitchingNode (AudioTrack& at,
                                                              std::unique_ptr<Node> arrangerNode_,
                                                              std::unique_ptr<Node> launcherNode_)
    : track (at),
      arrangerNode (std::move (arrangerNode_)),
      launcherNode (std::move (launcherNode_))
{
    assert (arrangerNode || launcherNode);
    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties ArrangerLauncherSwitchingNode::getNodeProperties()
{
    constexpr size_t seed = 7653239033668669842; // std::hash<std::string_view>{} ("ArrangerLauncherSwitchingNode"sv)
    NodeProperties props = { .nodeID = hash (seed, track->itemID) };

    for (auto n : getDirectInputNodes())
    {
        auto nodeProps = n->getNodeProperties();
        props.hasAudio = props.hasAudio || nodeProps.hasAudio;
        props.hasMidi = props.hasMidi || nodeProps.hasMidi;
        props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
        props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        hash_combine (props.nodeID, nodeProps.nodeID);
    }

    return props;
}

std::vector<tracktion::graph::Node*> ArrangerLauncherSwitchingNode::getDirectInputNodes()
{
    std::vector<tracktion::graph::Node*> nodes;

    if (arrangerNode)
        nodes.push_back (arrangerNode.get());

    if (launcherNode)
        nodes.push_back (launcherNode.get());

    return nodes;
}

void ArrangerLauncherSwitchingNode::prepareToPlay (const PlaybackInitialisationInfo& info)
{
    const auto props = getNodeProperties();
    const int numChannels = props.numberOfChannels;

    if (auto oldGraph = info.nodeGraphToReplace)
    {
        if (auto oldNode = findNodeWithID<ArrangerLauncherSwitchingNode> (*oldGraph, props.nodeID))
        {
            wasPlayingSlots = oldNode->wasPlayingSlots;

            if (oldNode->channelState && ssize (*oldNode->channelState) == numChannels)
               channelState = oldNode->channelState;
        }
    }

    if (! channelState)
    {
        channelState = std::make_shared<std::vector<float>>();

        for (int i = numChannels; --i >= 0;)
            channelState->emplace_back (0.0f);
    }
}

bool ArrangerLauncherSwitchingNode::isReadyToProcess()
{
    if (! launcherNode)
        return arrangerNode->hasProcessed();

    if (! arrangerNode)
        return launcherNode->hasProcessed();

    return arrangerNode->hasProcessed() && launcherNode->hasProcessed();
}

void ArrangerLauncherSwitchingNode::process (ProcessContext& pc)
{
    auto destAudioView = pc.buffers.audio;
    const auto numDestChannels = destAudioView.getNumChannels();
    const auto numFrames = destAudioView.getNumFrames();
    assert (numDestChannels == channelState->size());
    const bool isPlayingSlots = track->playSlotClips.get();

    if (isPlayingSlots)
    {
        if (launcherNode)
        {
            auto sourceBuffers = launcherNode->getProcessedOutput();
            const auto numSourceChannels = sourceBuffers.audio.getNumChannels();
            copyIfNotAliased (destAudioView.getFirstChannels (numSourceChannels), sourceBuffers.audio);
            pc.buffers.midi.copyFrom (sourceBuffers.midi);

            if (auto numChansToClear = numDestChannels - numSourceChannels; numChansToClear > 0)
                destAudioView.getChannelRange ({ numSourceChannels, numSourceChannels + numChansToClear }).clear();

            // Ramp in track
            if (! wasPlayingSlots)
            {
                auto buffer = tracktion::graph::toAudioBuffer (destAudioView);
                buffer.applyGainRamp (0, std::min (10, buffer.getNumSamples()), 0.0f, 1.0f);
            }
        }
        else
        {
            destAudioView.clear();
        }
    }
    else
    {
        if (arrangerNode)
        {
            auto sourceBuffers = arrangerNode->getProcessedOutput();
            const auto numSourceChannels = sourceBuffers.audio.getNumChannels();
            copyIfNotAliased (destAudioView.getFirstChannels (numSourceChannels), sourceBuffers.audio);
            pc.buffers.midi.copyFrom (sourceBuffers.midi);

            if (auto numChansToClear = numDestChannels - numSourceChannels; numChansToClear > 0)
                destAudioView.getChannelRange ({ numSourceChannels, numSourceChannels + numChansToClear }).clear();

            // Ramp in launcher
            if (wasPlayingSlots)
            {
                auto buffer = tracktion::graph::toAudioBuffer (destAudioView);
                buffer.applyGainRamp (0, std::min (10, buffer.getNumSamples()), 0.0f, 1.0f);
            }
        }
        else
        {
            destAudioView.clear();
        }
    }

    const auto fadeLength = wasPlayingSlots != isPlayingSlots ? std::min (10u, numFrames) : 0;
    wasPlayingSlots = isPlayingSlots;

    for (choc::buffer::ChannelCount channel = 0; channel < numDestChannels; ++channel)
    {
        const auto dest = pc.buffers.audio.getIterator (channel).sample;
        auto& lastSample = (*channelState)[(size_t) channel];
        lastSample = dest[numFrames - 1];

        if (fadeLength == 0)
            continue;

        for (uint32_t i = 0; i < fadeLength; ++i)
        {
            auto alpha = i / (float) fadeLength;
            dest[i] = alpha * dest[i] + lastSample * (1.0f - alpha);
        }
    }
}

}} // namespace tracktion { inline namespace engine
