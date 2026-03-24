/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** A (virtual) audio output device.

    There'll be one or more instances of these, each one representing a group of
    channels from a physical device.
*/
class WaveOutputDevice  : public OutputDevice
{
public:
    WaveOutputDevice (Engine&, const WaveDeviceDescription&);
    ~WaveOutputDevice() override;

    void resetToDefault();
    juce::String getDeviceTypeDescription() const override          { return NEEDS_TRANS("Wave Audio Output"); }
    void setEnabled (bool) override;
    const ChannelConfiguration& getChannels() const noexcept        { return deviceDescription.channels; }
    const juce::AudioChannelSet& getChannelSet() const noexcept     { return channelSet; }

    void reverseChannels (bool);
    bool isReversed() const         { return leftRightReversed; }

    void setDithered (bool);
    bool isDithered() const         { return ditheringEnabled; }

    /** Returns the first channel's device index.
        N.B. this reflects any channel reversal that has been applied.
    */
    int getLeftChannel() const;

    /** Returns the second channel's device index.
        N.B. this reflects any channel reversal that has been applied.
    */
    int getRightChannel() const;

    bool isStereoPair() const;
    void setStereoPair (bool);
    juce::PopupMenu createChannelGroupMenu (bool includeSetAllChannelsOptions);

    WaveOutputDeviceInstance* createInstance (EditPlaybackContext&);

    WaveDeviceDescription deviceDescription;

protected:
    juce::String openDevice() override;
    void closeDevice() override;

private:
    friend class DeviceManager;
    friend class WaveOutputDeviceInstance;

    ChannelConfiguration originalChannels;
    juce::AudioChannelSet channelSet;
    bool ditheringEnabled, leftRightReversed;

    void loadProps();
    void saveProps();
    void applyChannelOrdering();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveOutputDevice)
};

//==============================================================================
class WaveOutputDeviceInstance   : public OutputDeviceInstance
{
public:
    WaveOutputDeviceInstance (WaveOutputDevice&, EditPlaybackContext&);

    void prepareToPlay (double sampleRate, int blockSizeSamples);

protected:
    std::vector<Ditherer> ditherers;
    MidiMessageArray midiBuffer;
    juce::AudioBuffer<float> outputBuffer;

    WaveOutputDevice& getWaveOutput() const     { return static_cast<WaveOutputDevice&> (owner); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveOutputDeviceInstance)
};

} // namespace tracktion::inline engine
