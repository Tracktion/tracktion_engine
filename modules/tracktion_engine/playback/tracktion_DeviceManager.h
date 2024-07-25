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

class HostedAudioDeviceInterface;


//==============================================================================
/**
*/
class DeviceManager     : public juce::ChangeBroadcaster,
                          public juce::ChangeListener,
                          private juce::AudioIODeviceCallback,
                          private juce::AsyncUpdater,
                          private juce::Timer
{
    friend class Engine;
    DeviceManager (Engine&);

public:
    //==============================================================================
    ~DeviceManager() override;

    void initialise (int defaultNumInputChannelsToOpen = 512,
                     int defaultNumOutputChannelsToOpen = 512);
    void closeDevices();
    void saveSettings();

    void resetToDefaults (bool deviceSettings,
                          bool resetInputDevices,
                          bool resetOutputDevices,
                          bool latencySettings,
                          bool mixSettings);

    void rescanMidiDeviceList();
    void rescanWaveDeviceList();

    int getMidiDeviceScanIntervalSeconds() const        { return midiRescanIntervalSeconds; }
    void setMidiDeviceScanIntervalSeconds (int intervalSeconds);

    //==============================================================================
    double getSampleRate() const;
    int getBitDepth() const;
    int getBlockSize() const;
    TimeDuration getBlockLength() const;
    double getBlockSizeMs() const;
    double getOutputLatencySeconds() const;

    // get a time by which recordings should be shifted to sync up with active output channels
    double getRecordAdjustmentMs();
    int getRecordAdjustmentSamples();

    //==============================================================================
    float getCpuUsage() const noexcept                  { return (float) currentCpuUsage; }

    // Sets an upper limit on the proportion of CPU time being used - if getCpuUsage() exceeds this,
    // the processing will be muted to keep the system running. Defaults to 0.98
    void setCpuLimitBeforeMuting (double newLimit)      { jassert (newLimit > 0); cpuLimitBeforeMuting = newLimit; }

    PerformanceMeasurement::Statistics getCPUStatistics() const;
    void restCPUStatistics();

    void updateNumCPUs(); // should be called when active num CPUs is changed

    //==============================================================================
    /** If set to true, clips the output at 0.0. */
    void enableOutputClipping (bool clipOutput);

    /** Checks if the output has clipped.
        @param reset    Resets the clipped flag
    */
    bool hasOutputClipped (bool reset);


    //==============================================================================
    /** If you are using the engine in a plugin or an application
        that accesses the audio device directly, use this interface
        to pass audio and midi to the DeviceManager.
    */
    HostedAudioDeviceInterface& getHostedAudioDeviceInterface();

    /** Returns true if the hosted interface is available and in use. */
    bool isHostedAudioDeviceInterfaceInUse() const;

    /** Removes the hosted audio device.
        You shouldn't normally need to call this but can be useful for running tests.
        Afterwards, you'll need to call initialise again.
    */
    void removeHostedAudioDeviceInterface();

    //==============================================================================
    // list of all input devices..
    int getNumInputDevices() const;
    InputDevice* getInputDevice (int index) const;

    int getNumOutputDevices() const;
    OutputDevice* getOutputDeviceAt (int index) const;

    InputDevice* findInputDeviceForID (const juce::String& id) const;
    InputDevice* findInputDeviceWithName (const juce::String& name) const;
    std::shared_ptr<MidiInputDevice> findMidiInputDeviceForID (const juce::String& id) const;

    OutputDevice* findOutputDeviceForID (const juce::String& id) const;
    OutputDevice* findOutputDeviceWithName (const juce::String& name) const;

    //==============================================================================
    int getNumWaveOutDevices() const                            { return waveOutputs.size(); }
    WaveOutputDevice* getWaveOutDevice (int index) const        { return waveOutputs[index]; }

    void setDefaultWaveOutDevice (juce::String deviceID);
    WaveOutputDevice* getDefaultWaveOutDevice() const;
    juce::String getDefaultWaveOutDeviceID() const              { return defaultWaveOutID; }

    int getNumWaveInDevices() const                             { return waveInputs.size(); }
    WaveInputDevice* getWaveInDevice (int index) const          { return waveInputs[index]; }

    void setDefaultWaveInDevice (juce::String deviceID);
    WaveInputDevice* getDefaultWaveInDevice() const;
    juce::String getDefaultWaveInDeviceID() const               { return defaultWaveInID; }

    std::vector<WaveOutputDevice*> getWaveOutputDevices();
    void setWaveOutChannelsEnabled (const std::vector<ChannelIndex>&, bool);
    void setDeviceOutChannelStereo (int channelNum, bool isStereoPair);
    bool isDeviceOutChannelStereo (int chan) const              { return ! outMonoChans[chan / 2]; }
    bool isDeviceOutEnabled (int chanNum)                       { return outEnabled[chanNum]; }

    std::vector<WaveInputDevice*> getWaveInputDevices();
    void setWaveInChannelsEnabled (const std::vector<ChannelIndex>&, bool);
    void setDeviceInChannelStereo (int channelNum, bool isStereoPair);
    bool isDeviceInChannelStereo (int chan) const               { return inStereoChans[chan / 2]; }
    bool isDeviceInEnabled (int chanNum)                        { return inEnabled[chanNum]; }

    //==============================================================================
    int getNumMidiOutDevices() const                            { return (int) midiOutputs.size(); }
    MidiOutputDevice* getMidiOutDevice (int index) const        { return index >= 0 && index < (int) midiOutputs.size() ? midiOutputs[(size_t) index].get() : nullptr; }

    void setDefaultMidiOutDevice (juce::String deviceID);
    MidiOutputDevice* getDefaultMidiOutDevice() const;
    juce::String getDefaultMidiOutDeviceID() const              { return defaultMidiOutID; }

    int getNumMidiInDevices() const;
    std::shared_ptr<MidiInputDevice> getMidiInDevice (int index) const;

    std::vector<std::shared_ptr<MidiInputDevice>> getMidiInDevices() const;

    void setDefaultMidiInDevice (juce::String deviceID);
    MidiInputDevice* getDefaultMidiInDevice() const;
    juce::String getDefaultMidiInDeviceID() const               { return defaultMidiInID; }

    void injectMIDIMessageToDefaultDevice (const juce::MidiMessage&);
    void broadcastMessageToAllVirtualDevices (MidiInputDevice&, const juce::MidiMessage&);

    void broadcastStreamTimeToMidiDevices (double streamTime);
    bool shouldSendMidiTimecode() const noexcept                { return sendMidiTimecode; }

    /** Returns the current block's stream time.
        This shouldn't really be used and may be removed in future.
    */
    double getCurrentStreamTime() const noexcept                { return streamTime; }

    bool isMSWavetableSynthPresent() const;

    /** Changes to the devices get applied asyncronously so this function can be called to trigger any pending updates to be flushed. */
    void dispatchPendingUpdates();

    //==============================================================================
    void checkDefaultDevicesAreValid();

    static juce::String getDefaultAudioOutDeviceName (bool translated);
    static juce::String getDefaultMidiOutDeviceName (bool translated);

    static juce::String getDefaultAudioInDeviceName (bool translated);
    static juce::String getDefaultMidiInDeviceName (bool translated);

    juce::Result createVirtualMidiDevice (const juce::String& name);
    void deleteVirtualMidiDevice (VirtualMidiInputDevice&);

    Engine& engine;

    //==============================================================================
    /**
        Subclass of an AudioDeviceManager which can be used to avoid adding the
        system audio devices in plugin builds.
        @see EngineBehaviour::addSystemAudioIODeviceTypes
    */
    struct TracktionEngineAudioDeviceManager  : public juce::AudioDeviceManager
    {
        TracktionEngineAudioDeviceManager (Engine&);
        void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>&) override;

        Engine& engine;
    };

    TracktionEngineAudioDeviceManager deviceManager { engine };

    //==============================================================================
    std::unique_ptr<HostedAudioDeviceInterface> hostedAudioDeviceInterface;

    std::vector<std::shared_ptr<MidiInputDevice>> midiInputs; // Only thread-safe from the message thread
    std::vector<std::shared_ptr<MidiOutputDevice>> midiOutputs;

    juce::OwnedArray<WaveInputDevice> waveInputs;
    juce::OwnedArray<WaveOutputDevice> waveOutputs;

    //==============================================================================
    void addContext (EditPlaybackContext*);
    void removeContext (EditPlaybackContext*);

    //==============================================================================
    /** Sets a global processor to be applied to the output.
        This can be used to set a limiter or similar on the whole ouput.
        It shouldn't be used for musical effects.
    */
    void setGlobalOutputAudioProcessor (std::unique_ptr<juce::AudioProcessor>);

    /** Returns a previously set globalOutputAudioProcessor. */
    juce::AudioProcessor* getGlobalOutputAudioProcessor() const     { return globalOutputAudioProcessor.get(); }

    /** If this is set, it will get called (possibly on the midi thread) when incoming
        messages seem to be unused. May want to use it to warn the user.
    */
    std::function<void(InputDevice*)> warnOfWastedMidiMessagesFunction;


private:
    //==============================================================================
    bool finishedInitialising = false;
    bool sendMidiTimecode = false;

    std::atomic<double> currentCpuUsage { 0 }, streamTime { 0 }, cpuLimitBeforeMuting { 0.98 };
    std::atomic<bool> isSuspended { true }, outputHasClipped { false }, outputClippingEnabled { false };
    double currentLatencyMs = 0, outputLatencyTime = 0, currentSampleRate = 0;
    int maxBlockSize = 0;

    int defaultNumInputChannelsToOpen = 512, defaultNumOutputChannelsToOpen = 512;
    juce::BigInteger outEnabled, inEnabled, activeOutChannels, outMonoChans, inStereoChans;
    juce::String defaultWaveOutID, defaultMidiOutID, defaultWaveInID, defaultMidiInID;

    int midiRescanIntervalSeconds = 4;
    bool onlyRescanMidiOnHardwareChange = true;

    struct MIDIDeviceList;
    std::unique_ptr<MIDIDeviceList> lastMIDIDeviceList;

    struct AvailableWaveDeviceList;
    std::unique_ptr<AvailableWaveDeviceList> lastAvailableWaveDeviceList;

    struct PrepareToStartCaller;
    std::unique_ptr<PrepareToStartCaller> prepareToStartCaller;

    std::shared_mutex contextLock;
    juce::Array<EditPlaybackContext*> activeContexts;
    std::unique_ptr<juce::AudioProcessor> globalOutputAudioProcessor;
    juce::HeapBlock<const float*> inputChannelsScratch;
    juce::HeapBlock<float*> outputChannelsScratch;

    mutable std::shared_mutex midiInputsMutex;

   #if JUCE_ANDROID
    ScopedSteadyLoad::Context steadyLoadContext;
   #endif

    PerformanceMeasurement performanceMeasurement { "tracktion_engine::DeviceManager", -1, false };
    crill::seqlock_object<PerformanceMeasurement::Statistics> performanceStats;
    std::atomic<bool> clearStatsFlag { false };

    void applyNewMidiDeviceList();
    void restartMidiCheckTimer();

    void clearAllContextDevices();
    void reloadAllContextDevices();

    void loadSettings();
    void sanityCheckEnabledChannels();

    bool usesHardwareMidiDevices();
    void timerCallback() override;

    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int totalNumInputChannels,
                                           float* const* outputChannelData, int totalNumOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void prepareToStart();

    void audioDeviceIOCallbackInternal (const float* const* inputChannelData, int numInputChannels,
                                        float* const* outputChannelData, int totalNumOutputChannels,
                                        int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceManager)
};

}} // namespace tracktion { inline namespace engine
