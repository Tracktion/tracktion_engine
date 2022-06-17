/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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
    ~WaveInputDevice() override;

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

    //==============================================================================
    juce::String getSelectableDescription() override;

protected:
    juce::String openDevice();
    void closeDevice();

private:
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
    std::unique_ptr<RetrospectiveRecordBuffer> retrospectiveBuffer;

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

    // BEATCONNECT MODIFICATION START
    struct AudioFifo
    {
        #define AUDIOFIFO_SIZE  256

        void addToFifo(juce::AudioBuffer<float>& NextBuffer)
        {
            int start1, size1, start2, size2;
            m_AbstractFifo.prepareToWrite(1, start1, size1, start2, size2);

            jassert(size1 == 1);
            jassert(size2 == 0);
            jassert(start2 == 0);

            if (size1 > 0)
            {
                for (int i = start1; i < start1 + size1; i++)
                    m_BufferArray[i] = NextBuffer;
            }

            m_AbstractFifo.finishedWrite(size1);
        }

        void addToFifo(const juce::AudioBuffer<float>* NextBuffer, int numItems)
        {
            int start1, size1, start2, size2;
            m_AbstractFifo.prepareToWrite(numItems, start1, size1, start2, size2);

            jassert(numItems <= (size1 + size2));

            if (size1 > 0)
            {
                int j = 0;
                for (int i = start1; i < start1 + size1; i++, j++)
                    m_BufferArray[i] = NextBuffer[j];
            }
            
            if (size2 > 0)
            {
                int j = 0;
                for (int i = start2; i < start2 + size2; i++, j++)
                    m_BufferArray[i] = NextBuffer[size1 + j];
            }

            m_AbstractFifo.finishedWrite(size1 + size2);
        }

        void readFromFifo(juce::AudioBuffer<float>* NextBuffer, int numItems)
        {
            int start1, size1, start2, size2;
            m_AbstractFifo.prepareToRead(numItems, start1, size1, start2, size2);

            jassert(numItems <= (size1 + size2));

            if (size1 > 0)
            {
                int j = 0;
                for (int i = start1; i < start1 + size1; i++, j++)
                    NextBuffer[j] = m_BufferArray[i];
            }

            if (size2 > 0)
            {
                int j = size1;
                for (int i = start2; i < start2 + size2; i++, j++)
                    NextBuffer[j] = m_BufferArray[i];
            }

            m_AbstractFifo.finishedRead(size1 + size2);
        }

        juce::AbstractFifo m_AbstractFifo{ AUDIOFIFO_SIZE };
        juce::AudioBuffer<float> m_BufferArray[AUDIOFIFO_SIZE];
    }
    m_AudioFifo;
    // BEATCONNECT MODIFICATION END

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
