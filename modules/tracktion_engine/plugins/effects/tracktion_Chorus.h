/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class ChorusPlugin  : public Plugin
{
public:
    ChorusPlugin (PluginCreationInfo);
    ~ChorusPlugin();

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Chorus"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("Chorus"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return getName(); }

    int getLatencySamples() override                    { return latencySamples; }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;
    juce::String getSelectableDescription() override    { return TRANS("Chorus Plugin"); }
    bool needsConstantBufferSize() override             { return false; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> depthMs, width, mixProportion, speedHz;

private:
    //==============================================================================
    DelayBufferBase delayBuffer;
    float phase = 0;

    int latencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusPlugin)
};

} // namespace tracktion_engine
