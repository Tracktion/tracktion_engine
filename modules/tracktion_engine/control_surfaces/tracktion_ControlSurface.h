/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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
    virtual ~ControlSurface();

    //==============================================================================
    /*
        The following methods need to be implemented by the controller class, and will be
        called by tracktion to tell it when stuff happens, such as midi messages arriving,
        or the user doing things on the interface that should be reflected on the device.

        Inside these callbacks, the device can call the methods specified further
        down to tell tracktion about what it needs to do in response.
    */

    virtual void initialiseDevice (bool connect) = 0;
    virtual void shutDownDevice() = 0;

    // most settings will be updated by the ExternalControllerManager, but this allows a device
    // a chance to do some extra stuff when it needs to refresh itself
    virtual void updateMiscFeatures() = 0;

    // called by tracktion when a midi message comes in from the controller. The
    // subclass must translate this and call methods in this class accordingly to
    // trigger whatever action the user is trying to do.
    virtual void acceptMidiMessage (const juce::MidiMessage&) = 0;

    // tells the device to move one of its faders.
    // the channel number is the physical channel on the device, regardless of bank selection
    // slider pos is 0 to 1.0
    virtual void moveFader (int channelNum, float newSliderPos) = 0;

    // tells the device to move the master faders, if it has them. If it just has one master
    // fader, it can use the average of these levels.
    // slider pos is 0 to 1.0
    virtual void moveMasterLevelFader (float newLeftSliderPos,
                                       float newRightSliderPos) = 0;

    // tells the device to move a pan pot.
    // the channel number is the physical channel on the device, regardless of bank selection
    // pan is -1.0 to 1.0
    virtual void movePanPot (int channelNum, float newPan) = 0;

    virtual void moveAux (int channel, const char* bus, float newPos) = 0;

    virtual void clearAux (int) {}

    // the channel number is the physical channel on the device, regardless of bank selection
    virtual void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) = 0;

    // count of number of tracks soloed
    virtual void soloCountChanged (bool anySoloTracks) = 0;

    // tells the device that playback has stopped or started, and it should turn its lights on
    // accordingly.
    virtual void playStateChanged (bool isPlaying) = 0;
    virtual void recordStateChanged (bool isRecording) = 0;

    // tells the device about automation read/write status changing.
    virtual void automationReadModeChanged (bool isReading) = 0;
    virtual void automationWriteModeChanged (bool isWriting) = 0;

    // tells the device that the user has changed fader banks, so if the device has a bank number
    // indicator, this tells it to update its contents.
    //
    // the newStartChannelNumber is the number of the first virtual channel now being mapped to the
    // first physical fader channel on the device, starting from zero.
    //
    // the trackname array is the set of names for the tracks that now map onto the device's physical
    // fader channels, so if it has a display that can show track names, it should update this.
    virtual void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) = 0;

    // if the device has per-channel level meters, this should update one of them.
    // the channel number is the physical channel on the device, regardless of bank selection
    // level is 0 to 1.0
    virtual void channelLevelChanged (int channel, float level) = 0;

    // when a track is selected or deselected
    virtual void trackSelectionChanged (int channel, bool isSelected) = 0;

    virtual void trackRecordEnabled (int channel, bool isEnabled) = 0;

    // if the device has a master level readout, this should update it.
    virtual void masterLevelsChanged (float leftLevel,
                                      float rightLevel) = 0;

    // tells the device that the playback position has changed, so if it has a timecode
    // display, it should update it.
    //
    // If isBarsBeats is true, barsOrHours is the bar number (starting from 1), beatsOrMinutes is
    // the beat number (also from 1), ticksOrSeconds is the number of ticks (0 to 959) and millisecs is
    // unused.
    //
    // If isBarsBeats is false, barsOrHours = hours, beatsOrMinutes = minutes, ticksOrSeconds = seconds, and
    // millisecs is (surprise surprise) the milliseconds.
    virtual void timecodeChanged (int barsOrHours,
                                  int beatsOrMinutes,
                                  int ticksOrSeconds,
                                  int millisecs,
                                  bool isBarsBeats,
                                  bool isFrames) = 0;

    // tells the device that the click has been turned on or off.
    virtual void clickOnOffChanged (bool isClickOn) = 0;
    // tells the device that snapping has been turned on or off.
    virtual void snapOnOffChanged (bool isSnapOn) = 0;
    // tells the device that looping has been turned on or off.
    virtual void loopOnOffChanged (bool isLoopOn) = 0;

    virtual void slaveOnOffChanged (bool isSlaving) = 0;
    virtual void punchOnOffChanged (bool isPunching) = 0;
    virtual void undoStatusChanged (bool, bool)         {}

    // tells the device that one of the parameters has been changed.
    //
    // parameterNumber is the physical parameter number on the device, not the
    // virtual one.
    virtual void parameterChanged (int parameterNumber, const ParameterSetting& newValue) = 0;
    virtual void clearParameter (int parameterNumber) = 0;

    virtual void markerChanged (int parameterNumber, const MarkerSetting& newValue) = 0;
    virtual void clearMarker (int parameterNumber) = 0;

    virtual void auxBankChanged (int)                   {}

    virtual bool wantsMessage (const juce::MidiMessage&) { return true; }
    virtual bool eatsAllMessages()                      { return true; }
    virtual bool canSetEatsAllMessages()                { return false; }
    virtual void setEatsAllMessages(bool)               {}
    virtual bool canChangeSelectedPlugin()              { return true; }
    virtual void currentSelectionChanged()              {}

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

    // sends a MIDI message to the device's back-channel
    void sendMidiCommandToController (const void* midiData, int numBytes);
    void sendMidiCommandToController (const juce::MidiMessage&);

    template <size_t size>
    void sendMidiArray (const juce::uint8 (&rawData)[size])   { sendMidiCommandToController (rawData, (int) size); }

    // tells tracktion that the user has moved a fader.
    // the channel number is the physical channel on the device, regardless of bank selection
    // range 0 to 1.0
    void userMovedFader (int channelNum, float newFaderPosition);

    // tells tracktion that the user has moved a pan pot
    // the channel number is the physical channel on the device, regardless of bank selection
    // range -1.0 to 1.0
    void userMovedPanPot (int channelNum, float newPanPosition);

    // tells tracktion that the master fader has moved.
    void userMovedMasterLevelFader (float newLevel);
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
    /** These values need to be set by the subclass.. */
    juce::String deviceDescription;

    // does this driver need to be able to send MIDI messages
    bool needsMidiChannel = true;

    // does this driver need to be able to send MIDI messages back to the
    // controller as well as receive them?
    bool needsMidiBackChannel = false;

    // device wants MIDI clock
    bool wantsClock = false;

    // can be deleted
    bool deletable = false;

    // number of physical faders
    int numberOfFaderChannels = 0;
    int numCharactersForTrackNames = 0;

    // number of labelled rotary dials that can control things like plugin parameters
    int numParameterControls = 0;
    int numCharactersForParameterLabels = 0;

    // number of markers that can be displayed
    int numMarkers = 0;
    int numCharactersForMarkerLabels = 0;

    int numAuxes = 0;
    int numCharactersForAuxLabels = 0;
    bool wantsAuxBanks = 0;
    bool followsTrackSelection = false;

    Engine& engine;
    ExternalControllerManager& externalControllerManager;
    ExternalController* owner = nullptr;

private:
    Edit* edit = nullptr;

    bool isSafeRecording() const;
    void performIfNotSafeRecording (const std::function<void()>&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlSurface)
};

} // namespace tracktion_engine
