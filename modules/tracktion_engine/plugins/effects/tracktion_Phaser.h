/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class PhaserPlugin  : public Plugin
{
public:
    PhaserPlugin (PluginCreationInfo);
    ~PhaserPlugin();

    //==============================================================================
    static const char* getPluginName()                      { return NEEDS_TRANS("Phaser"); }
    static const char* xmlTypeName;

    juce::String getName() override                         { return TRANS("Phaser"); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    bool needsConstantBufferSize() override                 { return false; }
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const AudioRenderContext&) override;
    juce::String getSelectableDescription() override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> depth, rate, feedbackGain;

private:
    //==============================================================================
    double filterVals[2][8];
    double sweep, sweepFactor, minSweep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserPlugin)
};

} // namespace tracktion_engine
