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
    A Node that passes its incoming audio and MIDI through a LevelMeasurer.
*/
class LevelMeasuringNode final  : public tracktion::graph::Node
{
public:
    LevelMeasuringNode (std::unique_ptr<tracktion::graph::Node> inputNode, LevelMeasurer&);

    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override  { return { input.get() }; }
    bool isReadyToProcess() override                                    { return input->hasProcessed(); }
    void process (tracktion::graph::Node::ProcessContext&) override;

private:
    std::unique_ptr<tracktion::graph::Node> input;
    LevelMeasurer& levelMeasurer;
};

}} // namespace tracktion { inline namespace engine
