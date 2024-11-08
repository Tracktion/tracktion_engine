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

RackInstanceNode::RackInstanceNode (std::unique_ptr<Node> inputNode, ChannelMap channelMapToUse)
    : input (std::move (inputNode)), channelMap (std::move (channelMapToUse))
{
    assert (input);

    for (auto& chan : channelMap)
    {
        assert (std::get<2> (chan) != nullptr);
        maxNumChannels = std::max (maxNumChannels,
                                   std::get<1> (chan) + 1);
    }

    for (size_t chan = 0; chan < 2; ++chan)
        lastGain[chan] = dbToGain (std::get<2> (channelMap[chan])->getCurrentValue());

}

std::vector<tracktion::graph::Node*> RackInstanceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion::graph::NodeProperties RackInstanceNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.numberOfChannels = (int) maxNumChannels;
    props.hasMidi = true;
    props.hasAudio = true;

    if (props.nodeID != 0)
        hash_combine (props.nodeID, static_cast<size_t> (3615177560026405684)); // "RackInstanceNode"

    return props;
}

void RackInstanceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    if (input->numOutputNodes > 1)
        return;

    const auto inputNumChannels = input->getNodeProperties().numberOfChannels;
    const auto desiredNumChannels = getNodeProperties().numberOfChannels;

    if (info.enableNodeMemorySharing && inputNumChannels >= desiredNumChannels)
    {
        canUseSourceBuffers = true;
        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::no });
    }
}

bool RackInstanceNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void RackInstanceNode::preProcess (choc::buffer::FrameCount, juce::Range<int64_t>)
{
    if (canUseSourceBuffers)
        setBufferViewToUse (input.get(), input->getProcessedOutput().audio);
}

void RackInstanceNode::process (ProcessContext& pc)
{
    assert ((int) pc.buffers.audio.getNumChannels() == maxNumChannels);
    auto inputBuffers = input->getProcessedOutput();

    // Always copy MIDI
    pc.buffers.midi.copyFrom (inputBuffers.midi);

    // Copy audio applying gain
    int channel = 0;

    for (auto& chan : channelMap)
    {
        auto srcChan = std::get<0> (chan);
        auto destChan = std::get<1> (chan);

        if (srcChan < 0)
            continue;

        if (destChan < 0)
            continue;

        if ((choc::buffer::ChannelCount) srcChan >= inputBuffers.audio.getNumChannels())
            continue;

        auto src = inputBuffers.audio.getChannel ((choc::buffer::ChannelCount) srcChan);
        auto dest = pc.buffers.audio.getChannel ((choc::buffer::ChannelCount) destChan);
        auto gain = dbToGain (std::get<2> (chan)->getCurrentValue());

        copyIfNotAliased (dest, src);

        if (gain == lastGain[channel])
        {
            if (gain != 1.0f)
                applyGain (dest, gain);
        }
        else
        {
            juce::SmoothedValue<float> smoother (lastGain[channel]);
            smoother.setTargetValue (gain);
            smoother.reset ((int) dest.getNumFrames());
            applyGainPerFrame (dest, [&] { return smoother.getNextValue(); });

            lastGain[channel] = gain;
        }

        ++channel;
    }
}

}} // namespace tracktion { inline namespace engine
