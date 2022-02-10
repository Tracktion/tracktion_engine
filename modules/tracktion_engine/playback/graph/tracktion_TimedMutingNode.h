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
    A Node that mutes its input at specific time ranges.
*/
class TimedMutingNode final : public tracktion_graph::Node
{
public:
    TimedMutingNode (std::unique_ptr<tracktion_graph::Node>,
                     juce::Array<TimeRange> muteTimes,
                     tracktion_graph::PlayHeadState&);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<tracktion_graph::Node> input;
    tracktion_graph::PlayHeadState& playHeadState;
    juce::Array<TimeRange> muteTimes;
    double sampleRate = 44100.0;

    //==============================================================================
    void processSection (choc::buffer::ChannelArrayView<float>, TimeRange);
    void muteSection (choc::buffer::ChannelArrayView<float>, int64_t startSample, int64_t numSamples);
};

} // namespace tracktion_engine
