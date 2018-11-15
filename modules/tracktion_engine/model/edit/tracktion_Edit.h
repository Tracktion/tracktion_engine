/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    The Tracktion Edit class!
*/
class Edit  : public Selectable,
              private juce::Timer
{
public:
    //==============================================================================
    /** Enum used to determine what an Edit is being used for. */
    enum EditRole
    {
        playDisabled        = 1,    // determines if an EditPlaybackContext is created
        proxiesDisabled     = 2,    // determines if clips and create proxies
        pluginsDisabled     = 4,

        forEditing          = 0,
        forRendering        = playDisabled,
        forExporting        = playDisabled + proxiesDisabled + pluginsDisabled,
        forExamining        = playDisabled + proxiesDisabled + pluginsDisabled
    };

    struct LoadContext
    {
        std::atomic<float> progress  { 0.0f };
        std::atomic<bool> completed  { false };
        std::atomic<bool> shouldExit { false };
    };

    //==============================================================================
    Edit (Engine&, juce::ValueTree, EditRole, LoadContext*, int numUndoLevelsToStore);
    ~Edit();

    Engine& engine;

    //==============================================================================
    /** Returns the name of the Edit if a ProjectItem can be found for it. */
    juce::String getName();

    /** This callback can be set to return the file for this Edit.
        By default this uses ProjectManager to find a matching file for the Edit's
        ProjectItemID but this can be overriden for custom behaviour.
    */
    std::function<juce::File()> editFileRetriever;

    /** This callback can be set to resolve file paths.
        By default:
        1) If the path is an absolute path it will simply return it
        2) If it is a relative path, the editFileRetriever will be used to try and find
           a File for the Edit and the path will be resolved against that

        You can set a custom resolver here in case the Edit is
        in memory or the files directory is different to the Edit file's location.
    */
    std::function<juce::File (const juce::String&)> filePathResolver;

    void setProjectItemID (ProjectItemID);
    ProjectItemID getProjectItemID() const noexcept    { return editProjectItemID; }

    EditRole getEditRole() const noexcept   { return editRole; }
    bool shouldPlay() const noexcept        { return (editRole & playDisabled) == 0; }
    bool canRenderProxies() const noexcept  { return (editRole & proxiesDisabled) == 0; }
    bool shouldLoadPlugins() const noexcept { return (editRole & pluginsDisabled) == 0; }
    bool getIsPreviewEdit() const noexcept  { return isPreviewEdit; }

    //==============================================================================
    static constexpr double maximumLength = 48.0 * 60.0 * 60.0;
    static EditTimeRange getMaximumEditTimeRange()  { return { 0.0, maximumLength }; }

    static const int maxNumTracks = 400;
    static const int maxClipsInTrack = 1500;
    static const int maxPluginsOnClip = 5;
    static const int maxPluginsOnTrack = 16;
    static const int maxNumMasterPlugins = 4;
    static const int ticksPerQuarterNote = 960;

    //==============================================================================
    /** Saves the plugin, automap and ARA states to the state ValueTree. */
    void flushState();
    void flushPluginStateIfNeeded (Plugin&);

    /** Returns the time the project was last saved if no modifications occurred since this object was initialised. */
    juce::Time getTimeOfLastChange() const;

    void resetChangedStatus();
    bool hasChangedSinceSaved() const;

    /** Returns true if the Edit's not yet fully loaded */
    bool isLoading() const                                              { return isLoadInProgress; }

    static std::unique_ptr<Edit> createEditForPreviewingFile (Engine&, const juce::File&, const Edit* editToMatch,
                                                              bool tryToMatchTempo, bool tryToMatchPitch, bool* couldMatchTempo,
                                                              juce::ValueTree midiPreviewPlugin);

    static std::unique_ptr<Edit> createEditForPreviewingPreset (Engine& engine, juce::ValueTree, const Edit* editToMatch,
                                                                bool tryToMatchTempo, bool* couldMatchTempo,
                                                                juce::ValueTree midiPreviewPlugin);

    static std::unique_ptr<Edit> createEditForPreviewingClip (Clip&);
    static std::unique_ptr<Edit> createSingleTrackEdit (Engine&);

    //==============================================================================
    /** Statically kept lists of EditItems. */
    EditItemCache<Track> trackCache;
    EditItemCache<Clip> clipCache;

    //==============================================================================
    EditInputDevices& getEditInputDevices() noexcept;
    InputDeviceInstance* getCurrentInstanceForInputDevice (InputDevice*) const;
    juce::Array<InputDeviceInstance*> getAllInputDevices() const;

    //==============================================================================
    TransportControl& getTransport() const noexcept                     { return *transportControl; }
    EditPlaybackContext* getCurrentPlaybackContext() const;

    ParameterChangeHandler& getParameterChangeHandler() noexcept        { return *parameterChangeHandler; }
    ParameterControlMappings& getParameterControlMappings() noexcept    { return *parameterControlMappings; }
    AutomationRecordManager& getAutomationRecordManager() noexcept      { return *automationRecordManager; }

    AbletonLink& getAbletonLink() const noexcept                        { return *abletonLink; }

    /** Temporarily removes an Edit from the device manager. */
    struct ScopedRenderStatus
    {
        ScopedRenderStatus (Edit&);
        ~ScopedRenderStatus();

        Edit& edit;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedRenderStatus)
    };

    bool isRendering() const noexcept                           { return performingRender; }

    //==============================================================================
    void initialiseAllPlugins();

    //==============================================================================
    juce::String getSelectableDescription() override            { return TRANS("Edit") + " - \"" + getName() + "\""; }

    //==============================================================================
    /** undo/redo */
    juce::UndoManager& getUndoManager() noexcept                { return undoManager; }

    void undo();
    void redo();

    /** Disables the creation of a new transaction.
        Useful for lengthly operation like renders but don't keep around for very
        long or you will ruin your undo chain.
    */
    struct UndoTransactionInhibitor
    {
        UndoTransactionInhibitor (Edit&);
        UndoTransactionInhibitor (const UndoTransactionInhibitor&);
        ~UndoTransactionInhibitor();

    private:
        juce::WeakReference<Edit> edit;
    };

    static int getDefaultNumUndoLevels() noexcept               { return 30; }

    /** use this to tell the play engine to rebuild the audio graph and restart. */
    void restartPlayback();

    //==============================================================================
    TrackList& getTrackList()                                   { return *trackList; }

    void visitAllTracksRecursive (std::function<bool(Track&)>) const;
    void visitAllTopLevelTracks (std::function<bool(Track&)>) const;
    void visitAllTracks (std::function<bool(Track&)>, bool recursive) const;

    juce::ReferenceCountedObjectPtr<AudioTrack> insertNewAudioTrack (TrackInsertPoint, SelectionManager* selectionMnagerToSelectWith);
    juce::ReferenceCountedObjectPtr<FolderTrack> insertNewFolderTrack (TrackInsertPoint, SelectionManager* selectionMnagerToSelectWith, bool asSubmix);
    juce::ReferenceCountedObjectPtr<AutomationTrack> insertNewAutomationTrack (TrackInsertPoint, SelectionManager* selectionMnagerToSelectWith);
    Track::Ptr insertNewTrack (TrackInsertPoint, const juce::Identifier& xmlType, SelectionManager* selectionMnagerToSelectWith);
    Track::Ptr insertTrack (TrackInsertPoint, juce::ValueTree, SelectionManager* selectionMnagerToSelectWith);
    Track::Ptr insertTrack (juce::ValueTree, juce::ValueTree parent, juce::ValueTree preceeding, SelectionManager* selectionMnagerToSelectWith);
    juce::ReferenceCountedObjectPtr<AudioTrack> getOrInsertAudioTrackAt (int trackIndex);

    void moveTrack (Track::Ptr, TrackInsertPoint);
    void deleteTrack (Track*);
    void ensureNumberOfAudioTracks (int minimumNumTracks);
    void ensureTempoTrack();
    void ensureMarkerTrack();
    void ensureChordTrack();

    MarkerTrack* getMarkerTrack() const;
    TempoTrack* getTempoTrack() const;
    ChordTrack* getChordTrack() const;

    bool areAnyTracksMuted() const;
    bool areAnyTracksSolo() const;
    bool areAnyTracksSoloIsolate() const;

    void updateMuteSoloStatuses();

    EditItemID createNewItemID() const;
    EditItemID createNewItemID (const std::vector<EditItemID>& idsToAvoid) const;

    //==============================================================================
    juce::Array<Clip*> findClipsInLinkGroup (EditItemID linkGroupID) const;

    //==============================================================================
    void updateMirroredPlugin (Plugin&);
    void sendMirrorUpdateToAllPlugins (Plugin&) const;

    void sendStartStopMessageToPlugins();
    void muteOrUnmuteAllPlugins();

    PluginList& getMasterPluginList() const noexcept            { return *masterPluginList; }

    void addModifierTimer (ModifierTimer&);
    void removeModifierTimer (ModifierTimer&);
    void updateModifierTimers (PlayHead&, EditTimeRange streamTime, int numSamples) const;

    struct GlobalMacros : public MacroParameterElement
    {
        GlobalMacros (Edit&);
        Edit& edit;
    };

    MacroParameterElement& getGlobalMacros() const              { return *globalMacros; }

    //==============================================================================
    /** Toggles low latency monitoring for a set of plugins. */
    void setLowLatencyMonitoring (bool enabled, const juce::Array<EditItemID>& plugins);

    /** Returns true if in low latency monitoring mode. */
    bool getLowLatencyMonitoring() const noexcept               { return lowLatencyMonitoring; }

    /** First enables all currently disabled latency plugins and then disables the new set.
        Don't call directly, use setLowLatencyMonitoring as you might be in low latency mode
        but not actually have to disable any plugins.
    */
    void setLowLatencyDisabledPlugins (const juce::Array<EditItemID>& plugins);

    /** Returns the current set of diabled plugins. */
    juce::Array<EditItemID> getLowLatencyDisabledPlugins()      { return lowLatencyDisabledPlugins; }

    //==============================================================================
    RackTypeList& getRackList() const noexcept                  { jassert (rackTypes != nullptr); return *rackTypes; }
    TrackCompManager& getTrackCompManager() const noexcept      { jassert (trackCompManager != nullptr); return *trackCompManager; }

    //==============================================================================
    juce::String getAuxBusName (int bus) const;
    void setAuxBusName (int bus, const juce::String& name);

    //==============================================================================
    juce::Array<AutomatableParameter*> getAllAutomatableParams (bool includeTrackParams) const;

    //==============================================================================
    juce::File getVideoFile() const;
    void setVideoFile (const juce::File&, juce::String importDesc);
    void pickVideoFile();

    //==============================================================================
    /** Finds the next marker or start/end of a clip after a certain time. */
    double getNextTimeOfInterest (double afterThisTime);
    double getPreviousTimeOfInterest (double beforeThisTime);

    //==============================================================================
    PluginCache& getPluginCache() noexcept;

    double getFirstClipTime() const;
    double getLength() const;

    //==============================================================================
    VolumeAndPanPlugin::Ptr getMasterVolumePlugin() const               { return masterVolumePlugin; }

    // NB this is in slider position, not dB
    AutomatableParameter::Ptr getMasterSliderPosParameter() const       { jassert (masterVolumePlugin != nullptr); return masterVolumePlugin->volParam; }
    AutomatableParameter::Ptr getMasterPanParameter() const             { jassert (masterVolumePlugin != nullptr); return masterVolumePlugin->panParam; }

    void setMasterVolumeSliderPos (float);
    void setMasterPanPos (float);

    // Plugins should call this when one of their parameters or state changes to mark the edit as unsaved
    void pluginChanged (Plugin&) noexcept;

    //==============================================================================
    TempoSequence tempoSequence;

    TimecodeDisplayFormat getTimecodeFormat() const;
    void setTimecodeFormat (TimecodeDisplayFormat);
    void toggleTimecodeMode();

    void sendTempoOrPitchSequenceChangedUpdates();

    //==============================================================================
    PitchSequence pitchSequence;

    //==============================================================================
    MidiInputDevice* getCurrentMidiTimecodeSource() const;
    void setCurrentMidiTimecodeSource (MidiInputDevice* newDevice);
    void enableTimecodeSync (bool);
    bool isTimecodeSyncEnabled() const noexcept             { return midiTimecodeSourceDeviceEnabled; }

    double getTimecodeOffset() const noexcept               { return timecodeOffset; }
    void setTimecodeOffset (double newOffset);

    bool isMidiTimecodeIgnoringHours() const                { return midiTimecodeIgnoringHours; }
    void setMidiTimecodeIgnoringHours (bool shouldIgnore);

    MidiInputDevice* getCurrentMidiMachineControlSource() const;
    void setCurrentMidiMachineControlSource (MidiInputDevice*);

    MidiOutputDevice* getCurrentMidiMachineControlDest() const;
    void setCurrentMidiMachineControlDest (MidiOutputDevice*);

    void updateMidiTimecodeDevices();

    //==============================================================================
    void setClickTrackRange (EditTimeRange) noexcept;
    EditTimeRange getClickTrackRange() const noexcept;

    float getClickTrackVolume() const noexcept              { return juce::jlimit (0.2f, 1.0f, clickTrackGain.get()); }
    juce::String getClickTrackDevice() const;
    bool isClickTrackDevice (OutputDevice&) const;

    void setClickTrackOutput (const juce::String& deviceName);
    void setClickTrackVolume (float gain);

    //==============================================================================
    enum class CountIn
    {
        none      = 0,
        oneBar    = 1,
        twoBar    = 2,
        twoBeat   = 3,
        oneBeat   = 4
    };

    void setCountInMode (CountIn);
    CountIn getCountInMode() const;

    int getNumCountInBeats() const;

    //==============================================================================
    // sends a 'sourceMediaChanged' call to all the clips.
    void sendSourceFileUpdate();

    //==============================================================================
    /** Marks the edit as being significantly changed and should therefore be saved. */
    void markAsChanged();

    void invalidateStoredLength() noexcept              { totalEditLength = -1.0; }

    /** if there's a change to send out to the listeners, do it now rather than
        waiting for the next timer message.
    */
    void dispatchPendingUpdatesSynchronously();

    //==============================================================================
    void purgeOrphanFreezeAndProxyFiles();
    juce::String getFreezeFilePrefix() const;
    juce::Array<juce::File> getFrozenTracksFiles() const;

    bool areAnyClipsUsingFile (const AudioFile&);
    void cancelAllProxyGeneratorJobs() const;

    juce::File getTempDirectory (bool createIfNonExistent) const;
    void setTempDirectory (const juce::File&);

    //==============================================================================
    struct Metadata
    {
        juce::String artist, title, album, date, genre, comment, trackNumber;
    };

    Metadata getEditMetadata();
    void setEditMetadata (Metadata);

    //==============================================================================
    juce::ValueTree getAutomapState() const             { return automapState; }

    //==============================================================================
    MarkerManager& getMarkerManager() const noexcept    { return *markerManager; }

    struct EditChangeResetterTimer;
    std::unique_ptr<EditChangeResetterTimer> changeResetterTimer;

    //==============================================================================
    /** Calls an editFinishedLoading method on OwnerType once after the Edit has finished loading. */
    template<typename OwnerType>
    struct LoadFinishedCallback   : public Timer
    {
        LoadFinishedCallback (OwnerType& o, Edit& e) : owner (o), edit (e)
        {
            startTimer (10);
        }

        void timerCallback() override
        {
            if (edit.isLoading())
                return;

            stopTimer();
            owner.editFinishedLoading();
        }

        OwnerType& owner;
        Edit& edit;
    };

    //==============================================================================
    struct WastedMidiMessagesListener
    {
        virtual ~WastedMidiMessagesListener() {}
        virtual void warnOfWastedMidiMessages (InputDevice*, Track*) = 0;
    };

    void addWastedMidiMessagesListener (WastedMidiMessagesListener*);
    void removeWastedMidiMessagesListener (WastedMidiMessagesListener*);
    void warnOfWastedMidiMessages (InputDevice*, Track*);

    std::unique_ptr<ARADocumentHolder> araDocument;

    juce::CachedValue<juce::String> lastSignificantChange;
    juce::CachedValue<double> masterFadeIn, masterFadeOut, timecodeOffset, videoOffset;
    juce::CachedValue<AudioFadeCurve::Type> masterFadeInType, masterFadeOutType;
    juce::CachedValue<bool> midiTimecodeSourceDeviceEnabled, midiTimecodeIgnoringHours, videoMuted,
                            clickTrackEnabled, clickTrackEmphasiseBars, clickTrackRecordingOnly,
                            recordingPunchInOut, playInStopEnabled;
    juce::CachedValue<float> clickTrackGain;
    juce::CachedValue<ProjectItemID> videoSource;

    juce::ValueTree state { IDs::EDIT };
    juce::ValueTree inputDeviceState;

    SharedLevelMeasurer::Ptr getPreviewLevelMeasurer()          { return previewLevelMeasurer; }
    void setPreviewLevelMeasurer (SharedLevelMeasurer::Ptr p)   { previewLevelMeasurer = p; }

    using WeakRef = juce::WeakReference<Edit>;
    WeakRef::Master masterReference;

    WeakRef getWeakRef()                                        { return WeakRef (this); }

private:
    //==============================================================================
    const int instanceId;
    ProjectItemID editProjectItemID;

    // persistent properties (i.e. stuff that gets saved)
    juce::CachedValue<juce::String> clickTrackDevice;
    juce::CachedValue<juce::String> midiTimecodeSourceDevice, midiMachineControlSourceDevice, midiMachineControlDestDevice;
    juce::CachedValue<TimecodeDisplayFormat> timecodeFormat;
    juce::ValueTree auxBusses, controllerMappings, automapState;
    std::unique_ptr<ParameterControlMappings> parameterControlMappings;
    std::unique_ptr<RackTypeList> rackTypes;
    std::unique_ptr<PluginList> masterPluginList;

    // transient properties (i.e. stuff that doesn't get saved)
    struct MirroredPluginUpdateTimer;
    std::unique_ptr<MirroredPluginUpdateTimer> mirroredPluginUpdateTimer;
    juce::ReferenceCountedObjectPtr<VolumeAndPanPlugin> masterVolumePlugin;
    std::unique_ptr<TransportControl> transportControl;
    std::unique_ptr<AbletonLink> abletonLink;
    std::unique_ptr<AutomationRecordManager> automationRecordManager;
    std::unique_ptr<MarkerManager> markerManager;
    struct UndoTransactionTimer;
    std::unique_ptr<UndoTransactionTimer> undoTransactionTimer;
    struct PluginChangeTimer;
    std::unique_ptr<PluginChangeTimer> pluginChangeTimer;
    struct FrozenTrackCallback;
    std::unique_ptr<FrozenTrackCallback> frozenTrackCallback;
    std::unique_ptr<ParameterChangeHandler> parameterChangeHandler;
    std::unique_ptr<PluginCache> pluginCache;
    std::unique_ptr<TrackCompManager> trackCompManager;
    juce::Array<ModifierTimer*, juce::CriticalSection> modifierTimers;
    std::unique_ptr<GlobalMacros> globalMacros;

    mutable double totalEditLength = -1.0;
    std::atomic<bool> isLoadInProgress { true };
    bool performingRender = false;
    bool shouldRestartPlayback = false;
    bool blinkBright = false;
    bool lowLatencyMonitoring = false;
    bool hasChanged = false;
    bool ignoreLeftViewLimit;
    LoadContext* loadContext = nullptr;
    juce::UndoManager undoManager;
    int numUndoTransactionInhibitors = 0;
    mutable juce::File tempDirectory;
    juce::Array<EditItemID> lowLatencyDisabledPlugins;
    double normalLatencyBufferSizeSeconds = 0.0;
    bool isPreviewEdit = false;
    std::atomic<double> clickMark1Time { 0.0 }, clickMark2Time { 0.0 };
    std::atomic<bool> isFullyConstructed { false };

    const EditRole editRole;

    struct TreeWatcher;
    std::unique_ptr<TreeWatcher> treeWatcher;

    std::unique_ptr<TrackList> trackList;
    std::unique_ptr<EditInputDevices> editInputDevices;

    struct ChangedPluginsList;
    std::unique_ptr<ChangedPluginsList> changedPluginsList;

    SharedLevelMeasurer::Ptr previewLevelMeasurer;
    juce::ListenerList<WastedMidiMessagesListener> wastedMidiMessagesListeners;

    //==============================================================================
    friend struct TrackList;

    Track::Ptr createTrack (const juce::ValueTree&);
    Track::Ptr loadTrackFrom (juce::ValueTree&);
    void updateTrackStatuses();
    void updateTrackStatusesAsync();
    void moveTrackInternal (Track::Ptr, TrackInsertPoint);

    //==============================================================================
    void initialise();
    void undoOrRedo (bool isUndo);

    //==============================================================================
    void initialiseTempoAndPitch();
    void initialiseTimecode (juce::ValueTree&);
    void initialiseTransport();
    void initialiseMasterVolume();
    void initialiseVideo();
    void initialiseClickTrack();
    void initialiseTracks();
    void initialiseAudioDevices();
    void initialiseRacks();
    void initialiseAuxBusses();
    void initialiseMasterPlugins();
    void initialiseMetadata();
    void initialiseControllerMappings();
    void initialiseAutomap();
    void initialiseARA();
    void removeZeroLengthClips();
    void loadTracks();
    void loadOldTimeSigInfo();
    void loadOldVideoInfo (const juce::ValueTree&);
    void loadOldStyleMarkers();

    void timerCallback() override;

    void readFrozenTracksFiles();
    void updateFrozenTracks();
    void needToUpdateFrozenTracks();

    void sanityCheckTrackNames();
    void updateMirroredPlugins();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Edit)
};

//==============================================================================
/** Deferred Edit object deleter. @see Engine::getEditDeleter() */
struct EditDeleter  : private juce::AsyncUpdater
{
    ~EditDeleter();

    void deleteEdit (std::unique_ptr<Edit>);

private:
    friend class Engine;
    EditDeleter();

    juce::CriticalSection listLock;
    juce::OwnedArray<Edit> editsToDelete;

    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditDeleter)
};

//==============================================================================
/** A list of all currently open edits. @see Engine::getActiveEdits() */
struct ActiveEdits
{
    juce::Array<Edit*> getEdits() const;

private:
    friend class Engine;
    friend class Edit;
    friend class TransportControl;
    juce::Array<Edit*, juce::CriticalSection> edits;
    std::atomic<int> numTransportsPlaying;

    ActiveEdits();
};

} // namespace tracktion_engine
