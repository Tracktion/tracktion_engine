/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class TextPlugin   : public Plugin
{
public:
    TextPlugin (PluginCreationInfo);
    ~TextPlugin();

    static const char* getPluginName()                  { return NEEDS_TRANS("Text"); }
    static juce::ValueTree create();

    //==============================================================================
    static const char* xmlTypeName;

    bool canBeAddedToFolderTrack() override             { return true; }
    juce::String getName() override                     { return textTitle.get().isNotEmpty() ? textTitle : TRANS("Text Plugin"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    void initialise (const PlaybackInitialisationInfo&) override {}
    void deinitialise() override                        {}
    void applyToBuffer (const AudioRenderContext&) override {}
    int getNumOutputChannelsGivenInputs (int numInputChannels) override     { return numInputChannels; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    bool needsConstantBufferSize() override             { return false; }
    juce::String getSelectableDescription() override    { return TRANS("Text Plugin"); }

    juce::CachedValue<juce::String> textTitle, textBody;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextPlugin)
};

} // namespace tracktion_engine
