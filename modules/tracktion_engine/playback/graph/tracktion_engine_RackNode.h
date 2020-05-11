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
struct InputProvider
{
    InputProvider() = default;
    InputProvider (int numChannelsToUse) : numChannels (numChannelsToUse) {}

    void setInputs (tracktion_graph::Node::AudioAndMidiBuffer newBuffers)
    {
        audio = numChannels > 0 ? newBuffers.audio.getSubsetChannelBlock (0, (size_t) numChannels) : newBuffers.audio;
        midi.copyFrom (newBuffers.midi);
    }
    
    tracktion_graph::Node::AudioAndMidiBuffer getInputs()
    {
        return { audio, midi };
    }
    
    void setContext (tracktion_engine::AudioRenderContext* rc)
    {
        context = rc;
    }
    
    /** Returns the context currently in use.
        This is only valid for the duration of the process call.
    */
    tracktion_engine::AudioRenderContext& getContext()
    {
        jassert (context != nullptr);
        return *context;
    }
    
    int numChannels = 0;
    juce::dsp::AudioBlock<float> audio;
    tracktion_engine::MidiMessageArray midi;

    tracktion_engine::AudioRenderContext* context = nullptr;
};


//==============================================================================
//==============================================================================
class InputNode : public tracktion_graph::Node
{
public:
    InputNode (std::shared_ptr<InputProvider> inputProviderToUse,
               int numAudioChannels, bool hasMidiInput)
        : inputProvider (std::move (inputProviderToUse)),
          numChannels (numAudioChannels),
          hasMidi (hasMidiInput)
    {
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        tracktion_graph::NodeProperties props;
        props.hasAudio = numChannels > 0;
        props.hasMidi = hasMidi;
        props.numberOfChannels = numChannels;
        props.nodeID = nodeID;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
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
            
            // For testing purposes, the last block might be smaller than the InputProvider
            // so we'll just take the number of samples required
            jassert (inputBuffers.audio.getNumSamples() >= outputBuffers.audio.getNumSamples());
            auto inputAudioBlock = inputBuffers.audio.getSubsetChannelBlock (0, numInputChannelsToCopy)
                                    .getSubBlock (0, outputBuffers.audio.getNumSamples());
            auto outputAudioBlock = outputBuffers.audio.getSubsetChannelBlock (0, numInputChannelsToCopy);
            outputAudioBlock.add (inputAudioBlock);
        }
        
        if (hasMidi)
            pc.buffers.midi = inputBuffers.midi;
    }
    
private:
    std::shared_ptr<InputProvider> inputProvider;
    const int numChannels;
    const bool hasMidi;
    const size_t nodeID { (size_t) juce::Random::getSystemRandom().nextInt() };
};


//==============================================================================
//==============================================================================
class PluginNode   : public tracktion_graph::Node
{
public:
    PluginNode (std::unique_ptr<Node> inputNode,
                tracktion_engine::Plugin::Ptr pluginToProcess,
                std::shared_ptr<InputProvider> contextProvider)
        : input (std::move (inputNode)),
          plugin (std::move (pluginToProcess)),
          audioRenderContextProvider (std::move (contextProvider))
    {
        jassert (input != nullptr);
        jassert (plugin != nullptr);
    }
    
    ~PluginNode() override
    {
        if (isInitialised && ! plugin->baseClassNeedsInitialising())
            plugin->baseClassDeinitialise();
    }
    
    Plugin& getPlugin()
    {
        return *plugin;
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        const auto latencyNumSamples = roundToInt (plugin->getLatencySeconds() * sampleRate);

        props.numberOfChannels = jmax (1, props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (props.numberOfChannels));
        props.hasAudio = plugin->producesAudioWhenNoAudioInput();
        props.hasMidi  = plugin->takesMidiInput();
        props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);
        props.nodeID = (size_t) plugin->itemID.getRawID();

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
        tracktion_engine::PlayHead playHead;
        tracktion_engine::PlaybackInitialisationInfo teInfo =
        {
            0.0, info.sampleRate, info.blockSize, {}, playHead
        };
        
        plugin->baseClassInitialise (teInfo);
        isInitialised = true;

        sampleRate = info.sampleRate;
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        auto& inputAudioBlock = inputBuffers.audio;
        
        auto& outputBuffers = pc.buffers;
        auto& outputAudioBlock = outputBuffers.audio;
        jassert (inputAudioBlock.getNumSamples() == outputAudioBlock.getNumSamples());

        // Copy the inputs to the outputs, then process using the
        // output buffers as that will be the correct size
        {
            const size_t numInputChannelsToCopy = std::min (inputAudioBlock.getNumChannels(), outputAudioBlock.getNumChannels());
            
            if (numInputChannelsToCopy > 0)
                outputAudioBlock.copyFrom (inputAudioBlock.getSubsetChannelBlock (0, numInputChannelsToCopy));
        }

        // Setup audio buffers
        float* channels[32] = {};

        for (size_t i = 0; i < outputAudioBlock.getNumChannels(); ++i)
            channels[i] = outputAudioBlock.getChannelPointer (i);

        AudioBuffer<float> outputAudioBuffer (channels,
                                              (int) outputAudioBlock.getNumChannels(),
                                              (int) outputAudioBlock.getNumSamples());

        // Then MIDI buffers
        midiMessageArray.copyFrom (inputBuffers.midi);

        // Then prepare the AudioRenderContext
        auto sourceContext = audioRenderContextProvider->getContext();
        tracktion_engine::AudioRenderContext rc (sourceContext);
        rc.destBuffer = &outputAudioBuffer;
        rc.bufferStartSample = 0;
        rc.bufferNumSamples = outputAudioBuffer.getNumSamples();
        rc.bufferForMidiMessages = &midiMessageArray;
        rc.midiBufferOffset = 0.0;

        // Process the plugin
        plugin->applyToBufferWithAutomation (rc);
        
        // Then copy the buffers to the outputs
        outputBuffers.midi.mergeFrom (midiMessageArray);
    }
    
private:
    std::unique_ptr<Node> input;
    tracktion_engine::Plugin::Ptr plugin;
    std::shared_ptr<InputProvider> audioRenderContextProvider;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    tracktion_engine::MidiMessageArray midiMessageArray;
};


//==============================================================================
//==============================================================================
class ModifierNode  : public tracktion_graph::Node
{
public:
    ModifierNode (std::unique_ptr<Node> inputNode,
                  tracktion_engine::Modifier::Ptr modifierToProcess,
                  std::shared_ptr<InputProvider> contextProvider)
        : input (std::move (inputNode)),
          modifier (std::move (modifierToProcess)),
          audioRenderContextProvider (std::move (contextProvider))
    {
        jassert (input != nullptr);
        jassert (modifier != nullptr);
    }
    
    ~ModifierNode() override
    {
        if (isInitialised)
            modifier->baseClassDeinitialise();
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        jassert (isInitialised);
        auto props = input->getNodeProperties();

        props.numberOfChannels = jmax (props.numberOfChannels, modifier->getAudioInputNames().size());
        props.hasAudio = modifier->getAudioInputNames().size() > 0;
        props.hasMidi  = modifier->getMidiInputNames().size() > 0;

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
        tracktion_engine::PlayHead playHead;
        tracktion_engine::PlaybackInitialisationInfo teInfo =
        {
            0.0, info.sampleRate, info.blockSize, {}, playHead
        };
        
        modifier->baseClassInitialise (teInfo);
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

        AudioBuffer<float> outputAudioBuffer (channels,
                                              (int) outputAudioBlock.getNumChannels(),
                                              (int) outputAudioBlock.getNumSamples());

        // Then MIDI buffers
        midiMessageArray.copyFrom (inputBuffers.midi);

        // Then prepare the AudioRenderContext
        auto sourceContext = audioRenderContextProvider->getContext();
        tracktion_engine::AudioRenderContext rc (sourceContext);
        rc.destBuffer = &outputAudioBuffer;
        rc.bufferStartSample = 0;
        rc.bufferNumSamples = outputAudioBuffer.getNumSamples();
        rc.bufferForMidiMessages = &midiMessageArray;
        rc.midiBufferOffset = 0.0;

        // Process the plugin
        modifier->applyToBuffer (rc);
        
        // Then copy the buffers to the outputs
        outputBuffers.midi.mergeFrom (midiMessageArray);
    }
    
private:
    std::unique_ptr<Node> input;
    tracktion_engine::Modifier::Ptr modifier;
    std::shared_ptr<InputProvider> audioRenderContextProvider;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    tracktion_engine::MidiMessageArray midiMessageArray;
};

//==============================================================================
//==============================================================================
/** Takes a non-owning input node and simply forwards its outputs on. */
class ForwardingNode  : public tracktion_graph::Node
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
class HoldingNode  : public tracktion_graph::Node
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
/**
    Simple processor for a Node which uses an InputProvider to pass input in to the graph.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class RackNodePlayer
{
public:
    /** Creates an RackNodePlayer to process an Node with input. */
    RackNodePlayer (std::unique_ptr<tracktion_graph::Node> nodeToProcess,
                    std::shared_ptr<InputProvider> inputProviderToUse,
                    bool overrideInputProvider)
        : input (std::move (nodeToProcess)),
          inputProvider (std::move (inputProviderToUse)),
          overrideInputs (overrideInputProvider)
    {
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        jassert (sampleRate != 0.0);
        jassert (blockSize != 0);
        
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const tracktion_graph::PlaybackInitialisationInfo info { sampleRate, blockSize, *input };
        visitNodes (*input, [&] (tracktion_graph::Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes = tracktion_graph::getNodes (*input, tracktion_graph::VertexOrdering::postordering);
    }
    
    int getLatencySamples() const
    {
        jassert (! allNodes.empty()); // Must be initialised first
        return input->getNodeProperties().latencyNumSamples;
    }

    /** Processes a block of audio and MIDI data. */
    void process (const tracktion_graph::Node::ProcessContext& pc)
    {
        if (overrideInputs)
            inputProvider->setInputs (pc.buffers);

        // The internal nodes won't be interested in the top level audio/midi inputs
        // They should only be referencing this for time and continuity
        tracktion_engine::AudioRenderContext rc (playHead, {},
                                                 nullptr, juce::AudioChannelSet(), 0, 0,
                                                 nullptr, 0.0,
                                                 tracktion_engine::AudioRenderContext::contiguous, false);

        inputProvider->setContext (&rc);
        
        for (auto node : allNodes)
            node->prepareForNextBlock();
        
        for (;;)
        {
            int processedAnyNodes = false;
            
            for (auto node : allNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (pc.streamSampleRange);
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                auto output = input->getProcessedOutput();
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.copyFrom (output.midi);
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<tracktion_graph::Node> input;
    std::vector<tracktion_graph::Node*> allNodes;
    std::shared_ptr<InputProvider> inputProvider;
    bool overrideInputs = true;
    tracktion_engine::PlayHead playHead;
};


//==============================================================================
//==============================================================================
namespace RackNodeBuilder
{
    namespace te = tracktion_engine;

    static inline tracktion_graph::SummingNode& getPluginSummingNode (tracktion_graph::Node& pluginNode)
    {
        auto summingNode = dynamic_cast<PluginNode*> (&pluginNode)->getDirectInputNodes().front();
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

    static inline std::vector<std::pair<int, int>> createPinConnections (std::vector<RackConnection> connections)
    {
        // TODO: Check all the connections are from a single source/dest pair
        std::vector<std::pair<int, int>> pinConnections;
        
        for (auto c : connections)
            pinConnections.push_back ({ c.sourcePin, c.destPin });
            
        return pinConnections;
    }

    static inline ChannelMap createChannelMap (std::vector<std::pair<int, int>> pinConnections)
    {
        ChannelMap channelMap;
        
        for (auto c : pinConnections)
        {
            if (c.first == 0 && c.second == 0)
                channelMap.passMidi = true;
            else
                channelMap.channels.push_back ({ c.first - 1, c.second - 1 });
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
            // Connected to Plugin
            for (const auto& node : pluginNodes)
                if (auto pluginNode = dynamic_cast<PluginNode*> (node.get()))
                    if (pluginNode->getPlugin().itemID == sourceID)
                        inputNode = tracktion_graph::makeNode<ForwardingNode> (pluginNode);
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

    static inline std::vector<std::vector<RackConnection>> getConnectionsTo (te::RackType& rack, te::EditItemID itemID)
    {
        std::vector<std::vector<RackConnection>> connections;
        auto allConnections = rack.getConnections();
        std::vector<std::pair<te::EditItemID, te::EditItemID>> sourcesDestsDone;
        
        for (auto c : allConnections)
        {
            if (c->destID != itemID)
                continue;
            
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

    static inline std::unique_ptr<tracktion_graph::Node> createRackNode (te::RackType& rack,
                                                                         std::shared_ptr<InputProvider> inputProvider)
    {
        // Gather all the PluginNodes in a vector
        std::vector<std::unique_ptr<tracktion_graph::Node>> pluginNodes;
        
        for (auto plugin : rack.getPlugins())
            pluginNodes.push_back (tracktion_graph::makeNode<PluginNode> (tracktion_graph::makeNode<tracktion_graph::SummingNode>(),
                                                                          plugin, inputProvider));
        
        // Iterate all the plugins and find all the inputs to them grouped by input
        for (auto& node : pluginNodes)
        {
            if (auto pluginNode = dynamic_cast<PluginNode*> (node.get()))
            {
                auto& plugin = pluginNode->getPlugin();
                auto& summingNode = getPluginSummingNode (*pluginNode);

                for (auto connectionsBetweenSingleSourceAndDest : getConnectionsTo (rack, plugin.itemID))
                    summingNode.addInput (createChannelRemappingNode (connectionsBetweenSingleSourceAndDest.front().sourceID,
                                                                      createChannelMap (createPinConnections (connectionsBetweenSingleSourceAndDest)),
                                                                      inputProvider,
                                                                      pluginNodes));
            }
        }
        
        auto outputNode = std::make_unique<tracktion_graph::SummingNode>();
        
        // Next find all the plugins connected to the outputs (this will include direct input/output connections)
        for (auto connectionsBetweenSingleSourceAndRackOutputs : getConnectionsTo (rack, {}))
        {
            outputNode->addInput (createChannelRemappingNode (connectionsBetweenSingleSourceAndRackOutputs.front().sourceID,
                                                              createChannelMap (createPinConnections (connectionsBetweenSingleSourceAndRackOutputs)),
                                                              inputProvider,
                                                              pluginNodes));

        }

        // Finally store all the PluginNodes somewhere so they don't get processed again
        outputNode->addInput (tracktion_graph::makeNode<HoldingNode> (std::move (pluginNodes)));
        
        return outputNode;
    }
}

}
