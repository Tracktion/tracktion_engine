/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_ENABLE_AUTOMAP

namespace tracktion_engine
{

class AutoMap;

class NovationAutomap  : public ControlSurface,
                         private juce::ChangeListener
{
public:
    NovationAutomap (ExternalControllerManager&);
    ~NovationAutomap();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void currentEditChanged (Edit*) override;

    void paramChanged (AutomatableParameter*);

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;
    void updateMiscFeatures() override;
    void acceptMidiMessage (const juce::MidiMessage&) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channel, const char* bus, float newPos) override;
    void clearAux (int channel) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void channelLevelChanged (int channel, float level) override;
    void trackSelectionChanged(int channel, bool isSelected) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void masterLevelsChanged(float leftLevel, float rightLevel) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void punchOnOffChanged (bool isPunching) override;
    void undoStatusChanged (bool undo, bool redo) override;
    void parameterChanged (int parameterNumber, const ParameterSetting& newValue) override;
    void clearParameter (int parameterNumber) override;
    void markerChanged (int parameterNumber, const MarkerSetting& newValue) override;
    void clearMarker (int parameterNumber) override;
    void auxBankChanged (int bank) override;
    bool wantsMessage (const juce::MidiMessage&) override;
    bool eatsAllMessages() override;
    bool canSetEatsAllMessages() override;
    void setEatsAllMessages(bool eatAll) override;
    bool canChangeSelectedPlugin() override;
    void currentSelectionChanged() override;
    bool showingPluginParams() override;
    bool showingMarkers() override;
    bool showingTracks() override;
    void pluginBypass (bool) override;
    bool isPluginSelected (Plugin*) override;

    void removePlugin (Plugin*);
    void pluginChanged (Plugin*);

    void save (Edit&);
    void load (Edit&);

    bool mapPlugin = false;
    bool mapNative = false;

private:
    void createAllPluginAutomaps();

    friend class PluginAutoMap;

    std::unique_ptr<AutoMap> hostAutomap;
    juce::OwnedArray<AutoMap> pluginAutomap;

    bool enabled = false;
    bool pluginSelected = false;
    SelectionManager* selectionManager = nullptr;

    mutable juce::StringPairArray guids;
};

} // namespace tracktion_engine

#endif
