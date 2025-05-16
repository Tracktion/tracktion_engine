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
    Sends an input Node to a Rack bus handling the channel mapping and channel gain levels.
*/
class RackInstanceNode final    : public tracktion::graph::Node,
                                  public TracktionEngineNode
{
public:
    using ChannelMap = std::array<std::tuple<int, int, AutomatableParameter::Ptr>, 2>;

    /** Creates a RackInstanceNode that maps an input node channel to an output channel
        and applies a gain parameter to each mapped channel.
    */
    RackInstanceNode (RackInstance::Ptr, std::unique_ptr<Node>, ChannelMap channelMap,
                      ProcessState&, SampleRateAndBlockSize);
    ~RackInstanceNode() override;

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t>) override;
    void preProcess (choc::buffer::FrameCount, juce::Range<int64_t>) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    RackInstance::Ptr plugin;
    std::unique_ptr<Node> input;
    ChannelMap channelMap;
    TimeDuration automationAdjustmentTime;
    int maxNumChannels = 0;
    float lastGain[2];
    bool canUseSourceBuffers = false, isInitialised = false;
};

}} // namespace tracktion { inline namespace engine
