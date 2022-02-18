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

std::vector<tracktion::graph::Node*> RackReturnNode::getDirectInputNodes()
{
    return { wetInput.get(), dryInput };
}

tracktion::graph::NodeProperties RackReturnNode::getNodeProperties()
{
    auto wetProps = wetInput->getNodeProperties();
    auto dryProps = dryInput->getNodeProperties();
    
    auto props = wetProps;
    props.hasAudio = true;
    props.numberOfChannels = std::max (wetProps.numberOfChannels, dryProps.numberOfChannels);
    props.latencyNumSamples = std::max (wetProps.latencyNumSamples, dryProps.latencyNumSamples);
    hash_combine (props.nodeID, dryProps.nodeID);

    constexpr size_t rackReturnNodeMagicHash = 0x726b52657475726e;
    
    if (props.nodeID != 0)
        hash_combine (props.nodeID, rackReturnNodeMagicHash);

    return props;
}

bool RackReturnNode::transform (Node&)
{
    if (hasTransformed)
        return false;
    
    hasTransformed = true;
    auto wetProps = wetInput->getNodeProperties();
    auto dryProps = dryInput->getNodeProperties();
    assert (dryProps.latencyNumSamples <= wetProps.latencyNumSamples);
    
    if (wetProps.latencyNumSamples > dryProps.latencyNumSamples)
    {
        const int numLatencySamples = wetProps.latencyNumSamples - dryProps.latencyNumSamples;
        dryLatencyNode = tracktion::graph::makeNode<tracktion::graph::LatencyNode> (dryInput, numLatencySamples);
        dryInput = dryLatencyNode.get();
        
        return true;
    }

    return false;
}

void RackReturnNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
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

    
    // Always copy MIDI (N.B. MIDI is always the wet signal with no gain applied)
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
            tracktion::graph::add (dryDestView, drySource.audio, dryGain);
    }
    else
    {
        tracktion::graph::addApplyingGainRamp (dryDestView, drySource.audio, lastDryGain, dryGain);
        lastDryGain = dryGain;
    }
}

}} // namespace tracktion { inline namespace engine
