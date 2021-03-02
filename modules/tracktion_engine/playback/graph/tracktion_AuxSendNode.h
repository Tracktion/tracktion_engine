/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_engine
{

class TrackMuteState;

/**
    Node for an aux send.
    This is a SendNode but needs to update automation with the correct latency.
*/
class AuxSendNode final : public tracktion_graph::SendNode
{
public:
    //==============================================================================
    /** Creates a AuxSendNode to process an aux send. */
    AuxSendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
                 AuxSendPlugin&, tracktion_graph::PlayHeadState&, const TrackMuteState*);

    //==============================================================================
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    void process (ProcessContext&) override;
    
private:
    //==============================================================================
    tracktion_graph::PlayHeadState& playHeadState;

    Plugin::Ptr pluginPtr;
    AuxSendPlugin& sendPlugin;
    
    double sampleRate = 44100.0;
    double automationAdjustmentTime = 0.0;
};

}
