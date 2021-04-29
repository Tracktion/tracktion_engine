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
    A Node that passes its incomming audio and MIDI through a LevelMeasurer.
*/
class LevelMeasuringNode final  : public tracktion_graph::Node
{
public:
    LevelMeasuringNode (std::unique_ptr<tracktion_graph::Node> inputNode, LevelMeasurer&);
    
    tracktion_graph::NodeProperties getNodeProperties() override        { return input->getNodeProperties(); }
    std::vector<tracktion_graph::Node*> getDirectInputNodes() override  { return { input.get() }; }
    bool isReadyToProcess() override                                    { return input->hasProcessed(); }
    void process (tracktion_graph::Node::ProcessContext&) override;
    
private:
    std::unique_ptr<tracktion_graph::Node> input;
    LevelMeasurer& levelMeasurer;
};

} // namespace tracktion_engine
