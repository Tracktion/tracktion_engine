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

TrackWaveInputDeviceNode::TrackWaveInputDeviceNode (WaveInputDevice& owner, std::unique_ptr<Node> inputNode)
    : waveInputDevice (owner), input (std::move (inputNode)), copyInputsToOutputs (owner.isEndToEndEnabled())
{
    jassert (waveInputDevice.isTrackDevice());
}

std::vector<Node*> TrackWaveInputDeviceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion_graph::NodeProperties TrackWaveInputDeviceNode::getNodeProperties()
{
    return input->getNodeProperties();
}

void TrackWaveInputDeviceNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    offsetSamples = info.blockSize;
}

bool TrackWaveInputDeviceNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TrackWaveInputDeviceNode::process (const ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    // Pass on input to output
    auto sourceBuffers = input->getProcessedOutput();

    if (copyInputsToOutputs)
    {
        pc.buffers.audio.copyFrom (sourceBuffers.audio);
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
    }

    // And pass audio to device
    const int numChans = juce::jmin (2, (int) sourceBuffers.audio.getNumChannels());

    if (numChans > 0)
    {
        const float* chans[3] = { sourceBuffers.audio.getChannelPointer (0),
                                  numChans > 1 ? sourceBuffers.audio.getChannelPointer (1) : nullptr,
                                  nullptr };
        const double streamTime = sampleToTime (pc.referenceSampleRange.getStart() - offsetSamples, sampleRate);
        waveInputDevice.consumeNextAudioBlock (chans, numChans, (int) sourceBuffers.audio.getNumSamples(), streamTime);
    }
}

}
