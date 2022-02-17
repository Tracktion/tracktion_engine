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

TrackWaveInputDeviceNode::TrackWaveInputDeviceNode (WaveInputDevice& owner, std::unique_ptr<Node> inputNode)
    : waveInputDevice (owner), input (std::move (inputNode)), copyInputsToOutputs (owner.isEndToEndEnabled())
{
    jassert (waveInputDevice.isTrackDevice());

    if (copyInputsToOutputs)
        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::no });
}

std::vector<tracktion::graph::Node*> TrackWaveInputDeviceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion::graph::NodeProperties TrackWaveInputDeviceNode::getNodeProperties()
{
    return input->getNodeProperties();
}

void TrackWaveInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
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
        setAudioOutput (input.get(), sourceBuffers.audio);
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
                                    + tracktion::graph::sampleToTime (offsetSamples, sampleRate);

        waveInputDevice.consumeNextAudioBlock (chans, (int) numChans, (int) sourceBuffers.audio.getNumFrames(), streamTime);
    }
}

}} // namespace tracktion { inline namespace engine
