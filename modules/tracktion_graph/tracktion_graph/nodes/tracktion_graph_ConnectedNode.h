/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_graph_LatencyNode.h"


namespace tracktion_graph
{

//==============================================================================
//==============================================================================
/*
    Represents a connection between a source and destination channel.
 */
struct ChannelConnection
{
    int sourceChannel = -1;     /**< The source channel. */
    int destChannel = -1;       /**< The destination channel. */
    
    bool operator== (const ChannelConnection& o) const noexcept
    {
        return sourceChannel == o.sourceChannel
            && destChannel == o.destChannel;
    }
};


//==============================================================================
//==============================================================================
/**
    An Node which sums together the multiple inputs adding additional latency
    to provide a coherent output. This is similar to a SummingNode except you can
    specify the channels to include.
*/
class ConnectedNode : public Node
{
public:
    /** Creates a ConnectedNode with no connections. */
    ConnectedNode() = default;
    
    //==============================================================================
    /** Adds an audio connection. */
    void addAudioConnection (std::shared_ptr<Node>, ChannelConnection);

    /** Adds a MIDI connection. */
    void addMidiConnection (std::shared_ptr<Node>);

    //==============================================================================
    NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    bool transform (Node&) override;
    bool isReadyToProcess() override;
    void prepareToPlay (const PlaybackInitialisationInfo&) override;
    void process (const ProcessContext&) override;
    
private:
    struct NodeConnection
    {
        std::shared_ptr<Node> node;
        bool connectMidi = false;
        std::vector<ChannelConnection> connectedChannels;
    };
    
    static int getMaxDestChannel (const NodeConnection& connection)
    {
        int maxChannel = -1;
        
        for (const auto& channelConnection : connection.connectedChannels)
            maxChannel = std::max (maxChannel, channelConnection.destChannel);
        
        return maxChannel;
    }
    
    std::vector<NodeConnection> connections;
    std::vector<std::shared_ptr<Node>> latencyNodeInputs;

    bool createLatencyNodes();
};


//==============================================================================
//==============================================================================
inline void ConnectedNode::addAudioConnection (std::shared_ptr<Node> input, ChannelConnection newConnection)
{
    // Check for existing connections first
    for (auto& connection : connections)
    {
        if (connection.node.get() == input.get())
        {
            for (const auto& channelConnection : connection.connectedChannels)
                if (newConnection == channelConnection)
                    return;
            
            connection.connectedChannels.push_back (newConnection);
            return;
        }
    }

    // Otherwise add a new connection
    connections.push_back ({ std::move (input), false, { newConnection } });
}

inline void ConnectedNode::addMidiConnection (std::shared_ptr<Node> input)
{
    // Check for existing MIDI connections first
    for (auto& connection : connections)
    {
        if (connection.node.get() == input.get())
        {
            connection.connectMidi = true;
            return;
        }
    }

    // Otherwise add a new connection
    connections.push_back ({ std::move (input), true, {} });
}

inline NodeProperties ConnectedNode::getNodeProperties()
{
    NodeProperties props;
    props.hasAudio = false;
    props.hasMidi = false;
    props.numberOfChannels = 0;

    for (const auto& connection : connections)
    {
        auto nodeProps = connection.node->getNodeProperties();
        props.hasAudio = props.hasAudio || ! connection.connectedChannels.empty();
        props.hasMidi = props.hasMidi || connection.connectMidi;
        props.numberOfChannels = std::max (props.numberOfChannels, getMaxDestChannel (connection));
        props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        
        // Hash inputs
        hash_combine (props.nodeID, nodeProps.nodeID);
        hash_combine (props.nodeID, connection.connectMidi);

        for (const auto& channelConnection : connection.connectedChannels)
        {
            hash_combine (props.nodeID, channelConnection.sourceChannel);
            hash_combine (props.nodeID, channelConnection.destChannel);
        }
    }

    return props;
}

inline std::vector<Node*> ConnectedNode::getDirectInputNodes()
{
    std::vector<Node*> inputs;
    
    for (const auto& connection : connections)
        inputs.push_back (connection.node.get());

    return inputs;
}

inline bool ConnectedNode::transform (Node&)
{
    return createLatencyNodes();
}

inline bool ConnectedNode::isReadyToProcess()
{
    for (const auto& connection : connections)
        if (! connection.node->hasProcessed())
            return false;

    return true;
}

inline void ConnectedNode::prepareToPlay (const PlaybackInitialisationInfo&)
{
}

inline void ConnectedNode::process (const ProcessContext& pc)
{
    auto destAudio = pc.buffers.audio;
    auto& destMIDI = pc.buffers.midi;

    const int numSamples = (int) pc.referenceSampleRange.getLength();
    jassert (destAudio.getNumChannels() == 0 || numSamples == (int) destAudio.getNumSamples());

    for (const auto& connection : connections)
    {
        auto sourceOutput = connection.node->getProcessedOutput();
        auto sourceAudio = sourceOutput.audio;
        auto& sourceMidi = sourceOutput.midi;
        jassert (destAudio.getNumChannels() == 0 || numSamples == (int) sourceAudio.getNumSamples());
        
        if (connection.connectMidi)
            destMIDI.mergeFrom (sourceMidi);

        for (const auto& channelConnection : connection.connectedChannels)
        {
            auto sourceChan = channelConnection.sourceChannel;
            auto destChan = channelConnection.sourceChannel;
            
            if (sourceChan <= 0 || destChan <= 0)
            {
                jassertfalse;
                continue;
            }
            
            destAudio.getSingleChannelBlock ((size_t) sourceChan).copyFrom (sourceAudio.getSingleChannelBlock ((size_t) destChan));
        }
    }
}

inline bool ConnectedNode::createLatencyNodes()
{
    bool topologyChanged = false;
    const int maxLatency = getNodeProperties().latencyNumSamples;
    std::vector<std::shared_ptr<Node>> nodes;
    
    for (auto& connection : connections)
    {
        auto& node = connection.node;
        const int nodeLatency = node->getNodeProperties().latencyNumSamples;
        const int latencyToAdd = maxLatency - nodeLatency;
        
        if (latencyToAdd == 0)
            continue;
        
        // We should be the only thing owning this Node or we can't release it!
        jassert (node.use_count() == 1);
        
        // We shouldn't be stacking LatencyNodes, rather we should modify their latency
        jassert (dynamic_cast<LatencyNode*> (node.get()) == nullptr);
        latencyNodeInputs.push_back (node);
        node = std::make_shared<LatencyNode> (node.get(), latencyToAdd);

        topologyChanged = true;
    }
    
    return topologyChanged;
}

}
