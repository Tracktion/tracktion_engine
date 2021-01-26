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
InsertSendReturnDependencyNode::InsertSendReturnDependencyNode (std::unique_ptr<Node> inputNode, InsertPlugin& ip)
    : input (std::move (inputNode)), plugin (ip)
{
    setOptimisations ({ tracktion_graph::ClearBuffers::no,
                        tracktion_graph::AllocateAudioBuffer::no });
}

//==============================================================================
tracktion_graph::NodeProperties InsertSendReturnDependencyNode::getNodeProperties()
{
    return input->getNodeProperties();
}

std::vector<tracktion_graph::Node*> InsertSendReturnDependencyNode::getDirectInputNodes()
{
    std::vector<tracktion_graph::Node*> inputs { input.get() };
    
    if (sendNode)
        inputs.push_back (sendNode);
    
    if (returnNode)
        inputs.push_back (returnNode);

    return inputs;
}

bool InsertSendReturnDependencyNode::transform (Node& rootNode)
{
    if (sendNode && returnNode)
        return false;
    
    bool foundSend = false, foundReturn = false;
    
    for (auto n : getNodes (rootNode, tracktion_graph::VertexOrdering::postordering))
    {
        if (! sendNode)
        {
            if (auto isn = dynamic_cast<InsertSendNode*> (n))
            {
                if (&isn->getInsert() == plugin.get())
                {
                    sendNode = isn;
                    foundSend = true;
                }
            }
        }

        if (! returnNode)
        {
            if (auto irn = dynamic_cast<InsertReturnNode*> (n))
            {
               if (&irn->getInsert() == plugin.get())
               {
                   returnNode = irn;
                   foundReturn = true;
               }
            }
        }
    }
    
    return foundSend || foundReturn;
}

void InsertSendReturnDependencyNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool InsertSendReturnDependencyNode::isReadyToProcess()
{
    return input->hasProcessed()
        && (sendNode == nullptr || sendNode->hasProcessed())
        && (returnNode == nullptr || returnNode->hasProcessed());
}

void InsertSendReturnDependencyNode::process (ProcessContext& pc)
{
    auto inputBuffers = input->getProcessedOutput();
    setAudioOutput (inputBuffers.audio.getStart (pc.buffers.audio.getNumFrames()));
    pc.buffers.midi.copyFrom (inputBuffers.midi);
}


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
