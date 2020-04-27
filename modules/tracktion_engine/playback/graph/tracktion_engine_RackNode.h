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
            auto& audioOutputBlock = outputBuffers.audio;
            
            // For testing purposes, the last block might be smaller than the InputProvider
            // so we'll just take the number of samples required
            jassert (inputBuffers.audio.getNumSamples() >= audioOutputBlock.getNumSamples());
            jassert (inputBuffers.audio.getNumChannels() >= (size_t) numChannels);
            
            auto inputAudioBlock = inputBuffers.audio.getSubsetChannelBlock (0, (size_t) numChannels)
                                    .getSubBlock (0, audioOutputBlock.getNumSamples());
            jassert (inputAudioBlock.getNumChannels() == audioOutputBlock.getNumChannels());
            
            audioOutputBlock.add (inputAudioBlock);
        }
        
        if (hasMidi)
            pc.buffers.midi = inputBuffers.midi;
    }
    
private:
    std::shared_ptr<InputProvider> inputProvider;
    const int numChannels;
    const bool hasMidi;
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
        if (isInitialised)
            plugin->baseClassDeinitialise();
    }
    
    tracktion_graph::NodeProperties getNodeProperties() override
    {
        jassert (isInitialised);
        auto props = input->getNodeProperties();
        const auto latencyNumSamples = roundToInt (plugin->getLatencySeconds() * sampleRate);

        props.numberOfChannels = jmax (1, props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (props.numberOfChannels));
        props.hasAudio = plugin->producesAudioWhenNoAudioInput();
        props.hasMidi  = plugin->takesMidiInput();
        props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);

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
/**
    Simple processor for a Node which uses an InputProvider to pass input in to the graph.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class RackAudioNodeProcessor
{
public:
    /** Creates an RackAudioNodeProcessor to process an Node with input. */
    RackAudioNodeProcessor (std::unique_ptr<tracktion_graph::Node> nodeToProcess,
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
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const tracktion_graph::PlaybackInitialisationInfo info { sampleRate, blockSize, *input };
        std::function<void (tracktion_graph::Node&)> visitor = [&] (tracktion_graph::Node& n) { n.initialise (info); };
        visitInputs (*input, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes.clear();
        
        std::function<void (tracktion_graph::Node&)> visitor2 = [&] (tracktion_graph::Node& n) { allNodes.push_back (&n); };
        visitInputs (*input, visitor2);
    }

    /** Processes a block of audio and MIDI data. */
    void process (const tracktion_graph::Node::ProcessContext& pc)
    {
        if (overrideInputs)
            inputProvider->setInputs (pc.buffers);

        // The internal nodes won't be interested in the top level audio/midi inputs
        // They should only be referencing this for time and continuity
        tracktion_engine::PlayHead playHead;
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
};


//==============================================================================
//==============================================================================
namespace RackNodeBuilder
{
    namespace te = tracktion_engine;

    struct Connection
    {
        te::EditItemID sourceID, destID;
    };

    /** Returns a vector of connections between inputs and outputs. */
    static inline std::vector<Connection> getSimplifiedConnections (te::RackType& rack)
    {
        std::vector<Connection> connections;

        for (auto c : rack.getConnections())
            connections.push_back (Connection { c->sourceID.get(), c->destID.get() });

        std::sort (connections.begin(), connections.end(),
                   [] (auto c1, auto c2)
                   {
                       return (c1.sourceID.toString() + c1.destID.toString()).hash()
                            < (c2.sourceID.toString() + c2.destID.toString()).hash();
                   });
        auto last = std::unique (connections.begin(), connections.end(),
                                 [] (auto c1, auto c2) { return c1.sourceID == c2.sourceID && c1.destID == c2.destID; });
        connections.erase (last, connections.end());
        //TODO: This should be smarter and remove cycles

        return connections;
    }

    static inline Array<const te::RackConnection*> getConnectionsBetween (te::RackType& rack,
                                                                          te::EditItemID sourceID, te::EditItemID destID)
    {
        Array<const te::RackConnection*> connections;

        for (auto c : rack.getConnections())
            if (c->sourceID == sourceID && c->destID == destID)
                connections.add (c);
        
        return connections;
    }

    static inline std::unique_ptr<tracktion_graph::Node> createChannelMappingNodeForConnections (std::unique_ptr<tracktion_graph::Node> input,
                                                                                                 const Array<const te::RackConnection*>& connections)
    {
        jassert (! connections.isEmpty());
        std::vector<std::pair<int, int>> chanelMap;
        bool passMidi = false;
        
        auto firstSourceID = connections.getFirst()->sourceID.get();
        auto firstDestID = connections.getFirst()->destID.get();
        ignoreUnused (firstSourceID, firstDestID);

        for (auto c : connections)
        {
            jassert (c->sourceID == firstSourceID);
            jassert (c->destID == firstDestID);

            if (c->sourcePin.get() == 0 && c->destPin.get() == 0)
                passMidi = true;
            else
                chanelMap.emplace_back (c->sourcePin - 1, c->destPin - 1);
        }
        
        if (! chanelMap.empty() || passMidi)
            return tracktion_graph::makeNode<tracktion_graph::ChannelMappingNode> (std::move (input), std::move (chanelMap), passMidi);
        
        return {};
    }

    //==============================================================================
    static inline std::unique_ptr<tracktion_graph::Node> createNodeForDirectConnectionsBetween (te::RackType& rack,
                                                                                                te::EditItemID sourceID, te::EditItemID destID,
                                                                                                std::shared_ptr<InputProvider> inputProvider)
    {
        auto connections = getConnectionsBetween (rack, sourceID, destID);
        
        if (connections.isEmpty())
            return {};
        
        int numChannels = 0;
        bool hasMidi = false;
        
        for (auto c : connections)
        {
            const int sourcePin = c->sourcePin.get();
            const int destPin = c->destPin.get();
            
            if (sourcePin == 0 && destPin == 0)
                hasMidi = true;
            else
                numChannels = jmax (numChannels, sourcePin, destPin);
        }
        
        return createChannelMappingNodeForConnections (tracktion_graph::makeNode<InputNode> (inputProvider, numChannels, hasMidi),
                                                       connections);
    }

    static inline std::unique_ptr<tracktion_graph::Node> createNodeForRackInputOutputDirectConnections (te::RackType& rack,
                                                                                                        std::shared_ptr<InputProvider> inputProvider)
    {
        return createNodeForDirectConnectionsBetween (rack, {}, {}, inputProvider);
    }

    //==============================================================================
    static inline std::unique_ptr<tracktion_graph::Node> createNodeForPlugin (te::RackType& rack, te::Plugin::Ptr plugin,
                                                                              std::shared_ptr<InputProvider> inputProvider)
    {
        std::vector<std::unique_ptr<tracktion_graph::Node>> nodes;

        const auto pluginID = plugin->itemID;
        
        // Start with any Rack inputs connected directly to this plugin
        if (auto inputNode = createNodeForDirectConnectionsBetween (rack, {}, pluginID, inputProvider))
            nodes.push_back (std::move (inputNode));
        
        // Then find any connections between plugins and this plugin
        for (auto c : getSimplifiedConnections (rack))
        {
            if (c.destID != pluginID)
                continue;

            if (auto p = rack.getPluginForID (c.sourceID))
            {
                auto pluginNode = createNodeForPlugin (rack, *p, inputProvider);
                auto mappedNode = createChannelMappingNodeForConnections (std::move (pluginNode),
                                                                          getConnectionsBetween (rack, c.sourceID, pluginID));
                nodes.push_back (std::move (mappedNode));
            }
        }

        return tracktion_graph::makeNode<PluginNode> (tracktion_graph::makeNode<tracktion_graph::SummingNode> (std::move (nodes)), *plugin, inputProvider);
    }

    static inline std::unique_ptr<tracktion_graph::Node> createRackNode (te::RackType& rack,
                                                                         std::shared_ptr<InputProvider> inputProvider)
    {
        std::vector<std::unique_ptr<tracktion_graph::Node>> nodes;
        
        // Start with any inputs connected directly to the outputs
        if (auto inputNode = createNodeForRackInputOutputDirectConnections (rack, inputProvider))
            nodes.push_back (std::move (inputNode));
        
        // Then find any connections between plugins and rack output
        for (auto c : getSimplifiedConnections (rack))
        {
            // Connected to a plugin's input
            if (c.destID.isValid())
                continue;

            // Otherwise connected to the Rack output
            if (auto plugin = rack.getPluginForID (c.sourceID))
            {
                auto pluginNode = createNodeForPlugin (rack, *plugin, inputProvider);
                auto mappedNode = createChannelMappingNodeForConnections (std::move (pluginNode),
                                                                          getConnectionsBetween (rack, c.sourceID, {}));
                nodes.push_back (std::move (mappedNode));
            }
        }

        return tracktion_graph::makeNode<tracktion_graph::SummingNode> (std::move (nodes));
    }
}

}
