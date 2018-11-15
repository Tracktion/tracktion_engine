/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class LowPassPlugin  : public Plugin
{
public:
    LowPassPlugin (PluginCreationInfo);
    ~LowPassPlugin();

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Low/High-Pass Filter"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return "LPF/HPF"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "HP/LP"; }
    juce::String getSelectableDescription() override    { return TRANS("Low/High-Pass Filter"); }
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;

    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const AudioRenderContext&) override;

    bool isLowPass() const noexcept                     { return mode.get() != "highpass"; }

    juce::CachedValue<float> frequencyValue;
    juce::CachedValue<juce::String> mode;
    AutomatableParameter::Ptr frequency;

private:
    juce::IIRFilter filter[2];
    float currentFilterFreq = 0;
    bool isCurrentlyLowPass = false;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowPassPlugin)
};

} // namespace tracktion_engine
