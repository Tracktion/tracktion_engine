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

class MidiNoteDispatcher   : private juce::HighResolutionTimer
{
public:
    MidiNoteDispatcher();
    ~MidiNoteDispatcher() override;

    //==============================================================================
    void setMidiDeviceList (const juce::OwnedArray<MidiOutputDeviceInstance>&);

    void dispatchPendingMessagesForDevices (TimePosition editTime);
    
    void masterTimeUpdate (TimePosition editTime);
    void prepareToPlay (TimePosition editTime);

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
    TimePosition masterTime;
    double hiResClockOfMasterTime = 0;

    TimePosition getCurrentTime() const;
    void dispatchPendingMessages (DeviceState&, TimePosition editTime);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteDispatcher)
};

}} // namespace tracktion { inline namespace engine
