/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

/**
    A Node that switches between two of its inputs based on a flag.
*/
class ArrangerLauncherSwitchingNode final    : public tracktion::graph::Node
{
public:
    ArrangerLauncherSwitchingNode (AudioTrack&,
                                   std::unique_ptr<Node> arrangerNode,
                                   std::unique_ptr<Node> launcherNode);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    AudioTrack::Ptr track;
    std::unique_ptr<Node> arrangerNode, launcherNode;
    bool wasPlayingSlots = track->playSlotClips.get();
    std::shared_ptr<std::vector<float>> channelState;
};

}} // namespace tracktion { inline namespace engine
