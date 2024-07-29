/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
    ~InsertPlugin() override;

    //==============================================================================
    static const char* getPluginName()          { return NEEDS_TRANS("Insert"); }
    static const char* xmlTypeName;
    static juce::ValueTree create();

    juce::String getName() const override;
    juce::String getPluginType() override;
    juce::String getShortName (int) override;
    double getLatencySeconds() override;
    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    bool takesAudioInput() override;
    bool takesMidiInput() override;
    bool canBeAddedToClip() override;
    bool needsConstantBufferSize() override;

    void initialise (const PluginInitialisationInfo&) override;
    void initialiseWithoutStopping (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    DeviceType getSendDeviceType() const        { return sendDeviceType; }
    DeviceType getReturnDeviceType() const      { return returnDeviceType; }

    juce::CachedValue<juce::String> name, inputDevice, outputDevice;
    juce::CachedValue<double> manualAdjustMs;

    void updateDeviceTypes();

    /** Returns true if either the send or return types are audio. */
    bool hasAudio() const;

    /** Returns true if either the send or return types are MIDI. */
    bool hasMidi() const;

    static void getPossibleDeviceNames (Engine&,
                                        juce::StringArray& devices,
                                        juce::StringArray& aliases,
                                        juce::BigInteger& hasAudio,
                                        juce::BigInteger& hasMidi,
                                        bool forInput);

    /** @internal */
    int getLatencyNumSamples() const;

private:
    //==============================================================================
    std::atomic<int> latencyNumSamples { 0 };
    std::atomic<double> latencySeconds { 0.0 };
    DeviceType sendDeviceType = noDevice, returnDeviceType = noDevice;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertPlugin)
};

}} // namespace tracktion { inline namespace engine
