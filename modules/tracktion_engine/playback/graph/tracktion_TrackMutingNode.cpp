/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

TrackMuteState::TrackMuteState (Track& t, bool muteForInputsWhenRecording, bool processMidiWhenMuted_)
    : edit (t.edit), track (&t),
      processMidiWhileMuted (processMidiWhenMuted_)
{
    processMidiWhileMuted = true;
    callInputWhileMuted = t.processAudioNodesWhileMuted();
    
    if (muteForInputsWhenRecording)
        if (auto at = dynamic_cast<AudioTrack*> (&t))
            for (auto in : edit.getAllInputDevices())
                if (in->isRecordingActive (t) && in->getTargetTracks().contains (at))
                    inputDevicesToMuteFor.add (in);

    wasBeingPlayedFlag = t.shouldBePlayed();
}

TrackMuteState::TrackMuteState (Edit& e)
    : edit (e)
{
    wasBeingPlayedFlag = ! edit.areAnyTracksSolo();
}

void TrackMuteState::update()
{
    const bool isPlayingNow = isBeingPlayed();
    wasJustMutedFlag = wasBeingPlayedFlag && ! isPlayingNow;
    wasJustUnMutedFlag = ! wasBeingPlayedFlag && isPlayingNow;
    wasBeingPlayedFlag = isPlayingNow;
}

bool TrackMuteState::shouldTrackBeAudible() const
{
    return wasBeingPlayedFlag || wasJustMuted();
}

bool TrackMuteState::shouldTrackContentsBeProcessed() const
{
    return callInputWhileMuted && wasBeingPlayedFlag;
}

bool TrackMuteState::shouldTrackMidiBeProcessed() const
{
    return processMidiWhileMuted;
}

bool TrackMuteState::isBeingPlayed() const
{
    bool playing = track != nullptr ? track->shouldBePlayed() : ! edit.areAnyTracksSolo();

    if (! playing)
        return false;

    for (int i = inputDevicesToMuteFor.size(); --i >= 0;)
        if (inputDevicesToMuteFor.getUnchecked (i)->shouldTrackContentsBeMuted())
            return false;

    return true;
}


//==============================================================================
//==============================================================================
TrackMutingNode::TrackMutingNode (const TrackMuteState& muteState, std::unique_ptr<tracktion_graph::Node> inputNode)
    : trackMuteState (muteState), input (std::move (inputNode))
{
}

//==============================================================================
tracktion_graph::NodeProperties TrackMutingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<Node*> TrackMutingNode::getDirectInputNodes()
{
    return { input.get() };
}

bool TrackMutingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TrackMutingNode::process (const ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    jassert (sourceBuffers.audio.getNumChannels() == destAudioBlock.getNumChannels());

    if (trackMuteState.shouldTrackBeAudible())
    {
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
        destAudioBlock.copyFrom (sourceBuffers.audio);
    }

    if (trackMuteState.wasJustMuted())
        rampBlock (destAudioBlock, 1.0f, 0.0f);
    else if (trackMuteState.wasJustUnMuted())
        rampBlock (destAudioBlock, 0.0f, 1.0f);
}

//==============================================================================
void TrackMutingNode::rampBlock (juce::dsp::AudioBlock<float>& audioBlock, float start, float end)
{
    if (audioBlock.getNumChannels() == 0)
        return;
    
    auto buffer = tracktion_graph::test_utilities::createAudioBuffer (audioBlock);
    buffer.applyGainRamp (0, buffer.getNumSamples(), start, end);
}

} // namespace tracktion_engine
