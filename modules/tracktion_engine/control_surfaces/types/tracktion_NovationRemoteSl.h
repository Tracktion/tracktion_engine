/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class NovationRemoteSl  : public ControlSurface,
                          private juce::AsyncUpdater
{
public:
    NovationRemoteSl (ExternalControllerManager&);
    ~NovationRemoteSl();

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
    void parameterChanged (int parameterNumber, const ParameterSetting&) override;
    void clearParameter (int parameterNumber) override;
    bool wantsMessage (const juce::MidiMessage&) override;
    bool eatsAllMessages() override;
    bool canChangeSelectedPlugin() override;
    void currentSelectionChanged() override;
    bool showingPluginParams() override;
    bool showingTracks() override;
    void markerChanged (int parameterNumber, const MarkerSetting&) override;
    void clearMarker (int parameterNumber) override;

private:
    enum RightMode
    {
        rmVol,
        rmPan,
        rmMute,
        rmSolo,
    };

    enum LeftMode
    {
        lmTracks,
        lmPlugins,
        lmParam1,
        lmParam2,
    };

    RightMode rightMode = rmVol;
    LeftMode leftMode = lmTracks;
    int activeTrack = 0;
    int trackOffset = 0;
    int pluginOffset = 0;
    int tempo = 0;
    bool isLocked = false;
    bool wasLocked = false;

    bool leftTopDirty = true;
    bool leftBottomDirty = true;
    bool rightTopDirty = true;
    bool rightBottomDirty = true;

    bool mute[8];
    bool solo[8];
    juce::String trackNames[8];
    float pan[8];
    float level[8];
    ParameterSetting param[16];
    bool online = true;

    juce::String screenContents[4];

    void setRightMode (RightMode);
    void setLeftMode (LeftMode);
    void setLock (bool l, bool w);
    void clearLeft();
    void clearRight();
    void clearAllLights();
    void refreshLeft (bool includeTitle);
    void refreshRight (bool includeTitle);
    void drawString (const juce::String&, int panel);
    juce::String padAndLimit (const juce::String&, int max);

    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovationRemoteSl)
};

} // namespace tracktion_engine
