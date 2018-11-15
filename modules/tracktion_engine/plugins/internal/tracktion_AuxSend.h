/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AuxSendPlugin   : public Plugin
{
public:
    AuxSendPlugin (PluginCreationInfo);
    ~AuxSendPlugin();

    //==============================================================================
    void setGainDb (float newDb);
    float getGainDb() const                 { return volumeFaderPositionToDB (gain->getCurrentValue()); }

    void setMute (bool m);
    bool isMute();

    int getBusNumber() const                { return busNumber; }
    juce::String getBusName();

    enum { maxNumBusses = 32 };
    static juce::StringArray getBusNames (Edit&);
    static juce::String getDefaultBusName (int busIndex);

    //==============================================================================
    static const char* getPluginName()              { return NEEDS_TRANS("Aux Send"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getShortName (int suggestedMaxLength) override;
    juce::String getPluginType() override           { return xmlTypeName; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PlaybackInitialisationInfo&) override;
    void initialiseWithoutStopping (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    juce::String getSelectableDescription() override { return TRANS("Aux Send Plugin"); }

    bool takesAudioInput() override                  { return true; }
    bool canBeAddedToClip() override                 { return false; }
    bool canBeAddedToRack() override                 { return false; }
    double getLatencySeconds() override              { return latencySeconds; }
    bool needsConstantBufferSize() override          { return true; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    juce::CachedValue<int> busNumber;
    juce::CachedValue<float> gainLevel;

    AutomatableParameter::Ptr gain;

private:
    //==============================================================================
    juce::Array<AuxReturnPlugin*> allAuxReturns;
    float lastVolumeBeforeMute = 0.0f, lastGain = 1.0f;
    juce::AudioBuffer<float> delayBuffer { 2, 32 };
    double latencySeconds = 0.0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxSendPlugin)
};

} // namespace tracktion_engine
