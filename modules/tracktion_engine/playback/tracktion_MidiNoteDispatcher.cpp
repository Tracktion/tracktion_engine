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

MidiNoteDispatcher::MidiNoteDispatcher()
{
}

MidiNoteDispatcher::~MidiNoteDispatcher()
{
    stopTimer();
}

void MidiNoteDispatcher::dispatchPendingMessagesForDevices (TimePosition editTime)
{
    juce::ScopedLock s (deviceLock);

    for (auto state : devices)
        dispatchPendingMessages (*state, editTime);
}

void MidiNoteDispatcher::masterTimeUpdate (TimePosition editTime)
{
    const juce::ScopedLock s (timeLock);
    masterTime = editTime;
    hiResClockOfMasterTime = juce::Time::getMillisecondCounterHiRes();
}

void MidiNoteDispatcher::prepareToPlay (TimePosition editTime)
{
    masterTimeUpdate (editTime);
}

TimePosition MidiNoteDispatcher::getCurrentTime() const
{
    const juce::ScopedLock s (timeLock);
    return masterTime + TimeDuration::fromSeconds ((juce::Time::getMillisecondCounterHiRes() - hiResClockOfMasterTime) * 0.001);
}

void MidiNoteDispatcher::dispatchPendingMessages (DeviceState& state, TimePosition editTime)
{
    // N.B. This should only be called under a deviceLock
    auto& pendingBuffer = state.device.getPendingMessages();
    state.device.context.masterLevels.processMidi (pendingBuffer, nullptr);
    const auto delay = state.device.getMidiOutput().getDeviceDelay();
    
    if (! state.device.sendMessages (pendingBuffer, editTime - delay))
        state.buffer.mergeFromAndClear (pendingBuffer);
}

void MidiNoteDispatcher::setMidiDeviceList (const juce::OwnedArray<MidiOutputDeviceInstance>& newList)
{
    CRASH_TRACER
    juce::OwnedArray<DeviceState> newDevices;

    for (auto d : newList)
        newDevices.add (new DeviceState (*d));

    if (newList.isEmpty())
        stopTimer();

    bool startTimerFlag = false;

    {
        const juce::ScopedLock sl (deviceLock);
        newDevices.swapWith (devices);
        startTimerFlag = ! devices.isEmpty();
    }

    if (startTimerFlag)
        startTimer (1);
}

void MidiNoteDispatcher::hiResTimerCallback()
{
    struct MessageToSend
    {
        MidiOutputDeviceInstance* device;
        juce::MidiMessage message;
    };

    juce::Array<MessageToSend> messagesToSend;
    messagesToSend.ensureStorageAllocated (32);

    {
        const juce::ScopedLock sl (deviceLock);

        for (auto d : devices)
        {
            auto& device = d->device;
            auto& buffer = d->buffer;
            auto& midiOut = device.getMidiOutput();

            if (buffer.isAllNotesOff)
            {
                buffer.clear();
                midiOut.sendNoteOffMessages();
            }
            else
            {
                while (buffer.isNotEmpty())
                {
                    auto& message = buffer[0];

                    auto noteTime = TimePosition::fromSeconds (message.getTimeStamp());
                    auto currentTime = getCurrentTime();

                    if (noteTime > currentTime + TimeDuration::fromSeconds (0.25))
                    {
                        buffer.remove (0);
                    }
                    else if (noteTime <= currentTime)
                    {
                        messagesToSend.add ({ &device, message });
                        buffer.remove (0);
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    for (auto& m : messagesToSend)
        m.device->getMidiOutput().fireMessage (m.message);
}

}} // namespace tracktion { inline namespace engine
