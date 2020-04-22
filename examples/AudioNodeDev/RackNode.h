/*
  ==============================================================================

    RackNode.h
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TestAudioNodes.h"

// To process a Rack we need to:
//  1. Find all the plugins in the Rack
//  2. Start by creating an output node (with the num channels as the num outputs) and then find all plugins attached to it
//        - This is a SummingAudioNode
//  3. Inputs are handled by a SendAudioNode feeding in to the output SummingAudioNode
//      - If there are no direct connections between the input and output, the ChannelMappingAudioNode will be empty
//      - Otherwise it will need to map input to output channels
//      - There needs to be a speacial InputAudioNode which is processed first and receives the graph audio/MIDI input
//      - This then passes to the SendAudioNode
//      - Which looks like: InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//  4. Iterate each plugin, if it connects to the output add a channel mapper and then add this to the output summer
//      - InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//                        PluginAudioNode -> ChannelMappingAudioNode -^
//                        PluginAudioNode -> ChannelMappingAudioNode -^
//  5. Whilst iterating each plugin, also iterate each of its input plugins and add ChannelMappingAudioNode and SumminAudioNodes
//      -                           InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//                                                  PluginAudioNode -> ChannelMappingAudioNode -^
//     ChannelMappingAudioNode -> SummingAudioNode -^
//  6. Quickly, all plugins should have been iterated and the summind audionodes will have balanced the latency

//==============================================================================
//==============================================================================
struct InputProvider
{
    InputProvider() = default;
    InputProvider (int numChannelsToUse) : numChannels (numChannelsToUse) {}

    void setInputs (AudioNode::AudioAndMidiBuffer newBuffers)
    {
        audio = numChannels > 0 ? newBuffers.audio.getSubsetChannelBlock (0, (size_t) numChannels) : newBuffers.audio;
        midi = newBuffers.midi;
    }
    
    AudioNode::AudioAndMidiBuffer getInputs()
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
    MidiBuffer midi;

    tracktion_engine::AudioRenderContext* context = nullptr;
};


//==============================================================================
//==============================================================================
class InputAudioNode    : public AudioNode
{
public:
    InputAudioNode (std::shared_ptr<InputProvider> inputProviderToUse,
                    int numAudioChannels, bool hasMidiInput)
        : inputProvider (std::move (inputProviderToUse)),
          numChannels (numAudioChannels),
          hasMidi (hasMidiInput)
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
        props.hasAudio = numChannels > 0;
        props.hasMidi = hasMidi;
        props.numberOfChannels = numChannels;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo&) override
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
class PluginAudioNode   : public AudioNode
{
public:
    PluginAudioNode (std::unique_ptr<AudioNode> inputNode,
                     tracktion_engine::Plugin::Ptr pluginToProcess,
                     std::shared_ptr<InputProvider> contextProvider)
        : input (std::move (inputNode)),
          plugin (std::move (pluginToProcess)),
          audioRenderContextProvider (std::move (contextProvider))
    {
        jassert (input != nullptr);
        jassert (plugin != nullptr);
    }
    
    ~PluginAudioNode() override
    {
        if (isInitialised)
            plugin->baseClassDeinitialise();
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        jassert (isInitialised);
        auto props = input->getAudioNodeProperties();
        const auto latencyNumSamples = roundToInt (plugin->getLatencySeconds() * sampleRate);

        props.numberOfChannels = jmax (props.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (props.numberOfChannels));
        props.hasAudio = plugin->producesAudioWhenNoAudioInput();
        props.hasMidi  = plugin->takesMidiInput();
        props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);

        return props;
    }
    
    std::vector<AudioNode*> getDirectInputNodes() override
    {
        return { input.get() };
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
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
        auto& audioOutputBlock = outputBuffers.audio;
        jassert (inputAudioBlock.getNumChannels() == audioOutputBlock.getNumChannels());
        jassert (inputAudioBlock.getNumSamples() == audioOutputBlock.getNumSamples());
        
        // Setup audio buffers
        float* channels[32] = {};

        for (size_t i = 0; i < inputAudioBlock.getNumChannels(); ++i)
            channels[i] = inputAudioBlock.getChannelPointer (i);

        AudioBuffer<float> inputAudioBuffer (channels,
                                             (int) inputAudioBlock.getNumChannels(),
                                             (int) inputAudioBlock.getNumSamples());

        // Then MIDI buffers
        midiMessageArray.clear();

        for (MidiBuffer::Iterator iter (inputBuffers.midi);;)
        {
            MidiMessage result;
            int samplePosition = 0;

            if (! iter.getNextEvent (result, samplePosition))
                break;

            const double time = samplePosition / sampleRate;
            midiMessageArray.addMidiMessage (std::move (result), time, tracktion_engine::MidiMessageArray::notMPE);
        }

        // Then prepare the AudioRenderContext
        auto sourceContext = audioRenderContextProvider->getContext();
        tracktion_engine::AudioRenderContext rc (sourceContext);
        rc.destBuffer = &inputAudioBuffer;
        rc.bufferStartSample = 0;
        rc.bufferNumSamples = inputAudioBuffer.getNumSamples();
        rc.bufferForMidiMessages = &midiMessageArray;
        rc.midiBufferOffset = 0.0;

        // Process the plugin
        plugin->applyToBufferWithAutomation (rc);
        
        // Then copy the buffers to the outputs
        audioOutputBlock.add (inputAudioBlock);
        
        for (auto& m : midiMessageArray)
        {
            const int sampleNum = roundToInt (sampleRate * m.getTimeStamp());
            outputBuffers.midi.addEvent (m, sampleNum);
        }
    }
    
private:
    std::unique_ptr<AudioNode> input;
    tracktion_engine::Plugin::Ptr plugin;
    std::shared_ptr<InputProvider> audioRenderContextProvider;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    tracktion_engine::MidiMessageArray midiMessageArray;
};


//==============================================================================
//==============================================================================
/**
    Simple processor for an AudioNode which uses an InputProvider to pass input in to the graph.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class RackAudioNodeProcessor
{
public:
    /** Creates an RackAudioNodeProcessor to process an AudioNode with input. */
    RackAudioNodeProcessor (std::unique_ptr<AudioNode> nodeToProcess,
                            std::shared_ptr<InputProvider> inputProviderToUse,
                            bool overrideInputProvider = true)
        : node (std::move (nodeToProcess)),
          inputProvider (std::move (inputProviderToUse)),
          overrideInputs (overrideInputProvider)
    {
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRateToUse, int blockSize)
    {
        sampleRate = sampleRateToUse;
        
        // First, initiliase all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *node };
        std::function<void (AudioNode&)> visitor = [&] (AudioNode& n) { n.initialise (info); };
        visitInputs (*node, visitor);
        
        // Then find all the nodes as it might have changed after initialisation
        allNodes.clear();
        
        std::function<void (AudioNode&)> visitor2 = [&] (AudioNode& n) { allNodes.push_back (&n); };
        visitInputs (*node, visitor2);
    }

    /** Processes a block of audio and MIDI data. */
    void process (const AudioNode::ProcessContext& pc)
    {
        if (overrideInputs)
            inputProvider->setInputs (pc.buffers);

        //ddd TODO: Remove this
        float* channels[32] = {};

        for (size_t i = 0; i < pc.buffers.audio.getNumChannels(); ++i)
            channels[i] = pc.buffers.audio.getChannelPointer (i);

        AudioBuffer<float> inputAudioBuffer (channels,
                                             (int) pc.buffers.audio.getNumChannels(),
                                             (int) pc.buffers.audio.getNumSamples());

        midiMessageArray.clear();

        for (MidiBuffer::Iterator iter (pc.buffers.midi);;)
        {
            MidiMessage result;
            int samplePosition = 0;

            if (! iter.getNextEvent (result, samplePosition))
                break;

            const double time = samplePosition / sampleRate;
            midiMessageArray.addMidiMessage (std::move (result), time, tracktion_engine::MidiMessageArray::notMPE);
        }

        tracktion_engine::PlayHead playHead;
        tracktion_engine::AudioRenderContext rc (playHead, {},
                                                 &inputAudioBuffer,
                                                 juce::AudioChannelSet::canonicalChannelSet (inputAudioBuffer.getNumChannels()),
                                                 0, inputAudioBuffer.getNumSamples(),
                                                 &midiMessageArray, 0.0,
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
                    node->process (juce::Range<int64_t>::withStartAndLength (0, (int64_t) pc.buffers.audio.getNumSamples()));
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                auto output = node->getProcessedOutput();
                pc.buffers.audio.copyFrom (output.audio);
                pc.buffers.midi.addEvents (output.midi, 0, -1, 0);
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::vector<AudioNode*> allNodes;
    std::shared_ptr<InputProvider> inputProvider;
    bool overrideInputs = true;

    double sampleRate = 44100.0;
    tracktion_engine::MidiMessageArray midiMessageArray;
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
    std::vector<Connection> getSimplifiedConnections (te::RackType& rack)
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

    Array<const te::RackConnection*> getConnectionsBetween (te::RackType& rack,
                                                            te::EditItemID sourceID, te::EditItemID destID)
    {
        Array<const te::RackConnection*> connections;

        for (auto c : rack.getConnections())
            if (c->sourceID == sourceID && c->destID == destID)
                connections.add (c);
        
        return connections;
    }

    std::unique_ptr<AudioNode> createChannelMappingNodeForConnections (std::unique_ptr<AudioNode> input,
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
            return makeAudioNode<ChannelMappingAudioNode> (std::move (input), std::move (chanelMap), passMidi);
        
        return {};
    }

    //==============================================================================
    std::unique_ptr<AudioNode> createNodeForDirectConnectionsBetween (te::RackType& rack,
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
        
        return createChannelMappingNodeForConnections (makeAudioNode<InputAudioNode> (inputProvider, numChannels, hasMidi),
                                                       connections);
    }

    std::unique_ptr<AudioNode> createNodeForRackInputOutputDirectConnections (te::RackType& rack,
                                                                              std::shared_ptr<InputProvider> inputProvider)
    {
        return createNodeForDirectConnectionsBetween (rack, {}, {}, inputProvider);
    }

    //==============================================================================
    std::unique_ptr<AudioNode> createNodeForPlugin (te::RackType& rack, te::Plugin::Ptr plugin,
                                                    std::shared_ptr<InputProvider> inputProvider)
    {
        std::vector<std::unique_ptr<AudioNode>> nodes;

        const auto pluginID = plugin->itemID;
        
        // Start with any Rack inputs connected directly to this plugin
        if (auto inputNode = createNodeForDirectConnectionsBetween (rack, {}, pluginID, inputProvider))
            nodes.push_back (std::move (inputNode));
        
        // Then find any connections between plugins and this plugin
        for (auto c : getSimplifiedConnections (rack))
        {
            if (c.destID != pluginID)
                continue;

            if (auto plugin = rack.getPluginForID (c.sourceID))
            {
                auto pluginNode = createNodeForPlugin (rack, *plugin, inputProvider);
                auto mappedNode = createChannelMappingNodeForConnections (std::move (pluginNode),
                                                                          getConnectionsBetween (rack, c.sourceID, pluginID));
                nodes.push_back (std::move (mappedNode));
            }
        }

        return makeAudioNode<PluginAudioNode> (makeAudioNode<SummingAudioNode> (std::move (nodes)), *plugin, inputProvider);
    }

    std::unique_ptr<AudioNode> createRackAudioNode (te::RackType& rack,
                                                    std::shared_ptr<InputProvider> inputProvider)
    {
        std::vector<std::unique_ptr<AudioNode>> nodes;
        
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

        return makeAudioNode<SummingAudioNode> (std::move (nodes));
    }
}
