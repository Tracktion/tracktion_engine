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
    The built-in Tracktion volume/pan plugin.
*/
class VolumeAndPanPlugin  : public Plugin
{
public:
    VolumeAndPanPlugin (Edit&, const juce::ValueTree&, bool isMasterVolumeNode);
    VolumeAndPanPlugin (PluginCreationInfo, bool isMasterVolumeNode = false);
    ~VolumeAndPanPlugin() override;

    using Ptr = juce::ReferenceCountedObjectPtr<VolumeAndPanPlugin>;

    static const char* getPluginName()                      { return NEEDS_TRANS("Volume and Pan"); }
    static juce::ValueTree create();

    bool isMasterVolAndPan()                                { return isMasterVolume; }
    bool canBeAddedToRack() override                        { return ! isMasterVolume; }

    //==============================================================================
    float getVolumeDb() const;
    float getSliderPos() const                              { return volParam->getCurrentValue(); }
    void setVolumeDb (float vol);
    void setSliderPos (float pos);

    float getPan() const                                    { return panParam->getCurrentValue(); }
    void setPan (float pan);

    void setPanLaw (PanLaw newPanLaw)                       { panLaw = (int) newPanLaw; }
    PanLaw getPanLaw() const noexcept;

    void setAppliedToMidiVolumes (bool);
    bool isAppliedToMidiVolumes() const                     { return applyToMidi; }

    void muteOrUnmute();

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() override                         { return TRANS("Volume & Pan Plugin"); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getShortName (int) override                { return "VolPan"; }
    juce::String getSelectableDescription() override        { return getName(); }
    bool needsConstantBufferSize() override                 { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void initialiseWithoutStopping (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;
    int getNumOutputChannelsGivenInputs (int numInputs) override    { return juce::jmax (2, numInputs); }
    bool canBeMoved() override                              { return ! isMasterVolume; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    //==============================================================================
    juce::CachedValue<float> volume, pan;
    juce::CachedValue<bool> applyToMidi, ignoreVca, polarity;
    juce::CachedValue<int> panLaw;

    // NB the units used here are slider position
    AutomatableParameter::Ptr volParam, panParam;

private:
    float lastGainL = 0.0f, lastGainR = 0.0f, lastGainS = 0.0f, lastVolumeBeforeMute = 0.0f;

    juce::ReferenceCountedObjectPtr<AudioTrack> vcaTrack;
    const bool isMasterVolume = false;

    void refreshVCATrack();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeAndPanPlugin)
};

}} // namespace tracktion { inline namespace engine
