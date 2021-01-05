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
    Sends an input Node to a Rack bus handling the channel mapping and channel gain levels.
*/
class RackReturnNode final  : public tracktion_graph::Node
{
public:
    /** Creates a RackInstanceNode that maps an input node channel to an output channel
        and applies a gain parameter to each mapped channel.
    */
    RackReturnNode (std::unique_ptr<Node> wetNode,
                    std::function<float()> wetGainFunc,
                    Node* dryNode,
                    std::function<float()> dryGainFunc);

    std::vector<Node*> getDirectInputNodes() override;
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<Node> wetInput;
    Node* dryInput = nullptr;
    std::function<float()> wetGainFunction, dryGainFunction;
    float lastWetGain = 0.0f, lastDryGain = 0.0f;
};

} // namespace tracktion_engine
