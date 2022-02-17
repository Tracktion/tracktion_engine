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
struct InputProvider
{
    InputProvider() = default;
    InputProvider (choc::buffer::ChannelCount numChannelsToUse)
        : numChannels (numChannelsToUse)
    {}

    void setInputs (tracktion::graph::Node::AudioAndMidiBuffer newBuffers)
    {
        audio = numChannels > 0 ? newBuffers.audio.getFirstChannels (numChannels)
                                : newBuffers.audio;
        tracktion::graph::sanityCheckView (audio);
        midi.copyFrom (newBuffers.midi);
    }
    
    tracktion::graph::Node::AudioAndMidiBuffer getInputs()
    {
        tracktion::graph::sanityCheckView (audio);
        return { audio, midi };
    }
    
    void setContext (tracktion_engine::PluginRenderContext* pc)
    {
        context = pc;
    }
    
    /** Returns the context currently in use.
        This is only valid for the duration of the process call.
    */
    tracktion_engine::PluginRenderContext& getContext()
    {
        jassert (context != nullptr);
        return *context;
    }
    
    choc::buffer::ChannelCount numChannels = 0;
    choc::buffer::ChannelArrayView<float> audio;
    tracktion_engine::MidiMessageArray midi;

    tracktion_engine::PluginRenderContext* context = nullptr;
};


//==============================================================================
//==============================================================================
/**
    Simple processor for a Node which uses an InputProvider to pass input in to the graph.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
template<typename NodePlayerType>
class RackNodePlayer
{
public:
    /** Creates an RackNodePlayer to process an Node with input. */
    RackNodePlayer (std::unique_ptr<tracktion::graph::Node> nodeToProcess,
                    std::shared_ptr<InputProvider> inputProviderToUse,
                    bool overrideInputProvider)
        : inputProvider (std::move (inputProviderToUse)),
          overrideInputs (overrideInputProvider)
    {
        nodePlayer.setNode (std::move (nodeToProcess));
    }

    /** Creates an RackNodePlayer to process an Node with input, sample rate and block size. */
    RackNodePlayer (std::unique_ptr<tracktion::graph::Node> nodeToProcess,
                    std::shared_ptr<InputProvider> inputProviderToUse,
                    bool overrideInputProvider,
                    double sampleRateToUse, int blockSizeToUse)
        : inputProvider (std::move (inputProviderToUse)),
          overrideInputs (overrideInputProvider)
    {
        nodePlayer.setNode (std::move (nodeToProcess), sampleRateToUse, blockSizeToUse);
    }
    
    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        jassert (sampleRate != 0.0);
        jassert (blockSize != 0);
        nodePlayer.prepareToPlay (sampleRate, blockSize);
    }
    
    int getLatencySamples()
    {
        return nodePlayer.getNode()->getNodeProperties().latencyNumSamples;
    }

    /** Processes a block of audio and MIDI data.
        Returns the number of times a node was checked but unable to be processed.
    */
    int process (const tracktion::graph::Node::ProcessContext& pc)
    {
        return process (pc, {}, true, false, false);
    }
    
    /** Processes a block of audio and MIDI data with a given PlayHead and EditTimeRange.
        This should be used when processing ExternalPlugins or they will crash when getting the playhead info.
    */
    int process (const tracktion::graph::Node::ProcessContext& pc,
                 TimePosition editTime, bool isPlaying, bool isScrubbing, bool isRendering)
    {
        if (overrideInputs)
            inputProvider->setInputs (pc.buffers);

        // The internal nodes won't be interested in the top level audio/midi inputs
        // They should only be referencing this for time and continuity
        tracktion_engine::PluginRenderContext rc (nullptr, juce::AudioChannelSet(), 0, 0,
                                                  nullptr, 0.0,
                                                  editTime, isPlaying, isScrubbing, isRendering, true);

        inputProvider->setContext (&rc);
     
        return nodePlayer.process (pc);
    }
    
private:
    NodePlayerType nodePlayer;
    
    std::shared_ptr<InputProvider> inputProvider;
    bool overrideInputs = true;
};


//==============================================================================
//==============================================================================
namespace RackNodeBuilder
{
    enum class Algorithm
    {
        remappingNode, /**< An algorithm using ChannelRemappingNodes. */
        connectedNode  /**< An algorithm using ConnectedNodes. Should use less Nodes but is less well tested. */
    };

    //==============================================================================
    /** Creates a Node for processing a Rack.
        The InputProvider must be used for providing audio and MIDI input to the Rack.
    */
    std::unique_ptr<tracktion::graph::Node> createRackNode (Algorithm,
                                                           tracktion_engine::RackType&,
                                                           double sampleRate, int blockSize,
                                                           std::shared_ptr<InputProvider>,
                                                           tracktion::graph::PlayHeadState* playHeadState = nullptr,
                                                           bool isRendering = true);

    //==============================================================================
    /** Creates a Node for processing a Rack where the input comes from a Node. */
    std::unique_ptr<tracktion::graph::Node> createRackNode (Algorithm,
                                                           tracktion_engine::RackType&,
                                                           double sampleRate, int blockSize,
                                                           std::unique_ptr<tracktion::graph::Node>,
                                                           tracktion::graph::PlayHeadState&, bool isRendering);
}

}} // namespace tracktion { inline namespace engine
