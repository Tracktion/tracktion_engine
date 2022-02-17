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

TrackMidiInputDeviceNode::TrackMidiInputDeviceNode (MidiInputDevice& owner, std::unique_ptr<Node> inputNode, ProcessState& ps)
    : TracktionEngineNode (ps),
      midiInputDevice (owner), input (std::move (inputNode)), copyInputsToOutputs (owner.isEndToEndEnabled())
{
    jassert (midiInputDevice.isTrackDevice());

    setOptimisations ({ tracktion::graph::ClearBuffers::yes,
                        copyInputsToOutputs ? tracktion::graph::AllocateAudioBuffer::no
                                            : tracktion::graph::AllocateAudioBuffer::yes });
}

std::vector<tracktion::graph::Node*> TrackMidiInputDeviceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion::graph::NodeProperties TrackMidiInputDeviceNode::getNodeProperties()
{
    return input->getNodeProperties();
}

void TrackMidiInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    offsetSeconds = tracktion::graph::sampleToTime (info.blockSize, info.sampleRate);
}

bool TrackMidiInputDeviceNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TrackMidiInputDeviceNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    // Pass on input to output
    auto sourceBuffers = input->getProcessedOutput();

    if (copyInputsToOutputs)
    {
        setAudioOutput (input.get(), sourceBuffers.audio);
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
    }
    
    const double midiStreamTime = tracktion::graph::sampleToTime (getReferenceSampleRange().getStart(), getSampleRate())
                                   - midiInputDevice.getAdjustSecs();

    // And pass MIDI to device
    for (auto& m : sourceBuffers.midi)
        midiInputDevice.handleIncomingMidiMessage (nullptr, juce::MidiMessage (m, midiStreamTime + m.getTimeStamp()));
}

}} // namespace tracktion { inline namespace engine
