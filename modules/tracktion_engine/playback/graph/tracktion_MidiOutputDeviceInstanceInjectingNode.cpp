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

//==============================================================================
//==============================================================================
MidiOutputDeviceInstanceInjectingNode::MidiOutputDeviceInstanceInjectingNode (MidiOutputDeviceInstance& instance,
                                                                              std::unique_ptr<tracktion::graph::Node> inputNode,
                                                                              tracktion::graph::PlayHead& playHeadToUse)
    : deviceInstance (instance), input (std::move (inputNode)), playHead (playHeadToUse)
{
}

//==============================================================================
tracktion::graph::NodeProperties MidiOutputDeviceInstanceInjectingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.numberOfChannels = 0;
    props.nodeID = 0;

    return props;
}

std::vector<tracktion::graph::Node*> MidiOutputDeviceInstanceInjectingNode::getDirectInputNodes()
{
    return { input.get() };
}

void MidiOutputDeviceInstanceInjectingNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
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

    // Block the audio input from reaching the output by not copying
    pc.buffers.midi.copyFrom (sourceBuffers.midi);
    
    const auto timelineSampleRange = referenceSampleRangeToSplitTimelineRange (playHead, pc.referenceSampleRange);
    assert (! timelineSampleRange.isSplit);
    const auto editTimeRange = tracktion::timeRangeFromSamples (timelineSampleRange.timelineRange1, sampleRate);

    // Add MIDI clock for the current time to the device to be dispatched
    deviceInstance.addMidiClockMessagesToCurrentBlock (playHead.isPlaying(), playHead.isUserDragging(), editTimeRange);
    
    if (sourceBuffers.midi.isEmpty() && ! sourceBuffers.midi.isAllNotesOff)
        return;

    // Merge in the MIDI from the current block to the device to be dispatched
    deviceInstance.mergeInMidiMessages (sourceBuffers.midi, editTimeRange.getStart());
}

}} // namespace tracktion { inline namespace engine
