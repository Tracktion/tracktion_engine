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

struct InputProvider;
class TrackMuteState;

/**
    Node for processing a plugin.
*/
class PluginNode final  : public tracktion_graph::Node
{
public:
    //==============================================================================
    /** Creates a PluginNode to process a plugin on in a Rack with an InputProvider.
        @param InputProvider            The InputProvider to provide inputs and a PluginRenderContext
     
    */
    PluginNode (std::unique_ptr<Node> input,
                tracktion_engine::Plugin::Ptr,
                double sampleRateToUse, int blockSizeToUse,
                std::shared_ptr<InputProvider>);

    /** Creates a PluginNode to process a plugin on a Track.
        @param const TrackMuteState*    The optional TrackMuteState to use
        @param PlayHeadState            The PlayHeadState to monitor for jumps
        @param rendering                Should be true if this is an offline render
     
    */
    PluginNode (std::unique_ptr<Node> input,
                tracktion_engine::Plugin::Ptr,
                double sampleRateToUse, int blockSizeToUse,
                const TrackMuteState*,
                tracktion_graph::PlayHeadState&, bool rendering);

    /** Destructor. */
    ~PluginNode() override;
    
    //==============================================================================
    Plugin& getPlugin()                                 { return *plugin; }
    
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override   { return { input.get() }; }
    bool isReadyToProcess() override                    { return input->hasProcessed(); }
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    void prefetchBlock (juce::Range<int64_t>) override;
    void process (ProcessContext&) override;
    
private:
    //==============================================================================
    std::unique_ptr<Node> input;
    tracktion_engine::Plugin::Ptr plugin;
    std::shared_ptr<InputProvider> audioRenderContextProvider;

    const TrackMuteState* trackMuteState = nullptr;
    tracktion_graph::PlayHeadState* playHeadState = nullptr;
    bool isRendering = false;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    int latencyNumSamples = 0;
    tracktion_engine::MidiMessageArray midiMessageArray;
    int subBlockSizeToUse = -1;
    bool canProcessBypassed = false;
    double automationAdjustmentTime = 0.0;
    
    std::shared_ptr<tracktion_graph::LatencyProcessor> latencyProcessor;

    //==============================================================================
    void initialisePlugin (double sampleRateToUse, int blockSizeToUse);
    PluginRenderContext getPluginRenderContext (int64_t, juce::AudioBuffer<float>&);
    void replaceLatencyProcessorIfPossible (Node*);
};

}
