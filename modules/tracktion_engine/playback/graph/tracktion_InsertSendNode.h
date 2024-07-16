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

class InsertSendNode;

//==============================================================================
//==============================================================================
class InsertNode final : public tracktion::graph::Node
{
public:
    InsertNode (std::unique_ptr<Node> input, InsertPlugin&, std::unique_ptr<Node> returnNode, SampleRateAndBlockSize);
    ~InsertNode() override;

    InsertPlugin& getInsert() const     { return owner; }
    Node& getInputNode()                { return *input; }

    TransformResult transform (Node&, const std::vector<Node*>&, TransformCache&) override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    InsertPlugin& owner;
    Plugin::Ptr plugin;

    std::unique_ptr<Node> input;
    std::unique_ptr<Node> returnNode;
    InsertSendNode* sendNode = nullptr;

    bool isInitialised = false;
};


//==============================================================================
//==============================================================================
/**
    The send node picks up audio/MIDI data from the InsertPlugin and then its output will be
    sent to the corresponding OutputDevice..
*/
class InsertSendNode final  : public tracktion::graph::Node
{
public:
    InsertSendNode (InsertPlugin&);

    InsertPlugin& getInsert()           { return owner; }
    int getLatencyAtInput();

    //==============================================================================
    TransformResult transform (Node&, const std::vector<Node*>&, TransformCache&) override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    InsertPlugin& owner;
    Plugin::Ptr plugin;
    Node* input = nullptr;
};

}} // namespace tracktion { inline namespace engine
