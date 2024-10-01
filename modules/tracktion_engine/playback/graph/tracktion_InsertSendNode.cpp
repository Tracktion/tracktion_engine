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

InsertNode::InsertNode (std::unique_ptr<Node> input_, InsertPlugin& ip, std::unique_ptr<Node> returnNode_, SampleRateAndBlockSize info)
    : owner (ip), plugin (ip),
      input (std::move (input_)),
      returnNode (std::move (returnNode_))
{
    assert (input);
    assert (returnNode);

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::no });

    plugin->baseClassInitialise ({ TimePosition(), info.sampleRate, info.blockSize });
    isInitialised = true;
}

InsertNode::~InsertNode()
{
    if (isInitialised && ! plugin->baseClassNeedsInitialising())
        plugin->baseClassDeinitialise();
}

TransformResult InsertNode::transform (Node&, const std::vector<Node*>& postOrderedNodes, TransformCache&)
{
    if (sendNode)
        return TransformResult::none;

    for (auto n : postOrderedNodes)
    {
        if (auto in = dynamic_cast<InsertSendNode*> (n))
        {
            if (&in->getInsert() == plugin.get())
            {
                sendNode = in;
                return TransformResult::connectionsMade;
            }
        }
    }

    return TransformResult::none;
}

tracktion::graph::NodeProperties InsertNode::getNodeProperties()
{
    auto props = returnNode->getNodeProperties();
    props.latencyNumSamples += owner.getLatencyNumSamples();

    if (sendNode)
        props.latencyNumSamples += sendNode->getLatencyAtInput();

    if (props.nodeID != 0)
        hash_combine (props.nodeID, static_cast<size_t> (5738295899482615961ul)); // "InsertNode"

    return props;
}

std::vector<Node*> InsertNode::getDirectInputNodes()
{
    return { returnNode.get() };
}

void InsertNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
{
}

bool InsertNode::isReadyToProcess()
{
    return returnNode->hasProcessed();
}

void InsertNode::process (ProcessContext& pc)
{
    auto buffers = returnNode->getProcessedOutput();
    setAudioOutput (returnNode.get(), buffers.audio.getStart (pc.buffers.audio.getNumFrames()));
    pc.buffers.midi.copyFrom (buffers.midi);
}


//==============================================================================
//==============================================================================
InsertSendNode::InsertSendNode (InsertPlugin& ip)
    : owner (ip), plugin (ip)
{
}

int InsertSendNode::getLatencyAtInput()
{
    return input ? input->getNodeProperties().latencyNumSamples
                 : 0;
}

//==============================================================================
tracktion::graph::NodeProperties InsertSendNode::getNodeProperties()
{
    if (input)
    {
        auto props = input->getNodeProperties();
        props.latencyNumSamples = std::numeric_limits<int>::min();

        if (props.nodeID != 0)
            hash_combine (props.nodeID, static_cast<size_t> (18118259460788248666ul)); // "InsertSendNode"

        return props;
    }

    tracktion::graph::NodeProperties props;
    props.hasAudio = owner.hasAudio();
    props.hasMidi = owner.hasMidi();
    props.numberOfChannels = props.hasAudio ? 2 : 0;
    props.latencyNumSamples = owner.getLatencyNumSamples();

    return props;
}

std::vector<tracktion::graph::Node*> InsertSendNode::getDirectInputNodes()
{
    if (input)
        return { input };

    return {};
}

TransformResult InsertSendNode::transform (Node&, const std::vector<Node*>& postOrderedNodes, TransformCache&)
{
    if (input)
        return TransformResult::none;

    for (auto n : postOrderedNodes)
    {
        if (auto in = dynamic_cast<InsertNode*> (n))
        {
            if (&in->getInsert() == plugin.get())
            {
                input = &in->getInputNode();
                return TransformResult::connectionsMade;
            }
        }
    }

    return TransformResult::none;
}

void InsertSendNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
{
    if (input)
        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::no });
}

bool InsertSendNode::isReadyToProcess()
{
    return input == nullptr || input->hasProcessed();
}

void InsertSendNode::process (ProcessContext& pc)
{
    if (! input)
        return;

    auto buffers = input->getProcessedOutput();
    setAudioOutput (input, buffers.audio.getStart (pc.buffers.audio.getNumFrames()));
    pc.buffers.midi.copyFrom (buffers.midi);
}

}} // namespace tracktion { inline namespace engine
