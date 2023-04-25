/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_ENABLE_AUTOMAP

namespace tracktion { inline namespace engine
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
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    bool wantsMessage (int, const juce::MidiMessage&) override;
    bool canSetEatsAllMessages() override;
    bool canChangeSelectedPlugin() override;
    bool showingPluginParams() override;
    bool showingMarkers() override;
    bool showingTracks() override;
    bool isPluginSelected (Plugin*) override;
    bool eatsAllMessages() override;

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

}} // namespace tracktion { inline namespace engine

#endif
