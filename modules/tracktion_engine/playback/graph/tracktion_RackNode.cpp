/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_engine
{

//==============================================================================
//==============================================================================
/**
    Fills an InputProvider with output from a Node.
*/
class InputProviderFillerNode final : public tracktion_graph::Node
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
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        return input->getNodeProperties();
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override
    {
    }
    
    void process (const ProcessContext&) override
    {
        auto inputs = input->getProcessedOutput();
        inputProvider->setInputs (inputs);
    }
    
private:
    std::unique_ptr<Node> input;
    std::shared_ptr<InputProvider> inputProvider;
};


//==============================================================================
//==============================================================================
class InputNode final   : public tracktion_graph::Node
{
public:
    InputNode (std::shared_ptr<InputProvider> inputProviderToUse,
               int numAudioChannels, bool hasMidiInput)
        : inputProvider (std::move (inputProviderToUse)),
          numChannels (numAudioChannels),
          hasMidi (hasMidiInput)
    {
    }
    
    void setInputDependancy (Node* dependancy)
    {
        nodeDependancy = dependancy;
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        tracktion_graph::NodeProperties props;
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
    
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override
    {
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffers = inputProvider->getInputs();

        if (numChannels > 0)
        {
            auto& outputBuffers = pc.buffers;
            
            // The InputProvider may have less channels than this Node requires so only take the number available
            const size_t numInputChannelsToCopy = std::min (inputBuffers.audio.getNumChannels(), outputBuffers.audio.getNumChannels());
            
            if (numInputChannelsToCopy > 0)
            {
                // For testing purposes, the last block might be smaller than the InputProvider
                // so we'll just take the number of samples required
                jassert (inputBuffers.audio.getNumSamples() >= outputBuffers.audio.getNumSamples());
                auto inputAudioBlock = inputBuffers.audio.getSubsetChannelBlock (0, numInputChannelsToCopy)
                                        .getSubBlock (0, outputBuffers.audio.getNumSamples());
                auto outputAudioBlock = outputBuffers.audio.getSubsetChannelBlock (0, numInputChannelsToCopy);
                outputAudioBlock.add (inputAudioBlock);
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
class RackModifierNode final    : public tracktion_graph::Node
{
public:
    RackModifierNode (std::unique_ptr<Node> inputNode,
                      Modifier::Ptr modifierToProcess,
                      std::shared_ptr<InputProvider> contextProvider)
        : input (std::move (inputNode)),
          modifier (std::move (modifierToProcess)),
          audioRenderContextProvider (std::move (contextProvider))
    {
        jassert (input != nullptr);
        jassert (modifier != nullptr);
    }
    
    ~RackModifierNode() override
    {
        if (isInitialised)
            modifier->baseClassDeinitialise();
    }
    
    Modifier& getModifier()
    {
        return *modifier;
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();

        props.numberOfChannels = juce::jmax (props.numberOfChannels, modifier->getAudioInputNames().size());
        props.hasAudio = modifier->getAudioInputNames().size() > 0;
        props.hasMidi  = modifier->getMidiInputNames().size() > 0;
        props.nodeID = (size_t) modifier->itemID.getRawID();

        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info) override
    {
        modifier->baseClassInitialise (info.sampleRate, info.blockSize);
        isInitialised = true;
        sampleRate = info.sampleRate;
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        auto& inputAudioBlock = inputBuffers.audio;
        
        auto& outputBuffers = pc.buffers;
        auto& outputAudioBlock = outputBuffers.audio;

        // Copy the inputs to the outputs, then process using the
        // output buffers as that will be the correct size
        {
            const size_t numInputChannelsToCopy = std::min (inputAudioBlock.getNumChannels(), outputAudioBlock.getNumChannels());
            
            if (numInputChannelsToCopy > 0)
            {
                jassert (inputAudioBlock.getNumSamples() == outputAudioBlock.getNumSamples());
                outputAudioBlock.copyFrom (inputAudioBlock.getSubsetChannelBlock (0, numInputChannelsToCopy));
            }
        }

        // Setup audio buffers
        float* channels[32] = {};

        for (size_t i = 0; i < outputAudioBlock.getNumChannels(); ++i)
            channels[i] = outputAudioBlock.getChannelPointer (i);

        juce::AudioBuffer<float> outputAudioBuffer (channels,
                                                    (int) outputAudioBlock.getNumChannels(),
                                                    (int) outputAudioBlock.getNumSamples());

        // Then MIDI buffers
        midiMessageArray.copyFrom (inputBuffers.midi);

        // Then prepare the AudioRenderContext
        auto sourceContext = audioRenderContextProvider->getContext();
        PluginRenderContext prc (sourceContext);
        prc.destBuffer = &outputAudioBuffer;
        prc.bufferStartSample = 0;
        prc.bufferNumSamples = outputAudioBuffer.getNumSamples();
        prc.bufferForMidiMessages = &midiMessageArray;
        prc.midiBufferOffset = 0.0;

        // Process the plugin
        modifier->applyToBuffer (prc);
        
        // Then copy the buffers to the outputs
        outputBuffers.midi.mergeFrom (midiMessageArray);
    }
    
private:
    std::unique_ptr<Node> input;
    Modifier::Ptr modifier;
    std::shared_ptr<InputProvider> audioRenderContextProvider;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    MidiMessageArray midiMessageArray;
};

//==============================================================================
//==============================================================================
/** Takes a non-owning input node and simply forwards its outputs on. */
class ForwardingNode final  : public tracktion_graph::Node
{
public:
    ForwardingNode (tracktion_graph::Node* inputNode)
        : input (inputNode)
    {
        jassert (input);
    }
        
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        tracktion_graph::hash_combine (props.nodeID, nodeID);
        
        return props;
    }
    
    std::vector<tracktion_graph::Node*> getDirectInputNodes() override
    {
        return { input };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        pc.buffers.audio.copyFrom (inputBuffers.audio);
        pc.buffers.midi.copyFrom (inputBuffers.midi);
    }

private:
    tracktion_graph::Node* input;
    const size_t nodeID { (size_t) juce::Random::getSystemRandom().nextInt() };
};

//==============================================================================
//==============================================================================
/** Takes ownership of a number of nodes but doesn't do any processing. */
class HoldingNode final : public tracktion_graph::Node
{
public:
    HoldingNode (std::vector<std::unique_ptr<tracktion_graph::Node>> nodesToStore)
        : nodes (std::move (nodesToStore))
    {
    }
        
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        return { true, true, 0 };
    }
    
    std::vector<tracktion_graph::Node*> getDirectInputNodes() override
    {
        return {};
    }

    bool isReadyToProcess() override
    {
        return true;
    }
    
    void process (const ProcessContext&) override
    {
    }

private:
    std::vector<std::unique_ptr<tracktion_graph::Node>> nodes;
};


//==============================================================================
//==============================================================================
namespace RackNodeBuilder
{
    namespace te = tracktion_engine;

    static inline tracktion_graph::SummingNode& getSummingNode (tracktion_graph::Node& pluginOrModifierNode)
    {
        tracktion_graph::Node* node = dynamic_cast<PluginNode*> (&pluginOrModifierNode);
        
        if (node == nullptr)
            node = dynamic_cast<RackModifierNode*> (&pluginOrModifierNode);
        
        jassert (node != nullptr);
        auto summingNode = node->getDirectInputNodes().front();
        jassert (dynamic_cast<tracktion_graph::SummingNode*> (summingNode) != nullptr);
        return *dynamic_cast<tracktion_graph::SummingNode*> (summingNode);
    }

    struct RackConnection
    {
        te::EditItemID sourceID, destID;
        int sourcePin = -1, destPin = -1;
    };

    /** Represents all the pin connections between a single source and destination. */
    struct RackPinConnections
    {
        te::EditItemID sourceID, destID;
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
    static inline int getNumMidiInputPins (const std::vector<std::unique_ptr<tracktion_graph::Node>>& itemNodes, te::EditItemID itemID)
    {
        for (auto& node : itemNodes)
            if (auto modifierNode = dynamic_cast<RackModifierNode*> (node.get()))
                if (modifierNode->getModifier().itemID == itemID)
                    return modifierNode->getModifier().getMidiInputNames().size();
        
        return 1;
    }

    static inline std::vector<std::pair<int, int>> createPinConnections (std::vector<RackConnection> connections)
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

    static inline std::unique_ptr<tracktion_graph::Node> createChannelRemappingNode (te::EditItemID sourceID,
                                                                                     ChannelMap channelMap,
                                                                                     std::shared_ptr<InputProvider> inputProvider,
                                                                                     const std::vector<std::unique_ptr<tracktion_graph::Node>>& pluginNodes)
    {
        jassert (! channelMap.channels.empty() || channelMap.passMidi);
        std::unique_ptr<tracktion_graph::Node> inputNode;
        
        if (sourceID.isInvalid())
        {
            // Connected to Rack inputs
            inputNode = tracktion_graph::makeNode<InputNode> (std::move (inputProvider),
                                                              getMaxNumChannels (channelMap.channels),
                                                              channelMap.passMidi);
        }
        else
        {
            for (const auto& node : pluginNodes)
            {
                if (auto pluginNode = dynamic_cast<PluginNode*> (node.get()))
                {
                    if (pluginNode->getPlugin().itemID == sourceID)
                    {
                        inputNode = tracktion_graph::makeNode<ForwardingNode> (pluginNode);
                        break;
                    }
                }
                else if (auto modifierNode = dynamic_cast<RackModifierNode*> (node.get()))
                {
                    if (modifierNode->getModifier().itemID == sourceID)
                    {
                        inputNode = tracktion_graph::makeNode<ForwardingNode> (modifierNode);
                        break;
                    }
                }
            }
        }
        
        jassert (inputNode != nullptr);
        return tracktion_graph::makeNode<tracktion_graph::ChannelRemappingNode> (std::move (inputNode),
                                                                                 channelMap.channels, channelMap.passMidi);
    }

    static inline std::vector<RackConnection> getConnectionsBetween (juce::Array<const te::RackConnection*> allConnections,
                                                                     te::EditItemID sourceID, te::EditItemID destID)
    {
        std::vector<RackConnection> connections;
        
        for (auto c : allConnections)
            if (c->sourceID == sourceID && c->destID == destID)
                connections.push_back ({ c->sourceID.get(), c->destID.get(), c->sourcePin.get(), c->destPin.get() });
        
        return connections;
    }

    static inline std::vector<std::vector<RackConnection>> getConnectionsToOrFrom (te::RackType& rack, te::EditItemID itemID, bool itemIsSource)
    {
        std::vector<std::vector<RackConnection>> connections;
        auto allConnections = rack.getConnections();
        std::vector<std::pair<te::EditItemID, te::EditItemID>> sourcesDestsDone;
        
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

    static inline std::vector<std::vector<RackConnection>> getConnectionsTo (te::RackType& rack, te::EditItemID destID)
    {
        return getConnectionsToOrFrom (rack, destID, false);
    }

    static inline std::vector<std::vector<RackConnection>> getConnectionsFrom (te::RackType& rack, te::EditItemID sourceID)
    {
        return getConnectionsToOrFrom (rack, sourceID, true);
    }

    //==============================================================================
    std::unique_ptr<tracktion_graph::Node> createRackNode (te::RackType& rack,
                                                           double sampleRate, int blockSize,
                                                           std::shared_ptr<InputProvider> inputProvider,
                                                           tracktion_graph::PlayHeadState* playHeadState,
                                                           bool isRendering)
    {
        // Gather all the PluginNodes and ModifierNodes in a vector
        std::vector<std::unique_ptr<tracktion_graph::Node>> itemNodes;
        
        if (playHeadState)
        {
            for (auto plugin : rack.getPlugins())
                itemNodes.push_back (tracktion_graph::makeNode<PluginNode> (tracktion_graph::makeNode<tracktion_graph::SummingNode>(),
                                                                            plugin, sampleRate, blockSize, nullptr,
                                                                            *playHeadState, isRendering));
        }
        else
        {
            for (auto plugin : rack.getPlugins())
                itemNodes.push_back (tracktion_graph::makeNode<PluginNode> (tracktion_graph::makeNode<tracktion_graph::SummingNode>(),
                                                                            plugin, sampleRate, blockSize, inputProvider));
        }
        
        for (auto m : rack.getModifierList().getModifiers())
            itemNodes.push_back (tracktion_graph::makeNode<RackModifierNode> (tracktion_graph::makeNode<tracktion_graph::SummingNode>(),
                                                                              m, inputProvider));
        
        // Iterate all the plugin/modifiers and find all the inputs to them grouped by input
        for (auto& node : itemNodes)
        {
            te::EditItemID itemID;

            if (auto pluginNode = dynamic_cast<PluginNode*> (node.get()))
                itemID = pluginNode->getPlugin().itemID;
            else if (auto modifierNode = dynamic_cast<RackModifierNode*> (node.get()))
                itemID = modifierNode->getModifier().itemID;

            if (! itemID.isValid())
                continue;

            for (auto connectionsBetweenSingleSourceAndDest : getConnectionsTo (rack, itemID))
                getSummingNode (*node).addInput (createChannelRemappingNode (connectionsBetweenSingleSourceAndDest.front().sourceID,
                                                                             createChannelMap (createPinConnections (connectionsBetweenSingleSourceAndDest),
                                                                                               getNumMidiInputPins (itemNodes, itemID)),
                                                                             inputProvider,
                                                                             itemNodes));
        }
        
        auto outputNode = std::make_unique<tracktion_graph::SummingNode>();
        
        // Next find all the plugins connected to the outputs (this will include direct input/output connections)
        for (auto connectionsBetweenSingleSourceAndRackOutputs : getConnectionsTo (rack, {}))
        {
            outputNode->addInput (createChannelRemappingNode (connectionsBetweenSingleSourceAndRackOutputs.front().sourceID,
                                                              createChannelMap (createPinConnections (connectionsBetweenSingleSourceAndRackOutputs), 1),
                                                              inputProvider,
                                                              itemNodes));

        }
        
        // Next get any modifiers that aren't connected to the output or another plugin as
        // they'll still need to be processed but not pass any output on
        for (auto& node : itemNodes)
        {
            if (auto modifierNode = dynamic_cast<RackModifierNode*> (node.get()))
                if (getConnectionsFrom (rack, modifierNode->getModifier().itemID).empty())
                    outputNode->addInput (tracktion_graph::makeNode<tracktion_graph::SinkNode> (tracktion_graph::makeNode<ForwardingNode> (modifierNode)));
        }

        // Finally store all the Plugin/ModifierNodes somewhere so they don't get processed again
        outputNode->addInput (tracktion_graph::makeNode<HoldingNode> (std::move (itemNodes)));
        
        return outputNode;
    }

    //==============================================================================
    std::unique_ptr<tracktion_graph::Node> createRackNode (tracktion_engine::RackType& rackType,
                                                           double sampleRate, int blockSize,
                                                           std::unique_ptr<tracktion_graph::Node> node,
                                                           tracktion_graph::PlayHeadState& playHeadState, bool isRendering)
    {
        auto inputProvider = std::make_shared<InputProvider>();
        
        auto inputFiller = tracktion_graph::makeNode<InputProviderFillerNode> (std::move (node), inputProvider);
        auto rackNode = createRackNode (rackType, sampleRate, blockSize, inputProvider, &playHeadState, isRendering);
        
        // This could be provided as an argument to the createRackNode method to avoid the lookup here
        visitNodes (*rackNode,
                    [sourceInputNode = inputFiller.get()] (auto& n)
                    {
                        if (auto inputNode = dynamic_cast<InputNode*> (&n))
                            inputNode->setInputDependancy (sourceInputNode);
                    },
                    false);
        
        return tracktion_graph::makeSummingNode ({ inputFiller.release(), rackNode.release() });
    }
}

}
