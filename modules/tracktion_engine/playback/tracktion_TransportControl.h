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

/** Moves the transport to the start of the selected objects. */
void toStart (TransportControl&, const SelectableList&);

/** Moves the transport to the end of the selected objects. */
void toEnd (TransportControl&, const SelectableList&);

/** Moves the transport back to the previous point of interest. */
void tabBack (TransportControl&);

/** Moves the transport forwards to the next point of interest. */
void tabForward (TransportControl&);

/** Sets the mark in position to the current transport position. */
void markIn (TransportControl&);

/** Sets the mark out position to the current transport position. */
void markOut (TransportControl&);

/** Scrubs back and forth in 'units', where a unit is about 1/50th of
    the width of the strip window
*/
void scrub (TransportControl&, double unitsForwards);

/** Frees the playback context if no recording is in progress, useful for when
    an app is minimised.
*/
void freePlaybackContextIfNotRecording (TransportControl&);


//==============================================================================
/**
    Controls the transport of an Edit's playback.
    This is responsible for starting/stopping playback and recording and changing
    the position etc. It deals with looping/fast forward etc. and is resonsible
    for managing the EditPlaybackContext which attaches the Edit to the DeviceManager.
    
    It also has higher level user concepts such as dragging/scrubbing.
    @see EditPlaybackContext, DeviceManager
 
    You shouldn't need to directly create one of these, create an Edit and then
    obtain it from there. @see Edit::getTransport
*/
class TransportControl   : public juce::ChangeBroadcaster,
                           private juce::Timer

{
public:
    /** Constructs a TransportControl for an Edit. */
    TransportControl (Edit&, const juce::ValueTree&);
    /** Destructor. */
    ~TransportControl() override;

    //==============================================================================
    /** Starts playback of an Edit.
        @param justSendMMCIfEnabled If this is true, playback isn't actually started,
                                    an MMC message is just output and playback will start
                                    when the input MIDI device recieves one.
    */
    void play (bool justSendMMCIfEnabled);

    /** Plays a section of an Edit then stops playback, useful for previewing clips. */
    void playSectionAndReset (TimeRange rangeToPlay);

    /** Starts recording. This will also start playback if stopped.
        @param justSendMMCIfEnabled             If this is true, playback isn't actually started,
                                                an MMC message is just output and recording will start
                                                when the input MIDI device recieves one.
        @param allowRecordingIfNoInputsArmed    If true, no inputs need to actually be armed so you
                                                can live-punch on the fly.
    */
    void record (bool justSendMMCIfEnabled, bool allowRecordingIfNoInputsArmed = false);

    /** Stops recording, creating clips of newyly recorded files/MIDI data.
        @param discardRecordings    If true, recordings will be discarded
        @param clearDevices         If true, the playback graph will be cleared
        @param canSendMMCStop       If true, an MMC stop message will also be sent
        @param invertReturnToStartPosSelection  If true, the return to start behaviour will be inverted
    */
    void stop (bool discardRecordings,
               bool clearDevices,
               bool canSendMMCStop = true,
               bool invertReturnToStartPosSelection = false);
    
    /** Stops playback only if recording is currently in progress.
        @see isRecording
    */
    void stopIfRecording();

    //==============================================================================
    /** Applys a retrospective record to any assigned input devices, creating clips
        for any historical input.
    */
    juce::Result applyRetrospectiveRecord();
    
    /** Perfoms a retrospective record operation and returns any new files. */
    juce::Array<juce::File> getRetrospectiveRecordAsAudioFiles();

    /** Syncs this Edit's playback to another Edit.
        @param editToSyncTo             The Edit to sync playback to
        @param syncToTargetLoopLength   If true the sync interval will be the source's
                                        loop length, if false, it will be one bar
    */
    void syncToEdit (Edit* editToSyncTo, bool syncToTargetLoopLength);

    //==============================================================================
    /** Returns true if the transport is playing. (This is also true during recording). */
    bool isPlaying() const;
    
    /** Returns true if recording is in progress. */
    bool isRecording() const;
    
    /** Returns true if safe-recording is in progress. */
    bool isSafeRecording() const;

    /** Returns true if the transport is currently being stopped.
        isPlaying will return false during this period but position changes etc. could still
        be sent out so this method lets you know if this.
    */
    bool isStopping() const;

    /** Returns the time when the transport was started. */
    TimePosition getTimeWhenStarted() const;

    //==============================================================================
    /** Returns the current transport position.
        N.B. This might be different from the actual audible time if the graph
        introduces latency.
        @see EditPlaybackContext::getAudibleTimelineTime, EditPlaybackContext::getLatencySamples
    */
    double getCurrentPosition() const;
    TimePosition getPosition() const;

    /** Sets a new transport position. */
    void setCurrentPosition (double time);
    void setPosition (TimePosition);

    /** Signifies a scrub-drag operation has started/stopped.
        While dragging, a short section of the play position is looped repeatedly.
    */
    void setUserDragging (bool);
    
    /** Returns true if a drag/scrub operation has been enabled.
        @see setUserDragging
    */
    bool isUserDragging() const noexcept;
    
    /** Returns true if the current position change was triggered from an update
        directly from the playhead (rather than a call to setCurrentPosition).
    */
    bool isPositionUpdatingFromPlayhead() const;

    //==============================================================================
    /** Sets the loop in position. */
    void setLoopIn (TimePosition);

    /** Sets a loop out position. */
    void setLoopOut (TimePosition);

    /** Sets a loop point 1 position. */
    void setLoopPoint1 (TimePosition);

    /** Sets a loop point 2 position. */
    void setLoopPoint2 (TimePosition);

    /** Sets the loop points from a given range. */
    void setLoopRange (TimeRange);

    /** Returns the loop range. The loop range is between the two loop points. */
    TimeRange getLoopRange() const noexcept;

    /** Sets a snap type to use. */
    void setSnapType (TimecodeSnapType);
    
    /** Returns the current snap type. */
    TimecodeSnapType getSnapType() const noexcept               { return currentSnapType; }

    //==============================================================================
    /** Returns the active EditPlaybackContext if this Edit is attached to the DeviceManager for playback. */
    EditPlaybackContext* getCurrentPlaybackContext() const      { return playbackContext.get(); }
    
    /** Returns true if this Edit is attached to the DeviceManager for playback. */
    bool isPlayContextActive() const                            { return playbackContext != nullptr; }

    /** Ensures an active EditPlaybackContext has been created so this Edit can be played back.
        @param alwaysReallocate If true, this will always create a new playback graph.
    */
    void ensureContextAllocated (bool alwaysReallocate = false);
    
    /** Detaches the current EditPlaybackContext, removing it from the DeviceManager.
        Can be used to free up resources if you have multiple Edits open.
    */
    void freePlaybackContext();

    /** Triggers a graph rebuild when playback stops. Used internally to adjust
        latency in response to plugin reported latency changes.
    */
    void triggerClearDevicesOnStop();
    
    /** Triggers a cleanup of any unused freeze and proxy files. */
    void forceOrphanFreezeAndProxyFilesPurge();

    //==============================================================================
    /** Starts/stops a rewind operation. */
    void setRewindButtonDown (bool isDown);

    /** Starts/stops a fast-forward operation. */
    void setFastForwardButtonDown (bool isDown);

    /** Moves the transport back slightly. */
    void nudgeLeft();

    /** Moves the transport forwards slightly. */
    void nudgeRight();

    //==============================================================================
    /** Triggers a playback graph rebuild.
        Called internally by Edit objects to rebuild the node graph when things change.
    */
    void editHasChanged();

    /** Prevents the nodes being regenerated while one of these exists, e.g. while
        dragging clips around, etc.
    */
    struct ReallocationInhibitor
    {
        /** Stops an Edit creating a new playback graph. */
        ReallocationInhibitor (TransportControl&);
        
        /** Enables playback graph regeneration. */
        ~ReallocationInhibitor();

    private:
        TransportControl& transport;
        JUCE_DECLARE_NON_COPYABLE (ReallocationInhibitor)
    };

    /** Returns true if no ReallocationInhibitors currently exist. */
    bool isAllowedToReallocate() const noexcept;

    //==============================================================================
    /** Is an Edit is playing back, this resumes playback when destroyed. */
    struct ScopedPlaybackRestarter
    {
        /** Saves the playback state. */
        ScopedPlaybackRestarter (TransportControl& o) : tc (o), wasPlaying (tc.isPlaying()) {}
        
        /** Starts playback if playing when constructed. */
        ~ScopedPlaybackRestarter()   { if (wasPlaying) tc.play (false); }

        TransportControl& tc;
        bool wasPlaying = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedPlaybackRestarter)
    };

    //==============================================================================
    /** Frees the playback context and then re-allocates it upon destruction. */
    struct ScopedContextAllocator
    {
        /** Saves whether the Edit was allocated. */
        ScopedContextAllocator (TransportControl& o)
            : tc (o), wasAllocated (tc.isPlayContextActive())
        {}
        
        /** Allocated the Edit if it was allocated on construction. */
        ~ScopedContextAllocator()
        {
            if (wasAllocated)
                tc.ensureContextAllocated();
        }

        TransportControl& tc;
        bool wasAllocated = false;
        ScopedPlaybackRestarter playbackRestarter { tc };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedContextAllocator)
    };

    //==============================================================================
    /** Returns all the active TransportControl[s] in the Engine. */
    static juce::Array<TransportControl*> getAllActiveTransports (Engine&);
    
    /** Returns the number of Edits currently playing. */
    static int getNumPlayingTransports (Engine&);
    
    /** Stops all TransportControl[s] in the Engine playing. @see stop. */
    static void stopAllTransports (Engine&, bool discardRecordings, bool clearDevices);
    
    /** Restarts all TransportControl[s] in the Edit. @see stop. */
    static std::vector<std::unique_ptr<ScopedContextAllocator>> restartAllTransports (Engine&, bool clearDevices);

    //==============================================================================
    /** Listener interface to be notified of changes to the transport. */
    struct Listener
    {
        /** Destructor. */
        virtual ~Listener() {}

        /** Called when an EditPlaybackContext is created or deleted. */
        virtual void playbackContextChanged() = 0;

        /** Called periodically to indicate the Edit has changed in an audible way and should be auto-saved. */
        virtual void autoSaveNow() = 0;

        /** If false, levels should be cleared. */
        virtual void setAllLevelMetersActive (bool metersBecameInactive) = 0;

        /** Should set a new position for any playing video. */
        virtual void setVideoPosition (TimePosition, bool forceJump) = 0;
        
        /** Should start video playback. */
        virtual void startVideo() = 0;

        /** Should stop video playback. */
        virtual void stopVideo() = 0;

        /** Called when recording stops for a specific input instance.
            @param InputDeviceInstance  The device instance that just stopped.
            @param recordedClips        The clips resulting from the recording.
        */
        virtual void recordingFinished (InputDeviceInstance&,
                                        juce::ReferenceCountedArray<Clip> /*recordedClips*/)
        {}
    };

    /** Adds a Listener. */
    void addListener (Listener* l)          { listeners.add (l); }

    /** Removes a Listener. */
    void removeListener (Listener* l)       { listeners.remove (l); }

    //==============================================================================
    Engine& engine;         /**< The Engine this Edit belongs to. */
    Edit& edit;             /**< The Edit this transport belongs to. @see Edit::getTransport. */
    juce::ValueTree state;  /**< The state of this transport. */

    /** @internal. */
    juce::CachedValue<TimePosition> position;
    juce::CachedValue<TimePosition> loopPoint1, loopPoint2;
    juce::CachedValue<TimeDuration> scrubInterval;
    /** @internal. */
    juce::CachedValue<bool> snapToTimecode, looping;

    //==============================================================================
    /** @internal */
    void callRecordingFinishedListeners (InputDeviceInstance&, juce::ReferenceCountedArray<Clip> recordedClips);

private:
    //==============================================================================
    struct TransportState;
    std::unique_ptr<TransportState> transportState;
    std::unique_ptr<EditPlaybackContext> playbackContext;
    juce::ListenerList<Listener> listeners;

    TimecodeSnapType currentSnapType;
    bool isDelayedChangePending = false;
    int loopUpdateCounter = 10;
    bool isStopInProgress = false;

    struct ScreenSaverDefeater;
    std::unique_ptr<ScreenSaverDefeater> screenSaverDefeater;

    struct PlayingFlag
    {
        PlayingFlag (Engine&) noexcept;
        ~PlayingFlag() noexcept;

        Engine& engine;
    };

    std::unique_ptr<PlayingFlag> playingFlag;
    void clearPlayingFlags();

    struct SectionPlayer;
    std::unique_ptr<SectionPlayer> sectionPlayer;
    
    struct PlayHeadWrapper;
    std::unique_ptr<PlayHeadWrapper> playHeadWrapper;

    bool lastPlayStatus = false, lastRecordStatus = false;
    void startedOrStopped();
    void releaseAudioNodes();

    void performPlay();
    bool performRecord();
    void performStop();

    void performPositionChange();
    void performRewindButtonChanged();
    void performFastForwardButtonChanged();
    void performNudgeLeft();
    void performNudgeRight();

    void sendMMC (const juce::MidiMessage&);
    void sendMMCCommand (juce::MidiMessage::MidiMachineControlCommand);
    bool sendMMCStartPlay();
    bool sendMMCStartRecord();

    bool areAnyInputsRecording();

    struct FileFlushTimer;
    std::unique_ptr<FileFlushTimer> fileFlushTimer;

    struct ButtonRepeater;
    std::unique_ptr<ButtonRepeater> rwRepeater, ffRepeater;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControl)
};

}} // namespace tracktion { inline namespace engine
