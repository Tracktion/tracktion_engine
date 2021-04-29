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

//==============================================================================
//==============================================================================
/**
    A Node that injects the MIDI buffer of its input to the MidiOutputDevice.
*/
class MidiOutputDeviceInstanceInjectingNode final   : public tracktion_graph::Node
{
public:
    MidiOutputDeviceInstanceInjectingNode (MidiOutputDeviceInstance&,
                                           std::unique_ptr<tracktion_graph::Node>,
                                           tracktion_graph::PlayHead&);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    MidiOutputDeviceInstance& deviceInstance;
    std::unique_ptr<tracktion_graph::Node> input;
    tracktion_graph::PlayHead& playHead;
    double sampleRate = 44100.0;
};

} // namespace tracktion_engine
