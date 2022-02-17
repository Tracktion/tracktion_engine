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

class LowPassPlugin  : public Plugin
{
public:
    LowPassPlugin (PluginCreationInfo);
    ~LowPassPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Low/High-Pass Filter"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return "LPF/HPF"; }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "HP/LP"; }
    juce::String getSelectableDescription() override    { return TRANS("Low/High-Pass Filter"); }
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;

    int getNumOutputChannelsGivenInputs (int numInputChannels) override  { return juce::jmin (numInputChannels, 2); }
    void applyToBuffer (const PluginRenderContext&) override;

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

}} // namespace tracktion { inline namespace engine
