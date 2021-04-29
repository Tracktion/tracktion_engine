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

MidiNoteDispatcher::MidiNoteDispatcher()
{
}

MidiNoteDispatcher::~MidiNoteDispatcher()
{
    stopTimer();
}

void MidiNoteDispatcher::renderDevices (PlayHead& playhead, EditTimeRange streamTime, int blockSize)
{
    auto editTime = playhead.streamTimeToSourceTime (streamTime.getStart());
    
    ScopedLock s (deviceLock);

    for (auto state : devices)
    {
        auto delay = state->device.getMidiOutput().getDeviceDelay();
        state->device.refillBuffer (playhead, streamTime - delay, blockSize);
        dispatchPendingMessages (*state, editTime);
    }
}

void MidiNoteDispatcher::dispatchPendingMessagesForDevices (double editTime)
{
    ScopedLock s (deviceLock);

    for (auto state : devices)
        dispatchPendingMessages (*state, editTime);
}

void MidiNoteDispatcher::masterTimeUpdate (double editTime)
{
    const ScopedLock s (timeLock);
    masterTime = editTime;
    hiResClockOfMasterTime = Time::getMillisecondCounterHiRes();
}

void MidiNoteDispatcher::prepareToPlay (double editTime)
{
    masterTimeUpdate (editTime);
}

double MidiNoteDispatcher::getCurrentTime() const
{
    const ScopedLock s (timeLock);
    return masterTime + (Time::getMillisecondCounterHiRes() - hiResClockOfMasterTime) * 0.001;
}

void MidiNoteDispatcher::dispatchPendingMessages (DeviceState& state, double editTime)
{
    // N.B. This should only be called under a deviceLock
    auto& pendingBuffer = state.device.getPendingMessages();
    state.device.context.masterLevels.processMidi (pendingBuffer, nullptr);
    const double delay = state.device.getMidiOutput().getDeviceDelay();
    
    if (! state.device.sendMessages (pendingBuffer, editTime - delay))
        state.buffer.mergeFromAndClear (pendingBuffer);
}

void MidiNoteDispatcher::setMidiDeviceList (const OwnedArray<MidiOutputDeviceInstance>& newList)
{
    CRASH_TRACER
    OwnedArray<DeviceState> newDevices;

    for (auto d : newList)
        newDevices.add (new DeviceState (*d));

    if (newList.isEmpty())
        stopTimer();

    bool startTimerFlag = false;

    {
        const ScopedLock sl (deviceLock);
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
        MidiMessage message;
    };

    Array<MessageToSend> messagesToSend;
    messagesToSend.ensureStorageAllocated (32);

    {
        const ScopedLock sl (deviceLock);

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

                    auto noteTime = message.getTimeStamp();
                    auto currentTime = getCurrentTime();

                    if (noteTime > currentTime + 0.25)
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

}
