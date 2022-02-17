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
