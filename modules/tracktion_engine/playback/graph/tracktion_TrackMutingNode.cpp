/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
    wasJustMutedFlag.store (wasBeingPlayedFlag && ! isPlayingNow, std::memory_order_release);
    wasJustUnMutedFlag.store (! wasBeingPlayedFlag && isPlayingNow, std::memory_order_release);
    wasBeingPlayedFlag = isPlayingNow;
}

bool TrackMuteState::shouldTrackBeAudible() const
{
    return wasBeingPlayedFlag || wasJustMuted();
}

bool TrackMuteState::shouldTrackContentsBeProcessed() const
{
    return callInputWhileMuted || wasBeingPlayedFlag;
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
size_t TrackMuteState::getItemID() const
{
    return track != nullptr ? (size_t) track->itemID.getRawID()
                            : (size_t) edit.getProjectItemID().getRawID();
}


//==============================================================================
//==============================================================================
TrackMutingNode::TrackMutingNode (std::unique_ptr<TrackMuteState> muteState, std::unique_ptr<tracktion::graph::Node> inputNode,
                                  bool dontMuteIfTrackContentsShouldBeProcessed_)
    : trackMuteState (std::move (muteState)), input (std::move (inputNode)),
      dontMuteIfTrackContentsShouldBeProcessed (dontMuteIfTrackContentsShouldBeProcessed_)
{
    assert (trackMuteState);

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties TrackMutingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    hash_combine (props.nodeID, trackMuteState->getItemID());

    return props;
}

std::vector<tracktion::graph::Node*> TrackMutingNode::getDirectInputNodes()
{
    return { input.get() };
}

bool TrackMutingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TrackMutingNode::prefetchBlock (juce::Range<int64_t>)
{
    trackMuteState->update();
}

void TrackMutingNode::process (ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioView = pc.buffers.audio;
    jassert (sourceBuffers.audio.getSize() == destAudioView.getSize());
    
    const bool ignoreMuteStates = dontMuteIfTrackContentsShouldBeProcessed && trackMuteState->shouldTrackContentsBeProcessed();
    const bool wasJustMuted     = ! ignoreMuteStates && trackMuteState->wasJustMuted();
    const bool wasJustUnMuted   = ! ignoreMuteStates && trackMuteState->wasJustUnMuted();

    if (trackMuteState->shouldTrackBeAudible() || ignoreMuteStates)
    {
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
        
        // If we've just been muted/unmuted we need to copy the data to
        // apply a fade to, otherwise we can just pass on the view
        if (wasJustMuted || wasJustUnMuted)
            copy (destAudioView, sourceBuffers.audio);
        else
            setAudioOutput (input.get(), sourceBuffers.audio);
    }
    else
    {
        destAudioView.clear();
        pc.buffers.midi.clear();
    }

    if (wasJustMuted)
        rampBlock (destAudioView, 1.0f, 0.0f);
    else if (wasJustUnMuted)
        rampBlock (destAudioView, 0.0f, 1.0f);
}

//==============================================================================
void TrackMutingNode::rampBlock (choc::buffer::ChannelArrayView<float> view, float start, float end)
{
    if (view.getNumChannels() == 0)
        return;
    
    auto buffer = tracktion::graph::toAudioBuffer (view);
    buffer.applyGainRamp (0, buffer.getNumSamples(), start, end);
}

}} // namespace tracktion { inline namespace engine
