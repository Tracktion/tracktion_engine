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
InsertReturnNode::InsertReturnNode (InsertPlugin& ip, std::unique_ptr<tracktion::graph::Node> inputNode)
    : owner (ip), plugin (&ip), input (std::move (inputNode))
{
}

//==============================================================================
tracktion::graph::NodeProperties InsertReturnNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion::graph::Node*> InsertReturnNode::getDirectInputNodes()
{
    return { input.get() };
}

void InsertReturnNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
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

}} // namespace tracktion { inline namespace engine
