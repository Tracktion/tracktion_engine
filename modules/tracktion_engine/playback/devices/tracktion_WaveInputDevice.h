/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** A (virtual) audio input device.

    There'll be one or more instances of these, each one representing a group of
    channels from a physical device.
*/
class WaveInputDevice   : public InputDevice
{
public:
    //==============================================================================
    WaveInputDevice (Engine&, const WaveDeviceDescription&, DeviceType);
    ~WaveInputDevice() override;

    static juce::StringArray getMergeModes();
    juce::StringArray getRecordFormatNames();

    void resetToDefault();
    void setEnabled (bool) override;

    DeviceType getDeviceType() const override                   { return deviceType; }
    juce::String getDeviceTypeDescription() const override;

    InputDeviceInstance* createInstance (EditPlaybackContext&) override;

    //==============================================================================
    void setRecordAdjustment (TimeDuration);
    TimeDuration getRecordAdjustment() const                    { return TimeDuration::fromSeconds (recordAdjustMs / 1000.0); }
    void setRecordAdjustmentMs (double ms);
    double getRecordAdjustmentMs() const                        { return recordAdjustMs; }
    /** @deprecated Use getChannels().getNumChannels() == 2 instead. */
    [[deprecated ("Use getChannels().getNumChannels() == 2 instead")]]
    bool isStereoPair() const;

    /** @deprecated Use setChannelConfiguration() instead. */
    [[deprecated ("Use setChannelConfiguration() instead")]]
    void setStereoPair (bool);
    juce::PopupMenu createChannelGroupMenu (bool includeSetAllChannelsOptions);
    juce::Array<int> getAvailableBitDepths() const;
    void setBitDepth (int);
    int getBitDepth() const                                     { return bitDepth; }
    void checkBitDepth();
    void setInputGainDb (float gain);
    float getInputGainDb() const                                { return inputGainDb; }
    void setRecordTriggerDb (float);
    float getRecordTriggerDb() const                            { return recordTriggerDb; }

    /** Closes or opens the input meter gate. When closed, the level meter is
        suppressed until the input level exceeds the Record Trigger Level, at
        which point the gate opens automatically and stays open until closed
        again. This lets the user audition the trigger threshold without
        actually recording. Also closed automatically when recording starts on
        a context whose trigger level is meaningful. */
    void setMeterGateClosed (bool);
    bool isMeterGateClosed() const noexcept                     { return meterGateClosed.load (std::memory_order_relaxed); }

    /** True if any of this device's instances currently has an active
        recording context. Safe to call from the message thread. */
    bool isAnyRecordingActive() const;

    juce::String getFilenameMask() const;
    void setFilenameMask (const juce::String&);
    void setFilenameMaskToDefault();

    void setOutputFormat (const juce::String&);
    juce::String getOutputFormat() const                        { return outputFormat; }
    void setMergeMode (const juce::String&);
    juce::String getMergeMode() const;

    const ChannelConfiguration& getChannels() const noexcept        { return deviceDescription.channels; }
    const juce::AudioChannelSet& getChannelSet() const noexcept     { return channelSet; }

    /** Updates the channel configuration for track devices.
        Call this before recording to match the source track's channel layout. */
    void setChannelConfiguration (const ChannelConfiguration&);

    //==============================================================================
    void masterTimeUpdate (double) override {}
    void consumeNextAudioBlock (const float* const* allChannels, int numChannels, int numSamples, double streamTime);

    RetrospectiveRecordBuffer* getRetrospectiveRecordBuffer()   { return retrospectiveBuffer.get(); }
    void updateRetrospectiveBufferLength (double length) override;

    //==============================================================================
    WaveDeviceDescription deviceDescription;

protected:
    juce::String openDevice();
    void closeDevice();

private:
    friend class DeviceManager;
    friend class WaveInputDeviceInstance;

    const DeviceType deviceType;
    juce::AudioChannelSet channelSet;

    juce::String filenameMask;
    float inputGainDb = 0;
    juce::String outputFormat;
    int bitDepth = 0, mergeMode = 0;
    float recordTriggerDb = 0;
    double recordAdjustMs = 0;
    std::atomic<bool> meterGateClosed { false };
    std::unique_ptr<RetrospectiveRecordBuffer> retrospectiveBuffer;

    void feedLevelMeasurerThroughGate (juce::AudioBuffer<float>&, int start, int numSamples);

    void loadProps();
    void saveProps() override;
    juce::AudioFormat* getFormatToUse() const;

    juce::Array<WaveInputDeviceInstance*> instances;
    juce::CriticalSection instanceLock;

    void addInstance (WaveInputDeviceInstance*);
    void removeInstance (WaveInputDeviceInstance*);

    TimeDuration getAdjustmentSeconds();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputDevice)
};


//==============================================================================
class WaveInputRecordingThread  : public juce::Thread,
                                  private juce::Timer
{
public:
    //==============================================================================
    WaveInputRecordingThread (Engine&);
    ~WaveInputRecordingThread() override;

    //==============================================================================
    struct ScopedInitialiser
    {
        ScopedInitialiser (WaveInputRecordingThread& t) : owner (t)     { owner.addUser(); }
        ~ScopedInitialiser()                                            { owner.removeUser(); }

        WaveInputRecordingThread& owner;
    };

    void addUser();
    void removeUser();

    //==============================================================================
    void addBlockToRecord (AudioFileWriter&, const juce::AudioBuffer<float>&,
                           int start, int numSamples, const RecordingThumbnailManager::Thumbnail::Ptr&);
    void waitForWriterToFinish (AudioFileWriter&);
    void run() override;
    void timerCallback() override;

    Engine& engine;

private:
    int activeUsers = 0;
    bool hasWarned = false, hasSentStop = false;

    struct BlockQueue;
    std::unique_ptr<BlockQueue> queue;

    void prepareToStart();
    void flushAndStop();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputRecordingThread)
};

} // namespace tracktion::inline engine
