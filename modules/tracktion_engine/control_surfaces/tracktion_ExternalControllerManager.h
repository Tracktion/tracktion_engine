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

/** An area of the edit currently control by a control surface */
struct ColourArea
{
    juce::Colour colour;
    Track& firstTrack;
    Track& lastTrack;
    int firstScene;
    int lastScene;

    bool contains (ClipSlot&) const;
    bool isLeft (ClipSlot&) const;
    bool isRight (ClipSlot&) const;
    bool isTop (ClipSlot&) const;
    bool isBottom (ClipSlot&) const;
};

/** Keeps a list of external controllers and keeps them connected to the
    right MIDI in/out devices
*/
class ExternalControllerManager  : public juce::ChangeBroadcaster,
                                   private juce::ChangeListener,
                                   private juce::Timer
{
public:
    //==============================================================================
    ~ExternalControllerManager() override;

    void initialise();
    void shutdown();

    //==============================================================================
    /** Callback that can be set to determine if a track is visible on a controller or not. */
    std::function<bool (const Track&)> isVisibleOnControlSurface;

    /** Optional callback that can be set to map an Edit track number to a control surface channel number.
        If this isn't set, the manager will iterate the tracks to find the mapping. This can be slow however
        so this function can be used to create your own cache and speed up the process.
    */
    std::function<int (Edit&, int)> mapEditTrackNumToControlSurfaceChannelNum;

    /** Callback that can be set allow surfaces to open/close folders. */
    std::function<void (FolderTrack&, bool)> setFolderTrackOpen;

    /** Callback that can be set allow surfaces to show the open/close status of folders. */
    std::function<bool (FolderTrack&)> isFolderTrackOpen;

    /** Callback that can be set allow surfaces to set the scroll status of an Edit. */
    std::function<void (Edit&, bool)> setScrollingEnabled;

    /** Callback that can be set allow surfaces to show the scroll status of an Edit. */
    std::function<bool (Edit&)> isScrollingEnabled;

    /** Launch clip by track and index */
    std::function<void (Edit&, Track&, int, bool)> launchClip;

    /** Stop all clips on track. If track is nullptr, stop all tracks */
    std::function<void (Edit&, Track*, bool)> stopClip;

    /** Launch scene by index */
    std::function<void (Edit&, int, bool)> launchScene;

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

    enum Protocol
    {
        midi,
        osc
    };

    bool createCustomController (const juce::String& name, Protocol);
    ExternalController* addController (ControlSurface*);
    void deleteController (ExternalController*);

    //==============================================================================
    // these get called by stuff in the application to make the controllers react
    // appropriately..

    // channels are virtual - i.e. not restricted to physical chans
    void moveFader (int channelNum, float newSliderPos);
    void moveMasterFader (float newPos);
    void movePanPot (int channelNum, float newPan);
    void moveMasterPanPot (float newPan);
    void playStateChanged (bool isPlaying);
    void recordStateChanged (bool isRecording);
    void automationModeChanged (bool isReading, bool isWriting);
    void channelLevelChanged (int channel, float l, float r);
    void masterLevelsChanged (float leftLevel, float rightLevel);
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds,
                          int millisecs, bool isBarsBeats, bool isFrames);
    void editPositionChanged (Edit*, TimePosition newCursorPosition);
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

    std::vector<ColourArea> getColouredArea (const Edit&);

    bool shouldPluginBeColoured (Plugin*);
    juce::Colour getPluginColour (Plugin*);

    void updateAllDevices();
    void updateParameters();
    void updateMarkers();
    void updateTrackRecordLights();
    void updatePunchLights();
    void updateScrollLights();
    void updateUndoLights();
    void updatePadColours();

    int getNumChannelTracks() const;
    Track* getChannelTrack (int channel) const;
    int mapTrackNumToChannelNum (int channelNum) const;

    //==============================================================================
    int getXTCount (const juce::String& controller);
    void setXTCount (const juce::String& controller, int);
    void refreshXTOrder();

    //==============================================================================
    // these are called back by the controller to make the app respond
    void userMovedFader (int channelNum, float newSliderPos, bool delta);
    void userMovedMasterFader (Edit*, float newLevel, bool delta);
    void userMovedMasterPanPot (Edit*, float newLevel, bool delta);
    void userMovedPanPot (int channelNum, float newPan, bool delta);
    void userPressedSolo (int channelNum);
    void userPressedSoloIsolate (int channelNum);
    void userPressedMute (int channelNum, bool muteVolumeControl);
    void userSelectedTrack (int channelNum);
    void userSelectedOneTrack (int channelNum);
    void userSelectedClipInTrack (int channelNum);
    void userSelectedPluginInTrack (int channelNum);
    void userMovedAux (int channelNum, int auxNum, AuxPosition ap, float newPosition, bool delta);
    void userPressedAux (int channelNum, int auxNum);
    void userMovedQuickParam (float newLevel);
    void userLaunchedClip (int channelNum, int clip, bool press);
    void userStoppedClip (int channelNum, bool press);
    void userLaunchedScene (int scene, bool press);

    void updateDeviceState();

    void repaintTrack (int channelNum);
    void repaintPlugin (Plugin&);
    void repaintSlots (int channelNum);

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    NovationAutomap* getAutomap() const noexcept        { return automap; }
   #endif

    Engine& engine;

private:
    std::shared_ptr<LaunchHandle> getLaunchHandle (int channelNum, int sceneNum);

    friend class Engine;
    ExternalControllerManager (Engine&);

    struct EditTreeWatcher;
    std::unique_ptr<EditTreeWatcher> editTreeWatcher;
    juce::OwnedArray<ExternalController> devices;

    NovationAutomap* automap = nullptr;

    uint32_t lastUpdate = 0;
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

    LambdaTimer masterLevelsTimer;

    ExternalController* addNewController (ControlSurface*);

    juce::Array<ExternalController*> getActiveDevices() const;
    void blinkNow();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalControllerManager)
};

}} // namespace tracktion { inline namespace engine
