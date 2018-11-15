/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MidiNoteDispatcher   : private juce::HighResolutionTimer
{
public:
    MidiNoteDispatcher();
    ~MidiNoteDispatcher();

    //==============================================================================
    void setMidiDeviceList (const juce::OwnedArray<MidiOutputDeviceInstance>&);

    void nextBlockStarted (PlayHead& playhead, EditTimeRange streamTime, int blockSize);
    void masterTimeUpdate (PlayHead& playhead, double streamTime);
    void prepareToPlay (PlayHead& playhead, double start);

    void hiResTimerCallback() override;

private:
    //==============================================================================
    struct DeviceState
    {
        DeviceState (MidiOutputDeviceInstance* d) : device (d) {}

        MidiOutputDeviceInstance* device;
        MidiMessageArray buffer;
    };

    //==============================================================================
    juce::OwnedArray<DeviceState> devices;
    juce::CriticalSection timeLock, deviceLock;
    double masterTime = 0, hiResClockOfMasterTime = 0;

    double getCurrentTime() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteDispatcher)
};

} // namespace tracktion_engine
