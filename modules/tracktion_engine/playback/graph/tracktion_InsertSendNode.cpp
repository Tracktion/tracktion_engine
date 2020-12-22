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
InsertSendNode::InsertSendNode (InsertPlugin& ip)
    : owner (ip), plugin (ip)
{
}

//==============================================================================
tracktion_graph::NodeProperties InsertSendNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = owner.hasAudio();
    props.hasMidi = owner.hasMidi();
    props.numberOfChannels = props.hasAudio ? 2 : 0;
    
    return props;
}

std::vector<tracktion_graph::Node*> InsertSendNode::getDirectInputNodes()
{
    return {};
}

void InsertSendNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool InsertSendNode::isReadyToProcess()
{
    return true;
}

void InsertSendNode::process (ProcessContext& pc)
{
    owner.fillSendBuffer (&pc.buffers.audio, &pc.buffers.midi);
}

} // namespace tracktion_engine
