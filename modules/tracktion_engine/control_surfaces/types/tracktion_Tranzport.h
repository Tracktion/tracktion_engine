/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class TranzportControlSurface   : public ControlSurface
{
public:
    TranzportControlSurface (ExternalControllerManager&);
    ~TranzportControlSurface();

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;

    void updateMiscFeatures() override;
    void acceptMidiMessage (const juce::MidiMessage&) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channel, const char* bus, float newPos) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void channelLevelChanged (int channel, float level) override;
    void trackSelectionChanged (int channel, bool isSelected) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void masterLevelsChanged (float leftLevel, float rightLevel) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void punchOnOffChanged (bool isPunching) override;
    void parameterChanged (int parameterNumber, const ParameterSetting& newValue) override;
    void clearParameter (int parameterNumber) override;
    bool canChangeSelectedPlugin() override;
    void currentSelectionChanged() override;
    void markerChanged(int parameterNumber, const MarkerSetting& newValue) override;
    void clearMarker (int parameterNumber) override;

private:
    void updateDisplay();
    void updateTCDisplay (const char*);
    void displayPrint (int pos, const char*);

    int currentTrack = -1;
    bool shift = false;
    float faderPos = 0, pan = 0;
    juce::String trackName;
    bool snap = false, panMode = false, init = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TranzportControlSurface)
};

} // namespace tracktion_engine
