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
    A Node that takes audio from the output of a track and sends it to its corresponding InputDevice.
*/
class TrackWaveInputDeviceNode final    : public tracktion_graph::Node
{
public:
    TrackWaveInputDeviceNode (WaveInputDevice&, std::unique_ptr<Node>);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    WaveInputDevice& waveInputDevice;
    std::unique_ptr<Node> input;
    double sampleRate = 44100.0;
    int offsetSamples = 0;
    const bool copyInputsToOutputs = false;
};

} // namespace tracktion_engine
