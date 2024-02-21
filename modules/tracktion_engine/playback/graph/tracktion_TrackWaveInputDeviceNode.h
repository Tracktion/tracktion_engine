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
    A Node that takes audio from the output of a track and sends it to its corresponding InputDevice.
*/
class TrackWaveInputDeviceNode final    : public tracktion::graph::Node
{
public:
    TrackWaveInputDeviceNode (WaveInputDevice&, std::unique_ptr<Node>, bool copyInputsToOutputs);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
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

}} // namespace tracktion { inline namespace engine
