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

TrackMutingNode::TrackMutingNode (Track& t, std::unique_ptr<tracktion_graph::Node> inputNode,
                                  bool muteForInputsWhenRecording, bool processMidiWhenMuted_)
    : input (std::move (inputNode)), edit (t.edit), track (&t),
      processMidiWhileMuted (processMidiWhenMuted_)
{
    callInputWhileMuted = t.processAudioNodesWhileMuted();
    
    if (muteForInputsWhenRecording)
        if (auto at = dynamic_cast<AudioTrack*> (&t))
            for (auto in : edit.getAllInputDevices())
                if (in->isRecordingActive (t) && in->getTargetTracks().contains (at))
                    inputDevicesToMuteFor.add (in);

    wasBeingPlayed = t.shouldBePlayed();
}

TrackMutingNode::TrackMutingNode (Edit& e, std::unique_ptr<tracktion_graph::Node> inputNode)
    : input (std::move (inputNode)), edit (e)
{
    wasBeingPlayed = ! edit.areAnyTracksSolo();
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
    auto& sourceAudioBlock = sourceBuffers.audio;
    auto destAudioBlock = pc.buffers.audio;
    jassert (sourceAudioBlock.getNumChannels() == destAudioBlock.getNumChannels());

    auto& sourceMidi = sourceBuffers.midi;
    auto& destMidi = pc.buffers.midi;

    const bool isPlayingNow = isBeingPlayed();

    if (wasJustMuted (isPlayingNow))
    {
        //TODO: Sending all-notes-off here won't work as the input will have already been processed
        // send midi off events if we don't want to process midi while muted
        if (! callInputWhileMuted && ! processMidiWhileMuted)
            sendAllNotesOffIfDesired (destMidi);

        destAudioBlock.copyFrom (sourceAudioBlock);
        rampBlock (destAudioBlock, 1.0f, 0.0f);

        if (! callInputWhileMuted && ! processMidiWhileMuted)
        {
            destMidi.clear();
            sendAllNotesOffIfDesired (destMidi);
        }
    }
    else if (wasJustUnMuted (isPlayingNow))
    {
        destMidi.copyFrom (sourceMidi);
        destAudioBlock.copyFrom (sourceAudioBlock);
        rampBlock (destAudioBlock, 1.0f, 0.0f);
    }
    else if (wasBeingPlayed)
    {
        destMidi.copyFrom (sourceMidi);
        destAudioBlock.copyFrom (sourceAudioBlock);
    }
    else if (callInputWhileMuted || processMidiWhileMuted)
    {
        if (processMidiWhileMuted)
        {
            destMidi.copyFrom (sourceMidi);
            destAudioBlock.clear();
        }
        else
        {
            destMidi.clear();
            destAudioBlock.clear();
        }
    }
    else
    {
        destMidi.clear();
        destAudioBlock.clear();
    }

    updateLastMutedState (isPlayingNow);
}

//==============================================================================
bool TrackMutingNode::isBeingPlayed() const
{
    bool playing = track != nullptr ? track->shouldBePlayed() : ! edit.areAnyTracksSolo();

    if (! playing)
        return false;

    for (int i = inputDevicesToMuteFor.size(); --i >= 0;)
        if (inputDevicesToMuteFor.getUnchecked (i)->shouldTrackContentsBeMuted())
            return false;

    return true;
}

bool TrackMutingNode::wasJustMuted (bool isPlayingNow) const
{
    return wasBeingPlayed && ! isPlayingNow;
}

bool TrackMutingNode::wasJustUnMuted (bool isPlayingNow) const
{
    return ! wasBeingPlayed && isPlayingNow;
}

void TrackMutingNode::updateLastMutedState (bool isPlayingNow)
{
    wasBeingPlayed = isPlayingNow;
}

void TrackMutingNode::rampBlock (juce::dsp::AudioBlock<float>& audioBlock, float start, float end)
{
    if (audioBlock.getNumChannels() == 0)
        return;
    
    auto buffer = tracktion_graph::test_utilities::createAudioBuffer (audioBlock);
    buffer.applyGainRamp (0, buffer.getNumSamples(), start, end);
}

void TrackMutingNode::sendAllNotesOffIfDesired (MidiMessageArray& midi)
{
    if (! processMidiWhileMuted)
        midi.isAllNotesOff = true;
}

} // namespace tracktion_engine
