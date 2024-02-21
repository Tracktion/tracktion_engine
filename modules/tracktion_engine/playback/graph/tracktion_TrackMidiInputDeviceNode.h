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
    A Node that takes MIDI from the output of a track and sends it to its corresponding InputDevice.
*/
class TrackMidiInputDeviceNode final    : public tracktion::graph::Node,
                                          private TracktionEngineNode
{
public:
    TrackMidiInputDeviceNode (MidiInputDevice&, std::unique_ptr<Node>, ProcessState&,
                              bool copyInputsToOutputs);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    MidiInputDevice& midiInputDevice;
    std::unique_ptr<Node> input;
    double offsetSeconds = 0.0;
    const bool copyInputsToOutputs = false;
};

}} // namespace tracktion { inline namespace engine
