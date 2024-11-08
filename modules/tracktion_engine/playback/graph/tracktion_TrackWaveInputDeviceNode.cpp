/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace engine
{

TrackWaveInputDeviceNode::TrackWaveInputDeviceNode (ProcessState& processState_, WaveInputDevice& owner, std::unique_ptr<Node> inputNode,
                                                    bool copyInputsToOutputs_)
    : TracktionEngineNode (processState_),
      waveInputDevice (owner), input (std::move (inputNode)), copyInputsToOutputs (copyInputsToOutputs_)
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
    auto props = input->getNodeProperties();

    if (props.nodeID != 0)
        hash_combine (props.nodeID, static_cast<size_t> (16857601999796838624ul)); // "TrackWaveInputDeviceNode"

    return props;
}

void TrackWaveInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;

    auto latencyUpToThisPoint = getNodeProperties().latencyNumSamples;
    const auto latencyUpToThisPointS = TimeDuration::fromSamples (latencyUpToThisPoint, sampleRate);
    offset = -latencyUpToThisPointS;
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

        const auto streamPos = TimePosition::fromSamples (getReferenceSampleRange().getStart(), sampleRate);
        waveInputDevice.consumeNextAudioBlock (chans, (int) numChans, (int) sourceBuffers.audio.getNumFrames(),
                                               (streamPos + offset).inSeconds());
    }
}

}} // namespace tracktion { inline namespace engine
