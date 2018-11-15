/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MidiNoteDispatcher::MidiNoteDispatcher()
{
}

MidiNoteDispatcher::~MidiNoteDispatcher()
{
    stopTimer();
}

void MidiNoteDispatcher::nextBlockStarted (PlayHead& playhead, EditTimeRange streamTime, int blockSize)
{
    ScopedLock s (deviceLock);

    for (auto state : devices)
    {
        auto delay = state->device->getMidiOutput().getDeviceDelay();
        auto& buffer = state->device->refillBuffer (playhead, streamTime - delay, blockSize);
        state->device->context.masterLevels.processMidi (buffer, 0);
        state->buffer.mergeFromAndClear (buffer);
    }
}

void MidiNoteDispatcher::masterTimeUpdate (PlayHead& playhead, double streamTime)
{
    const ScopedLock s (timeLock);
    masterTime = playhead.streamTimeToSourceTime (streamTime);
    hiResClockOfMasterTime = Time::getMillisecondCounterHiRes();
}

void MidiNoteDispatcher::prepareToPlay (PlayHead& playhead, double start)
{
    const ScopedLock s (timeLock);
    masterTime = playhead.streamTimeToSourceTime (start);
    hiResClockOfMasterTime = Time::getMillisecondCounterHiRes();
}

double MidiNoteDispatcher::getCurrentTime() const
{
    const ScopedLock s (timeLock);
    return masterTime + (Time::getMillisecondCounterHiRes() - hiResClockOfMasterTime) * 0.001;
}

void MidiNoteDispatcher::setMidiDeviceList (const OwnedArray<MidiOutputDeviceInstance>& newList)
{
    CRASH_TRACER
    OwnedArray<DeviceState> newDevices;

    for (auto* d : newList)
        newDevices.add (new DeviceState (d));

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
            auto* device = d->device;
            auto& buffer = d->buffer;
            auto& midiOut = device->getMidiOutput();

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

                    if (noteTime > currentTime + 0.2)
                    {
                        buffer.remove (0);
                    }
                    else if (noteTime <= currentTime)
                    {
                        messagesToSend.add ({ device, message });
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
