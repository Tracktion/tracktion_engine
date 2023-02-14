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

class TrackMuteState;

/**
    Node for processing a Modifier.
*/
class ModifierNode final    : public tracktion::graph::Node
{
public:
    //==============================================================================
    /** Creates a ModifierNode to process a Modifier.
        @param const TrackMuteState*    The optional TrackMuteState to use
        @param PlayHeadState            The PlayHeadState to monitor for jumps
        @param rendering                Should be true if this is an offline render
     
    */
    ModifierNode (std::unique_ptr<Node> input,
                  tracktion::engine::Modifier::Ptr,
                  double sampleRateToUse, int blockSizeToUse,
                  const TrackMuteState*,
                  tracktion::graph::PlayHeadState&, bool rendering);

    /** Creates a ModifierNode to process a plugin on in a Rack with an InputProvider.
        @param InputProvider            The InputProvider to provide inputs and a PluginRenderContext
     
    */
    ModifierNode (std::unique_ptr<Node> input,
                  tracktion::engine::Modifier::Ptr,
                  double sampleRateToUse, int blockSizeToUse,
                  std::shared_ptr<InputProvider>);

    /** Destructor. */
    ~ModifierNode() override;
    
    //==============================================================================
    Modifier& getModifier()                             { return *modifier; }

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override   { return { input.get() }; }
    bool isReadyToProcess() override                    { return input->hasProcessed(); }
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    void process (ProcessContext&) override;
    
private:
    //==============================================================================
    std::unique_ptr<Node> input;
    tracktion::engine::Modifier::Ptr modifier;
    std::shared_ptr<InputProvider> audioRenderContextProvider;
    
    const TrackMuteState* trackMuteState = nullptr;
    tracktion::graph::PlayHeadState* playHeadState = nullptr;
    bool isRendering = false;
    
    bool isInitialised = false;
    double sampleRate = 44100.0;
    tracktion::engine::MidiMessageArray midiMessageArray;
    TimeDuration automationAdjustmentTime;

    //==============================================================================
    void initialiseModifier (double sampleRateToUse, int blockSizeToUse);
    PluginRenderContext getPluginRenderContext (juce::Range<int64_t>, juce::AudioBuffer<float>&);
};

}} // namespace tracktion { inline namespace engine
