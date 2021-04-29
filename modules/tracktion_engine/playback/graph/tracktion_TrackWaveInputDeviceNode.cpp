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

    if (copyInputsToOutputs)
        setOptimisations ({ tracktion_graph::ClearBuffers::no,
                            tracktion_graph::AllocateAudioBuffer::no });
}

std::vector<tracktion_graph::Node*> TrackWaveInputDeviceNode::getDirectInputNodes()
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
    offsetSamples = waveInputDevice.engine.getDeviceManager().getBlockSize();
}

bool TrackWaveInputDeviceNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void TrackWaveInputDeviceNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    // Pass on input to output
    auto sourceBuffers = input->getProcessedOutput();

    if (copyInputsToOutputs)
    {
        setAudioOutput (sourceBuffers.audio);
        pc.buffers.midi.copyFrom (sourceBuffers.midi);
    }

    // And pass audio to device
    if (auto numChans = juce::jmin (2u, sourceBuffers.audio.getNumChannels()))
    {
        const float* chans[3] = { sourceBuffers.audio.getChannel(0).data.data,
                                  numChans > 1 ? sourceBuffers.audio.getChannel(1).data.data
                                               : nullptr,
                                  nullptr };

        const double streamTime = waveInputDevice.engine.getDeviceManager().getCurrentStreamTime()
                                    + tracktion_graph::sampleToTime (offsetSamples, sampleRate);

        waveInputDevice.consumeNextAudioBlock (chans, (int) numChans, (int) sourceBuffers.audio.getNumFrames(), streamTime);
    }
}

}
