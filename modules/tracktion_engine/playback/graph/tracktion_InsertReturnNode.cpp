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
InsertReturnNode::InsertReturnNode (InsertPlugin& ip, std::unique_ptr<tracktion_graph::Node> inputNode)
    : owner (ip), plugin (&ip), input (std::move (inputNode))
{
}

//==============================================================================
tracktion_graph::NodeProperties InsertReturnNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion_graph::Node*> InsertReturnNode::getDirectInputNodes()
{
    return { input.get() };
}

void InsertReturnNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool InsertReturnNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void InsertReturnNode::process (ProcessContext&)
{
    auto sourceBuffers = input->getProcessedOutput();
    owner.fillReturnBuffer (&sourceBuffers.audio, &sourceBuffers.midi);

}

} // namespace tracktion_engine
