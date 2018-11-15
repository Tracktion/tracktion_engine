/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class InsertPlugin  : public Plugin
{
public:
    enum DeviceType
    {
        noDevice,
        audioDevice,
        midiDevice
    };

    InsertPlugin (PluginCreationInfo);
    ~InsertPlugin();

    AudioNode* createSendAudioNode (OutputDevice&);

    //==============================================================================
    static const char* getPluginName()          { return NEEDS_TRANS("Insert"); }
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override;
    double getLatencySeconds() override;
    int getLatencySamples() override;
    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    bool takesAudioInput() override;
    bool takesMidiInput() override;
    bool canBeAddedToClip() override;
    bool needsConstantBufferSize() override;

    void initialise (const PlaybackInitialisationInfo&) override;
    void initialiseWithoutStopping (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;
    juce::String getSelectableDescription() override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    DeviceType getSendDeviceType() const        { return sendDeviceType; }
    DeviceType getReturnDeviceType() const      { return returnDeviceType; }

    juce::CachedValue<juce::String> name, inputDevice, outputDevice;
    juce::CachedValue<double> manualAdjustMs;

    void updateDeviceTypes();
    void showLatencyTester();

    static void getPossibleDeviceNames (Engine&,
                                        juce::StringArray& devices,
                                        juce::StringArray& aliases,
                                        juce::BigInteger& hasAudio,
                                        juce::BigInteger& hasMidi,
                                        bool forInput);

private:
    //==============================================================================
    juce::AudioBuffer<float> sendBuffer { 2, 32 }, returnBuffer;
    MidiMessageArray sendMidiBuffer, returnMidiBuffer;

    double latencySeconds = 0.0;
    int latencySamples = 0;
    DeviceType sendDeviceType = noDevice, returnDeviceType = noDevice;

    bool hasAudio() const;
    bool hasMidi() const;

    void fillSendBuffer (const AudioRenderContext&);
    void fillReturnBuffer (const AudioRenderContext&);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    class InsertAudioNode;
    class InsertReturnAudioNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertPlugin)
};

} // namespace tracktion_engine
