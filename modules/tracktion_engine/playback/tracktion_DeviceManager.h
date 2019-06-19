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

class HostedAudioDeviceInterface;

/**
*/
class DeviceManager     : public juce::ChangeBroadcaster,
                          public juce::ChangeListener,
                          private juce::AudioIODeviceCallback
{
    friend class Engine;
    DeviceManager (Engine&);

public:
    //==============================================================================
    ~DeviceManager();

    void initialise (int defaultNumInputChannelsToOpen = 512,
                     int defaultNumOutputChannelsToOpen = 512);
    void closeDevices();
    void saveSettings();

    // If you are using the engine in a plugin or an application
    // that accesses the audio device directly, use this interface
    // to pass audio and midi to the DeviceManager.
    HostedAudioDeviceInterface& getHostedAudioDeviceInterface();

    //==============================================================================
    float getCpuUsage() const noexcept                  { return (float) currentCpuUsage; }

    // Sets an upper limit on the proportion of CPU time being used - if getCpuUsage() exceeds this,
    // the processing will be muted to keep the system running. Defaults to 0.95
    void setCpuLimitBeforeMuting (double newLimit)      { jassert (newLimit > 0); cpuLimitBeforeMuting = newLimit; }

    void updateNumCPUs(); // should be called when active num CPUs is changed

    //==============================================================================
    struct CPUUsageListener
    {
        virtual ~CPUUsageListener() {}

        // this is called from the audio thread and therefore musn't allocate, lock or
        // throw exceptions
        virtual void reportCPUUsage (float cpuAvg, float cpuMin, float cpuMax, int numGlitches) = 0;
    };

    void addCPUUsageListener (CPUUsageListener* listener)       { cpuUsageListeners.add (listener); }
    void removeCPUUsageListener (CPUUsageListener* listener)    { cpuUsageListeners.remove (listener); }

    //==============================================================================
    double getSampleRate() const;
    int getBitDepth() const;
    int getBlockSize() const;
    double getBlockSizeMs() const;

    int getNumWaveOutDevices() const                            { return waveOutputs.size(); }
    WaveOutputDevice* getWaveOutDevice (int index) const        { return waveOutputs[index]; }
    WaveOutputDevice* getDefaultWaveOutDevice() const           { return getWaveOutDevice (defaultWaveOutIndex); }
    void setDefaultWaveOutDevice (int index);

    int getNumWaveInDevices() const                             { return waveInputs.size(); }
    WaveInputDevice* getWaveInDevice (int index) const          { return waveInputs[index]; }
    WaveInputDevice* getDefaultWaveInDevice() const             { return getWaveInDevice (defaultWaveInIndex); }
    void setDefaultWaveInDevice (int index);

    void setDeviceOutChannelStereo (int channelNum, bool isStereoPair);
    bool isDeviceOutChannelStereo (int chan) const              { return ! outMonoChans[chan / 2]; }
    bool isDeviceOutEnabled (int chanNum)                       { return outEnabled[chanNum]; }

    void setDeviceInChannelStereo (int channelNum, bool isStereoPair);
    bool isDeviceInChannelStereo (int chan) const               { return inStereoChans[chan / 2]; }
    bool isDeviceInEnabled (int chanNum)                        { return inEnabled[chanNum]; }

    void setWaveOutChannelsEnabled (const std::vector<ChannelIndex>&, bool);
    void setWaveInChannelsEnabled (const std::vector<ChannelIndex>&, bool);

    // set to 1.0 to make it speed up a bit, -1.0 to slow down, 0 for normal.
    void setSpeedCompensation (double plusOrMinus);

    void setInternalBufferMultiplier (int multiplier);
    int getInternalBufferMultiplier()                           { return internalBufferMultiplier; }
    int getInternalBufferSize() const                           { return getBlockSize() * internalBufferMultiplier; }

    //==============================================================================
    bool rebuildWaveDeviceListIfNeeded();
    void rescanMidiDeviceList (bool forceRescan);

    int getNumMidiOutDevices() const                            { return midiOutputs.size(); }
    MidiOutputDevice* getMidiOutDevice (int index) const        { return midiOutputs[index]; }
    MidiOutputDevice* getDefaultMidiOutDevice() const           { return getMidiOutDevice (defaultMidiOutIndex); }

    void setDefaultMidiOutDevice (int index);

    int getNumMidiInDevices() const;
    MidiInputDevice* getMidiInDevice (int index) const;
    MidiInputDevice* getDefaultMidiInDevice() const             { return getMidiInDevice (defaultMidiInIndex); }

    void setDefaultMidiInDevice (int index);

    void broadcastStreamTimeToMidiDevices (double streamTime);
    bool shouldSendMidiTimecode() const noexcept                { return sendMidiTimecode; }

    bool isMSWavetableSynthPresent() const;
    void resetToDefaults (bool resetInputDevices, bool resetOutputDevices);

    //==============================================================================
    // list of all input devices..
    int getNumInputDevices() const;
    InputDevice* getInputDevice (int index) const;

    int getNumOutputDevices() const;
    OutputDevice* getOutputDeviceAt (int index) const;

    OutputDevice* findOutputDeviceForID (const juce::String& id) const;
    OutputDevice* findOutputDeviceWithName (const juce::String& name) const;

    //==============================================================================
    void checkDefaultDevicesAreValid();

    static juce::String getDefaultAudioOutDeviceName (bool translated);
    static juce::String getDefaultMidiOutDeviceName (bool translated);

    static juce::String getDefaultAudioInDeviceName (bool translated);
    static juce::String getDefaultMidiInDeviceName (bool translated);

    juce::Result createVirtualMidiDevice (const juce::String& name);
    void deleteVirtualMidiDevice (VirtualMidiInputDevice*);

    // get a time by which recordings should be shifted to sync up with active output channels
    double getRecordAdjustmentMs();
    int getRecordAdjustmentSamples();

    double getOutputLatencySeconds() const;

    std::unique_ptr<HostedAudioDeviceInterface> hostedAudioDeviceInterface;
    juce::AudioDeviceManager deviceManager;

    juce::OwnedArray<MidiInputDevice, juce::CriticalSection> midiInputs;
    juce::OwnedArray<MidiOutputDevice> midiOutputs;
    juce::OwnedArray<WaveInputDevice> waveInputs;
    juce::OwnedArray<WaveOutputDevice> waveOutputs;

    void addContext (EditPlaybackContext*);
    void removeContext (EditPlaybackContext*);

    void clearAllContextDevices();
    void reloadAllContextDevices();

    struct ContextDeviceListRebuilder
    {
        ContextDeviceListRebuilder (DeviceManager&);
        ~ContextDeviceListRebuilder();

        DeviceManager& dm;
    };

    void setGlobalOutputAudioProcessor (juce::AudioProcessor*);
    juce::AudioProcessor* getGlobalOutputAudioProcessor() const { return globalOutputAudioProcessor.get(); }

    // If this is set, it will get called (possibly on the midi thread) when incoming
    // messages seem to be unused. May want to use it to warn the user.
    std::function<void(InputDevice*)> warnOfWastedMidiMessagesFunction;

    Engine& engine;

private:
    struct WaveDeviceList;
    bool finishedInitialising = false;
    bool sendMidiTimecode = false;

    std::atomic<double> currentCpuUsage { 0 }, streamTime { 0 };
    double cpuLimitBeforeMuting = 0.95;
    double currentLatencyMs = 0, outputLatencyTime = 0, currentSampleRate = 0;
    double speedCompensation = 0;
    int internalBufferMultiplier = 1;
    juce::Array<EditPlaybackContext*> contextsToRestart;

    juce::StringArray lastMidiOutNames, lastMidiInNames;

    int defaultNumInputChannelsToOpen = 512, defaultNumOutputChannelsToOpen = 512;
    juce::BigInteger outEnabled, inEnabled, activeOutChannels, outMonoChans, inStereoChans;
    int defaultWaveOutIndex = 0, defaultMidiOutIndex = 0, defaultWaveInIndex = 0, defaultMidiInIndex = 0;

    std::unique_ptr<WaveDeviceList> lastWaveDeviceList;

    juce::CriticalSection contextLock;
    juce::Array<EditPlaybackContext*> activeContexts;
    std::unique_ptr<juce::AudioProcessor> globalOutputAudioProcessor;

   #if JUCE_ANDROID
    ScopedSteadyLoad::Context steadyLoadContext;
   #endif

    juce::ListenerList<CPUUsageListener> cpuUsageListeners;

    void initialiseMidi();
    void rebuildWaveDeviceList();
    bool waveDeviceListNeedsRebuilding();
    void sanityCheckEnabledChannels();

    void loadSettings();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void audioDeviceIOCallback (const float** inputChannelData, int totalNumInputChannels,
                                float** outputChannelData, int totalNumOutputChannels, int numSamples) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    //==============================================================================
    int cpuReportingInterval = 1;
    int cpuAvgCounter = 0;
    int glitchCntr = 0;
    float cpuAvg = 0;
    float cpuMin = 0;
    float cpuMax = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceManager)
};

} // namespace tracktion_engine
