/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
    mutable RealTimeSpinLock timeLock;
    std::shared_mutex deviceMutex;
    TimePosition masterTime;
    double hiResClockOfMasterTime = 0;

    TimePosition getCurrentTime() const;
    void dispatchPendingMessages (DeviceState&, TimePosition editTime);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteDispatcher)
};

}} // namespace tracktion { inline namespace engine
