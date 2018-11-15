/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class LevelMeterPlugin  : public Plugin,
                          private juce::Timer
{
public:
    LevelMeterPlugin (PluginCreationInfo);
    ~LevelMeterPlugin();

    static juce::ValueTree create();

    //==============================================================================
    static const char* getPluginName()              { return NEEDS_TRANS("Level Meter"); }
    static const char* xmlTypeName;

    juce::String getName() override                 { return TRANS("Level Meter"); }
    juce::String getPluginType() override           { return xmlTypeName; }
    juce::String getShortName (int) override        { return "Meter"; }
    juce::String getTooltip() override              { return TRANS("Level meter plugin") + "$levelmeterplugin"; }
    bool canBeDisabled() override                   { return false; }
    bool needsConstantBufferSize() override         { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return juce::jmin (numInputChannels, 2); }

    juce::String getSelectableDescription() override                        { return TRANS("Level Meter Plugin"); }
    void initialise (const PlaybackInitialisationInfo&) override;
    void initialiseWithoutStopping (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;
    void timerCallback() override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    //==============================================================================
    LevelMeasurer measurer;

    juce::CachedValue<bool> showMidiActivity;

private:
    int controllerTrack = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterPlugin)
};

} // namespace tracktion_engine
