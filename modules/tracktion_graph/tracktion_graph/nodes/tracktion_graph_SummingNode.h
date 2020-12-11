/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_graph
{

//==============================================================================
//==============================================================================
/**
    An Node which sums together the multiple inputs adding additional latency
    to provide a coherent output.
*/
class SummingNode final : public Node
{
public:
    SummingNode() = default;
    
    SummingNode (std::vector<std::unique_ptr<Node>> inputs)
        : ownedNodes (std::move (inputs))
    {
        for (auto& ownedNode : ownedNodes)
            nodes.push_back (ownedNode.get());
    }

    SummingNode (std::vector<Node*> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    SummingNode (std::vector<std::unique_ptr<Node>> ownedInputs,
                 std::vector<Node*> referencedInputs)
        : SummingNode (std::move (ownedInputs))
    {
        nodes.insert (nodes.begin(), referencedInputs.begin(), referencedInputs.end());
    }

    //==============================================================================
    /** Adds an input to be summed in this Node. */
    void addInput (std::unique_ptr<Node> newInput)
    {
        assert (newInput != nullptr);
        nodes.push_back (newInput.get());
        ownedNodes.push_back (std::move (newInput));
    }

    /** Enables Nodes to be intermediately summed using double precision.
        This must be set before the node is initialised.
        N.B. The output will still be single-precision floats.
    */
    void setDoubleProcessingPrecision (bool shouldSumInDoublePrecision)
    {
        useDoublePrecision = shouldSumInDoublePrecision;
    }
    
    //==============================================================================
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : nodes)
        {
            auto nodeProps = node->getNodeProperties();
            props.hasAudio = props.hasAudio || nodeProps.hasAudio;
            props.hasMidi = props.hasMidi || nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
            props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
            hash_combine (props.nodeID, nodeProps.nodeID);
        }

        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        std::vector<Node*> inputNodes;
        
        for (auto& node : nodes)
            inputNodes.push_back (node);

        return inputNodes;
    }
    
    bool transform (Node&) override
    {
        const bool hasFlattened = flattenSummingNodes();
        const bool hasCreatedLatency = createLatencyNodes();
        
        return hasFlattened || hasCreatedLatency;
    }

    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        useDoublePrecision = useDoublePrecision && nodes.size() > 1;
        
        if (useDoublePrecision)
            tempDoubleBuffer.resize ({ (choc::buffer::ChannelCount) getNodeProperties().numberOfChannels,
                                       (choc::buffer::FrameCount) info.blockSize });
    }

    bool isReadyToProcess() override
    {
        for (auto& node : nodes)
            if (! node->hasProcessed())
                return false;
        
        return true;
    }
    
    void process (ProcessContext& pc) override
    {
        if (useDoublePrecision)
            processDoublePrecision (pc);
        else
            processSinglePrecision (pc);
    }

private:
    //==============================================================================
    std::vector<std::unique_ptr<Node>> ownedNodes;
    std::vector<Node*> nodes;
    
    bool useDoublePrecision = false;
    choc::buffer::ChannelArrayBuffer<double> tempDoubleBuffer;
    
    //==============================================================================
    template<typename DestType, typename SourceType>
    static void addBlock (choc::buffer::ChannelArrayView<DestType>& dest,
                          const choc::buffer::ChannelArrayView<SourceType>& src,
                          choc::buffer::ChannelCount numChannels)
    {
        assert (dest.getNumChannels() <= numChannels);
        assert (src.getNumChannels() <= numChannels);
        assert (dest.getNumFrames() == src.getNumFrames());
        const auto numSamples = dest.getNumFrames();

        for (choc::buffer::ChannelCount c = 0; c < numChannels; ++c)
        {
            auto destIterator = dest.getIterator (c);
            auto srcIterator = src.getIterator (c);
            
            for (choc::buffer::FrameCount i = 0; i < numSamples; ++i)
                *destIterator++ += static_cast<DestType> (*srcIterator++);
        }
    }
    
    void processSinglePrecision (const ProcessContext& pc)
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            const auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels);

            if (numChannelsToAdd > 0)
                choc::buffer::add (pc.buffers.audio.getChannelRange ({ 0, numChannelsToAdd }),
                                   node->getProcessedOutput().audio.getChannelRange ({ 0, numChannelsToAdd }));
            
            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }
    }
    
    void processDoublePrecision (const ProcessContext& pc)
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();
        tempDoubleBuffer.clear();
        assert (tempDoubleBuffer.getNumChannels() == (choc::buffer::ChannelCount) numChannels);
        auto doubleBlock = tempDoubleBuffer.getView().getStart (pc.buffers.audio.getNumFrames());

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            const auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels);

            if (numChannelsToAdd > 0)
            {
                auto destBlock = doubleBlock.getChannelRange ({ 0, numChannelsToAdd });
                auto srcBlock = node->getProcessedOutput().audio.getChannelRange ({ 0, numChannelsToAdd });
                addBlock (destBlock, srcBlock, numChannelsToAdd);
            }
            
            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }
        
        if (numChannels > 0)
        {
            auto floatBlock = pc.buffers.audio.getChannelRange ({ 0, numChannels });
            addBlock (floatBlock, doubleBlock, numChannels);
        }
    }

    //==============================================================================
    bool flattenSummingNodes()
    {
        bool hasChanged = false;
        
        std::vector<std::unique_ptr<Node>> ownedNodesToAdd;
        std::vector<Node*> nodesToAdd, nodesToErase;

        for (auto& n : ownedNodes)
        {
            if (auto summingNode = dynamic_cast<SummingNode*> (n.get()))
            {
                for (auto& ownedNodeToAdd : summingNode->ownedNodes)
                    ownedNodesToAdd.push_back (std::move (ownedNodeToAdd));

                for (auto& nodeToAdd : summingNode->nodes)
                    nodesToAdd.push_back (std::move (nodeToAdd));

                nodesToErase.push_back (n.get());
                hasChanged = true;
            }
        }
        
        for (auto& n : ownedNodesToAdd)
            ownedNodes.push_back (std::move (n));

        for (auto& n : nodesToAdd)
            nodes.push_back (std::move (n));

        ownedNodes.erase (std::remove_if (ownedNodes.begin(), ownedNodes.end(),
                                          [&] (auto& n) { return std::find (nodesToErase.begin(), nodesToErase.end(), n.get()) != nodesToErase.end(); }),
                          ownedNodes.end());

        nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                     [&] (auto& n) { return std::find (nodesToErase.begin(), nodesToErase.end(), n) != nodesToErase.end(); }),
                     nodes.end());

        return hasChanged;
    }
    
    bool createLatencyNodes()
    {
        bool topologyChanged = false;
        const int maxLatency = getNodeProperties().latencyNumSamples;
        std::vector<std::unique_ptr<Node>> ownedNodesToAdd;

        for (auto& node : nodes)
        {
            const int nodeLatency = node->getNodeProperties().latencyNumSamples;
            const int latencyToAdd = maxLatency - nodeLatency;
            
            if (latencyToAdd == 0)
                continue;
            
            auto getOwnedNode = [this] (auto nodeToFind)
            {
                for (auto& ownedNode : ownedNodes)
                {
                    if (ownedNode.get() == nodeToFind)
                    {
                        auto nodeToReturn = std::move (ownedNode);
                        ownedNode.reset();
                        return nodeToReturn;
                    }
                }
                
                return std::unique_ptr<Node>();
            };
            
            auto ownedNode = getOwnedNode (node);
            auto latencyNode = ownedNode != nullptr ? makeNode<LatencyNode> (std::move (ownedNode), latencyToAdd)
                                                    : makeNode<LatencyNode> (node, latencyToAdd);
            ownedNodesToAdd.push_back (std::move (latencyNode));
            node = nullptr;
            topologyChanged = true;
        }
        
        // Take ownership of any new nodes and also ensure they're reference in the raw array
        for (auto& newNode : ownedNodesToAdd)
        {
            nodes.push_back (newNode.get());
            ownedNodes.push_back (std::move (newNode));
        }

        nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                     [] (auto& n) { return n == nullptr; }),
                     nodes.end());

        ownedNodes.erase (std::remove_if (ownedNodes.begin(), ownedNodes.end(),
                                          [] (auto& n) { return n == nullptr; }),
                          ownedNodes.end());
        
        return topologyChanged;
    }
};


/** Creates a SummingNode from a number of Nodes. */
static inline std::unique_ptr<SummingNode> makeSummingNode (std::initializer_list<Node*> nodes)
{
    std::vector<std::unique_ptr<Node>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<Node> (node));
        
    return std::make_unique<SummingNode> (std::move (nodeVector));
}

}
