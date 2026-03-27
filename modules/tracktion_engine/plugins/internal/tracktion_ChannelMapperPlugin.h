/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
/** A plugin that applies an explicit channel mapping.

    This allows users to remap, duplicate, or sum audio channels on a track.
    The mapping is stored as a serialized string of source->dest pairs in the
    plugin's ValueTree state.
*/
class ChannelMapperPlugin  : public Plugin
{
public:
    ChannelMapperPlugin (PluginCreationInfo);
    ~ChannelMapperPlugin() override;

    //==============================================================================
    static const char* getPluginName()              { return NEEDS_TRANS("Channel Mapper"); }
    static const char* xmlTypeName;
    static juce::ValueTree create();

    juce::String getName() const override                                      { return TRANS("Channel Mapper"); }
    juce::String getPluginType() override                                      { return xmlTypeName; }
    juce::String getShortName (int) override                                   { return "ChMap"; }
    juce::String getSelectableDescription() override                           { return TRANS("Channel Mapper Plugin"); }
    bool takesAudioInput() override                                            { return true; }
    bool takesMidiInput() override                                             { return true; }
    bool canBeAddedToClip() override                                           { return false; }
    bool producesAudioWhenNoAudioInput() override                              { return false; }
    ChannelConfiguration getMainBusInputChannelConfiguration() const override  { return {}; }
    ChannelConfiguration getMainBusOutputChannelConfiguration() const override { return {}; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override;

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    //==============================================================================
    /** Returns the current channel map. */
    ChannelMap getChannelMap() const;

    /** Sets a new channel map and stores it in the plugin state. */
    void setChannelMap (const ChannelMap&);

    /** Returns the number of input channels configured. */
    int getNumInputChannels() const                 { return numInputChannels; }

    /** Returns the number of output channels configured. */
    int getNumOutputChannels() const                { return numOutputChannels; }

    /** Sets the number of input channels. */
    void setNumInputChannels (int);

    /** Sets the number of output channels. */
    void setNumOutputChannels (int);

private:
    //==============================================================================
    juce::CachedValue<juce::String> channelMapState;
    juce::CachedValue<int> numInputChannels, numOutputChannels;
    ChannelMap currentMap;

    void updateMapFromState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelMapperPlugin)
};

} // namespace tracktion::inline engine
