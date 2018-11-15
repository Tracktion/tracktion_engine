/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MidiModifierPlugin   : public Plugin
{
public:
    MidiModifierPlugin (PluginCreationInfo);
    ~MidiModifierPlugin();

    //==============================================================================
    juce::CachedValue<float> semitonesValue;
    AutomatableParameter::Ptr semitones;

    //==============================================================================
    static float getMaximumSemitones()      { return 3.0f * 12.0f; }

    static const char* getPluginName()      { return NEEDS_TRANS("MIDI Modifier"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override;
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    double getLatencySeconds() override;
    int getNumOutputChannelsGivenInputs (int) override;
    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    bool takesAudioInput() override;
    bool canBeAddedToClip() override;
    bool needsConstantBufferSize() override;

    void applyToBuffer (const AudioRenderContext&) override;

    juce::String getSelectableDescription() override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiModifierPlugin)
};

} // namespace tracktion_engine
