/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
    props.latencyNumSamples = owner.getLatencyNumSamples();

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
