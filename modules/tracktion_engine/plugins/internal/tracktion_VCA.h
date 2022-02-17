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

/**
    The VCA plugin sits on a folder track to control the overall level of all the volume/pan
    plugins in its child tracks.
*/
class VCAPlugin   : public Plugin
{
public:
    VCAPlugin (PluginCreationInfo);
    ~VCAPlugin() override;

    static juce::ValueTree create();

    //==============================================================================
    float getSliderPos() const;
    void setVolumeDb (float dB);
    float getVolumeDb() const;
    void setSliderPos (float position);
    void muteOrUnmute();

    float updateAutomationStreamAndGetVolumeDb (TimePosition);

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("VCA"); }
    juce::String getSelectableDescription() override    { return getName(); }
    juce::String getPluginType() override               { return xmlTypeName; }
    bool canBeAddedToClip() override                    { return false; }
    bool canBeAddedToRack() override                    { return false; }
    bool canBeAddedToFolderTrack()   override           { return true;  }
    bool canBeMoved() override;
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<float> volumeValue;
    AutomatableParameter::Ptr volParam;

private:
    float lastVolumeBeforeMute = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCAPlugin)
};

}} // namespace tracktion { inline namespace engine
