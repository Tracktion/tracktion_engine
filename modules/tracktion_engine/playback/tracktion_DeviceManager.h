/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

class HostedAudioDeviceInterface;

//==============================================================================
//==============================================================================
/**
    Subclass of an AudioDeviceManager which can be used to avoid adding the
    system audio devices in plugin builds.
    @see EngineBehaviour::addSystemAudioIODeviceTypes
*/
class TracktionEngineAudioDeviceManager : public juce::AudioDeviceManager
{
public:
    /** Creates a TracktionEngineAudioDeviceManager. */
    TracktionEngineAudioDeviceManager (Engine&);

    /** EngineBehaviour::addSystemAudioIODeviceTypes can be used to avoid creating
        system devices in plugin or parsing builds.
    */
    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>&) override;

private:
    Engine& engine;
};


//==============================================================================
//==============================================================================
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
    ~DeviceManager() override;

    void initialise (int defaultNumInputChannelsToOpen = 512,
                     int defaultNumOutputChannelsToOpen = 512);
    void closeDevices();
    void saveSettings();

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
    float getCpuUsage() const noexcept                  { return (float) currentCpuUsage; }

    // Sets an upper limit on the proportion of CPU time being used - if getCpuUsage() exceeds this,
    // the processing will be muted to keep the system running. Defaults to 0.98
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
    TimeDuration getBlockLength() const;

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

    /** Returns the current block's stream time.
        This shouldn't really be used and may be removed in future.
    */
    double getCurrentStreamTime() const noexcept                { return streamTime; }

    bool isMSWavetableSynthPresent() const;
    void resetToDefaults (bool deviceSettings, bool resetInputDevices, bool resetOutputDevices, bool latencySettings, bool mixSettings);

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

    Engine& engine;

    std::unique_ptr<HostedAudioDeviceInterface> hostedAudioDeviceInterface;
    TracktionEngineAudioDeviceManager deviceManager { engine };

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

    /** Sets a global processor to be applied to the output.
        This can be used to set a limiter or similar on the whole ouput.
        It shouldn't be used for musical effects.
    */
    void setGlobalOutputAudioProcessor (juce::AudioProcessor*);

    /** Returns a previously set globalOutputAudioProcessor. */
    juce::AudioProcessor* getGlobalOutputAudioProcessor() const { return globalOutputAudioProcessor.get(); }

    /** If this is set, it will get called (possibly on the midi thread) when incoming
        messages seem to be unused. May want to use it to warn the user.
    */
    std::function<void (InputDevice*)> warnOfWastedMidiMessagesFunction;

private:
    struct WaveDeviceList;
    struct ContextDeviceClearer;
    bool finishedInitialising = false;
    bool sendMidiTimecode = false;

    std::atomic<double> currentCpuUsage { 0 }, streamTime { 0 }, cpuLimitBeforeMuting { 0.98 };
    double currentLatencyMs = 0, outputLatencyTime = 0, currentSampleRate = 0;
    juce::Array<EditPlaybackContext*> contextsToRestart;

    juce::StringArray lastMidiOutNames, lastMidiInNames;

    int defaultNumInputChannelsToOpen = 512, defaultNumOutputChannelsToOpen = 512;
    juce::BigInteger outEnabled, inEnabled, activeOutChannels, outMonoChans, inStereoChans;
    int defaultWaveOutIndex = 0, defaultMidiOutIndex = 0, defaultWaveInIndex = 0, defaultMidiInIndex = 0;

    std::unique_ptr<WaveDeviceList> lastWaveDeviceList;
    std::unique_ptr<ContextDeviceClearer> contextDeviceClearer;

    juce::CriticalSection contextLock;
    juce::Array<EditPlaybackContext*> activeContexts;
    std::unique_ptr<juce::AudioProcessor> globalOutputAudioProcessor;
    juce::HeapBlock<const float*> inputChannelsScratch;
    juce::HeapBlock<float*> outputChannelsScratch;

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

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int totalNumInputChannels,
                                           float* const* outputChannelData, int totalNumOutputChannels, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    void audioDeviceIOCallbackInternal (const float* const* inputChannelData, int numInputChannels,
                                        float* const* outputChannelData, int totalNumOutputChannels,
                                        int numSamples);

    //==============================================================================
    int maxBlockSize = 0;
    int cpuReportingInterval = 1;
    int cpuAvgCounter = 0;
    int glitchCntr = 0;
    float cpuAvg = 0;
    float cpuMin = 0;
    float cpuMax = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceManager)
};

}} // namespace tracktion { inline namespace engine
