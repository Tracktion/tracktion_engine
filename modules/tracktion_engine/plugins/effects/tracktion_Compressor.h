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

class CompressorPlugin : public Plugin
{
public:
    CompressorPlugin (PluginCreationInfo);
    ~CompressorPlugin() override;

    //==============================================================================
    float getThreshold() const;
    void setThreshold (float);
    float getRatio() const;
    void setRatio (float);

    //==============================================================================
    static const char* getPluginName()      { return NEEDS_TRANS("Compressor/Limiter"); }
    static const char* xmlTypeName;

    juce::String getName() const override                               { return TRANS("Compressor"); }
    juce::String getPluginType() override                               { return xmlTypeName; }
    juce::String getShortName (int) override                            { return TRANS("Comp"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    bool needsConstantBufferSize() override                             { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    juce::String getSelectableDescription() override                    { return TRANS("Compressor/Limiter Plugin"); }

    juce::CachedValue<float> thresholdValue, ratioValue, attackValue,
                             releaseValue, outputValue, sidechainValue;
    juce::CachedValue<bool> useSidechainTrigger;
    AutomatableParameter::Ptr thresholdGain, ratio, attackMs,
                              releaseMs, outputDb, sidechainDb;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    static float getMinThreshold()      { return 0.01f; }
    static float getMaxThreshold()      { return 1.0f; }

private:
    double currentLevel = 0.0;
    float lastSamp = 0.0f;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorPlugin)
};

}} // namespace tracktion { inline namespace engine
