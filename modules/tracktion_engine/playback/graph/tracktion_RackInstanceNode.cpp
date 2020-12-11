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

RackInstanceNode::RackInstanceNode (std::unique_ptr<Node> inputNode,
                                    std::array<std::tuple<int, int, AutomatableParameter::Ptr>, 2>  channelMapToUse)
    : input (std::move (inputNode)), channelMap (std::move (channelMapToUse))
{
    assert (input);
    
    for (auto& chan : channelMap)
    {
		assert (std::get<2> (chan) != nullptr);
        maxNumChannels = std::max (maxNumChannels,
                                   std::max (std::get<0> (chan), std::get<1> (chan)) + 1);
    }
    
    for (size_t chan = 0; chan < 2; ++chan)
        lastGain[chan] = dbToGain (std::get<2> (channelMap[chan])->getCurrentValue());

}

std::vector<tracktion_graph::Node*> RackInstanceNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion_graph::NodeProperties RackInstanceNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.numberOfChannels = maxNumChannels;
    props.hasMidi = true;
    props.hasAudio = true;
    
    return props;
}

void RackInstanceNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool RackInstanceNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void RackInstanceNode::process (ProcessContext& pc)
{
    assert (pc.buffers.audio.getNumChannels() == (size_t) maxNumChannels);
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
        
        choc::buffer::copy (dest, src);

        if (gain == lastGain[channel])
        {
            if (gain != 1.0f)
                choc::buffer::applyGain (dest, gain);
        }
        else
        {
            juce::SmoothedValue<float> smoother (lastGain[channel]);
            smoother.setTargetValue (gain);
            smoother.reset ((int) dest.getNumFrames());
            tracktion_graph::multiplyBy (dest, smoother);
            
            lastGain[channel] = gain;
        }
        
        ++channel;
    }
}

}
