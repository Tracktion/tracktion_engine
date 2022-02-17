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
InsertSendReturnDependencyNode::InsertSendReturnDependencyNode (std::unique_ptr<Node> inputNode, InsertPlugin& ip)
    : input (std::move (inputNode)), plugin (ip)
{
    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::no });
}

//==============================================================================
tracktion::graph::NodeProperties InsertSendReturnDependencyNode::getNodeProperties()
{
    return input->getNodeProperties();
}

std::vector<tracktion::graph::Node*> InsertSendReturnDependencyNode::getDirectInputNodes()
{
    std::vector<tracktion::graph::Node*> inputs { input.get() };
    
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
    
    for (auto n : getNodes (rootNode, tracktion::graph::VertexOrdering::postordering))
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

void InsertSendReturnDependencyNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
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
    setAudioOutput (input.get(), inputBuffers.audio.getStart (pc.buffers.audio.getNumFrames()));
    pc.buffers.midi.copyFrom (inputBuffers.midi);
}


//==============================================================================
//==============================================================================
InsertSendNode::InsertSendNode (InsertPlugin& ip)
    : owner (ip), plugin (ip)
{
}

//==============================================================================
tracktion::graph::NodeProperties InsertSendNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = owner.hasAudio();
    props.hasMidi = owner.hasMidi();
    props.numberOfChannels = props.hasAudio ? 2 : 0;
    
    return props;
}

std::vector<tracktion::graph::Node*> InsertSendNode::getDirectInputNodes()
{
    return {};
}

void InsertSendNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
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

}} // namespace tracktion { inline namespace engine
