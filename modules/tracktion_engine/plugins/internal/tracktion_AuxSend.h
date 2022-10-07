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

class AuxSendPlugin   : public Plugin
{
public:
    AuxSendPlugin (PluginCreationInfo);
    ~AuxSendPlugin() override;

    //==============================================================================
    void setGainDb (float newDb);
    float getGainDb() const                 { return volumeFaderPositionToDB (gain->getCurrentValue()); }

    void setMute (bool m);
    bool isMute();

    int getBusNumber() const                { return busNumber; }
    juce::String getBusName();

    static juce::StringArray getBusNames (Edit&, int maxNumBusses);
    static juce::String getDefaultBusName (int busIndex);

    //==============================================================================
    static const char* getPluginName()              { return NEEDS_TRANS("Aux Send"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getShortName (int suggestedMaxLength) override;
    juce::String getPluginType() override           { return xmlTypeName; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PluginInitialisationInfo&) override;
    void initialiseWithoutStopping (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

    juce::String getSelectableDescription() override { return TRANS("Aux Send Plugin"); }

    bool takesAudioInput() override                  { return true; }
    bool canBeAddedToClip() override                 { return false; }
    bool canBeAddedToRack() override                 { return false; }
    bool needsConstantBufferSize() override          { return true; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<int> busNumber;
    juce::CachedValue<float> gainLevel;
    juce::CachedValue<bool> invertPhase;

    AutomatableParameter::Ptr gain;

private:
    bool shouldProcess();
    //==============================================================================
    juce::CachedValue<float> lastVolumeBeforeMute;
    float lastGain = 1.0f;
    Track* ownerTrack = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxSendPlugin)
};

}} // namespace tracktion { inline namespace engine
