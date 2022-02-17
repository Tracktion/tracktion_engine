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

/**
    Sends an input Node to a Rack bus handling the channel mapping and channel gain levels.
*/
class RackInstanceNode final    : public tracktion::graph::Node
{
public:
    using ChannelMap = std::array<std::tuple<int, int, AutomatableParameter::Ptr>, 2>;

    /** Creates a RackInstanceNode that maps an input node channel to an output channel
        and applies a gain parameter to each mapped channel.
    */
    RackInstanceNode (std::unique_ptr<Node>, ChannelMap channelMap);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<Node> input;
    ChannelMap channelMap;
    int maxNumChannels = 0;
    float lastGain[2];
};

}} // namespace tracktion { inline namespace engine
