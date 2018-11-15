/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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
class TransportControl   : public juce::ChangeBroadcaster,
                           private juce::Timer

{
public:
    TransportControl (Edit&, const juce::ValueTree&);
    ~TransportControl();

    //==============================================================================
    PlayHead* getCurrentPlayhead() const;

    //==============================================================================
    void play (bool justSendMMCIfEnabled);
    void playSectionAndReset (EditTimeRange rangeToPlay);

    void record (bool justSendMMCIfEnabled, bool allowRecordingIfNoInputsArmed = false);

    void stop (bool discardRecordings,
               bool clearDevices,
               bool canSendMMCStop = true,
               bool invertReturnToStartPosSelection = false);
    void stopIfRecording();

    juce::Result applyRetrospectiveRecord();
    juce::Array<juce::File> getRetrospectiveRecordAsAudioFiles();

    void syncToEdit (Edit* editToSyncTo, bool isPreview); // plays in sync with another Edit

    /** (this is also true if we're recording */
    bool isPlaying() const;
    bool isRecording() const;
    bool isSafeRecording() const;

    double getTimeWhenStarted() const;

    //==============================================================================
    double getCurrentPosition() const;
    void setCurrentPosition (double time);

    void setUserDragging (bool);
    bool isUserDragging() const noexcept;

    //==============================================================================
    void setLoopIn (double);
    void setLoopOut (double);
    void setLoopPoint1 (double);
    void setLoopPoint2 (double);
    void setLoopRange (EditTimeRange range);
    EditTimeRange getLoopRange() const noexcept;

    void setSnapType (TimecodeSnapType);
    TimecodeSnapType getSnapType() const noexcept               { return currentSnapType; }

    //==============================================================================
    EditPlaybackContext* getCurrentPlaybackContext() const      { return playbackContext.get(); }
    bool isPlayContextActive() const                            { return playbackContext != nullptr; }

    void ensureContextAllocated (bool alwaysReallocate = false);
    void freePlaybackContext();

    void triggerClearDevicesOnStop();
    void forceOrphanFreezeAndProxyFilesPurge();

    //==============================================================================
    void setRewindButtonDown (bool isDown);
    void setFastForwardButtonDown (bool isDown);

    void nudgeLeft();
    void nudgeRight();

    //==============================================================================
    // Called by edit objects to rebuild the node graph when things change
    void editHasChanged();

    // prevents the nodes being regenerated while one of these exists, e.g. while
    // dragging clips around, etc
    struct ReallocationInhibitor
    {
        ReallocationInhibitor (TransportControl&);
        ~ReallocationInhibitor();

    private:
        TransportControl& transport;
        JUCE_DECLARE_NON_COPYABLE (ReallocationInhibitor)
    };

    // Returns true if no ReallocationInhibitors currently exist
    int isAllowedToReallocate() const noexcept;

    //==============================================================================
    static juce::Array<TransportControl*> getAllActiveTransports (Engine&);
    static int getNumPlayingTransports (Engine&);
    static void stopAllTransports (Engine&, bool discardRecordings, bool clearDevices);
    static void restartAllTransports (Engine&, bool clearDevices);

    //==============================================================================
    struct ScopedPlaybackRestarter
    {
        ScopedPlaybackRestarter (TransportControl& o) : tc (o), wasPlaying (tc.isPlaying()) {}
        ~ScopedPlaybackRestarter()   { if (wasPlaying) tc.play (false); }

        TransportControl& tc;
        bool wasPlaying;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedPlaybackRestarter)
    };

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() {}

        /** Called when an EditPlaybackContext is created or deleted. */
        virtual void playbackContextChanged() = 0;

        virtual void autoSaveNow() = 0;

        virtual void setAllLevelMetersActive (bool) = 0;

        virtual void setVideoPosition (double time, bool forceJump) = 0;
        virtual void startVideo() = 0;
        virtual void stopVideo() = 0;
    };

    void addListener (Listener* l)          { listeners.add (l); }
    void removeListener (Listener* l)       { listeners.remove (l); }

    Engine& engine;
    Edit& edit;
    juce::ValueTree state;

    juce::CachedValue<double> position, loopPoint1, loopPoint2, scrubInterval;
    juce::CachedValue<bool> snapToTimecode, looping;

private:
    //==============================================================================
    struct TransportState;
    std::unique_ptr<TransportState> transportState;
    std::unique_ptr<EditPlaybackContext> playbackContext;
    juce::ListenerList<Listener> listeners;

    TimecodeSnapType currentSnapType;
    bool isDelayedChangePending = false;
    int loopUpdateCounter = 10;

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

    void timerCallback();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControl)
};

} // namespace tracktion_engine
