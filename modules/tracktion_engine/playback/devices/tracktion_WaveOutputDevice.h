/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** A (virtual) audio output device.

    There'll be multiple instances of these, representing mono or stereo pairs of
    output channels.
*/
class WaveOutputDevice  : public OutputDevice
{
public:
    WaveOutputDevice (Engine&, const juce::String& name, const std::vector<ChannelIndex>& channels);
    ~WaveOutputDevice();

    void resetToDefault();
    void setEnabled (bool) override;
    const std::vector<ChannelIndex>& getChannels() const noexcept   { return deviceChannels; }
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

    /** Returns true if the output is a stereo pair. I.e. has two channels. */
    bool isStereoPair() const;

    WaveOutputDeviceInstance* createInstance (EditPlaybackContext&);

protected:
    juce::String openDevice() override;
    void closeDevice() override;

private:
    friend class DeviceManager;
    friend class WaveOutputDeviceInstance;

    const std::vector<ChannelIndex> deviceChannels;
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
    void fillNextAudioBlock (PlayHead&, EditTimeRange streamTime,
                             float** allChannels, int numSamples);

protected:
    Ditherer ditherers[2];
    MidiMessageArray midiBuffer;
    juce::AudioBuffer<float> outputBuffer;

    WaveOutputDevice& getWaveOutput() const     { return static_cast<WaveOutputDevice&> (owner); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveOutputDeviceInstance)
};

} // namespace tracktion_engine
