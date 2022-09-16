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

 class ClipEffect;
//==============================================================================
/**
    The Tracktion Edit class!

    An Edit is the containing class for an arrangement that can be played back.
    It contains all the per-session objects such as tracks, tempo sequences,
    pitches, input devices, Racks, master plugins etc. and any per-Edit management
    objects such as UndoManager, PluginCache etc.

    Most sub-objects such as Tracks, Clips etc. will all know which Edit they
    belong to by having a reference to this Edit.

    To create an Edit to you need construct one with an Edit::Options instance
    supplying at least the Engine to use, the ValueTree state and a ProjectItemID
    to uniquely identify this Edit.
 
    This is a high level overview of the Edit structure and the relevant objects.
    Note that this isn't an exhaustive list but should help you find the most relevant classes.
    - Edit
        - TempoSequence
            - TimeSigSetting[s]
            - TempoSetting[s]
        - PitchSequence
            - PitchSetting
        - TrackList
            - Track[s] (ArrangerTrack, MarkerTrack, TempoTrack, ChordTrack, AudioTrack, FolderTrack, AutomationTrack)
                - Clip[s] (WaveAudioClip, MidiClip, StepClip, EditClip)
                - PluginList
                    - Plugin[s] (VolumeAndPanPlugin, LevelMeterPlugin, ExternalPlugin)
                        - AutomatableParameter[s]
                            - AutomationCurve
                - TrackList (AudioTrack, FolderTrack, AutomationTrack)
        - PluginList
            - Plugin[s]
        - RackTypeList
            - RackType[s]
        - TransportControl
        - ParameterChangeHandler
        - ParameterControlMappings
        - AutomationRecordManager
        - juce::UndoManager

    @see EditFileOperations, tracktion_EditUtilities.h
*/
class Edit  : public Selectable,
              private juce::Timer
{
public:
    //==============================================================================
    /** Enum used to determine what an Edit is being used for. */
    enum EditRole
    {
        playDisabled        = 1,    /**< Determines if an EditPlaybackContext is created */
        proxiesDisabled     = 2,    /**< Determines if clips and create proxies. */
        pluginsDisabled     = 4,    /**< Determines if plugins should be loaded. */

        forEditing          = 0,                                                    /**< Creates an Edit for normal use. */
        forRendering        = playDisabled,                                         /**< Creates an Edit for rendering, not output device playback. */
        forExporting        = playDisabled + proxiesDisabled + pluginsDisabled,     /**< Creates an Edit for exporting/archiving, not playback/rendering. */
        forExamining        = playDisabled + proxiesDisabled + pluginsDisabled      /**< Creates an Edit for examining (listing source files etc). */
    };

    /**
        A context passed to the Options struct which will get updated about load
        process and can be signaled to stop loading the Edit.
    */
    struct LoadContext
    {
        std::atomic<float> progress  { 0.0f };  /**< Progress will be updated as the Edit loads. */
        std::atomic<bool> completed  { false }; /**< Set to true once the Edit has loaded. */
        std::atomic<bool> shouldExit { false }; /**< Can be set to true to cancel loading the Edit. */
    };

    //==============================================================================
    /** Determines how the Edit will be created */
    struct Options
    {
        Engine& engine;                                                         /**< The Engine to use. */
        juce::ValueTree editState;                                              /**< The Edit state. @see createEmptyEdit */
        ProjectItemID editProjectItemID;                                        /**< The editProjectItemID, must be valid. */

        EditRole role = forEditing;                                             /**< An optional role to open the Edit with. */
        LoadContext* loadContext = nullptr;                                     /**< An optional context to be monitor for loading status. */
        int numUndoLevelsToStore = Edit::getDefaultNumUndoLevels();             /**< The number of undo levels to use. */

        std::function<juce::File()> editFileRetriever = {};                     /**< An optional editFileRetriever to use. */
        std::function<juce::File (const juce::String&)> filePathResolver = {};  /**< An optional filePathResolver to use. */
    };

    /** Creates an Edit from a set of Options. */
    Edit (Options);

    /** Legacy Edit constructor, will be deprecated soon, use the other consturctor that takes an Options. */
    Edit (Engine&, juce::ValueTree, EditRole, LoadContext*, int numUndoLevelsToStore);

    /** Destructor. */
    ~Edit() override;

    /** A reference to the Engine. */
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
        1. If the path is an absolute path it will simply return it
        2. If it is a relative path, the editFileRetriever will be used to try and find
           a File for the Edit and the path will be resolved against that

        You can set a custom resolver here in case the Edit is
        in memory or the files directory is different to the Edit file's location.
    */
    std::function<juce::File (const juce::String&)> filePathResolver;

    /** Sets the ProjectItemID of the Edit, this is also stored in the state. */
    void setProjectItemID (ProjectItemID);

    /** Returns the ProjectItemID of the Edit. */
    ProjectItemID getProjectItemID() const noexcept    { return editProjectItemID; }

    //==============================================================================
    /** Returns the EditRole. */
    EditRole getEditRole() const noexcept   { return editRole; }
    
    /** Returns true if this Edit should be played back (or false if it was just opened for inspection). */
    bool shouldPlay() const noexcept        { return (editRole & playDisabled) == 0; }

    /** Returns true if this Edit can render proxy files. */
    bool canRenderProxies() const noexcept  { return (editRole & proxiesDisabled) == 0; }

    /** Returns true if this Edit can load Plugin[s]. */
    bool shouldLoadPlugins() const noexcept { return (editRole & pluginsDisabled) == 0; }

    /** Returns true if this Edit is a temporary Edit for previewing files/clips etc. */
    bool getIsPreviewEdit() const noexcept  { return isPreviewEdit; }

    //==============================================================================
    /** The maximum length an Edit can be. */
    static constexpr double maximumLength = 48.0 * 60.0 * 60.0;
    
    /** Returns the maximum length an Edit can be. */
    static TimeDuration getMaximumLength()          { return TimeDuration::fromSeconds (maximumLength); }

    /** Returns the maximum length an Edit can be. */
    static TimeRange getMaximumEditTimeRange()      { return { TimePosition(), TimePosition::fromSeconds (maximumLength) }; }

    static TimePosition getMaximumEditEnd()         { return getMaximumEditTimeRange().getEnd(); }

    static const int ticksPerQuarterNote = 960; /**< The number of ticks per quarter note. */

    //==============================================================================
    /** Saves the plugin, automap and ARA states to the state ValueTree. */
    void flushState();
    
    /** Saves the specified plugin state to the state ValueTree. */
    void flushPluginStateIfNeeded (Plugin&);

    /** Returns the time the last change occurred.
        If no modifications occurred since this object was initialised, this returns the Time the Edit was last saved.
    */
    juce::Time getTimeOfLastChange() const;

    /** Resets the changed status so hasChangedSinceSaved returns false. */
    void resetChangedStatus();

    /** Returns true if the Edit has changed since it was last saved. */
    bool hasChangedSinceSaved() const;

    /** Returns true if the Edit's not yet fully loaded */
    bool isLoading() const                                              { return isLoadInProgress; }

    /** Creates an Edit for previewing a file. */
    static std::unique_ptr<Edit> createEditForPreviewingFile (Engine&, const juce::File&, const Edit* editToMatch,
                                                              bool tryToMatchTempo, bool tryToMatchPitch, bool* couldMatchTempo,
                                                              juce::ValueTree midiPreviewPlugin,
                                                              juce::ValueTree midiDrumPreviewPlugin = {},
                                                              bool forceMidiToDrums = false,
                                                              Edit* editToUpdate = {});

    /** Creates an Edit for previewing a preset. */
    static std::unique_ptr<Edit> createEditForPreviewingPreset (Engine& engine, juce::ValueTree, const Edit* editToMatch,
                                                                bool tryToMatchTempo, bool* couldMatchTempo,
                                                                juce::ValueTree midiPreviewPlugin,
                                                                juce::ValueTree midiDrumPreviewPlugin = {},
                                                                bool forceMidiToDrums = false,
                                                                Edit* editToUpdate = {});

    /** Creates an Edit for previewing a Clip. */
    static std::unique_ptr<Edit> createEditForPreviewingClip (Clip&);

    /** Creates an Edit with a single AudioTrack. */
    static std::unique_ptr<Edit> createSingleTrackEdit (Engine&);

    //==============================================================================
    /** Quick way to find and iterate all Track[s] in the Edit. */
    EditItemCache<Track> trackCache;

    /** Quick way to find and iterate all Clip[s] in the Edit. */
    EditItemCache<Clip> clipCache;

    //==============================================================================
    /** Returns the EditInputDevices for the Edit. */
    EditInputDevices& getEditInputDevices() noexcept;
    
    /** Returns an InputDeviceInstance for a global InputDevice. */
    InputDeviceInstance* getCurrentInstanceForInputDevice (InputDevice*) const;

    /** Returns all the active InputDeviceInstance[s] in the Edit. */
    juce::Array<InputDeviceInstance*> getAllInputDevices() const;

    //==============================================================================
    /** Returns the TransportControl which is used to stop/stop/position playback and recording. */
    TransportControl& getTransport() const noexcept                     { return *transportControl; }

    /** Returns the active EditPlaybackContext which is what is attached to the DeviceManager. */
    EditPlaybackContext* getCurrentPlaybackContext() const;

    //==============================================================================
    /** Returns the ParameterChangeHandler for the Edit. */
    ParameterChangeHandler& getParameterChangeHandler() noexcept        { return *parameterChangeHandler; }

    /** Returns the ParameterControlMappings for the Edit. */
    ParameterControlMappings& getParameterControlMappings() noexcept    { return *parameterControlMappings; }

    /** Returns the AutomationRecordManager for the Edit.
        Used to change automation read/write modes and start/stop automation recording.
    */
    AutomationRecordManager& getAutomationRecordManager() noexcept      { return *automationRecordManager; }

    /** Returns the AbletonLink object.
        Used to sync an Edit's playback with an AbletonLink session.
    */
    AbletonLink& getAbletonLink() const noexcept                        { return *abletonLink; }

    //==============================================================================
    /**
        Temporarily removes an Edit from the device manager, optionally re-adding it on destruction.
    */
    struct ScopedRenderStatus
    {
        /** Constructs a ScopedRenderStatus, removing an Edit from the device manager.
            @param shouldReallocateOnDestruction Re-attaches the Edit to the output when this goes out of scope
        */
        ScopedRenderStatus (Edit&, bool shouldReallocateOnDestruction);
        
        /** Destructor.
            Re-allocates an EditPlaybackContext if this is the last object for this Edit.
        */
        ~ScopedRenderStatus();

    private:
        Edit& edit;
        const bool reallocateOnDestruction;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedRenderStatus)
    };

    /** Returns true if the Edit is currently being rendered. */
    bool isRendering() const noexcept                           { return performingRenderCount.load() > 0; }

    //==============================================================================
    /** Initialises all the plugins. Usually you'd call this once after loading an Edit. */
    void initialiseAllPlugins();

    //==============================================================================
    /** Retuns the description of this Selectable. */
    juce::String getSelectableDescription() override            { return TRANS("Edit") + " - \"" + getName() + "\""; }

    //==============================================================================
    /** Returns the juce::UndoManager used for this Edit. */
    juce::UndoManager& getUndoManager() noexcept                { return undoManager; }

    /** Undoes the most recent changes made. */
    void undo();

    /** Redoes the changes undone by the last undo. */
    void redo();

    /** Disables the creation of a new transaction.
        Useful for lengthly operation like renders but don't keep around for very
        long or you will ruin your undo chain.
    */
    struct UndoTransactionInhibitor
    {
        /** Creates an UndoTransactionInhibitor for an Edit. */
        UndoTransactionInhibitor (Edit&);
        
        /** Creates a copy of UndoTransactionInhibitor for an Edit. */
        UndoTransactionInhibitor (const UndoTransactionInhibitor&);

        /** Destructor. If this is the last UndoTransactionInhibitor,
            this will re-allow undo transactions.
        */
        ~UndoTransactionInhibitor();

    private:
        Edit::WeakRef edit;
    };

    /** Returns the default number of undo levels that should be used. */
    static int getDefaultNumUndoLevels() noexcept               { return 30; }

    /** Use this to tell the play engine to rebuild the audio graph if the toplogy has changed.
        You shouldn't normally need to use this as it's called automatically as
        track/clips/plugins etc. are added/removed/changed.
    */
    void restartPlayback();

    //==============================================================================
    /** Returns the TrackList for the Edit which contains all the top level tracks. */
    TrackList& getTrackList()                                   { return *trackList; }

    /** Visits all tracks in the Edit with the given function. Return false to stop the traversal. */
    void visitAllTracksRecursive (std::function<bool(Track&)>) const;

    /** Visits all top-level tracks (i.e. non-recursively) in the Edit with the given function. */
    void visitAllTopLevelTracks (std::function<bool(Track&)>) const;

    /** Visits all tracks in the Edit with the given function.
        @param recursive Whether to visit nested tracks or just top level tracks.
    */
    void visitAllTracks (std::function<bool(Track&)>, bool recursive) const;

    //==============================================================================
    /** Inserts a new AudioTrack in the Edit. */
    juce::ReferenceCountedObjectPtr<AudioTrack> insertNewAudioTrack (TrackInsertPoint, SelectionManager*);

    /** Inserts a new FolderTrack in the Edit, optionally as a submix. */
    juce::ReferenceCountedObjectPtr<FolderTrack> insertNewFolderTrack (TrackInsertPoint, SelectionManager*, bool asSubmix);

    /** Inserts a new AutomationTrack in the Edit. */
    juce::ReferenceCountedObjectPtr<AutomationTrack> insertNewAutomationTrack (TrackInsertPoint, SelectionManager*);

    /** Inserts a new Track with the given type in the Edit. */
    Track::Ptr insertNewTrack (TrackInsertPoint, const juce::Identifier& xmlType, SelectionManager*);

    /** Inserts a new Track with the given state in the Edit. */
    Track::Ptr insertTrack (TrackInsertPoint, juce::ValueTree, SelectionManager*);

    /** Inserts a new Track with the given state in the Edit.
        @param parent       The track the new track should be nested in to.
                            If this is invalid, it will be a top-level track.
        @param preceeding   The track new track should go after.
                            If this is invalid, it will go at the end of the list.
    */
    Track::Ptr insertTrack (juce::ValueTree, juce::ValueTree parent, juce::ValueTree preceeding, SelectionManager*);

    //==============================================================================
    /** Moves a track to a new position. */
    void moveTrack (Track::Ptr, TrackInsertPoint);

    /** Deletes a Track. */
    void deleteTrack (Track*);

    /** Creates new tracks to ensure the minimum number. */
    void ensureNumberOfAudioTracks (int minimumNumTracks);

    /** Creates an ArrangerTrack if there isn't currently one. */
    void ensureArrangerTrack();

    /** Creates a TempoTrack if there isn't currently one. */
    void ensureTempoTrack();

    /** Creates a MarkerTrack if there isn't currently one. */
    void ensureMarkerTrack();

    /** Creates a ChordTrack if there isn't currently one. */
    void ensureChordTrack();

    /** Creates a ChordTrack if there isn't currently one. */
    void ensureMasterTrack();
    
    /** Returns the global ArrangerTrack. */
    ArrangerTrack* getArrangerTrack() const;

    /** Returns the global MarkerTrack. */
    MarkerTrack* getMarkerTrack() const;

    /** Returns the global TempoTrack. */
    TempoTrack* getTempoTrack() const;

    /** Returns the global ChordTrack. */
    ChordTrack* getChordTrack() const;

    /** Returns the global MasterTrack. */
    MasterTrack* getMasterTrack() const;

    //==============================================================================
    /** Returns true if any tracks are muted. */
    bool areAnyTracksMuted() const;

    /** Returns true if any tracks are soloed. */
    bool areAnyTracksSolo() const;

    /** Returns true if any tracks are solo isolated. */
    bool areAnyTracksSoloIsolate() const;

    /** Updates all the tracks and external controller mute/solo statuses.
        You shouldn't normally need to call this, it's internally updated when solo/mute properties change.
    */
    void updateMuteSoloStatuses();

    //==============================================================================
    /** Returns a new EditItemID to use for a new EditItem. */
    EditItemID createNewItemID() const;

    /** Returns a new EditItemID to use for a new EditItem, avoiding some IDs. */
    EditItemID createNewItemID (const std::vector<EditItemID>& idsToAvoid) const;

    //==============================================================================
    /** Returns and Clip[s] with the given linkGroupID.
        @see Clip::setLinkGroupID
    */
    juce::Array<Clip*> findClipsInLinkGroup (juce::String linkGroupID) const;

    //==============================================================================
    /** Adds this plugin to a list so mirrored Plugin[s] are updated asyncronously. */
    void updateMirroredPlugin (Plugin&);

    /** Syncronously updates all Plugins[s] mirroring this one. */
    void sendMirrorUpdateToAllPlugins (Plugin&) const;

    /** Calls Plugin::playStartedOrStopped to handle automation reacording.
        Not intended for public use as it will be called automatically by the TransportControl.
    */
    void sendStartStopMessageToPlugins();
    
    /** Returns the master PluginList. */
    PluginList& getMasterPluginList() const noexcept            { return *masterPluginList; }

    //==============================================================================
    /** Adds a ModifierTimer to be updated each block. */
    void addModifierTimer (ModifierTimer&);

    /** Removes a ModifierTimer previously added. */
    void removeModifierTimer (ModifierTimer&);

    /** Updates all the ModifierTimers with a given edit time and number of samples. */
    void updateModifierTimers (TimePosition editTime, int numSamples) const;

    /** Holds the global Macros for the Edit. */
    struct GlobalMacros : public MacroParameterElement
    {
        GlobalMacros (Edit&);
        Edit& edit;
    };

    /** Returns global MacroParameterElement. */
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
    /** Returns the RackTypeList which contains all the RackTypes for the Edit. */
    RackTypeList& getRackList() const noexcept                  { jassert (rackTypes != nullptr); return *rackTypes; }

    /** Returns the TrackCompManager for the Edit. */
    TrackCompManager& getTrackCompManager() const noexcept      { jassert (trackCompManager != nullptr); return *trackCompManager; }

    //==============================================================================
    /** Returns the name of an aux bus. */
    juce::String getAuxBusName (int bus) const;

    /** Sets the name of an aux bus. */
    void setAuxBusName (int bus, const juce::String& name);

    //==============================================================================
    /** Returns all automatiabel parameters in an Edit.
        @param includeTrackParams Whether to include plugins on tracks and clips
    */
    juce::Array<AutomatableParameter*> getAllAutomatableParams (bool includeTrackParams) const;

    //==============================================================================
    /** Returns the currently set video file. */
    juce::File getVideoFile() const;

    /** Sets a video file to display. */
    void setVideoFile (const juce::File&, juce::String importDesc);

    //==============================================================================
    /** Finds the next marker or start/end of a clip after a certain time. */
    TimePosition getNextTimeOfInterest (TimePosition afterThisTime);

    /** Finds the previous marker or start/end of a clip after a certain time. */
    TimePosition getPreviousTimeOfInterest (TimePosition beforeThisTime);

    //==============================================================================
    /** Returns the PluginCache which manages all active Plugin[s] for this Edit. */
    PluginCache& getPluginCache() noexcept;

    /** Returns the time of first clip. */
    TimePosition getFirstClipTime() const;

    /** Returns the end time of last clip. */
    TimeDuration getLength() const;

    //==============================================================================
    /** Returns the master VolumeAndPanPlugin. */
    VolumeAndPanPlugin::Ptr getMasterVolumePlugin() const               { return masterVolumePlugin; }

    /** Returns the master volume AutomatableParameter.
        N.B. this is in slider position, not dB
    */
    AutomatableParameter::Ptr getMasterSliderPosParameter() const       { jassert (masterVolumePlugin != nullptr); return masterVolumePlugin->volParam; }

    /** Returns the master pan AutomatableParameter. */
    AutomatableParameter::Ptr getMasterPanParameter() const             { jassert (masterVolumePlugin != nullptr); return masterVolumePlugin->panParam; }

    /** Sets the master volume level.
        N.B. this is in slider position, not dB
    */
    void setMasterVolumeSliderPos (float);

    /** Returns the master pan position. */
    void setMasterPanPos (float);

    /** Plugins should call this when one of their parameters or state changes to mark the edit as unsaved. */
    void pluginChanged (Plugin&) noexcept;

    //==============================================================================
    /** The global TempoSequence of this Edit. */
    TempoSequence tempoSequence;

    /** Returns the current TimecodeDisplayFormat. */
    TimecodeDisplayFormat getTimecodeFormat() const;

    /** Sets the TimecodeDisplayFormat to use. */
    void setTimecodeFormat (TimecodeDisplayFormat);

    /** Toggles the TimecodeDisplayFormat through the available TimecodeType[s].
        @see TimecodeType
    */
    void toggleTimecodeMode();

    /** Sends a message to all the clips to let them know the tempo or pitch sequence has changed.
        @seee Clip::pitchTempoTrackChanged
    */
    void sendTempoOrPitchSequenceChangedUpdates();

    //==============================================================================
    /** The global PitchSequence of this Edit. */
    PitchSequence pitchSequence;

    //==============================================================================
    /** Returns the MidiInputDevice being used as the MIDI timecode source. */
    MidiInputDevice* getCurrentMidiTimecodeSource() const;

    /** Sets the MidiInputDevice being to be used as the MIDI timecode source. */
    void setCurrentMidiTimecodeSource (MidiInputDevice* newDevice);

    /** Toggles syncing to MIDI timecode.
        @see midiTimecodeSourceDeviceEnabled
    */
    void enableTimecodeSync (bool);

    /** Returns true if syncing to MIDI timecode is enabled. */
    bool isTimecodeSyncEnabled() const noexcept             { return midiTimecodeSourceDeviceEnabled; }

    /** Returns the offset to apply to MIDI timecode. */
    TimeDuration getTimecodeOffset() const noexcept         { return timecodeOffset; }

    /** Sets the offset to apply to MIDI timecode. */
    void setTimecodeOffset (TimeDuration newOffset);

    /** Returns true if hours are ignored when syncing to MIDI timecode. */
    bool isMidiTimecodeIgnoringHours() const                { return midiTimecodeIgnoringHours; }

    /** Sets whether hours are ignored when syncing to MIDI timecode. */
    void setMidiTimecodeIgnoringHours (bool shouldIgnore);

    /** Returns the MidiInputDevice being used as an MMC source. */
    MidiInputDevice* getCurrentMidiMachineControlSource() const;

    /** Sets the MidiInputDevice to be used as an MMC source. */
    void setCurrentMidiMachineControlSource (MidiInputDevice*);

    /** Returns the MidiInputDevice being used as an MMC destination. */
    MidiOutputDevice* getCurrentMidiMachineControlDest() const;

    /** Sets the MidiInputDevice to be used as an MMC destination. */
    void setCurrentMidiMachineControlDest (MidiOutputDevice*);

    /** Updates the MIDI timecode/MMC devices.
        You shouldn't normally need to call this as it will be called automatically when one of the
        setter methods are called.
    */
    void updateMidiTimecodeDevices();

    //==============================================================================
    /** Sets a range for the click track to be audible within. */
    void setClickTrackRange (TimeRange) noexcept;

    /** Returns the range the click track will be audible within. */
    TimeRange getClickTrackRange() const noexcept;

    /** Returns the click track volume. */
    float getClickTrackVolume() const noexcept              { return juce::jlimit (0.2f, 1.0f, clickTrackGain.get()); }

    /** Returns the name of the device being used as the click track output. */
    juce::String getClickTrackDevice() const;

    /** Returns true if the given OutputDevice is being used as the click track output. */
    bool isClickTrackDevice (OutputDevice&) const;

    /** Sets the device to use as the click track output. */
    void setClickTrackOutput (const juce::String& deviceName);

    /** Sets the volume of the click track. */
    void setClickTrackVolume (float gain);

    //==============================================================================
    /** An enum to determine the duration of the count in. */
    enum class CountIn
    {
        none      = 0,  /**< No count in, play starts immidiately. */
        oneBar    = 1,  /**< One bar count in. */
        twoBar    = 2,  /**< Two bars count in. */
        twoBeat   = 3,  /**< Two beats count in. */
        oneBeat   = 4   /**< One beat count in. */
    };

    /** Sets the duration of the count in. */
    void setCountInMode (CountIn);

    /** Returns the duration of the count in. */
    CountIn getCountInMode() const;

    /** Returns the number of beats of the count in. */
    int getNumCountInBeats() const;

    //==============================================================================
    /** Sends a 'sourceMediaChanged' call to all the clips. */
    void sendSourceFileUpdate();

    //==============================================================================
    /** Marks the edit as being significantly changed and should therefore be saved. */
    void markAsChanged();

    /** Invalidates the stored length so the next call to getLength will update form the Edit contents.
        You shouldn't normally need to call this as it will happen automatically as clips are added/removed.
    */
    void invalidateStoredLength() noexcept              { totalEditLength = {}; }

    /** If there's a change to send out to the listeners, do it now rather than
        waiting for the next timer message.
    */
    void dispatchPendingUpdatesSynchronously();

    //==============================================================================
    /** Returns true if any clips are using this file. */
    bool areAnyClipsUsingFile (const AudioFile&);

    /** Stops all proxy generator jobs clips may be performing. */
    void cancelAllProxyGeneratorJobs() const;

    /** Returns the temp directory the Edit it using. */
    juce::File getTempDirectory (bool createIfNonExistent) const;

    /** Sets the temp directory for the Edit to use. */
    void setTempDirectory (const juce::File&);

    //==============================================================================
    /** Metadata for the Edit. */
    struct Metadata
    {
        juce::String artist, title, album, date, genre, comment, trackNumber;
    };

    /** Returns the current Metadata for the Edit. */
    Metadata getEditMetadata();

    /** Sets the Metadata for the Edit. */
    void setEditMetadata (Metadata);

    //==============================================================================
    /** Returns the ValueTree used as the Auotmap state.
        You shouldn't normally need this as it's onyl called by the Automap system.
        @see NovationAutomap
    */
    juce::ValueTree getAutomapState() const             { return automapState; }

    //==============================================================================
    /** Returns the MarkerManager. */
    MarkerManager& getMarkerManager() const noexcept    { return *markerManager; }

    //==============================================================================
    /** Calls an editFinishedLoading method on OwnerType once after the Edit has finished loading. */
    template<typename OwnerType>
    struct LoadFinishedCallback   : public Timer
    {
        LoadFinishedCallback (OwnerType& o, Edit& e)
            : owner (o), edit (e)
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
    /** Interface for classes that need to know about unused MIDI messages. */
    struct WastedMidiMessagesListener
    {
        /** Destructor. */
        virtual ~WastedMidiMessagesListener() = default;

        /** Callback to be ntified when a MIDI message isn't used by a track because it
            doesn't have a plugin which receives MIDI input on it.
        */
        virtual void warnOfWastedMidiMessages (InputDevice*, Track*) = 0;
    };

    /** Add a WastedMidiMessagesListener to be notified of wasted MIDI messages. */
    void addWastedMidiMessagesListener (WastedMidiMessagesListener*);

    /** Removes a previously added WastedMidiMessagesListener. */
    void removeWastedMidiMessagesListener (WastedMidiMessagesListener*);
    
    /** Triggers a callback to any registered WastedMidiMessagesListener[s]. */
    void warnOfWastedMidiMessages (InputDevice*, Track*);

    //==============================================================================
    /** Returns a previously set SharedLevelMeasurer. */
    SharedLevelMeasurer::Ptr getPreviewLevelMeasurer()          { return previewLevelMeasurer; }

    /** Sets a SharedLevelMeasurer to use. */
    void setPreviewLevelMeasurer (SharedLevelMeasurer::Ptr p)   { previewLevelMeasurer = p; }

    //==============================================================================
    juce::CachedValue<juce::String> lastSignificantChange;  /**< The last time a change was made to the Edit. @see getTimeOfLastChange */

    juce::CachedValue<TimeDuration> masterFadeIn,   /**< The duration in seconds of the fade in. */
                                    masterFadeOut,  /**< The duration in seconds of the fade out. */
                                    timecodeOffset,       /**< The duration in seconds of the timecode offset. */
                                    videoOffset;          /**< The duration in seconds of the video offset. */

    juce::CachedValue<AudioFadeCurve::Type> masterFadeInType,   /**< The curve type of the fade in. */
                                            masterFadeOutType;  /**< The curve type of the fade out. */

    juce::CachedValue<bool> midiTimecodeSourceDeviceEnabled,    /**< Whether a MIDI timecode source is enabled. */
                            midiTimecodeIgnoringHours,          /**< Whether the MIDI timecode source ignores hours. */
                            videoMuted,                         /**< Whether the video source is muted. */
                            clickTrackEnabled,                  /**< Whether the click track is enabled. */
                            clickTrackEmphasiseBars,            /**< Whether the click track should emphasise bars. */
                            clickTrackRecordingOnly,            /**< Whether the click track should be audible only when recording. */
                            recordingPunchInOut,                /**< Whether recoridng only happens within the in/out markers. */
                            playInStopEnabled;                  /**< Whether the audio engine should run when playback is stopped. */
    juce::CachedValue<float> clickTrackGain;        /**< The gain of the click track. */
    juce::CachedValue<ProjectItemID> videoSource;   /**< The ProjectItemID of the video source. */

    juce::ValueTree state { IDs::EDIT };    /**< The ValueTree of the Edit state. */
    juce::ValueTree inputDeviceState;   /**< The ValueTree of the input device states. */

    /** Holds the ARA state. */
    std::unique_ptr<ARADocumentHolder> araDocument;

private:
    //==============================================================================
    const int instanceId;
    std::atomic<ProjectItemID> editProjectItemID { ProjectItemID() };

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

    mutable std::optional<TimeDuration> totalEditLength;
    std::atomic<bool> isLoadInProgress { true };
    std::atomic<int> performingRenderCount { 0 };
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
    std::atomic<TimePosition> clickMark1Time { TimePosition() }, clickMark2Time { TimePosition() };
    std::atomic<bool> isFullyConstructed { false };
    mutable std::atomic<uint64_t> nextID { 0 }; // 0 is used as flag to initialise the next ID count
    
   #if JUCE_DEBUG
    mutable std::unordered_set<EditItemID> usedIDs;
   #endif
    
    const EditRole editRole;

    struct TreeWatcher;
    std::unique_ptr<TreeWatcher> treeWatcher;

    std::unique_ptr<TrackList> trackList;
    std::unique_ptr<EditInputDevices> editInputDevices;

    struct ChangedPluginsList;
    std::unique_ptr<ChangedPluginsList> changedPluginsList;

    struct EditChangeResetterTimer;
    std::unique_ptr<EditChangeResetterTimer> changeResetterTimer;

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
    ~EditDeleter() override;

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
    std::atomic<int> numTransportsPlaying { 0 };

    ActiveEdits();
};

}} // namespace tracktion { inline namespace engine
