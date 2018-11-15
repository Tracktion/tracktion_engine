/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class C4Translator;

class MackieC4  : public ControlSurface,
                  private juce::AsyncUpdater,
                  private juce::Timer
{
public:
    //==============================================================================
    MackieC4 (ExternalControllerManager&);
    ~MackieC4();

    //==============================================================================
    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;
    void updateMiscFeatures() override;
    void acceptMidiMessage (const juce::MidiMessage&) override;
    void currentSelectionChanged() override;
    void parameterChanged (int parameterNumber, const ParameterSetting& newValue) override;
    void clearParameter (int parameterNumber) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void moveAux (int channelNum, const char* bus, float newPos) override;
    void clearAux (int channel) override;
    void channelLevelChanged (int channel, float level) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void trackSelectionChanged (int channel, bool isSelected) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void punchOnOffChanged (bool isPunching) override;
    void masterLevelsChanged (float leftLevel, float rightLevel) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    bool canChangeSelectedPlugin() override;
    bool showingPluginParams() override;
    bool showingTracks() override;
    void markerChanged (int parameterNumber, const MarkerSetting& newValue) override;
    void clearMarker (int parameterNumber) override;
    void auxBankChanged (int bank) override;
    void pluginBypass (bool) override;

private:
    std::unique_ptr<C4Translator> c4;

    float currentPotPos[40];
    int currentPanPotPos[40];

    char currentText [64 * 2 * 4];
    char newText [64 * 2 * 4];

    int trackViewOffset = 0;
    int selectedTrackNum = 0;
    int auxBank = 0;
    bool initialised = false, isLocked = false, wasLocked = false;
    bool metersEnabled = false, bypass = false;

    enum Mode
    {
        PluginMode1,
        PluginMode2,
        PluginMode3,
        MixerMode,
        AuxMode,
        ButtonMode
    };

    Mode mode = PluginMode1;

    void timerCallback() override;
    void handleAsyncUpdate() override;

    void updateMiscLights();
    void updateDisplayAndPots();
    void setDisplayDirty (int potNumber, int row);
    void updateButtonLights();
    void enableMeters (bool);
    void turnOffPanPotLights();
    void setPotPosition (int potNumber, float value);
    void lightUpPotButton (int buttonNum, bool);
    void setDisplayText (int potNumber, int row, juce::String);
    void lightUpButton (int buttonNum, bool on);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MackieC4)
};

} // namespace tracktion_engine
