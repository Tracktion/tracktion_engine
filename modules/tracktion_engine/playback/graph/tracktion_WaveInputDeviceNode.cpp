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

WaveInputDeviceNode::WaveInputDeviceNode (InputDeviceInstance& idi, WaveInputDevice& owner)
    : instance (idi),
      waveInputDevice (owner)
{
}

WaveInputDeviceNode::~WaveInputDeviceNode()
{
    instance.removeConsumer (this);
}

tracktion_graph::NodeProperties WaveInputDeviceNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = (waveInputDevice.isStereoPair()) ? 2 : 1;

    return props;
}

void WaveInputDeviceNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    const int numChannels = getNodeProperties().numberOfChannels;
    audioFifo.setSize (numChannels, info.blockSize * 8);
    lastCallbackTime = juce::Time::getMillisecondCounter();

    instance.addConsumer (this);
}

bool WaveInputDeviceNode::isReadyToProcess()
{
    return true;
}

void WaveInputDeviceNode::process (const ProcessContext& pc)
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
    const int numSamples = (int) destAudio.getNumSamples();
    const int numChannels = (int) destAudio.getNumChannels();
    jassert (numChannels == audioFifo.getNumChannels());

    const auto numToRead = (size_t) std::min (audioFifo.getNumReady(), numSamples);

    if (numToRead > 0)
        audioFifo.readAdding (destAudio.getSubBlock (0, numToRead));
}

void WaveInputDeviceNode::acceptInputBuffer (const juce::dsp::AudioBlock<float>& newBlock)
{
    jassert (audioFifo.getNumChannels() == (int) newBlock.getNumChannels());
    audioFifo.reset();
    audioFifo.write (newBlock);
}

}
