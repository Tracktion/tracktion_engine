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
    A Node that passes its incomming audio and MIDI through a LevelMeasurer.
*/
class LevelMeasuringNode final  : public tracktion::graph::Node
{
public:
    LevelMeasuringNode (std::unique_ptr<tracktion::graph::Node> inputNode, LevelMeasurer&);

    tracktion::graph::NodeProperties getNodeProperties() override        { return input->getNodeProperties(); }
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override  { return { input.get() }; }
    bool isReadyToProcess() override                                    { return input->hasProcessed(); }
    void process (tracktion::graph::Node::ProcessContext&) override;

private:
    std::unique_ptr<tracktion::graph::Node> input;
    LevelMeasurer& levelMeasurer;
};

}} // namespace tracktion { inline namespace engine
