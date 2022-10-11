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

class TrackMuteState;

/**
    Node for an aux send.
    This is a SendNode but needs to update automation with the correct latency.
*/
class AuxSendNode final : public tracktion::graph::SendNode
{
public:
    //==============================================================================
    /** Creates a AuxSendNode to process an aux send. */
    AuxSendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
                 AuxSendPlugin&, tracktion::graph::PlayHeadState&, const TrackMuteState*);

    //==============================================================================
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    void process (ProcessContext&) override;
    
private:
    //==============================================================================
    tracktion::graph::PlayHeadState& playHeadState;

    Plugin::Ptr pluginPtr;
    AuxSendPlugin& sendPlugin;
    
    double sampleRate = 44100.0;
    TimeDuration automationAdjustmentTime;
};

}} // namespace tracktion { inline namespace engine
