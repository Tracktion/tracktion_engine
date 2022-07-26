/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    Fills an InputProvider with output from a Node.
*/
class InputProviderFillerNode final : public tracktion::graph::Node
{
public:
    InputProviderFillerNode (std::unique_ptr<Node> inputNode,
                             std::shared_ptr<InputProvider> inputProviderToUse)
        : input (std::move (inputNode)),
          inputProvider (std::move (inputProviderToUse))
    {
        assert (input);
        assert (inputProvider);
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }
    
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        return input->getNodeProperties();
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override
    {
    }
    
    void process (ProcessContext&) override
    {
        auto inputs = input->getProcessedOutput();
        tracktion::graph::sanityCheckView (inputs.audio);
        inputProvider->setInputs (inputs);
    }
    
private:
    std::unique_ptr<Node> input;
    std::shared_ptr<InputProvider> inputProvider;
};


//==============================================================================
//==============================================================================
class InputNode final   : public tracktion::graph::Node
{
public:
    InputNode (std::shared_ptr<InputProvider> inputProviderToUse,
               int numAudioChannels, bool hasMidiInput)
        : inputProvider (std::move (inputProviderToUse)),
          numChannels (numAudioChannels),
          hasMidi (hasMidiInput)
    {
    }

    InputNode (std::shared_ptr<InputProvider> inputProviderToUse)
        : inputProvider (std::move (inputProviderToUse)),
          numChannels ((int) (inputProvider->numChannels > 0 ? inputProvider->numChannels
                                                             : inputProvider->audio.getNumChannels())),
          hasMidi (true)
    {
    }

    void setInputDependancy (Node* dependancy)
    {
        nodeDependancy = dependancy;
    }
    
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        tracktion::graph::NodeProperties props;
        props.hasAudio = numChannels > 0;
        props.hasMidi = hasMidi;
        props.numberOfChannels = numChannels;
        props.nodeID = nodeID;
        
        if (nodeDependancy)
            props.latencyNumSamples = nodeDependancy->getNodeProperties().latencyNumSamples;
        
        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        if (nodeDependancy)
            return { nodeDependancy };
        
        return {};
    }
    
    bool isReadyToProcess() override
    {
        return nodeDependancy ? nodeDependancy->hasProcessed()
                              : true;
    }
    
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override
    {
    }
    
    void process (ProcessContext& pc) override
    {
        auto inputBuffers = inputProvider->getInputs();
        tracktion::graph::sanityCheckView (inputBuffers.audio);

        if (numChannels > 0)
        {
            auto& outputBuffers = pc.buffers;
            
            // The InputProvider may have less channels than this Node requires so only take the number available
            auto numInputChannelsToCopy = std::min (inputBuffers.audio.getNumChannels(),
                                                    outputBuffers.audio.getNumChannels());
            
            if (numInputChannelsToCopy > 0)
            {
                // For testing purposes, the last block might be smaller than the InputProvider
                // so we'll just take the number of samples required
                jassert (inputBuffers.audio.getNumFrames() >= outputBuffers.audio.getNumFrames());

                add (outputBuffers.audio.getFirstChannels (numInputChannelsToCopy),
                     inputBuffers.audio.getFirstChannels (numInputChannelsToCopy).getStart (outputBuffers.audio.getNumFrames()));
            }
        }
        
        if (hasMidi)
            pc.buffers.midi = inputBuffers.midi;
    }
    
private:
    std::shared_ptr<InputProvider> inputProvider;
    const int numChannels;
    const bool hasMidi;
    Node* nodeDependancy = nullptr;
    const size_t nodeID { (size_t) juce::Random::getSystemRandom().nextInt() };
};


//==============================================================================
//==============================================================================
/** Takes ownership of a number of nodes but doesn't do any processing. */
class HoldingNode final : public tracktion::graph::Node
{
public:
    HoldingNode (std::vector<std::unique_ptr<tracktion::graph::Node>> nodesToStore,
                 EditItemID rackID)
        : nodes (std::move (nodesToStore)),
          nodeID ((size_t) rackID.getRawID())
    {
    }
        
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        return { true, true, 0, 0, nodeID };
    }
    
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override
    {
        return {};
    }

    bool isReadyToProcess() override
    {
        return true;
    }
    
    void process (ProcessContext&) override
    {
    }

private:
    std::vector<std::unique_ptr<tracktion::graph::Node>> nodes;
    const size_t nodeID;
};


//==============================================================================
//==============================================================================
namespace RackNodeBuilder
{
    inline tracktion::graph::ConnectedNode& getConnectedNode (tracktion::graph::Node& pluginOrModifierNode)
    {
        tracktion::graph::Node* node = dynamic_cast<PluginNode*> (&pluginOrModifierNode);
        
        if (node == nullptr)
            node = dynamic_cast<ModifierNode*> (&pluginOrModifierNode);
        
        jassert (node != nullptr);
        auto connectedNode = node->getDirectInputNodes().front();
        jassert (dynamic_cast<tracktion::graph::ConnectedNode*> (connectedNode) != nullptr);
        return *dynamic_cast<tracktion::graph::ConnectedNode*> (connectedNode);
    }

    /** Returns a input channel index from 0 for a Modifier or Plugin Node.
     
        This horrible function is required because Modifier nodes may have a MIDI input pin in which
        case pin 0 is MIDI and any subsequent pins are audio but if they don't have an MIDI pin,
        audio channels start from pin 0.
        Plugin nodes always have a MIDI input pin so audio always starts from pin 1.
    */
    inline int getDestChannelIndex (tracktion::graph::Node& destPluginOrModifierNode, int destPinIndex)
    {
        if (dynamic_cast<PluginNode*> (&destPluginOrModifierNode) != nullptr)
            return destPinIndex - 1;
        
        if (auto node = dynamic_cast<ModifierNode*> (&destPluginOrModifierNode))
            return destPinIndex - node->getModifier().getMidiInputNames().size();

        jassertfalse;
        return -1;
    }

    static inline tracktion::graph::SummingNode& getSummingNode (tracktion::graph::Node& pluginOrModifierNode)
    {
        tracktion::graph::Node* node = dynamic_cast<PluginNode*> (&pluginOrModifierNode);
        
        if (node == nullptr)
            node = dynamic_cast<ModifierNode*> (&pluginOrModifierNode);
        
        jassert (node != nullptr);
        auto summingNode = node->getDirectInputNodes().front();
        jassert (dynamic_cast<tracktion::graph::SummingNode*> (summingNode) != nullptr);
        return *dynamic_cast<tracktion::graph::SummingNode*> (summingNode);
    }

    struct RackConnectionData
    {
        EditItemID sourceID, destID;
        int sourcePin = -1, destPin = -1;
    };

    /** Represents all the pin connections between a single source and destination. */
    struct RackPinConnections
    {
        EditItemID sourceID, destID;
        std::vector<std::pair<int /*source pin*/, int /*dest pin*/>> pins;
    };

    struct ChannelMap
    {
        std::vector<std::pair<int /*source channel*/, int /*dest channel*/>> channels;
        bool passMidi = false;
    };

    static inline int getMaxNumChannels (std::vector<std::pair<int, int>> channels)
    {
        int numChannels = 0;
        
        for (auto c : channels)
            numChannels = std::max (numChannels,
                                    std::max (c.first, c.second) + 1);
        
        return numChannels;
    }

    /** This horrible function is required because Modifier nodes may have a MIDI input pin in which
        case pin 0 is MIDI and any subsequent pins are audio but if they don't have an MIDI pin,
        audio channels start from pin 0.
        Plugin nodes always have a MIDI input pin so audio always starts from pin 1.
    */
    static inline int getNumMidiInputPins (const std::vector<std::unique_ptr<tracktion::graph::Node>>& itemNodes, EditItemID itemID)
    {
        for (auto& node : itemNodes)
            if (auto modifierNode = dynamic_cast<ModifierNode*> (node.get()))
                if (modifierNode->getModifier().itemID == itemID)
                    return modifierNode->getModifier().getMidiInputNames().size();
        
        return 1;
    }

    static inline std::vector<std::pair<int, int>> createPinConnections (std::vector<RackConnectionData> connections)
    {
        // Check all the connections are from a single source/dest pair
       #if JUCE_DEBUG
        for (auto c : connections)
        {
            jassert (c.sourceID == connections.front().sourceID);
            jassert (c.destID == connections.front().destID);
        }
       #endif
        
        std::vector<std::pair<int, int>> pinConnections;
        
        for (auto c : connections)
            pinConnections.push_back ({ c.sourcePin, c.destPin });
            
        return pinConnections;
    }

    static inline ChannelMap createChannelMap (std::vector<std::pair<int, int>> pinConnections,
                                               int numDestMidiPins)
    {
        ChannelMap channelMap;
        
        for (auto c : pinConnections)
        {
            if (c.first == 0 && c.second < numDestMidiPins)
                channelMap.passMidi = true;
            else
                channelMap.channels.push_back ({ c.first - 1, c.second - numDestMidiPins });
        }

        return channelMap;
    }

    static inline std::vector<RackConnectionData> getConnectionsBetween (juce::Array<const RackConnection*> allConnections,
                                                                         EditItemID sourceID, EditItemID destID)
    {
        std::vector<RackConnectionData> connections;
        
        for (auto c : allConnections)
            if (c->sourceID == sourceID && c->destID == destID)
                connections.push_back ({ c->sourceID.get(), c->destID.get(), c->sourcePin.get(), c->destPin.get() });
        
        return connections;
    }

    static inline std::vector<std::vector<RackConnectionData>> getConnectionsToOrFrom (RackType& rack, EditItemID itemID, bool itemIsSource)
    {
        std::vector<std::vector<RackConnectionData>> connections;
        auto allConnections = rack.getConnections();
        std::vector<std::pair<EditItemID, EditItemID>> sourcesDestsDone;
        
        for (auto c : allConnections)
        {
            if (itemIsSource && c->sourceID != itemID)
            {
                continue;
            }
            else if (c->destID != itemID)
            {
                continue;
            }
            
            auto sourceDest = std::make_pair (c->sourceID.get(), c->destID.get());

            // Skip any sources already done
            if (std::find (sourcesDestsDone.begin(), sourcesDestsDone.end(), sourceDest) != sourcesDestsDone.end())
                continue;
                
            connections.push_back (getConnectionsBetween (allConnections, c->sourceID, c->destID));
            sourcesDestsDone.push_back (sourceDest);
        }

        connections.erase (std::remove_if (connections.begin(), connections.end(),
                                           [] (auto c) { return c.empty(); }),
                           connections.end());
        
        return connections;
    }

    static inline std::vector<std::vector<RackConnectionData>> getConnectionsTo (RackType& rack, EditItemID destID)
    {
        return getConnectionsToOrFrom (rack, destID, false);
    }

    static inline std::vector<std::vector<RackConnectionData>> getConnectionsFrom (RackType& rack, EditItemID sourceID)
    {
        return getConnectionsToOrFrom (rack, sourceID, true);
    }

    //==============================================================================
    inline std::unique_ptr<tracktion::graph::Node> createRackNodeConnectedNode (RackType& rack,
                                                                                double sampleRate, int blockSize,
                                                                                std::unique_ptr<tracktion::graph::Node> inputNodeToUse,
                                                                                ProcessState& processState,
                                                                                bool isRendering)
    {
        using namespace tracktion::graph;
        
        std::shared_ptr<Node> inputNode (std::move (inputNodeToUse));
        
        // Gather all the PluginNodes and ModifierNodes with a ConnectedNode
        std::map<EditItemID, std::shared_ptr<Node>> itemNodes;
        
        jassert (inputNode);

        for (auto plugin : rack.getPlugins())
            itemNodes[plugin->itemID] = makeNode<PluginNode> (makeNode<ConnectedNode> ((size_t) plugin->itemID.getRawID()),
                                                              plugin, sampleRate, blockSize, nullptr,
                                                              processState, isRendering, true, -1);

        for (auto m : rack.getModifierList().getModifiers())
            itemNodes[m->itemID] = makeNode<ModifierNode> (makeNode<ConnectedNode> ((size_t) m->itemID.getRawID()),
                                                           m, sampleRate, blockSize, nullptr,
                                                           processState.playHeadState, isRendering);

        // Create an input node and an output summing node
        auto outputNode = std::make_unique<ConnectedNode>();

        // Iterate all the connections and make Node connections between them
        for (const auto& connection : rack.getConnections())
        {
            const EditItemID sourceID = connection->sourceID;
            const EditItemID destID = connection->destID;
            const int sourcePin = connection->sourcePin;
            const int destPin = connection->destPin;
            
            if (sourceID.isInvalid() && destID.isInvalid())
            {
                // Direct input -> output connection
                if (sourcePin == 0 && destPin == 0)
                    outputNode->addMidiConnection (inputNode);
                else if (sourcePin > 0 && destPin > 0)
                    outputNode->addAudioConnection (inputNode, ChannelConnection { sourcePin - 1, destPin - 1 });
                else
                    jassertfalse;
            }
            else if (sourceID.isInvalid() && destID.isValid())
            {
                // Input -> item connection
                if (auto destNode = itemNodes[destID])
                {
                    auto& destConnectedNode = getConnectedNode (*destNode);

                    if (sourcePin == 0 && destPin == 0)
                        destConnectedNode.addMidiConnection (inputNode);
                    else if (sourcePin > 0 && destPin >= 0) // N.B. This has to account for the dest pin being an audio input to a Modifier
                        destConnectedNode.addAudioConnection (inputNode, ChannelConnection { sourcePin - 1, getDestChannelIndex (*destNode, destPin) });
                    else
                        jassertfalse;
                }
                else
                {
                    jassertfalse;
                }
            }
            else if (sourceID.isValid() && destID.isInvalid())
            {
                // Item -> output connection
                if (auto sourceNode = itemNodes[sourceID])
                {
                    if (sourcePin == 0 && destPin == 0)
                        outputNode->addMidiConnection (sourceNode);
                    else if (sourcePin > 0 && destPin > 0)
                        outputNode->addAudioConnection (sourceNode, ChannelConnection { sourcePin - 1, destPin - 1 });
                    else
                        jassertfalse;
                }
                else
                {
                    jassertfalse;
                }
            }
            else if (sourceID.isValid() && destID.isValid())
            {
                // Item -> item connection
                auto sourceNode = itemNodes[sourceID];
                auto destNode = itemNodes[destID];
                
                if (sourceNode == nullptr || destNode == nullptr)
                {
                    jassertfalse;
                }
                else
                {
                    auto& destConnectedNode = getConnectedNode (*destNode);

                    if (sourcePin == 0 && destPin == 0)
                        destConnectedNode.addMidiConnection (sourceNode);
                    else if (sourcePin > 0 && destPin >= 0) // N.B. This has to account for the dest pin being an audio input to a Modifier
                        destConnectedNode.addAudioConnection (sourceNode, ChannelConnection { sourcePin - 1, getDestChannelIndex (*destNode, destPin) });
                    else
                        jassertfalse;
                }
            }
        }
        
        // Next get any modifiers that aren't connected to the output or another plugin as
        // they'll still need to be processed but not pass any output on
        auto finalOutput = std::make_unique<SummingNode>();
        
        for (auto& node : itemNodes)
            if (auto modifierNode = dynamic_cast<ModifierNode*> (node.second.get()))
                if (getConnectionsFrom (rack, modifierNode->getModifier().itemID).empty())
                    finalOutput->addInput (makeNode<ForwardingNode> (node.second));
        
        if (finalOutput->getDirectInputNodes().size() > 0)
        {
            finalOutput->addInput (std::move (outputNode));
            return finalOutput;
        }
        
        return outputNode;
    }

    //==============================================================================
    std::unique_ptr<tracktion::graph::Node> createRackNode (tracktion::engine::RackType& rackType,
                                                            double sampleRate, int blockSize,
                                                            std::unique_ptr<tracktion::graph::Node> node,
                                                            ProcessState& processState, bool isRendering)
    {
        return createRackNodeConnectedNode (rackType,
                                            sampleRate, blockSize,
                                            std::move (node),
                                            processState, isRendering);
    }
}

}} // namespace tracktion { inline namespace engine
