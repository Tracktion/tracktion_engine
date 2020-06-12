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

/**
    A Node that intercepts incoming live audio and inserts it in to the playback graph.
*/
class WaveInputDeviceNode final : public tracktion_graph::Node,
                                  public InputDeviceInstance::Consumer
{
public:
    WaveInputDeviceNode (InputDeviceInstance&, WaveInputDevice&);
    ~WaveInputDeviceNode() override;
    
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (const ProcessContext&) override;

    void acceptInputBuffer (const juce::dsp::AudioBlock<float>&) override;

private:
    //==============================================================================
    InputDeviceInstance& instance;
    WaveInputDevice& waveInputDevice;
    juce::uint32 lastCallbackTime = 0;
    tracktion_graph::AudioFifo audioFifo { 1, 32 };
};

} // namespace tracktion_engine
