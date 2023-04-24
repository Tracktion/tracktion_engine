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
    void createPlayAudioNodes (TimePosition startTime);
    void createPlayAudioNodesIfNeeded (TimePosition startTime);
    void reallocate();
    
    /** Returns true if a playback graph is currently allocated. */
    bool isPlaybackGraphAllocated() const       { return isAllocated; }

    // Prepares the graph but doesn't actually start the playhead
    void prepareForPlaying (TimePosition startTime);
    void prepareForRecording (TimePosition startTime, TimePosition punchIn);

    // Plays this context in sync with another context
    void syncToContext (EditPlaybackContext* contextToSyncTo, TimePosition previousBarTime, TimeDuration syncInterval);

    Clip::Array stopRecording (InputDeviceInstance&, TimeRange recordedRange, bool discardRecordings);
    Clip::Array recordingFinished (TimeRange recordedRange, bool discardRecordings);
    juce::Result applyRetrospectiveRecord (juce::Array<Clip*>* clipsCreated = nullptr);

    juce::Array<InputDeviceInstance*> getAllInputs();
    InputDeviceInstance* getInputFor (InputDevice*) const;
    OutputDeviceInstance* getOutputFor (OutputDevice*) const;

    Edit& edit;
    TransportControl& transport;
    LevelMeasurer masterLevels;
    MidiNoteDispatcher midiDispatcher;

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
    /**
        Used to temporarily reduce the process priority if a long operation like a file save is taking place.
        You shouldn't need to use this in normal use.
    */
    struct RealtimePriorityDisabler
    {
        RealtimePriorityDisabler (Engine&);
        ~RealtimePriorityDisabler();

        Engine& engine;
    };

    //==============================================================================
    // These methods deal directly with the playhead so won't have any latency induced by syncing to the messaged thread.
    bool isPlaying() const;
    bool isLooping() const;
    bool isDragging() const;

    TimePosition getPosition() const;
    TimePosition getUnloopedPosition() const;
    TimeRange getLoopTimes() const;
    
    /** Returns the overall latency of the currently prepared graph. */
    int getLatencySamples() const;
    TimePosition getAudibleTimelineTime();
    double getSampleRate() const;
    void updateNumCPUs();

    /** This will increase/decrease playback speed by resampling, pitching the output up or down. */
    void setSpeedCompensation (double plusOrMinus);

    /** This will increase/decrease playback speed by changing the tempo, maintaining pitch where possible. */
    void setTempoAdjustment (double plusOrMinusProportion);

    void postPosition (TimePosition);
    void play();
    void stop();
    
    TimePosition globalStreamTimeToEditTime (double) const;
    TimePosition globalStreamTimeToEditTimeUnlooped (double) const;
    void resyncToGlobalStreamTime (juce::Range<double>, double sampleRate);

    /** @internal. Will be removed in a future release. */
    tracktion::graph::PlayHead* getNodePlayHead() const;

    /** @see tracktion::graph::ThreadPoolStrategy */
    static void setThreadPoolStrategy (int);
    /** @see tracktion::graph::ThreadPoolStrategy */
    static int getThreadPoolStrategy();
    
    /** Enables reusing of audio buffers during graph processing
        which may reduce the memory use at the cost of some additional overhead.
    */
    static void enablePooledMemory (bool);

private:
    bool isAllocated = false;

    struct ProcessPriorityBooster
    {
        ProcessPriorityBooster (Engine& engine);
        ~ProcessPriorityBooster();

        Engine& engine;
    };

    std::unique_ptr<ProcessPriorityBooster> priorityBooster;

    juce::OwnedArray<InputDeviceInstance> waveInputs, midiInputs;
    juce::OwnedArray<WaveOutputDeviceInstance> waveOutputs;
    juce::OwnedArray<MidiOutputDeviceInstance> midiOutputs;

    tempo::Sequence lastTempoSequence { {{ BeatPosition(), 120.0, 0.0f }},
                                        {{ BeatPosition(), 4, 4, false }},
                                        tempo::LengthOfOneBeat::dependsOnTimeSignature };

    void releaseDeviceList();
    void rebuildDeviceList();

    void prepareOutputDevices (TimePosition start);
    void startRecording (TimePosition start, TimePosition punchIn);
    void startPlaying (TimePosition start);

    friend class DeviceManager;

    juce::WeakReference<EditPlaybackContext> contextToSyncTo;
    TimePosition previousBarTime;
    TimeDuration syncInterval;
    bool hasSynced = false;
    double lastStreamPos = 0;
    
    struct ContextSyncroniser;
    std::unique_ptr<ContextSyncroniser> contextSyncroniser;
    
    struct NodePlaybackContext;
    std::unique_ptr<NodePlaybackContext> nodePlaybackContext;

    juce::WeakReference<EditPlaybackContext> nodeContextToSyncTo;
    std::atomic<double> audiblePlaybackTime { 0.0 };

    void createNode();
    void fillNextNodeBlock (float* const* allChannels, int numChannels, int numSamples);

    JUCE_DECLARE_WEAK_REFERENCEABLE (EditPlaybackContext)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditPlaybackContext)
};

}} // namespace tracktion { inline namespace engine
