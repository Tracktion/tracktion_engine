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

/**
    A Node that takes audio from the output of a track and sends it to its corresponding InputDevice.
*/
class TrackWaveInputDeviceNode final    : public tracktion::graph::Node,
                                          private TracktionEngineNode
{
public:
    TrackWaveInputDeviceNode (ProcessState&, WaveInputDevice&, std::unique_ptr<Node>, bool copyInputsToOutputs);

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
    TimeDuration offset;
    const bool copyInputsToOutputs = false;
};

}} // namespace tracktion { inline namespace engine
