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

struct MidiTimecodeReader  : private juce::MessageListener,
                             private juce::Timer
{
    MidiTimecodeReader (MidiInputDeviceInstanceBase& m)
        : owner (m), transport (m.edit.getTransport())
    {
    }

    bool processMessage (const juce::MidiMessage& m)
    {
        if (m.isFullFrame())
        {
            m.getFullFrameParameters (hours, minutes, seconds, frames, midiTCType);

            lastTime = getTime();
            correctedTime = lastTime - owner.edit.getTimecodeOffset().inSeconds();

            postMessage (new TCMessage (1)); // stop
            jumpPending = true;
            postMessage (new TCMessage (3)); // go to lastTime

            return true;
        }

        if (m.isQuarterFrame())
        {
            const int value = m.getQuarterFrameValue();
            int speedComp = 0;

            startTimer (100);

            if (! transport.isPlaying())
                postMessage (new TCMessage (2)); // play

            switch (m.getQuarterFrameSequenceNumber())
            {
                case 0:   frames  = (frames & 0xf0)  | value;  break;
                case 1:   frames  = (frames & 0x0f)  | (value << 4);  break;
                case 2:   seconds = (seconds & 0xf0) | value;  break;
                case 3:   seconds = (seconds & 0x0f) | (value << 4);  break;
                case 4:   minutes = (minutes & 0xf0) | value;  break;
                case 5:   minutes = (minutes & 0x0f) | (value << 4);  break;
                case 6:   hours   = (hours & 0xf0)   | value;  break;

                case 7:
                {
                    hours = (hours & 0x0f) | ((value << 4) & 0x10);
                    midiTCType = (juce::MidiMessage::SmpteTimecodeType) (value >> 1);

                    lastTime = getTime() + 2.0 / getFPS();

                    correctedTime = lastTime - owner.edit.getTimecodeOffset().inSeconds();

                    const double drift = correctedTime - owner.context.getPosition().inSeconds();

                    averageDrift = averageDrift * 0.9 + drift * 0.1;
                    ++averageDriftNumSamples;

                    if (! jumpPending)
                    {
                        if (std::abs (drift) > 2.0)
                        {
                            owner.context.postPosition (TimePosition::fromSeconds (correctedTime));
                            averageDrift = 0.0;
                            averageDriftNumSamples = 0;
                        }
                        else if (std::abs (averageDrift) > 0.05
                                  && averageDriftNumSamples > 50)
                        {
                            speedComp = (averageDrift > 0.0) ? 1 : -1;
                            averageDrift = 0.0;
                            averageDriftNumSamples = 0;
                        }
                    }

                    if (auto epc = transport.getCurrentPlaybackContext())
                        epc->setSpeedCompensation (speedComp);
                        
                    break;
                }

                default:
                    break;
            }

            return true;
        }

        return false;
    }

    void handleMMCMessage (juce::MidiMessage::MidiMachineControlCommand type)
    {
        auto m = new TCMessage (10);
        m->command = type;
        postMessage (m);
    }

    void handleMMCGotoMessage (int h, int m, int s, int f)
    {
        auto mess = new TCMessage (11);
        mess->data[0] = h;
        mess->data[1] = m;
        mess->data[2] = s;
        mess->data[3] = f;
        postMessage (mess);
    }

private:
    MidiInputDeviceInstanceBase& owner;
    TransportControl& transport;

    int hours = 0, minutes = 0, seconds = 0, frames = 0;
    juce::MidiMessage::SmpteTimecodeType midiTCType;
    double lastTime = 0, correctedTime = 0, averageDrift = 0;
    int averageDriftNumSamples = 0;
    bool jumpPending = false;

    void timerCallback() override
    {
        stopTimer();

        if (transport.isPlaying())
        {
            transport.stop (false, false, false);
            transport.setCurrentPosition (correctedTime);
            averageDrift = 0.0;
            averageDriftNumSamples = 0;
        }
    }

    struct TCMessage  : public juce::Message
    {
        TCMessage (int tp) : type (tp) {}

        int type = 0;
        juce::MidiMessage::MidiMachineControlCommand command;
        int data[4];
    };

    void handleMessage (const juce::Message& message) override
    {
        if (auto m = dynamic_cast<const TCMessage*> (&message))
        {
            if (m->type == 1) // stop
            {
                stopTimer();

                if (transport.isPlaying())
                {
                    transport.stop (false, false, false);
                    transport.setCurrentPosition (correctedTime);
                    averageDrift = 0.0;
                    averageDriftNumSamples = 0;
                }
            }
            else if (m->type == 2) // play
            {
                if (! transport.isPlaying())
                {
                    transport.play (false);
                    startTimer (200);
                    averageDrift = 0.0;
                    averageDriftNumSamples = 0;
                }
            }
            else if (m->type == 3) // goto last time
            {
                transport.setCurrentPosition (correctedTime);

                averageDrift = 0.0;
                averageDriftNumSamples = 0;
                jumpPending = false;
            }
            else if (m->type == 10) // mmc message
            {
                handleMMC (m->command);
            }
            else if (m->type == 11) // mmc goto
            {
                handleMMCGoto (m->data[0], m->data[1], m->data[2], m->data[3]);
            }
        }
    }

    void handleMMC (juce::MidiMessage::MidiMachineControlCommand type)
    {
        switch (type)
        {
            case juce::MidiMessage::mmc_stop:            transport.stop (false, false, false); break;
            case juce::MidiMessage::mmc_play:            transport.play (false);   break;
            case juce::MidiMessage::mmc_deferredplay:    transport.play (false);   break;
            case juce::MidiMessage::mmc_fastforward:     transport.nudgeRight();   break;
            case juce::MidiMessage::mmc_rewind:          transport.nudgeLeft();    break;
            case juce::MidiMessage::mmc_recordStart:     transport.record (false); break;
            case juce::MidiMessage::mmc_recordStop:      transport.stop (false, false, false); break;

            case juce::MidiMessage::mmc_pause:
                if (transport.isPlaying())
                    transport.stop (false, false, false);
                else
                    transport.play (false);

                break;
        }
    }

    void handleMMCGoto (int hours_, int minutes_, int seconds_, int frames_)
    {
        const int fps = owner.edit.getTimecodeFormat().getFPS();
        transport.setCurrentPosition (hours_ * 3600 + minutes_ * 60 + seconds_ + (1.0 / double (fps) * frames_));
    }

    double getFPS() const noexcept
    {
        if (midiTCType == juce::MidiMessage::fps25) return 25.0;
        if (midiTCType == juce::MidiMessage::fps24) return 24.0;
        return 30.0;
    }

    double getTime() const noexcept
    {
        double timeWithoutHours = minutes * 60.0
                                    + seconds
                                    + frames / getFPS();

        if (auto pmi = dynamic_cast<PhysicalMidiInputDevice*> (&owner.getMidiInput()))
            if (pmi->isIgnoringHours())
                return timeWithoutHours;

        return hours * 3600.0 + timeWithoutHours;
    }

    JUCE_DECLARE_NON_COPYABLE (MidiTimecodeReader)
};

//==============================================================================
struct PhysicalMidiInputDeviceInstance  : public MidiInputDeviceInstanceBase
{
    PhysicalMidiInputDeviceInstance (PhysicalMidiInputDevice& d, EditPlaybackContext& c)
        : MidiInputDeviceInstanceBase (d, c)
    {
        timecodeReader = std::make_unique<MidiTimecodeReader> (*this);
    }

    void handleMMCMessage (const juce::MidiMessage& message) override
    {
        int hours, minutes, seconds, frames;

        if (message.isMidiMachineControlGoto (hours, minutes, seconds, frames))
            timecodeReader->handleMMCGotoMessage (hours, minutes, seconds, frames);
        else
            timecodeReader->handleMMCMessage (message.getMidiMachineControlCommand());
    }

    bool handleTimecodeMessage (const juce::MidiMessage& message) override
    {
        return timecodeReader->processMessage (message);
    }

    juce::String prepareToRecord (TimePosition start, TimePosition punchIn, double sampleRate,
                                  int blockSizeSamples, bool isLivePunch) override
    {
        MidiInputDeviceInstanceBase::prepareToRecord (start, punchIn, sampleRate, blockSizeSamples, isLivePunch);

        if (getPhysicalMidiInput().inputDevice != nullptr)
            return {};

        return TRANS("Couldn't open the MIDI port");
    }

    bool startRecording() override
    {
        // We need to keep a list of tracks the are being recorded to
        // here, since user may un-arm track to stop recording
        activeTracks.clear();

        for (auto destTrack : getTargetTracks())
            if (isRecordingActive (*destTrack))
                activeTracks.add (destTrack);
        
        if (getPhysicalMidiInput().inputDevice != nullptr)
        {
            getPhysicalMidiInput().masterTimeUpdate (startTime.inSeconds());
            recording = true;
        }

        return recording;
    }

    PhysicalMidiInputDevice& getPhysicalMidiInput() const   { return static_cast<PhysicalMidiInputDevice&> (owner); }

    std::unique_ptr<MidiTimecodeReader> timecodeReader;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhysicalMidiInputDeviceInstance)
};

//==============================================================================
PhysicalMidiInputDevice::PhysicalMidiInputDevice (Engine& e, const juce::String& deviceName, int deviceIndexToUse)
   : MidiInputDevice (e, TRANS("MIDI Input"), deviceName),
     deviceIndex (deviceIndexToUse)
{
    enabled = true;

    controllerParser.reset (new MidiControllerParser (e));
    loadProps();
}

PhysicalMidiInputDevice::~PhysicalMidiInputDevice()
{
    closeDevice();
}

InputDeviceInstance* PhysicalMidiInputDevice::createInstance (EditPlaybackContext& c)
{
    if (! isTrackDevice() && retrospectiveBuffer == nullptr)
        retrospectiveBuffer.reset (new RetrospectiveMidiBuffer (c.edit.engine));

    return new PhysicalMidiInputDeviceInstance (*this, c);
}

juce::String PhysicalMidiInputDevice::openDevice()
{
    std::memset (keysDown, 0, sizeof (keysDown));
    std::memset (keyDownVelocities, 0, sizeof (keyDownVelocities));

    if (inputDevice == nullptr)
    {
        CRASH_TRACER
        inputDevice = juce::MidiInput::openDevice (juce::MidiInput::getAvailableDevices()[deviceIndex].identifier, this);

        if (inputDevice != nullptr)
        {
            TRACKTION_LOG ("opening MIDI in device: " + getName());
            inputDevice->start();
        }
    }

    if (inputDevice != nullptr)
        return {};

    return TRANS("Couldn't open the MIDI port");
}

void PhysicalMidiInputDevice::closeDevice()
{
    jassert (instances.isEmpty());
    instances.clear();

    if (inputDevice != nullptr)
    {
        CRASH_TRACER
        TRACKTION_LOG ("Closing MIDI in device: " + getName());
        inputDevice = nullptr;
    }

    saveProps();
}

void PhysicalMidiInputDevice::setExternalController (ExternalController* ec)
{
    TRACKTION_LOG ("MIDI External controller assigned: " + getName());
    externalController = ec;
}

void PhysicalMidiInputDevice::removeExternalController (ExternalController* ec)
{
    if (externalController == ec)
        externalController = nullptr;
}

bool PhysicalMidiInputDevice::isAvailableToEdit() const
{
    return isEnabled() && (externalController == nullptr
                             || ! externalController->eatsAllMessages());
}

bool PhysicalMidiInputDevice::tryToSendTimecode (const juce::MidiMessage& message)
{
    if (isAcceptingMMC && message.isMidiMachineControlMessage())
    {
        const juce::ScopedLock sl (instanceLock);

        for (auto p : instances)
            p->handleMMCMessage (message);

        return true;
    }

    if (isReadingMidiTimecode)
    {
        const juce::ScopedLock sl (instanceLock);

        for (auto p : instances)
            if (p->handleTimecodeMessage (message))
                return true;
    }

    return false;
}

void PhysicalMidiInputDevice::handleIncomingMidiMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        if (activeNotes.isNoteActive (m.getChannel(), m.getNoteNumber()))
        {
            // If the note is on and we get another on, send the off before the on
            handleIncomingMidiMessageInt (juce::MidiMessage::noteOff (m.getChannel(), m.getNoteNumber(), 0.0f));
            handleIncomingMidiMessageInt (m);
        }
        else
        {
            // otherwise send the on and track its on
            activeNotes.startNote (m.getChannel(), m.getNoteNumber());
            handleIncomingMidiMessageInt (m);
        }
    }
    else if (m.isNoteOff())
    {
        // Ignore all note offs unless the note was already on
        if (activeNotes.isNoteActive (m.getChannel(), m.getNoteNumber()))
        {
            activeNotes.clearNote (m.getChannel(), m.getNoteNumber());
            handleIncomingMidiMessageInt (m);
        }
    }
    else
    {
        handleIncomingMidiMessageInt (m);
    }
}

void PhysicalMidiInputDevice::handleIncomingMidiMessageInt (const juce::MidiMessage& m)
{
    if (externalController != nullptr && externalController->wantsMessage (*this, m))
    {
        externalController->acceptMidiMessage (*this, m);
    }
    else
    {
        if (! (m.isActiveSense() || disallowedChannels[m.getChannel() - 1]))
        {
            auto message = m;

            if (m.getTimeStamp() == 0 || (! engine.getEngineBehaviour().isMidiDriverUsedForIncommingMessageTiming()))
                message.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);

            message.addToTimeStamp (adjustSecs);

            sendNoteOnToMidiKeyListeners (message);

            if (! retrospectiveRecordLock && retrospectiveBuffer != nullptr)
                retrospectiveBuffer->addMessage (message, adjustSecs);

            if (! tryToSendTimecode (message))
            {
                if (isTakingControllerMessages)
                    controllerParser->processMessage (message);

                sendMessageToInstances (message);
            }
        }

        VirtualMidiInputDevice::broadcastMessageToAllVirtualDevices (this, m);
    }
}

void PhysicalMidiInputDevice::loadProps()
{
    isTakingControllerMessages = true;

    auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::midiin, getName());

    if (n != nullptr)
        isTakingControllerMessages = n->getBoolAttribute ("controllerMessages", isTakingControllerMessages);

    MidiInputDevice::loadProps (n.get());
}

void PhysicalMidiInputDevice::saveProps()
{
    if (isTrackDevice())
        return;

    juce::XmlElement n ("SETTINGS");
    n.setAttribute ("controllerMessages", isTakingControllerMessages);

    MidiInputDevice::saveProps (n);

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::midiin, getName(), n);
}

void PhysicalMidiInputDevice::setReadingMidiTimecode (bool b)
{
    isReadingMidiTimecode = b;
}

void PhysicalMidiInputDevice::setIgnoresHours (bool b)
{
    ignoreHours = b;
}

void PhysicalMidiInputDevice::setAcceptingMMC (bool b)
{
    isAcceptingMMC = b;
}

void PhysicalMidiInputDevice::setReadingControllerMessages (bool b)
{
    isTakingControllerMessages = b;
}

}} // namespace tracktion { inline namespace engine
