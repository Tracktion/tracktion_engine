/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

/**
    A Node that intercepts incoming live audio and inserts it in to the playback graph.
*/
class WaveInputDeviceNode final : public tracktion::graph::Node,
                                  public InputDeviceInstance::Consumer
{
public:
    WaveInputDeviceNode (InputDeviceInstance&, WaveInputDevice&,
                         const juce::AudioChannelSet& destChannelsToFill);
    ~WaveInputDeviceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

    void acceptInputBuffer (choc::buffer::ChannelArrayView<float>) override;

private:
    //==============================================================================
    InputDeviceInstance& instance;
    WaveInputDevice& waveInputDevice;
    uint32_t lastCallbackTime = 0;
    tracktion::graph::AudioFifo audioFifo { 1, 32 };
    const juce::AudioChannelSet destChannels;
};

}} // namespace tracktion { inline namespace engine
