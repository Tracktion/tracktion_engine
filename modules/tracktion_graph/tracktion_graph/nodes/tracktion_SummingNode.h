/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
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

        assert (std::find (nodes.begin(), nodes.end(), nullptr) == nodes.end());
    }

    SummingNode (std::vector<Node*> inputs)
        : nodes (std::move (inputs))
    {
        assert (std::find (nodes.begin(), nodes.end(), nullptr) == nodes.end());
    }
    
    SummingNode (std::vector<std::unique_ptr<Node>> ownedInputs,
                 std::vector<Node*> referencedInputs)
        : SummingNode (std::move (ownedInputs))
    {
        nodes.insert (nodes.begin(), referencedInputs.begin(), referencedInputs.end());
        assert (std::find (nodes.begin(), nodes.end(), nullptr) == nodes.end());
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
    
    static void sortByTimestampUnstable (tracktion_engine::MidiMessageArray& messages) noexcept
    {
        std::sort (messages.begin(), messages.end(), [] (const juce::MidiMessage& a, const juce::MidiMessage& b)
        {
            auto t1 = a.getTimeStamp();
            auto t2 = b.getTimeStamp();

            if (t1 == t2)
            {
                if (a.isNoteOff() && b.isNoteOn()) return true;
                if (a.isNoteOn() && b.isNoteOff()) return false;
            }
            return t1 < t2;
        });
    }

    //==============================================================================
    void processSinglePrecision (const ProcessContext& pc)
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();

        int nodesWithMidi = pc.buffers.midi.isEmpty() ? 0 : 1;

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            if (auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels))
                add (pc.buffers.audio.getFirstChannels (numChannelsToAdd),
                     inputFromNode.audio.getFirstChannels (numChannelsToAdd));

            if (inputFromNode.midi.isNotEmpty())
                nodesWithMidi++;

            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }

        if (nodesWithMidi > 1)
            sortByTimestampUnstable (pc.buffers.midi);
    }

    void processDoublePrecision (const ProcessContext& pc)
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();
        auto doubleView = tempDoubleBuffer.getView().getStart (pc.buffers.audio.getNumFrames());
        doubleView.clear();

        int nodesWithMidi = pc.buffers.midi.isEmpty() ? 0 : 1;

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            if (auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels))
                add (doubleView.getFirstChannels (numChannelsToAdd),
                     inputFromNode.audio.getFirstChannels (numChannelsToAdd));

            if (inputFromNode.midi.isNotEmpty())
                nodesWithMidi++;

            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }

        assert (doubleView.getNumChannels() == (choc::buffer::ChannelCount) numChannels);

        if (numChannels != 0)
            add (pc.buffers.audio.getFirstChannels (numChannels), doubleView);

        if (nodesWithMidi > 1)
            sortByTimestampUnstable (pc.buffers.midi);
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
                for (auto& ownedN : ownedNodes)
                {
                    if (ownedN.get() == nodeToFind)
                    {
                        auto nodeToReturn = std::move (ownedN);
                        ownedN.reset();
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

}}
