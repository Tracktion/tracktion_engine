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

/**
    Acts as a holder for a ControlSurface object.
*/
class ExternalController  : private juce::AsyncUpdater,
                            private SelectableListener,
                            private AutomatableParameter::Listener,
                            private juce::Timer
{
public:
    //==============================================================================
    ExternalController (Engine&, ControlSurface*);
    ~ExternalController() override;

    //==============================================================================
    juce::String getName() const;

    bool needsMidiChannel() const               { return needsChannel; }
    bool needsMidiBackChannel() const           { return needsBackChannel; }

    juce::String getDesiredMidiChannel() const;
    juce::String getDesiredMidiBackChannel() const;

    bool needsOSCSocket() const                 { return needsOSC; }

    int getNumDevices() const;
    void setNumDevices (int);

    int getMainDevice() const;
    void setMainDevice (int);

    juce::String getMidiInputDevice (int idx) const;
    void setMidiInputDevice (int idx, const juce::String& nameOfMidiInput);

    juce::String getBackChannelDevice (int idx) const;
    void setBackChannelDevice (int idx, const juce::String& nameOfMidiOutput);
    bool isUsingMidiOutputDevice (const MidiOutputDevice* d) const noexcept;

    int getOSCInputPort()                       { return oscInputPort; }
    void setOSCInputPort (int port);
    int getOSCOutputPort()                      { return oscOutputPort; }
    void setOSCOutputPort (int port);
    juce::String getOSCOutputAddress()          { return oscOutputAddr; }
    void setOSCOutputAddress (const juce::String addr);

    bool isEnabled() const;
    void setEnabled (bool);

    bool isDeletable() const                    { return deletable; }
    void deleteController();

    bool wantsMidiClock() const                 { return wantsClock; }

    void currentEditChanged (Edit*);

    // these will be called by tracktion when stuff happens, and
    // will pass the message on to the controller.

    // channels are virtual - i.e. not restricted to physical chans
    void moveFader (int channelNum, float newSliderPos);
    void moveMasterFaders (float newLeftPos, float newRightPos);
    void movePanPot (int channelNum, float newPan);
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright);
    void soloCountChanged (bool);
    void playStateChanged (bool isPlaying);
    void recordStateChanged (bool isRecording);
    void automationModeChanged (bool isReading, bool isWriting);
    void snapChanged (bool isOn);
    void loopChanged (bool isOn);
    void clickChanged (bool isOn);
    void channelLevelChanged (int channel, float l, float r);
    void masterLevelsChanged (float leftLevel, float rightLevel);
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames);
    void trackSelected (int channel, bool isSelected);
    void selectOtherObject (SelectableClass::Relationship, bool moveFromCurrentPlugin);
    void muteOrUnmutePlugin();
    void muteOrUnmutePluginsInTrack();
    void changePluginPreset (int delta);
    void soloPluginTrack();
    void auxSendLevelsChanged();

    void updateDeviceState();
    void updateParameters();
    void updateMarkers();
    void selectedPluginChanged();
    void selectableObjectChanged (Selectable*) override;
    void selectableObjectAboutToBeDeleted (Selectable*) override;
    void curveHasChanged (AutomatableParameter&) override;
    void currentValueChanged (AutomatableParameter&, float /*newValue*/) override;
    void updateTrackSelectLights();
    void updateTrackRecordLights();
    void updatePunchLights();
    void updateScrollLights();
    void updateUndoLights();

    int getNumFaderChannels() const noexcept;
    int getFaderIndexInActiveRegion (int num) const noexcept;
    juce::Range<int> getActiveChannels() const noexcept;

    int getNumParameterControls() const noexcept;
    juce::Range<int> getActiveParams() const noexcept;

    void midiInOutDevicesChanged();
    void oscSettingsChanged();

    //==============================================================================
    void handleAsyncUpdate() override;
    void acceptMidiMessage (MidiInputDevice&, const juce::MidiMessage&);
    bool wantsMessage (MidiInputDevice&, const juce::MidiMessage&);
    bool eatsAllMessages() const;
    bool canSetEatsAllMessages();
    void setEatsAllMessages (bool eatAll);

    //==============================================================================
    juce::Colour getSelectionColour() const             { return selectionColour; }
    bool getShowSelectionColour() const                 { return showSelection; }
    void setSelectionColour (juce::Colour);
    void setShowSelectionColour (bool);

    bool shouldTrackBeColoured (int channelNum);
    void getTrackColour (int channelNum, juce::Colour&);

    bool shouldPluginBeColoured (Plugin*);
    void getPluginColour (Plugin*, juce::Colour&);
    void repaintParamSource();
    void redrawTracks();

    ControlSurface& getControlSurface() const noexcept  { return *controlSurface; }
    template <typename Type>
    Type* getControlSurfaceIfType() const noexcept      { return dynamic_cast<Type*> (controlSurface.get()); }

    ExternalControllerManager& getExternalControllerManager() const noexcept    { return controlSurface->externalControllerManager; }

    Edit* getEdit() const                               { return getControlSurface().getEdit(); }
    TransportControl* getTransport() const noexcept     { return getControlSurface().getTransport(); }

    static juce::String shortenName (juce::String, int maxLen);

    juce::String getInputDeviceName (int idx) const     { return inputDeviceName[idx]; }
    juce::String getOutputDeviceName (int idx) const    { return outputDeviceName[idx]; }

    juce::StringArray getMidiInputPorts() const;
    juce::StringArray getMidiOutputPorts() const;

    static juce::String getNoDeviceSelectedMessage();

    Engine& engine;

private:
    void timerCallback() override;

    static constexpr int maxDevices = 4;
    friend class ExternalControllerManager;
    friend class ControlSurface;
    friend class MackieC4;
    friend class MackieMCU;

    int numDevices = 1;
    int mainDevice = 0;
    juce::String inputDeviceName[maxDevices];
    juce::String outputDeviceName[maxDevices];

    std::unique_ptr<ControlSurface> controlSurface;
    int oscInputPort, oscOutputPort;
    juce::String oscOutputAddr;

    bool enabled;
    int maxTrackNameChars;
    int channelStart = 0;
    int startParamNumber = 0;
    bool needsBackChannel = false;
    bool needsChannel = false;
    bool needsOSC = false;
    bool usesSettings = false;
    bool deletable = false;
    bool allowBankingOffEnd = false;
    AutomatableParameter::Array currentParams;
    Selectable::WeakRef currentParamSource, lastRegisteredSelectable;
    bool showSelection = false;
    juce::Colour selectionColour;
    int startMarkerNumber = 0;
    int auxBank = 0;
    bool followsTrackSelection;
    bool processMidi = false, updateParams = false;

    MidiInputDevice* inputDevices[maxDevices] = { nullptr };
    MidiOutputDevice* outputDevices[maxDevices] = { nullptr };

    juce::Array<std::pair<int, juce::MidiMessage>> pendingMidiMessages;
    juce::CriticalSection incomingMidiLock;

    int getMarkerBankOffset() const   { return startMarkerNumber; }
    int getFaderBankOffset() const    { return channelStart;      }
    int getAuxBankOffset() const      { return auxBank;           }
    int getParamBankOffset() const    { return startParamNumber;  }

    void changeFaderBank (int delta, bool moveSelection);
    void changeParamBank (int delta);
    void updateParamList();
    void changeMarkerBank (int delta);
    void changeAuxBank (int delta);

    void userMovedParameterControl (int paramNumber, float newValue);
    void userPressedParameterControl (int paramNumber);
    void userPressedGoToMarker (int marker);

    Plugin* getCurrentPlugin() const;

    bool wantsClock;
};

}} // namespace tracktion { inline namespace engine
