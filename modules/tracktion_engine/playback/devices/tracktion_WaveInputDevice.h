/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** A (virtual) audio input device.

    There'll be multiple instances of these, representing mono or stereo pairs of
    input channels.
*/
class WaveInputDevice   : public InputDevice
{
public:
    //==============================================================================
    WaveInputDevice (Engine&, const juce::String& name, const juce::String& type,
                     const std::vector<ChannelIndex>&, DeviceType);
    ~WaveInputDevice();

    static juce::StringArray getMergeModes();
    juce::StringArray getRecordFormatNames();

    void resetToDefault();
    void setEnabled (bool) override;

    DeviceType getDeviceType() const override          { return deviceType; }
    InputDeviceInstance* createInstance (EditPlaybackContext&) override;

    //==============================================================================
    void flipEndToEnd() override;
    void setEndToEnd (bool);
    void setRecordAdjustmentMs (double ms);
    double getRecordAdjustmentMs() const                        { return recordAdjustMs; }
    bool isStereoPair() const;
    void setStereoPair (bool);
    juce::Array<int> getAvailableBitDepths() const;
    void setBitDepth (int);
    int getBitDepth() const                                     { return bitDepth; }
    void checkBitDepth();
    void setInputGainDb (float gain);
    float getInputGainDb() const                                { return inputGainDb; }
    void setRecordTriggerDb (float);
    float getRecordTriggerDb() const                            { return recordTriggerDb; }
    void setFilenameMask (const juce::String&);
    juce::String getFilenameMask() const                        { return filenameMask; }
    void setFilenameMaskToDefault();
    static juce::String getDefaultMask();

    void setOutputFormat (const juce::String&);
    juce::String getOutputFormat() const                        { return outputFormat; }
    void setMergeMode (const juce::String&);
    juce::String getMergeMode() const;

    const std::vector<ChannelIndex>& getChannels() const noexcept   { return deviceChannels; }
    const juce::AudioChannelSet& getChannelSet() const noexcept     { return channelSet; }

    //==============================================================================
    void masterTimeUpdate (double) override {}
    void consumeNextAudioBlock (const float** allChannels, int numChannels, int numSamples, double streamTime);

    RetrospectiveRecordBuffer* getRetrospectiveRecordBuffer()   { return retrospectiveBuffer.get(); }
    void updateRetrospectiveBufferLength (double length) override;

    /** Creates an AudioNode that calls consumeNextAudioBlock during it's render callback. */
    AudioNode* createWaveInputDeviceNode (AudioNode* input);

    //==============================================================================
    juce::String getSelectableDescription() override;

protected:
    juce::String openDevice();
    void closeDevice();

private:
    class WaveInputDeviceAudioNode;
    friend class DeviceManager;
    friend class WaveInputDeviceInstance;

    const std::vector<ChannelIndex> deviceChannels;
    const DeviceType deviceType;
    const juce::AudioChannelSet channelSet;

    juce::String filenameMask;
    float inputGainDb = 0;
    juce::String outputFormat;
    int bitDepth = 0, mergeMode = 0;
    float recordTriggerDb = 0;
    double recordAdjustMs = 0;
    juce::ScopedPointer<RetrospectiveRecordBuffer> retrospectiveBuffer;

    void loadProps();
    void saveProps();
    juce::AudioFormat* getFormatToUse() const;

    juce::Array<WaveInputDeviceInstance*> instances;
    juce::CriticalSection instanceLock;

    void addInstance (WaveInputDeviceInstance*);
    void removeInstance (WaveInputDeviceInstance*);

    double getAdjustmentSeconds();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputDevice)
};


//==============================================================================
class WaveInputRecordingThread  : public juce::Thread,
                                  private juce::Timer
{
public:
    //==============================================================================
    WaveInputRecordingThread (Engine&);
    ~WaveInputRecordingThread();

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

} // namespace tracktion_engine
