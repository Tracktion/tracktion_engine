/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

class MidiNoteDispatcher   : private juce::HighResolutionTimer
{
public:
    MidiNoteDispatcher();
    ~MidiNoteDispatcher() override;

    //==============================================================================
    void setMidiDeviceList (const juce::OwnedArray<MidiOutputDeviceInstance>&);

    void renderDevices (PlayHead&, EditTimeRange streamTime, int blockSize);
    void dispatchPendingMessagesForDevices (double editTime);
    
    void masterTimeUpdate (double editTime);
    void prepareToPlay (double editTime);

    void hiResTimerCallback() override;

private:
    //==============================================================================
    struct DeviceState
    {
        DeviceState (MidiOutputDeviceInstance& d) : device (d) {}

        MidiOutputDeviceInstance& device;
        MidiMessageArray buffer;
    };

    //==============================================================================
    juce::OwnedArray<DeviceState> devices;
    juce::CriticalSection timeLock, deviceLock;
    double masterTime = 0, hiResClockOfMasterTime = 0;

    double getCurrentTime() const;
    void dispatchPendingMessages (DeviceState&, double editTime);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteDispatcher)
};

} // namespace tracktion_engine
