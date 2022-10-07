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

//==============================================================================
//==============================================================================
/**
    The return node hooks into the input device and fills the insert's return
    buffer with data from the input.
*/
class InsertReturnNode final    : public tracktion::graph::Node
{
public:
    InsertReturnNode (InsertPlugin&, std::unique_ptr<tracktion::graph::Node>);

    InsertPlugin& getInsert() const     { return owner; }

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    InsertPlugin& owner;
    Plugin::Ptr plugin;
    std::unique_ptr<tracktion::graph::Node> input;
    MidiMessageArray midiScratch;
};

}} // namespace tracktion { inline namespace engine
