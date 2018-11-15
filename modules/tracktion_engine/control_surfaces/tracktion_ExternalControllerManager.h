/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Keeps a list of external controllers and keeps them connected to the
    right MIDI in/out devices
*/
class ExternalControllerManager  : public juce::ChangeBroadcaster,
                                   private juce::ChangeListener,
                                   private juce::Timer
{
public:
    //==============================================================================
    ~ExternalControllerManager();

    void initialise();
    void shutdown();

    //==============================================================================
    /** Callback that can be set to determine if a track is visible on a controller or not. */
    std::function<bool (const Track&)> isVisibleOnControlSurface;

    /** Callback that can be set allow surfaces to open/close folders. */
    std::function<void (FolderTrack&, bool)> setFolderTrackOpen;

    /** Callback that can be set allow surfaces to show the open/close status of folders. */
    std::function<bool (FolderTrack&)> isFolderTrackOpen;

    /** Callback that can be set allow surfaces to set the scroll status of an Edit. */
    std::function<void (Edit&, bool)> setScrollingEnabled;

    /** Callback that can be set allow surfaces to show the scroll status of an Edit. */
    std::function<bool (Edit&)> isScrollingEnabled;

    //==============================================================================
    void setCurrentEdit (Edit*, SelectionManager*);
    void detachFromEdit (Edit*);
    void detachFromSelectionManager (SelectionManager*);
    bool isAttachedToEdit (const Edit*) const noexcept;
    bool isAttachedToEdit (const Edit&) const noexcept;
    SelectionManager* getSelectionManager() const noexcept;

    //==============================================================================
    const juce::OwnedArray<ExternalController>& getControllers() const noexcept     { return devices; }

    juce::StringArray getAllControllerNames();

    // Returns the first available custom controler to use for assignments.
    ExternalController* getActiveCustomController();

    void midiInOutDevicesChanged();

    bool createCustomController (const juce::String& name);
    void deleteController (ExternalController*);

    //==============================================================================
    // these get called by stuff in the application to make the controllers react
    // appropriately..

    // channels are virtual - i.e. not restricted to physical chans
    void moveFader (int channelNum, float newSliderPos);
    void moveMasterFaders (float newLeftPos, float newRightPos);
    void movePanPot (int channelNum, float newPan);
    void playStateChanged (bool isPlaying);
    void recordStateChanged (bool isRecording);
    void automationModeChanged (bool isReading, bool isWriting);
    void channelLevelChanged (int channel, float level);
    void masterLevelsChanged (float leftLevel, float rightLevel);
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds,
                          int millisecs, bool isBarsBeats, bool isFrames);
    void editPositionChanged (Edit*, double newCursorPosition);
    void updateVolumePlugin (VolumeAndPanPlugin&);
    void updateVCAPlugin (VCAPlugin& vca);
    void snapChanged (bool isOn);
    void loopChanged (bool isOn);
    void clickChanged (bool isOn);
    void auxSendLevelsChanged();

    void updateMuteSoloLights (bool onlyUpdateFlashingLights);
    void soloCountChanged (bool);

    void changeListenerCallback (ChangeBroadcaster*) override;

    bool shouldTrackBeColoured (int channelNum);
    juce::Colour getTrackColour (int channelNum);

    bool shouldPluginBeColoured (Plugin*);
    juce::Colour getPluginColour (Plugin*);

    void updateAllDevices();
    void updateParameters();
    void updateMarkers();
    void updateTrackRecordLights();
    void updatePunchLights();
    void updateUndoLights();

    int getNumChannelTracks() const;
    Track* getChannelTrack (int channel) const;
    int mapTrackNumToChannelNum (int channelNum) const;

    //==============================================================================
    int getXTCount();
    void setXTCount (int);
    void refreshXTOrder();

    //==============================================================================
    // these are called back by the controller to make the app respond
    void userMovedFader (int channelNum, float newSliderPos);
    void userMovedMasterFader (Edit*, float newLevel);
    void userMovedMasterPanPot (Edit*, float newLevel);
    void userMovedPanPot (int channelNum, float newPan);
    void userPressedSolo (int channelNum);
    void userPressedSoloIsolate (int channelNum);
    void userPressedMute (int channelNum, bool muteVolumeControl);
    void userSelectedTrack (int channelNum);
    void userSelectedClipInTrack (int channelNum);
    void userSelectedPluginInTrack (int channelNum);
    void userMovedAux (int channelNum, int auxNum, float newPosition);
    void userPressedAux (int channelNum, int auxNum);
    void userMovedQuickParam (float newLevel);

    void updateDeviceState();

    void repaintTrack (int channelNum);
    void repaintPlugin (Plugin&);

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    NovationAutomap* getAutomap() const noexcept        { return automap; }
   #endif

    Engine& engine;

private:
    friend class Engine;
    ExternalControllerManager (Engine&);

    struct EditTreeWatcher;
    std::unique_ptr<EditTreeWatcher> editTreeWatcher;
    juce::OwnedArray<ExternalController> devices;

    NovationAutomap* automap = nullptr;

    juce::uint32 lastUpdate = 0;
    Edit* currentEdit = nullptr;
    SelectionManager* currentSelectionManager = nullptr;

    struct BlinkTimer : private Timer
    {
        BlinkTimer (ExternalControllerManager&);
        void timerCallback() override;
        ExternalControllerManager& ecm;
        bool isBright = false;
    };

    std::unique_ptr<BlinkTimer> blinkTimer;

    void addNewController (ControlSurface*);

    void blinkNow();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalControllerManager)
};

} // namespace tracktion_engine
