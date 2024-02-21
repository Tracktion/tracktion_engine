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

//==============================================================================
//==============================================================================
/**
    A Node that injects the MIDI buffer of its input to the MidiOutputDevice.
*/
class MidiOutputDeviceInstanceInjectingNode final   : public tracktion::graph::Node
{
public:
    MidiOutputDeviceInstanceInjectingNode (MidiOutputDeviceInstance&,
                                           std::unique_ptr<tracktion::graph::Node>,
                                           tracktion::graph::PlayHead&);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    MidiOutputDeviceInstance& deviceInstance;
    std::unique_ptr<tracktion::graph::Node> input;
    tracktion::graph::PlayHead& playHead;
    double sampleRate = 44100.0;
};

}} // namespace tracktion { inline namespace engine
