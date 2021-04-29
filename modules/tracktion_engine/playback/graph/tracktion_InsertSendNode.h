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
    This is a bit confusing but is required to ensure the following set of
    dependencies in the graph:
        InsertSendNode   -> InsertPluginNode
        InsertReturnNode ->
 
    Due to the way the graph is built, the InsertSendReturnDependencyNode feeds
    in to the InsertPlugin's PluginNode and creates a dependency on the
    InsertSendNode and InsertReturnNode to ensure they are processed first.
*/
class InsertSendReturnDependencyNode final  : public tracktion_graph::Node
{
public:
    InsertSendReturnDependencyNode (std::unique_ptr<Node>, InsertPlugin&);

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    bool transform (Node&) override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::unique_ptr<Node> input;
    Node* sendNode = nullptr, *returnNode = nullptr;
    Plugin::Ptr plugin;
};


//==============================================================================
//==============================================================================
/**
    The send node picks up audio/MIDI data from the InsertPlugin and then its output will be
    sent to the corresponding OutputDevice..
*/
class InsertSendNode final  : public tracktion_graph::Node
{
public:
    InsertSendNode (InsertPlugin&);

    InsertPlugin& getInsert() const     { return owner; }
    
    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    InsertPlugin& owner;
    Plugin::Ptr plugin;
};

} // namespace tracktion_engine
