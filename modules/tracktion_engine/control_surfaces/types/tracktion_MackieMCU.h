/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MackieMCU  : public ControlSurface,
                   private juce::Timer,
                   private juce::AsyncUpdater
{
public:
    MackieMCU (ExternalControllerManager&);
    ~MackieMCU();

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;
    void updateMiscFeatures() override;
    void cpuTimerCallback();
    void auxTimerCallback();
    void acceptMidiMessage (const juce::MidiMessage&) override;
    void acceptMidiMessage (int deviceIdx, const juce::MidiMessage&);
    void flip();
    void setAssignmentText (const juce::String&);
    void setDisplay (int devIdx, const char* topLine, const char* bottomLine);

    enum AssignmentMode
    {
        PanMode = 0,
        PluginMode,
        AuxMode,
        MarkerMode
    };

    enum
    {
        mcuFootswitch1Key = 0x123001,
        mcuFootswitch2Key = 0x123002
    };

    void indicesChanged();
    void setSignalMetersEnabled (bool);
    void setSignalMetersEnabled (int dev, bool);
    void setAssignmentMode (AssignmentMode);
    void moveFaderInt (int dev, int channelNum, float newSliderPos);
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPotInt (int dev, int channelNum, float newPan);
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channelNum, const char* bus, float newPos) override;
    void lightUpButton (int dev, int buttonNum, bool on);
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void trackSelectionChanged (int channel, bool isSelected) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void punchOnOffChanged (bool isPunching) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void undoStatusChanged (bool undo, bool redo) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void parameterChanged (int parameterNumber, const ParameterSetting&) override;
    void clearParameter (int parameterNumber) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void channelLevelChanged (int channel, float level) override;
    void masterLevelsChanged (float leftLevel, float rightLevel) override;
    void updateTCDisplay (const char* newDigits);
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds,
                          int millisecs, bool isBarsBeats, bool isFrames) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    bool showingPluginParams() override;
    bool showingMarkers() override;
    bool showingTracks() override;
    void markerChanged (int parameterNumber, const MarkerSetting&) override;
    void clearMarker (int parameterNumber) override;
    void auxBankChanged (int bank) override;
    void clearAux (int channel) override;

    //==============================================================================
    void registerXT (MackieXT*);
    void unregisterXT (MackieXT*);
    void sendMidiCommandToController (int devIdx, juce::uint8 byte1, juce::uint8 byte2, juce::uint8 byte3);
    void sendMidiCommandToController (int devIdx, const unsigned char* midiData, int numBytes);
    void setDeviceIndex (int idx)           { deviceIdx = idx; }

    bool isOneTouchRecord()                 { return oneTouchRecord;  }
    void setOneTouchRecord (bool b)         { oneTouchRecord = b;     }

    //==============================================================================
    enum
    {
        maxNumSurfaces = 4,
        maxNumChannels = maxNumSurfaces * 8,
        maxCharsOnDisplay = 128
    };

private:
    AssignmentMode assignmentMode = PanMode;
    float panPos[maxNumChannels];
    char timecodeDigits[9];
    bool lastSmpteBeatsSetting = false, isZooming = false, marker = false, nudge = false;
    juce::uint8 lastChannelLevels[maxNumChannels];
    bool recLight[maxNumChannels];
    bool isRecButtonDown = false, flipped = false, editFlip = false, cpuVisible = false, oneTouchRecord = false;
    int shiftKeysDown = 0;
    char currentDisplayChars[maxNumSurfaces][maxCharsOnDisplay];
    char newDisplayChars[maxNumSurfaces][maxCharsOnDisplay];
    juce::uint32 lastRewindPress = 0;
    juce::uint32 lastFaderPos[maxNumSurfaces][9];
    float auxLevels[maxNumChannels];
    char auxBusNames[maxNumChannels][7];
    int lastStartChan = 0;
    int deviceIdx = 0;
    int auxBank = 0;
    juce::Array<int> userMovedAuxes;

    void clearAllDisplays();
    void setDisplay (int devIdx, const juce::String& text, int pos);
    void clearDisplaySegment (int devIdx, int column, int row);
    void setDisplaySegment (int devIdx, int column, int row, const juce::String& text);
    void centreDisplaySegment (int devIdx, int column, int row, const juce::String& text);

    void handleAsyncUpdate() override;
    void timerCallback() override;

    struct CpuMeterTimer : public Timer
    {
        CpuMeterTimer (MackieMCU& mcu) : owner (mcu) {}
        void timerCallback() override   { owner.cpuTimerCallback(); }

        MackieMCU& owner;
    };

    struct AuxTimer : public Timer
    {
        AuxTimer (MackieMCU& mcu) : owner (mcu) {}
        void timerCallback() override   { owner.auxTimerCallback(); }

        MackieMCU& owner;
    };

    std::unique_ptr<CpuMeterTimer> cpuMeterTimer;
    std::unique_ptr<AuxTimer> auxTimer;
    juce::Array<MackieXT*> extenders;

    juce::String auxString (int chan) const;


    bool isEditValidAndNotSafeRecording() const;
};

} // namespace tracktion_engine
