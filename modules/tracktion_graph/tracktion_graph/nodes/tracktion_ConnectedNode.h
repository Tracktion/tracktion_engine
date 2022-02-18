/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_LatencyNode.h"


namespace tracktion { inline namespace graph
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

    /** Creates a ConnectedNode with no connections but a base nodeID. */
    ConnectedNode (size_t nodeID);

    //==============================================================================
    /** Adds an audio connection.
        Returns false if the Node was unable to be added due to a cycle or if there
        was already a connection between these Nodes.
    */
    bool addAudioConnection (std::shared_ptr<Node>, ChannelConnection);

    /** Adds a MIDI connection.
        Returns false if the Node was unable to be added due to a cycle or if there
        was already a connection between these Nodes.
    */
    bool addMidiConnection (std::shared_ptr<Node>);

    //==============================================================================
    NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    bool transform (Node&) override;
    bool isReadyToProcess() override;
    void prepareToPlay (const PlaybackInitialisationInfo&) override;
    void process (ProcessContext&) override;
    
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
    
    size_t nodeID = 0;
    std::vector<NodeConnection> connections;

    bool createLatencyNodes();
};


//==============================================================================
//==============================================================================
inline ConnectedNode::ConnectedNode (size_t nodeIDToUse)
    : nodeID (nodeIDToUse)
{
}

inline bool ConnectedNode::addAudioConnection (std::shared_ptr<Node> input, ChannelConnection newConnection)
{
    jassert (newConnection.sourceChannel >= 0);
    jassert (newConnection.destChannel >= 0);
   
    bool cycleDetected = false;
    visitNodes (*input, [this, &cycleDetected] (auto& n) { cycleDetected = cycleDetected || &n == this; }, false);
    
    if (cycleDetected)
        return false;

    // Check for existing connections first
    for (auto& connection : connections)
    {
        if (connection.node.get() == input.get())
        {
            for (const auto& channelConnection : connection.connectedChannels)
                if (newConnection == channelConnection)
                    return false;
            
            connection.connectedChannels.push_back (newConnection);
            return true;
        }
    }

    // Otherwise add a new connection
    connections.push_back ({ std::move (input), false, { newConnection } });
    
    return true;
}

inline bool ConnectedNode::addMidiConnection (std::shared_ptr<Node> input)
{
    bool cycleDetected = false;
    visitNodes (*input, [this, &cycleDetected] (auto& n) { cycleDetected = cycleDetected || &n == this; }, false);
    
    if (cycleDetected)
        return false;
    
    // Check for existing MIDI connections first
    for (auto& connection : connections)
    {
        if (connection.node.get() == input.get())
        {
            if (connection.connectMidi)
                return false;
            
            connection.connectMidi = true;
            return true;
        }
    }

    // Otherwise add a new connection
    connections.push_back ({ std::move (input), true, {} });

    return true;
}

inline NodeProperties ConnectedNode::getNodeProperties()
{
    NodeProperties props;
    props.hasAudio = false;
    props.hasMidi = false;
    props.numberOfChannels = 0;
    props.nodeID = nodeID;

    constexpr size_t connectedNodeMagicHash = 0x636f6e6e656374;
    
    if (props.nodeID != 0)
        hash_combine (props.nodeID, connectedNodeMagicHash);

    for (const auto& connection : connections)
    {
        auto nodeProps = connection.node->getNodeProperties();
        props.hasAudio = props.hasAudio || ! connection.connectedChannels.empty();
        props.hasMidi = props.hasMidi || connection.connectMidi;
        props.numberOfChannels = std::max (props.numberOfChannels, getMaxDestChannel (connection)) + 1;
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

inline void ConnectedNode::process (ProcessContext& pc)
{
    auto destAudio = pc.buffers.audio;
    auto& destMIDI = pc.buffers.midi;

    const int numSamples = (int) pc.referenceSampleRange.getLength();
    juce::ignoreUnused (numSamples);
    jassert (destAudio.getNumChannels() == 0 || numSamples == (int) destAudio.getNumFrames());

    for (const auto& connection : connections)
    {
        auto sourceOutput = connection.node->getProcessedOutput();
        auto sourceAudio = sourceOutput.audio;
        auto& sourceMidi = sourceOutput.midi;
        jassert (destAudio.getNumChannels() == 0 || numSamples == (int) sourceAudio.getNumFrames());
        
        if (connection.connectMidi)
            destMIDI.mergeFrom (sourceMidi);

        for (const auto& channelConnection : connection.connectedChannels)
        {
            auto sourceChan = channelConnection.sourceChannel;
            auto destChan = channelConnection.destChannel;
            
            if (sourceChan < 0 || destChan < 0)
            {
                jassertfalse;
                continue;
            }
            
            if (sourceChan >= (int) sourceAudio.getNumChannels())
                continue;
            
            add (destAudio.getChannel ((choc::buffer::ChannelCount) destChan),
                 sourceAudio.getChannel ((choc::buffer::ChannelCount) sourceChan));
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
        // We shouldn't be stacking LatencyNodes, rather we should modify their latency
        jassert (dynamic_cast<LatencyNode*> (node.get()) == nullptr);
        node = std::make_shared<LatencyNode> (std::move (node), latencyToAdd);

        topologyChanged = true;
    }
    
    return topologyChanged;
}

}}
