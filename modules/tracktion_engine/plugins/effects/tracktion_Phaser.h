/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

class PhaserPlugin  : public Plugin
{
public:
    PhaserPlugin (PluginCreationInfo);
    ~PhaserPlugin() override;

    //==============================================================================
    static const char* getPluginName()                      { return NEEDS_TRANS("Phaser"); }
    static const char* xmlTypeName;

    juce::String getName() override                         { return TRANS("Phaser"); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    bool needsConstantBufferSize() override                 { return false; }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override;
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> depth, rate, feedbackGain;

private:
    //==============================================================================
    double filterVals[2][8];
    double sweep = 0.0, sweepFactor = 0.0, minSweep = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserPlugin)
};

}} // namespace tracktion { inline namespace engine
