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

//==============================================================================
//==============================================================================
MidiOutputDeviceInstanceInjectingNode::MidiOutputDeviceInstanceInjectingNode (MidiOutputDeviceInstance& instance,
                                                                              std::unique_ptr<tracktion_graph::Node> inputNode,
                                                                              tracktion_graph::PlayHead& playHeadToUse)
    : deviceInstance (instance), input (std::move (inputNode)), playHead (playHeadToUse)
{
}

//==============================================================================
tracktion_graph::NodeProperties MidiOutputDeviceInstanceInjectingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion_graph::Node*> MidiOutputDeviceInstanceInjectingNode::getDirectInputNodes()
{
    return { input.get() };
}

void MidiOutputDeviceInstanceInjectingNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

bool MidiOutputDeviceInstanceInjectingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void MidiOutputDeviceInstanceInjectingNode::process (ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();

    pc.buffers.midi.copyFrom (sourceBuffers.midi);
    copy (pc.buffers.audio, sourceBuffers.audio);
    
    if (sourceBuffers.midi.isEmpty() && ! sourceBuffers.midi.isAllNotesOff)
        return;

    // Merge in the MIDI from the current block to the device to be dispatched
    const auto timelineSamplePosition = playHead.referenceSamplePositionToTimelinePosition (pc.referenceSampleRange.getStart());
    const double editTime = tracktion_graph::sampleToTime (timelineSamplePosition, sampleRate);
    deviceInstance.mergeInMidiMessages (sourceBuffers.midi, editTime);
}

} // namespace tracktion_engine
