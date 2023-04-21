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

AuxSendNode::AuxSendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
                          AuxSendPlugin& sourceSendPlugin, tracktion::graph::PlayHeadState& phs,
                          const TrackMuteState* trackMuteState)
    : SendNode (std::move (inputNode), busIDToUse,
                [&sourceSendPlugin, trackMuteState]
                {
                    if (trackMuteState
                        && ! trackMuteState->shouldTrackBeAudible()
                        && ! trackMuteState->shouldTrackContentsBeProcessed())
                       return 0.0f;

                    auto gain = volumeFaderPositionToGain (sourceSendPlugin.gain->getCurrentValue());
                    if (sourceSendPlugin.invertPhase)
                        gain *= -1.0f;

                    return gain;
               }),
      playHeadState (phs),
      pluginPtr (sourceSendPlugin),
      sendPlugin (sourceSendPlugin)
{
    jassert (pluginPtr != nullptr);
}

//==============================================================================
void AuxSendNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    
    if (auto props = getNodeProperties(); props.latencyNumSamples > 0)
        automationAdjustmentTime = TimeDuration::fromSamples (-props.latencyNumSamples, sampleRate);
}

void AuxSendNode::process (ProcessContext& pc)
{
    if (sendPlugin.isAutomationNeeded()
        && sendPlugin.edit.getAutomationRecordManager().isReadingAutomation())
    {
        const auto editSamplePos = playHeadState.playHead.referenceSamplePositionToTimelinePosition (pc.referenceSampleRange.getStart());
        const auto editTime = TimePosition::fromSamples (editSamplePos, sampleRate) + automationAdjustmentTime;
        sendPlugin.updateParameterStreams (editTime);
    }
    
    SendNode::process (pc);
}

}} // namespace tracktion { inline namespace engine
