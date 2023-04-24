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

struct ParameterSetting
{
    ParameterSetting() noexcept;
    void clear() noexcept;

    char label[32];
    char valueDescription[32];
    float value;
};

struct MarkerSetting
{
    MarkerSetting() noexcept;
    void clear() noexcept;

    char label[32];
    int number;
    bool absolute;
};

//==============================================================================
/**
    Base class for types of control surface.
*/
class ControlSurface   : public Selectable
{
public:
    ControlSurface (ExternalControllerManager&);
    ~ControlSurface() override;

    //==============================================================================
    /*
        The following methods need to be implemented by the controller class, and will be
        called by tracktion to tell it when stuff happens, such as midi messages arriving,
        or the user doing things on the interface that should be reflected on the device.

        Inside these callbacks, the device can call the methods specified further
        down to tell tracktion about what it needs to do in response.
    */

    virtual void initialiseDevice ([[maybe_unused]] bool connect) {}
    virtual void shutDownDevice() {}

    // If the device communicates via OSC, then this tells the device the new settings
    virtual void updateOSCSettings (int /*oscInputPort*/, int /*oscOutputPort*/, juce::String /*oscOutputAddr*/) {}

    // most settings will be updated by the ExternalControllerManager, but this allows a device
    // a chance to do some extra stuff when it needs to refresh itself
    virtual void updateMiscFeatures() {}

    // Called when the use changes the number of extenders
    virtual void numExtendersChanged ([[maybe_unused]] int num, [[maybe_unused]] int main) {}

    // called by tracktion when a midi message comes in from the controller. The
    // subclass must translate this and call methods in this class accordingly to
    // trigger whatever action the user is trying to do.
    virtual void acceptMidiMessage ([[maybe_unused]] int idx, const juce::MidiMessage&) {}

    // tells the device to move one of its faders.
    // the channel number is the physical channel on the device, regardless of bank selection
    // slider pos is 0 to 1.0
    //
    // If you override this, you must call parent class if you use pick mode
    virtual void moveFader (int channelNum, float newSliderPos);

    // tells the device to move the master faders, if it has them. If it just has one master
    // fader, it can use the average of these levels.
    // slider pos is 0 to 1.0
    virtual void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos);

    // tells the device to move a pan pot.
    // the channel number is the physical channel on the device, regardless of bank selection
    // pan is -1.0 to 1.0
    virtual void movePanPot ([[maybe_unused]] int channelNum, [[maybe_unused]] float newPan);

    virtual void moveAux ([[maybe_unused]] int channel, [[maybe_unused]] const char* bus, [[maybe_unused]] float newPos);

    virtual void clearAux (int) {}

    // the channel number is the physical channel on the device, regardless of bank selection
    virtual void updateSoloAndMute ([[maybe_unused]] int channelNum, Track::MuteAndSoloLightState, [[maybe_unused]] bool isBright) {}

    // count of number of tracks soloed
    virtual void soloCountChanged ([[maybe_unused]] bool anySoloTracks) {}

    // tells the device that playback has stopped or started, and it should turn its lights on
    // accordingly.
    virtual void playStateChanged ([[maybe_unused]] bool isPlaying) {}
    virtual void recordStateChanged ([[maybe_unused]] bool isRecording) {}

    // tells the device about automation read/write status changing.
    virtual void automationReadModeChanged ([[maybe_unused]] bool isReading) {}
    virtual void automationWriteModeChanged ([[maybe_unused]] bool isWriting) {}

    // tells the device that the user has changed fader banks, so if the device has a bank number
    // indicator, this tells it to update its contents.
    //
    // the newStartChannelNumber is the number of the first virtual channel now being mapped to the
    // first physical fader channel on the device, starting from zero.
    //
    // the trackname array is the set of names for the tracks that now map onto the device's physical
    // fader channels, so if it has a display that can show track names, it should update this.
    virtual void faderBankChanged ([[maybe_unused]] int newStartChannelNumber, [[maybe_unused]] const juce::StringArray& trackNames) {}

    // if the device has per-channel level meters, this should update one of them.
    // the channel number is the physical channel on the device, regardless of bank selection
    // if the channel is mono then l == r
    // level is 0 to 1.0
    virtual void channelLevelChanged ([[maybe_unused]] int channel, [[maybe_unused]] float l, [[maybe_unused]] float r) {}

    // when a track is selected or deselected
    virtual void trackSelectionChanged ([[maybe_unused]] int channel, [[maybe_unused]] bool isSelected) {}

    virtual void trackRecordEnabled ([[maybe_unused]] int channel, [[maybe_unused]] bool isEnabled) {}

    // if the device has a master level readout, this should update it.
    virtual void masterLevelsChanged ([[maybe_unused]] float leftLevel,
                                      [[maybe_unused]] float rightLevel) {}

    // tells the device that the playback position has changed, so if it has a timecode
    // display, it should update it.
    //
    // If isBarsBeats is true, barsOrHours is the bar number (starting from 1), beatsOrMinutes is
    // the beat number (also from 1), ticksOrSeconds is the number of ticks (0 to 959) and millisecs is
    // unused.
    //
    // If isBarsBeats is false, barsOrHours = hours, beatsOrMinutes = minutes, ticksOrSeconds = seconds, and
    // millisecs is (surprise surprise) the milliseconds.
    virtual void timecodeChanged ([[maybe_unused]]  int barsOrHours,
                                  [[maybe_unused]]  int beatsOrMinutes,
                                  [[maybe_unused]]  int ticksOrSeconds,
                                  [[maybe_unused]]  int millisecs,
                                  [[maybe_unused]]  bool isBarsBeats,
                                  [[maybe_unused]]  bool isFrames) {}

    // tells the device that the click has been turned on or off.
    virtual void clickOnOffChanged ([[maybe_unused]] bool isClickOn) {}
    // tells the device that snapping has been turned on or off.
    virtual void snapOnOffChanged ([[maybe_unused]] bool isSnapOn) {}
    // tells the device that looping has been turned on or off.
    virtual void loopOnOffChanged ([[maybe_unused]] bool isLoopOn) {}

    virtual void slaveOnOffChanged ([[maybe_unused]] bool isSlaving)        {}
    virtual void punchOnOffChanged ([[maybe_unused]] bool isPunching)       {}
    virtual void scrollOnOffChanged ([[maybe_unused]] bool isScroll)        {}
    virtual void undoStatusChanged ([[maybe_unused]] bool canUundo,
                                    [[maybe_unused]] bool canReo)           {}

    // tells the device that one of the parameters has been changed.
    //
    // parameterNumber is the physical parameter number on the device, not the
    // virtual one.
    virtual void parameterChanged ([[maybe_unused]] int parameterNumber, [[maybe_unused]] const ParameterSetting& newValue);
    virtual void clearParameter ([[maybe_unused]] int parameterNumber) {}

    virtual void markerChanged ([[maybe_unused]] int parameterNumber, [[maybe_unused]] const MarkerSetting& newValue) {}
    virtual void clearMarker ([[maybe_unused]] int parameterNumber) {}

    virtual void auxBankChanged (int)                   {}

    virtual bool wantsMessage (int, const juce::MidiMessage&) { return true; }
    virtual bool eatsAllMessages()                      { return true; }
    virtual bool canSetEatsAllMessages()                { return false; }
    virtual void setEatsAllMessages(bool)               {}
    virtual bool canChangeSelectedPlugin()              { return true; }
    virtual void currentSelectionChanged (juce::String) {}

    juce::String getSelectableDescription() override;

    virtual bool showingPluginParams()                  { return false; }
    virtual bool showingMarkers()                       { return false; }
    virtual bool showingTracks()                        { return true;  }

    virtual void deleteController()                     {}
    virtual void pluginBypass (bool)                    {}
    virtual bool isPluginSelected (Plugin*)             { return false; }

    virtual void currentEditChanged (Edit* e)           { edit = e; }

    Edit* getEdit() const noexcept                      { return edit; }
    TransportControl* getTransport() const noexcept     { return edit != nullptr ? &edit->getTransport() : nullptr; }

    //==============================================================================
    /*
        The following functions are called by the driver class to send commands
        to tracktion..
    */

    // Don't send commands when safe recording. All callbacks check this,
    // but your custom app code may not
    bool isSafeRecording() const;

    // Get the current bank offset. You shouldn't need to know this,
    // just work with physical channel numbers. But it may come in useful
    // rarely if you need to interact with the edit directly
    int getMarkerBankOffset() const;
    int getFaderBankOffset() const;
    int getAuxBankOffset() const;
    int getParamBankOffset() const;

    // sends a MIDI message to the device's back-channel
    void sendMidiCommandToController (int idx, const void* midiData, int numBytes);
    void sendMidiCommandToController (int idx, const juce::MidiMessage&);

    template <size_t size>
    void sendMidiArray (int idx, const uint8_t (&rawData)[size])   { sendMidiCommandToController (idx, rawData, (int) size); }

    // tells tracktion that the user has moved a fader.
    // the channel number is the physical channel on the device, regardless of bank selection
    // range 0 to 1.0 or -1.0 to 1.0
    // delta false means absolute position, otherwise adjust by delta
    void userMovedFader (int channelNum, float newFaderPosition, bool delta = false);

    // tells tracktion that the user has moved a pan pot
    // the channel number is the physical channel on the device, regardless of bank selection
    // range -1.0 to 1.0
    // delta false means absolute position, otherwise adjust by delta
    void userMovedPanPot (int channelNum, float newPanPosition, bool delta = false);

    // tells tracktion that the master fader has moved.
    void userMovedMasterLevelFader (float newLevel, bool delta = false);
    void userMovedMasterPanPot (float newLevel);

    void userMovedAux (int channelNum, float newPosition);
    void userPressedAux (int channelNum);
    void userMovedQuickParam (float newLevel);

    // these tell tracktion about buttons being pressed
    void userPressedSolo (int channelNum);
    void userPressedSoloIsolate (int channelNum);
    void userPressedMute (int channelNum, bool muteVolumeControl);
    void userSelectedTrack (int channelNum);
    void userSelectedClipInTrack (int channelNum);
    void userSelectedPluginInTrack (int channelNum);
    void userPressedRecEnable (int channelNum, bool enableEtoE);
    void userPressedPlay();
    void userPressedRecord();
    void userPressedStop();
    void userPressedHome(); // return to zero
    void userPressedEnd();
    void userPressedMarkIn();
    void userPressedMarkOut();
    void userPressedAutomationReading();
    void userPressedAutomationWriting();
    void userToggledBeatsSecondsMode();
    void userPressedSave();
    void userPressedSaveAs();
    void userPressedArmAll();
    void userPressedJumpToMarkIn();
    void userPressedJumpToMarkOut();
    void userPressedZoomIn();
    void userPressedZoomOut();
    void userPressedZoomToFit();
    void userPressedCreateMarker();
    void userPressedNextMarker();
    void userPressedPreviousMarker();
    void userPressedRedo();
    void userPressedUndo();
    void userPressedAbort();
    void userPressedAbortRestart();
    void userPressedCut();
    void userPressedCopy();
    void userPressedPaste (bool insert);
    void userPressedDelete (bool marked);
    void userPressedZoomFitToTracks();
    void userPressedInsertTempoChange();
    void userPressedInsertPitchChange();
    void userPressedInsertTimeSigChange();
    void userToggledVideoWindow();
    void userToggledMixerWindow (bool fullscreen);
    void userToggledMidiEditorWindow (bool fullscreen);
    void userToggledTrackEditorWindow (bool zoomed);
    void userToggledBrowserWindow();
    void userToggledActionsWindow();
    void userPressedUserAction (int);
    void userPressedFreeze();

    void userPressedClearAllSolo();
    void userPressedClearAllMute();

    void userToggledLoopOnOff();
    void userToggledPunchOnOff();
    void userToggledClickOnOff();
    void userToggledSnapOnOff();
    void userToggledSlaveOnOff();
    void userToggledEtoE();
    void userToggledScroll();

    void userSkippedToNextMarkerLeft();
    void userSkippedToNextMarkerRight();
    void userNudgedLeft();
    void userNudgedRight();
    void userZoomedIn();
    void userZoomedOut();
    void userScrolledTracksUp();
    void userScrolledTracksDown();
    void userScrolledTracksLeft();
    void userScrolledTracksRight();
    void userZoomedTracksIn();
    void userZoomedTracksOut();
    void selectOtherObject (SelectableClass::Relationship, bool moveFromCurrentPlugin);
    void muteOrUnmutePluginsInTrack();

    // tells tracktion to move the fader bank up or down by the specified number of channels.
    // After calling this, tracktion will call back the faderBankChanged() method to tell the
    // device what its new state is.
    void userChangedFaderBanks (int channelNumDelta);

    // tells tracktion to move the cursor.
    //
    // amount < 0 means moving backwards, amount > 0 forwards
    // magnitude of about 1.0 is a 'normal' speed
    void userMovedJogWheel (float amount);

    // to allow it to auto-repeat, these tell tracktion when the left/right buttons are pressed and
    // released.
    // (try not to make sure the buttons aren't left stuck down!)
    void userChangedRewindButton (bool isButtonDown);
    void userChangedFastForwardButton (bool isButtonDown);

    void userMovedParameterControl (int parameter, float newValue);
    void userPressedParameterControl (int paramNumber);

    void userChangedParameterBank (int deltaParams);
    void userChangedMarkerBank (int deltaMarkers);
    void userPressedGoToMarker (int marker);

    void userChangedAuxBank (int delta);
    void updateDeviceState();

    void redrawSelectedPlugin();
    void redrawSelectedTracks();

    Edit* getEditIfOnEditScreen() const;

    //==============================================================================
    /** These values need to be set by the subclass. */
    juce::String deviceDescription;

    // The number of multiple similar devices can be connected to add additional tracks
    int supportedExtenders = 0;

    // does this driver need to be able to send MIDI messages
    bool needsMidiChannel = true;

    // If the midi device always has the same name, set it and device will auto configure
    juce::String midiChannelName;

    // does this driver need to be able to send MIDI messages back to the
    // controller as well as receive them?
    bool needsMidiBackChannel = false;

    // If the midi device always has the same name, set it and device will auto configure
    juce::String midiBackChannelName;

    // does this driver need to be able to communicate via OSC
    bool needsOSCSocket = false;

    // device wants MIDI clock
    bool wantsClock = false;

    // can be deleted
    bool deletable = false;

    // number of physical faders
    int numberOfFaderChannels = 0;
    int numCharactersForTrackNames = 0;

    // is banking to the right allowed to show empty tracks
    bool allowBankingOffEnd = false;

    // number of labelled rotary dials that can control things like plugin parameters
    int numParameterControls = 0;
    int numCharactersForParameterLabels = 0;

    // number of markers that can be displayed
    int numMarkers = 0;
    int numCharactersForMarkerLabels = 0;

    // pick up mode. input value must cross current value before input is accepted
    // useful for non motorized faders, so the don't jump when adjusted
    bool pickUpMode = false;

    int numAuxes = 0;
    int numCharactersForAuxLabels = 0;
    bool wantsAuxBanks = false;
    bool followsTrackSelection = false;

    Engine& engine;
    ExternalControllerManager& externalControllerManager;
    ExternalController* owner = nullptr;

private:
    enum ControlType
    {
        ctrlFader,
        ctrlMasterFader,
        ctrlPan,
        ctrlAux,
        ctrlParam,
    };

    struct PickUpInfo
    {
        bool pickedUp = false;
        std::optional<float> lastIn;
        float lastOut = 0.0f;
    };

    Edit* edit = nullptr;

    void performIfNotSafeRecording (const std::function<void()>&);

    bool pickedUp (ControlType, int index, float value);
    bool pickedUp (ControlType, float value);

    std::map<std::pair<ControlType, int>, PickUpInfo> pickUpMap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlSurface)
};

}} // namespace tracktion { inline namespace engine
