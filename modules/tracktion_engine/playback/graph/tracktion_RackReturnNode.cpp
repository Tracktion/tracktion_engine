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

RackReturnNode::RackReturnNode (std::unique_ptr<Node> wetNode,
                                std::function<float()> wetGainFunc,
                                Node* dryNode,
                                std::function<float()> dryGainFunc)
    : wetInput (std::move (wetNode)),
      dryInput (dryNode),
      wetGainFunction (std::move (wetGainFunc)),
      dryGainFunction (std::move (dryGainFunc))
{
    assert (wetInput);
    assert (wetGainFunction);
    assert (dryInput);
    assert (dryGainFunction);

    lastWetGain = wetGainFunction();
    lastDryGain = dryGainFunction();
}

std::vector<tracktion_graph::Node*> RackReturnNode::getDirectInputNodes()
{
    return { wetInput.get(), dryInput };
}

tracktion_graph::NodeProperties RackReturnNode::getNodeProperties()
{
    auto wetProps = wetInput->getNodeProperties();
    auto dryProps = dryInput->getNodeProperties();
    
    assert (wetProps.latencyNumSamples == dryProps.latencyNumSamples);
    
    auto props = wetProps;
    props.hasAudio = true;
    props.numberOfChannels = std::max (wetProps.numberOfChannels, dryProps.numberOfChannels);
    tracktion_graph::hash_combine (props.nodeID, dryProps.nodeID);

    constexpr size_t rackReturnNodeMagicHash = 0x726b52657475726e;
    
    if (props.nodeID != 0)
        tracktion_graph::hash_combine (props.nodeID, rackReturnNodeMagicHash);

    return props;
}

void RackReturnNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool RackReturnNode::isReadyToProcess()
{
    return wetInput->hasProcessed() && dryInput->hasProcessed();
}

void RackReturnNode::process (ProcessContext& pc)
{
    auto destAudio = pc.buffers.audio;
    
    auto wetSource = wetInput->getProcessedOutput();
    auto drySource = dryInput->getProcessedOutput();

    assert (destAudio.getNumFrames() == wetSource.audio.getNumFrames());
    assert (wetSource.audio.getNumFrames() == drySource.audio.getNumFrames());
    
    const float wetGain = wetGainFunction();
    const float dryGain = dryGainFunction();

    
    // Always copy MIDI (N.B. MIDI is always the we signal with no gain applied)
    pc.buffers.midi.copyFrom (wetSource.midi);

    
    // Copy wet audio applying gain
    copy (destAudio.getFirstChannels (wetSource.audio.getNumChannels()),
          wetSource.audio);

    if (wetGain == lastWetGain)
    {
        if (wetGain != 1.0f)
            applyGain (destAudio, wetGain);
    }
    else
    {
        const auto step = (wetGain - lastWetGain) / destAudio.getNumFrames();
        applyGainPerFrame (destAudio, [start = lastWetGain, step]() mutable { return start += step; });

        lastWetGain = wetGain;
    }

    
    // Add dry audio applying gain
    auto dryDestView = destAudio.getFirstChannels (drySource.audio.getNumChannels());
    
    if (dryGain == lastDryGain)
    {
        if (dryGain == 1.0f)
            add (dryDestView, drySource.audio);
        else
            tracktion_graph::add (dryDestView, drySource.audio, dryGain);
    }
    else
    {
        tracktion_graph::addApplyingGainRamp (dryDestView, drySource.audio, lastDryGain, dryGain);
        lastDryGain = dryGain;
    }
}

} // namespace tracktion_engine
