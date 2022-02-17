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

WaveInputDeviceNode::WaveInputDeviceNode (InputDeviceInstance& idi, WaveInputDevice& owner,
                                          const juce::AudioChannelSet& destChannelsToFill)
    : instance (idi),
      waveInputDevice (owner),
      destChannels (destChannelsToFill)
{
}

WaveInputDeviceNode::~WaveInputDeviceNode()
{
    instance.removeConsumer (this);
}

tracktion::graph::NodeProperties WaveInputDeviceNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = destChannels.size();

    return props;
}

void WaveInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    auto numIncommingChannels = (waveInputDevice.isStereoPair()) ? 2u : 1u;
    audioFifo.setSize (numIncommingChannels, (uint32_t) info.blockSize * 8);
    lastCallbackTime = juce::Time::getMillisecondCounter();

    instance.addConsumer (this);
}

bool WaveInputDeviceNode::isReadyToProcess()
{
    return true;
}

void WaveInputDeviceNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    const auto timeNow = juce::Time::getMillisecondCounter();

    if (timeNow > lastCallbackTime + 150)
    {
        // If there's been a break in transmission, reset the fifo to keep in in sync
        audioFifo.reset();
    }

    lastCallbackTime = timeNow;

    auto& destAudio = pc.buffers.audio;
    auto numSamples = destAudio.getNumFrames();
    const auto numChannelsToRead = std::min (destAudio.getNumChannels(),
                                             audioFifo.getNumChannels());
    jassert (destAudio.getNumChannels() >= audioFifo.getNumChannels());

    if (auto numToRead = std::min ((choc::buffer::FrameCount) audioFifo.getNumReady(), numSamples))
    {
        auto destSubView = destAudio.getFirstChannels (numChannelsToRead)
                                    .getStart (numToRead);
        audioFifo.readAdding (destSubView);
    
        // Copy any additional channels from the last one
        auto lastChannelView = destSubView.getChannel (destSubView.getNumChannels() - 1);
        
        for (auto c = numChannelsToRead; c < destAudio.getNumChannels(); ++c)
            copy (destAudio.getChannel (c).getStart (numToRead), lastChannelView);
    }
}

void WaveInputDeviceNode::acceptInputBuffer (choc::buffer::ChannelArrayView<float> newBlock)
{
    jassert (audioFifo.getNumChannels() == newBlock.getNumChannels());
    audioFifo.reset();
    audioFifo.write (newBlock);
}

}} // namespace tracktion { inline namespace engine
