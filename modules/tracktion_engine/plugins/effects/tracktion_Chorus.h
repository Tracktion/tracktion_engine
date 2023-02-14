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

class ChorusPlugin  : public Plugin
{
public:
    ChorusPlugin (PluginCreationInfo);
    ~ChorusPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Chorus"); }
    static const char* xmlTypeName;

    juce::String getName() const override               { return TRANS("Chorus"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return getName(); }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override    { return TRANS("Chorus Plugin"); }
    bool needsConstantBufferSize() override             { return false; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> depthMs, width, mixProportion, speedHz;

private:
    //==============================================================================
    DelayBufferBase delayBuffer;
    float phase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusPlugin)
};

}} // namespace tracktion { inline namespace engine
