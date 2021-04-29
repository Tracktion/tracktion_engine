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
    Applies a SharedLevelMeter to the audio passing through this node.
*/
class SharedLevelMeasuringNode final    : public tracktion_graph::Node
{
public:
    SharedLevelMeasuringNode (SharedLevelMeasurer::Ptr, std::unique_ptr<Node>);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    SharedLevelMeasurer::Ptr levelMeasurer;
    std::unique_ptr<Node> input;
    double sampleRate = 44100.0;
};

} // namespace tracktion_engine
