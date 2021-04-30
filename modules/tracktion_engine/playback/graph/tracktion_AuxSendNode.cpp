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

AuxSendNode::AuxSendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
                          AuxSendPlugin& sourceSendPlugin, tracktion_graph::PlayHeadState& phs,
                          const TrackMuteState* trackMuteState)
    : SendNode (std::move (inputNode), busIDToUse,
                [&sourceSendPlugin, trackMuteState]
                {
                    if (trackMuteState
                        && ! trackMuteState->shouldTrackBeAudible()
                        && ! trackMuteState->shouldTrackContentsBeProcessed())
                       return 0.0f;

                    return volumeFaderPositionToGain (sourceSendPlugin.gain->getCurrentValue());
               }),
      playHeadState (phs),
      pluginPtr (sourceSendPlugin),
      sendPlugin (sourceSendPlugin)
{
    jassert (pluginPtr != nullptr);
}

//==============================================================================
void AuxSendNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    
    if (auto props = getNodeProperties(); props.latencyNumSamples > 0)
        automationAdjustmentTime = -tracktion_graph::sampleToTime (props.latencyNumSamples, sampleRate);
}

void AuxSendNode::process (ProcessContext& pc)
{
    if (sendPlugin.isAutomationNeeded()
        && sendPlugin.edit.getAutomationRecordManager().isReadingAutomation())
    {
        const auto editSamplePos = playHeadState.playHead.referenceSamplePositionToTimelinePosition (pc.referenceSampleRange.getStart());
        const auto editTime = tracktion_graph::sampleToTime (editSamplePos, sampleRate) + automationAdjustmentTime;
        sendPlugin.updateParameterStreams (editTime);
    }
    
    SendNode::process (pc);
}

}
