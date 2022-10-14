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

class TranzportControlSurface   : public ControlSurface
{
public:
    TranzportControlSurface (ExternalControllerManager&);
    ~TranzportControlSurface();

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;

    void acceptMidiMessage (int, const juce::MidiMessage&) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void recordStateChanged (bool isRecording) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void punchOnOffChanged (bool isPunching) override;
    bool canChangeSelectedPlugin() override;

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

}} // namespace tracktion { inline namespace engine
