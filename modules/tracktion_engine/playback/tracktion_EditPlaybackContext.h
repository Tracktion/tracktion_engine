/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class EditPlaybackContext
{
public:
    EditPlaybackContext (TransportControl&);
    ~EditPlaybackContext();

    void removeInstanceForDevice (InputDevice&);

    /** Note this doesn't check for device enablement */
    void addWaveInputDeviceInstance (InputDevice&);
    void addMidiInputDeviceInstance (InputDevice&);

    void clearNodes();
    void createPlayAudioNodes (double startTime);
    void createPlayAudioNodesIfNeeded (double startTime);
    void reallocate();

    // Prepares the graph but doesn't actually start the playhead
    void prepareForPlaying (double startTime);
    void prepareForRecording (double startTime, double punchIn);

    // Plays this context in sync with another context
    void syncToContext (EditPlaybackContext* contextToSyncTo, double previousBarTime, double syncInterval);

    Clip::Array stopRecording (InputDeviceInstance&, EditTimeRange recordedRange, bool discardRecordings);
    Clip::Array recordingFinished (EditTimeRange recordedRange, bool discardRecordings);
    juce::Result applyRetrospectiveRecord (juce::Array<Clip*>* clipsCreated = nullptr);

    juce::Array<InputDeviceInstance*> getAllInputs();
    InputDeviceInstance* getInputFor (InputDevice*) const;

    Edit& edit;
    TransportControl& transport;
    PlayHead playhead;
    LevelMeasurer masterLevels;
    MidiNoteDispatcher midiDispatcher;
    static std::function<AudioNode*(AudioNode*)> insertOptionalLastStageNode;

    /** Releases and then optionally reallocates the context's device list safely. */
    struct ScopedDeviceListReleaser
    {
        ScopedDeviceListReleaser (EditPlaybackContext&, bool reallocate);
        ~ScopedDeviceListReleaser();

        EditPlaybackContext& owner;
        const bool shouldReallocate;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedDeviceListReleaser)
    };

    // This is a global setting
    static bool shouldAddAntiDenormalisationNoise (Engine&);
    static void setAddAntiDenormalisationNoise (Engine&, bool);

    //==============================================================================
    // Only implemented on Windows
    struct RealtimePriorityDisabler
    {
        RealtimePriorityDisabler();
        ~RealtimePriorityDisabler();
    };

private:
    bool isAllocated = false;

    struct ProcessPriorityBooster
    {
        ProcessPriorityBooster();
        ~ProcessPriorityBooster();
    };

    juce::ScopedPointer<ProcessPriorityBooster> priorityBooster;

    juce::OwnedArray<InputDeviceInstance> waveInputs, midiInputs;
    juce::OwnedArray<WaveOutputDeviceInstance> waveOutputs;
    juce::OwnedArray<MidiOutputDeviceInstance> midiOutputs;

    TempoSequence::TempoSections lastTempoSections;

    void releaseDeviceList();
    void rebuildDeviceList();

    void createAudioNodes (double startTime, bool addAntiDenormalisationNoise);
    void prepareOutputDevices (double start);
    void startRecording (double start, double punchIn);
    void startPlaying (double start);

    friend class DeviceManager;
    void fillNextAudioBlock (EditTimeRange streamTime, float** allChannels, int numSamples);

    juce::WeakReference<EditPlaybackContext> contextToSyncTo;
    double previousBarTime = 0;
    double syncInterval = 0;
    bool hasSynced = false;
    double lastStreamPos = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (EditPlaybackContext)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditPlaybackContext)
};

} // namespace tracktion_engine
