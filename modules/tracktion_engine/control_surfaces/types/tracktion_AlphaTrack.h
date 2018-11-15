/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AlphaTrackControlSurface : public ControlSurface,
                                 private juce::MultiTimer,
                                 private juce::AsyncUpdater
{
public:
    AlphaTrackControlSurface (ExternalControllerManager&);
    ~AlphaTrackControlSurface();

    enum
    {
        alphaTrackF1      = 0x123003,
        alphaTrackF2      = 0x123004,
        alphaTrackF3      = 0x123005,
        alphaTrackF4      = 0x123006,
        alphaTrackF5      = 0x123007,
        alphaTrackF6      = 0x123008,
        alphaTrackF7      = 0x123009,
        alphaTrackF8      = 0x123010,
        alphaTrackFoot    = 0x123011
    };

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;
    void updateMiscFeatures() override;
    void acceptMidiMessage (const juce::MidiMessage&) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channel, const char* bus, float newPos) override;
    void clearAux (int channel) override;
    void soloCountChanged (bool) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
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
    void markerChanged (int parameterNumber, const MarkerSetting& newValue) override;
    void clearMarker (int parameterNumber) override;
    void currentSelectionChanged() override;
    void pluginBypass (bool b) override;
    bool isPluginSelected (Plugin*) override;

private:
    enum Mode { Pan, Send, Eq, Plugin1, Plugin2, Auto };

    char displayBuffer[33];
    Mode mode;
    bool init, shift;
    int currentTrack;
    juce::String trackName;
    int fingers, stripPos;
    bool jogged;
    float faderPos, pan;
    int flip;
    bool isReading, isWriting;
    int offset;
    AutomatableParameter::Ptr param;
    Plugin::Ptr plugin;
    int touch;
    float pos;
    bool isPunching, isLoopOn;
    int physicalFaderPos;

    void timerCallback (int) override;
    void handleAsyncUpdate() override;
    bool doKeyPress (int key, bool force = false);

    void displayPrint (int line, int pos, const char* const text);
    void setLed (int led, bool state);
    void updateDisplay();
    void clearAllLeds();
    void setParam (AutomatableParameter::Ptr);
    void getPlugin (int idx);
    void setMode (Mode);

    void touchParam (int param);
    void turnParam (int param, int delta);
    AutomatableParameter::Ptr getParam (int param) const;
    void updateFlip();
    void moveFaderInt (float newSliderPos);
    bool isOnEditScreen() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlphaTrackControlSurface)
};

} // namespace tracktion_engine
