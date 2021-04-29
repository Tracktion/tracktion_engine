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

TrackMidiInputDeviceNode::TrackMidiInputDeviceNode (MidiInputDevice& owner, std::unique_ptr<Node> inputNode)
    : midiInputDevice (owner), input (std::move (inputNode)), copyInputsToOutputs (owner.isEndToEndEnabled())
{
    jassert (midiInputDevice.isTrackDevice());

    setOptimisations ({ tracktion_graph::ClearBuffers::yes,
                        tracktion_graph::AllocateAudioBuffer::no });
}

std::vector<tracktion_graph::Node*> TrackMidiInputDeviceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion_graph::NodeProperties TrackMidiInputDeviceNode::getNodeProperties()
{
    return input->getNodeProperties();
}

void TrackMidiInputDeviceNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    offsetSeconds = tracktion_graph::sampleToTime (info.blockSize, info.sampleRate);
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
        setAudioOutput (sourceBuffers.audio);
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
    }

    // And pass MIDI to device
    for (auto& m : sourceBuffers.midi)
        midiInputDevice.handleIncomingMidiMessage (nullptr, juce::MidiMessage (m, juce::Time::getMillisecondCounterHiRes() * 0.001 + m.getTimeStamp()));
}

}
