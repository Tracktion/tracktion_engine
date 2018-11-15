/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class CompressorPlugin : public Plugin
{
public:
    CompressorPlugin (PluginCreationInfo);
    ~CompressorPlugin();

    //==============================================================================
    float getThreshold() const;
    void setThreshold (float);
    float getRatio() const;
    void setRatio (float);

    //==============================================================================
    static const char* getPluginName()      { return NEEDS_TRANS("Compressor/Limiter"); }
    static const char* xmlTypeName;

    juce::String getName() override                                     { return TRANS("Compressor"); }
    juce::String getPluginType() override                               { return xmlTypeName; }
    juce::String getShortName (int) override                            { return TRANS("Comp"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    bool needsConstantBufferSize() override                             { return false; }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

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

} // namespace tracktion_engine
