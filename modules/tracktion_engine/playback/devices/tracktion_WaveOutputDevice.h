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
    const std::vector<ChannelIndex>& getChannels() const noexcept   { return deviceDescription.channels; }
    const juce::AudioChannelSet& getChannelSet() const noexcept     { return channelSet; }

    void reverseChannels (bool);
    bool isReversed() const         { return leftRightReversed; }

    void setDithered (bool);
    bool isDithered() const         { return ditheringEnabled; }

    /** Returns the left channel index.
        N.B. this doesn't take into account if the channels are reversed as the reversing
        will be applied during the rendering stage.
    */
    int getLeftChannel() const;

    /** Returns the right channel index.
        N.B. this doesn't take into account if the channels are reversed as the reversing
        will be applied during the rendering stage.
    */
    int getRightChannel() const;

    bool isStereoPair() const;
    void setStereoPair (bool);
    juce::PopupMenu createChannelGroupMenu (bool includeSetAllChannelsOptions);

    WaveOutputDeviceInstance* createInstance (EditPlaybackContext&);

    const WaveDeviceDescription deviceDescription;

protected:
    juce::String openDevice() override;
    void closeDevice() override;

private:
    friend class DeviceManager;
    friend class WaveOutputDeviceInstance;

    const juce::AudioChannelSet channelSet;
    bool ditheringEnabled, leftRightReversed;

    void loadProps();
    void saveProps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveOutputDevice)
};

//==============================================================================
class WaveOutputDeviceInstance   : public OutputDeviceInstance
{
public:
    WaveOutputDeviceInstance (WaveOutputDevice&, EditPlaybackContext&);

    void prepareToPlay (double sampleRate, int blockSizeSamples);

protected:
    Ditherer ditherers[2];
    MidiMessageArray midiBuffer;
    juce::AudioBuffer<float> outputBuffer;

    WaveOutputDevice& getWaveOutput() const     { return static_cast<WaveOutputDevice&> (owner); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveOutputDeviceInstance)
};

}} // namespace tracktion { inline namespace engine
