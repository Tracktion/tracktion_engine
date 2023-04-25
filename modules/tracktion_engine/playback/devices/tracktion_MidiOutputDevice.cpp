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

static bool shouldSendAllControllersOffMessages = true;

//==============================================================================
class MidiTimecodeGenerator
{
public:
    MidiTimecodeGenerator()
    {
        update (nullptr);
    }

    void update (Edit* edit)
    {
        framesPerSecond = 24;
        midiTCType = juce::MidiMessage::fps24;
        isDropFrame = false;
        offset = 0;
        timeBetweenMessages = 0.1;

        if (edit != nullptr)
        {
            framesPerSecond = edit->getTimecodeFormat().getFPS();
            offset = edit->getTimecodeOffset().inSeconds();

            if (framesPerSecond == 25)
                midiTCType = juce::MidiMessage::fps25;
            else if (framesPerSecond == 30)
                midiTCType = juce::MidiMessage::fps30drop;

            timeBetweenMessages = 1.0 / (framesPerSecond  * 4);
        }
    }

    void addMessages (bool isPlaying, bool isScrubbing,
                      TransportControl* tc,
                      MidiMessageArray& buffer,
                      double blockStart, double blockEnd)
    {
        CRASH_TRACER
        if (isScrubbing || ! isPlaying)
        {
            auto len = blockEnd - blockStart;

            blockStart = tc != nullptr ? tc->getCurrentPosition() : 0.0;
            blockEnd = blockStart + len;
        }

        if (playing != isPlaying
            || scrubbing != isScrubbing)
        {
            playing = isPlaying;
            scrubbing = isScrubbing;

            updateParts (blockStart);
            buffer.addMidiMessage (juce::MidiMessage::fullFrame (hours, minutes, seconds, frames, midiTCType),
                                   0, MidiMessageArray::notMPE);
            lastMessageSent = juce::Time::getMillisecondCounter();
        }
        else if (lastTime != blockStart)
        {
            lastTime = blockStart;

            if (scrubbing)
            {
                auto now = juce::Time::getMillisecondCounter();

                if (now > lastMessageSent + 50)
                {
                    updateParts (blockStart);
                    buffer.addMidiMessage (juce::MidiMessage::fullFrame (hours, minutes, seconds, frames, midiTCType),
                                           0, MidiMessageArray::notMPE);
                    lastMessageSent = now;
                }
            }
            else
            {
                auto sequenceNum = (int) std::floor (blockStart / timeBetweenMessages);
                auto t = sequenceNum * timeBetweenMessages;

                while (t < blockEnd)
                {
                    if (t >= blockStart)
                    {
                        updateParts (t);

                        int value = 0;
                        auto sequenceIndex = (sequenceNum + 65536) & 7;

                        switch (sequenceIndex)
                        {
                            case 0:  value = frames & 0x0f;   break;
                            case 1:  value = (frames >> 4);   break;
                            case 2:  value = seconds & 0x0f;  break;
                            case 3:  value = (seconds >> 4);  break;
                            case 4:  value = minutes & 0x0f;  break;
                            case 5:  value = (minutes >> 4);  break;
                            case 6:  value = hours & 0x0f;    break;

                            case 7:
                                value = ((hours >> 4) & 1);

                                if (framesPerSecond == 25)
                                {
                                    value |= 0x02;
                                }
                                else if (framesPerSecond == 30)
                                {
                                    if (isDropFrame)
                                        value |= 0x04;
                                    else
                                        value |= 0x06;
                                }

                                break;
                        }

                        buffer.addMidiMessage (juce::MidiMessage::quarterFrame (sequenceIndex, value),
                                               t - blockStart, MidiMessageArray::notMPE);
                    }

                    sequenceNum++;
                    t += timeBetweenMessages;
                }
            }
        }
    }

private:
    bool playing = false, scrubbing = false;
    double lastTime = 0, offset = 0, timeBetweenMessages = 0;
    uint32_t lastMessageSent = 0;
    int framesPerSecond = 0;
    bool isDropFrame = false;
    juce::MidiMessage::SmpteTimecodeType midiTCType = juce::MidiMessage::fps24;
    int hours = 0, minutes = 0, seconds = 0, frames = 0;

    void updateParts (double t)
    {
        const double nudge = 0.05 / 96000.0;

        t = std::max (0.0, t + offset) + nudge;

        frames  = ((int) (t * framesPerSecond)) % framesPerSecond;
        hours   = (int) (t * (1.0 / 3600.0));
        minutes = (((int) t) / 60) % 60;
        seconds = (((int) t) % 60);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiTimecodeGenerator)
};

//==============================================================================
class MidiClockGenerator
{
public:
    MidiClockGenerator()
    {
        reset (nullptr);
    }

    void reset (Edit* edit)
    {
        const juce::ScopedLock sl (positionLock);
        wasPlaying = false;
        position = nullptr;
        needsToSendPosition = true;
        lastBlockStart = -100000.0s;
        lastBlockEndPPQ = 0;

        if (edit != nullptr)
            position = std::make_unique<tempo::Sequence::Position> (createPosition (edit->tempoSequence));
    }

    void addMessages (bool playHeadIsPlaying, TransportControl* tc, MidiMessageArray& buffer,
                      TimePosition blockStartTime, TimeDuration blockLength)
    {
        CRASH_TRACER
        const bool isPlaying = playHeadIsPlaying && position != nullptr;

        if (isPlaying != wasPlaying)
        {
            wasPlaying = isPlaying;
            needsToSendPosition = true;

            if (! isPlaying)
                buffer.addMidiMessage (juce::MidiMessage::midiStop(), 0, MidiMessageArray::notMPE);
        }

        if (isPlaying)
        {
            const juce::ScopedLock sl (positionLock);

            position->set (blockStartTime);
            auto blockStartPPQ = position->getPPQTime();

            position->set (blockStartTime + blockLength);
            auto endPPQ = position->getPPQTime();

            const bool jumped = std::abs (lastBlockEndPPQ - endPPQ) > 0.4;
            lastBlockEndPPQ = endPPQ;

            bool looped = blockStartTime < lastBlockStart;
            lastBlockStart = blockStartTime;

            needsToSendPosition = needsToSendPosition || jumped || looped;

            const double startNum = blockStartPPQ * 24.0;
            const double endNum = endPPQ * 24.0;
            const double timePerNum = blockLength.inSeconds() / (endNum - startNum);
            auto num = (int) std::floor (startNum + 0.999999);

            while (num < endNum)
            {
                if (needsToSendPosition)
                {
                    if ((num % 24) == 0)
                    {
                        buffer.addMidiMessage (juce::MidiMessage::songPositionPointer (num / 6), 0);

                        if (num == 0 || (tc != nullptr && tc->isRecording()))
                            buffer.addMidiMessage (juce::MidiMessage::midiStart(), 0, MidiMessageArray::notMPE);
                        else
                            buffer.addMidiMessage (juce::MidiMessage::midiContinue(), 0, MidiMessageArray::notMPE);

                        needsToSendPosition = false;
                    }
                }

                if (! needsToSendPosition)
                    buffer.addMidiMessage (juce::MidiMessage::midiClock(),
                                           (num - startNum) * timePerNum,
                                           MidiMessageArray::notMPE);

                ++num;
            }
        }
    }

private:
    bool wasPlaying = false;
    bool needsToSendPosition = false;
    juce::CriticalSection positionLock;
    std::unique_ptr<tempo::Sequence::Position> position;
    TimePosition lastBlockStart;
    double lastBlockEndPPQ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClockGenerator)
};

//==============================================================================
MidiOutputDevice::MidiOutputDevice (Engine& e, const juce::String& deviceName, int index)
    : OutputDevice (e, TRANS("MIDI Output"), deviceName),
      deviceIndex (index)
{
    enabled = true;
	
    timecodeGenerator = std::make_unique<MidiTimecodeGenerator>();
    midiClockGenerator = std::make_unique<MidiClockGenerator>();
    programNameSet = getMidiProgramManager().getDefaultCustomName();

    loadProps();
    shouldSendAllControllersOffMessages = getControllerOffMessagesSent (engine);
}

MidiOutputDevice::~MidiOutputDevice()
{
    notifyListenersOfDeletion();
    closeDevice();
}

void MidiOutputDevice::setEnabled (bool b)
{
    if (b != enabled || firstSetEnabledCall)
    {
        enabled = b;
        ScopedWaitCursor waitCursor;

        if (b)
        {
            enabled = false;
            saveProps();

            DeadMansPedalMessage dmp (engine.getPropertyStorage(),
                                      TRANS("The last time the app was started, the MIDI output device \"XZZX\" failed to "
                                            "start properly, and has been disabled.").replace ("XZZX", getName())
                                        + "\n\n" + TRANS("Use the settings panel to re-enable it."));

            enabled = true;
            auto error = openDevice();
            enabled = error.isEmpty();

            if (! enabled)
                engine.getUIBehaviour().showWarningMessage (error);
        }
        else
        {
            closeDevice();
        }

        firstSetEnabledCall = false;
        saveProps();
        engine.getDeviceManager().checkDefaultDevicesAreValid();

        engine.getExternalControllerManager().midiInOutDevicesChanged();
    }
}

juce::String MidiOutputDevice::prepareToPlay (Edit* edit, TimePosition)
{
    if (outputDevice == nullptr)
        return TRANS("Couldn't open the MIDI port");

    stop();

    timecodeGenerator->update (edit);
    midiClockGenerator->reset (edit);
    sampleRate = engine.getDeviceManager().getSampleRate();

    defaultMidiDevice = (engine.getDeviceManager().getDefaultMidiOutDevice() == this);

    return {};
}

bool MidiOutputDevice::start()
{
    if (outputDevice != nullptr)
    {
        audioAdjustmentDelay = juce::roundToInt (2.0 * engine.getDeviceManager().getBlockSizeMs());
        playing = true;
        return true;
    }

    return false;
}

void MidiOutputDevice::stop()
{
    if (outputDevice != nullptr)
    {
        if (playing)
        {
            playing = false;
            sendNoteOffMessages();
        }
    }
}

void MidiOutputDevice::setControllerOffMessagesSent (Engine& e, bool b)
{
    shouldSendAllControllersOffMessages = b;
    e.getPropertyStorage().setProperty (SettingID::sendControllerOffMessages, b);
}

bool MidiOutputDevice::getControllerOffMessagesSent (Engine& e)
{
    return e.getPropertyStorage().getProperty (SettingID::sendControllerOffMessages, true);
}

juce::String MidiOutputDevice::getNameForMidiNoteNumber (int note, int midiChannel, bool useSharp) const
{
    return midiChannel == 10 ? TRANS(juce::MidiMessage::getRhythmInstrumentName (note))
                             : juce::MidiMessage::getMidiNoteName (note, useSharp, true,
                                                                   engine.getEngineBehaviour().getMiddleCOctave());
}

void MidiOutputDevice::updateMidiTC (Edit* edit)
{
    timecodeGenerator->update (edit);
}

void MidiOutputDevice::setSendingMMC (bool b)
{
    sendingMMC = b;
}

bool MidiOutputDevice::isConnectedToExternalController() const
{
    for (auto* ec : engine.getExternalControllerManager().getControllers())
        if (ec->isUsingMidiOutputDevice (this) && ec->isEnabled())
            return true;

    return false;
}

void MidiOutputDevice::loadProps()
{
    preDelayMillisecs = 0;
    sendTimecode = false;
    sendMidiClock = false;
    sendControllerMidiClock = false;

    if (auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::midiout, getName()))
    {
        enabled = n->getBoolAttribute ("enabled", enabled);
        preDelayMillisecs = n->getIntAttribute ("preDelay", preDelayMillisecs);
        sendTimecode = n->getBoolAttribute ("sendTimecode", sendTimecode);
        sendMidiClock = n->getBoolAttribute ("sendMidiClock", sendMidiClock);
        timecodeGenerator->update (nullptr);

        if (getName() == "Microsoft GS Wavetable SW Synth")
            programNameSet = n->getStringAttribute ("programNames", TRANS("General MIDI"));
        else
            programNameSet = n->getStringAttribute ("programNames", getMidiProgramManager().getDefaultCustomName());
    }
}

void MidiOutputDevice::saveProps()
{
    juce::XmlElement n ("SETTINGS");

    n.setAttribute ("enabled", enabled);
    n.setAttribute ("preDelay", preDelayMillisecs);
    n.setAttribute ("sendTimecode", sendTimecode);
    n.setAttribute ("sendMidiClock", sendMidiClock);
    n.setAttribute ("programNames", programNameSet);

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::midiout, getName(), n);
}

juce::String MidiOutputDevice::openDevice()
{
    if (isEnabled())
    {
        if (outputDevice == nullptr)
        {
            CRASH_TRACER
            TRACKTION_LOG ("opening MIDI out device:" + getName());

            if (deviceIndex >= 0)
            {
                outputDevice = juce::MidiOutput::openDevice (juce::MidiOutput::getAvailableDevices()[deviceIndex].identifier);

                if (outputDevice == nullptr)
                {
                    TRACKTION_LOG_ERROR ("Failed to open MIDI output " + getName());
                    return TRANS("Couldn't open device");
                }
            }
            else if (softDevice)
            {
               #if JUCE_MAC
                outputDevice = juce::MidiOutput::createNewDevice (getName());
               #else
                outputDevice.reset();
                jassertfalse;
               #endif
            }
            else
            {
                outputDevice.reset();
            }
        }
    }

    return {};
}

void MidiOutputDevice::closeDevice()
{
    saveProps();

    if (outputDevice != nullptr)
    {
        TRACKTION_LOG ("closing MIDI output: " + getName());
        outputDevice = nullptr;
    }
}

void MidiOutputDevice::sendNoteOffMessages()
{
    if (outputDevice != nullptr)
    {
        const juce::ScopedLock sl (noteOnLock);

        for (int channel = channelsUsed.getHighestBit() + 1; --channel > 0;)
        {
            if (channelsUsed [channel])
            {
                for (int note = midiNotesOn.getHighestBit() + 1; --note >= 0;)
                    if (midiNotesOn [note])
                        sendMessageNow (juce::MidiMessage::noteOff (channel, note));

                if (sustain > 0)
                {
                    sendMessageNow (juce::MidiMessage::controllerEvent (channel, 64, 0));
                    sendMessageNow (juce::MidiMessage::controllerEvent (channel, 64, sustain));
                }

                sendMessageNow (juce::MidiMessage::allNotesOff (channel));

                if (shouldSendAllControllersOffMessages)
                    sendMessageNow (juce::MidiMessage::allControllersOff (channel));
            }
        }

        channelsUsed.clear();
        midiNotesOn.clear();
    }
}

void MidiOutputDevice::sendMessageNow (const juce::MidiMessage& message)
{
    if (outputDevice != nullptr)
        outputDevice->sendMessageNow (message);
}

void MidiOutputDevice::fireMessage (const juce::MidiMessage& message)
{
    if (! message.isMetaEvent())
    {
        sendMessageNow (message);

        if (message.isNoteOnOrOff())
        {
            const juce::ScopedLock sl (noteOnLock);

            if (message.isNoteOn())
                midiNotesOn.setBit (message.getNoteNumber());
            else if (message.isNoteOff())
                midiNotesOn.clearBit (message.getNoteNumber());

            channelsUsed.setBit (message.getChannel());
        }
        else if (message.isController() && message.getControllerNumber() == 64)
        {
            sustain = message.getControllerValue();
        }
    }
}

TimeDuration MidiOutputDevice::getDeviceDelay() const noexcept
{
    return TimeDuration::fromSeconds ((preDelayMillisecs + audioAdjustmentDelay) * 0.001);
}

void MidiOutputDevice::setPreDelayMs (int ms)
{
    if (preDelayMillisecs != ms)
    {
        preDelayMillisecs = ms;
        TransportControl::restartAllTransports (engine, false);
        changed();
        saveProps();
    }
}

void MidiOutputDevice::setSendingClock (bool b)
{
    if (sendMidiClock != b)
    {
        sendMidiClock = b;
        changed();
        saveProps();
    }
}

void MidiOutputDevice::flipSendingTimecode()
{
    sendTimecode = ! sendTimecode;
    changed();
    saveProps();
}

juce::StringArray MidiOutputDevice::getProgramSets() const
{
    return getMidiProgramManager().getMidiProgramSetNames();
}

int MidiOutputDevice::getCurrentSetIndex() const
{
    return getMidiProgramManager().getSetIndex (programNameSet);
}

void MidiOutputDevice::setCurrentProgramSet (const juce::String& newSet)
{
    if (programNameSet != newSet)
    {
        programNameSet = newSet;
        changed();
        SelectionManager::refreshAllPropertyPanels();
    }
}

juce::String MidiOutputDevice::getProgramName (int programNumber, int bank)
{
    return getMidiProgramManager().getProgramName (getCurrentSetIndex(), bank, programNumber);
}

bool MidiOutputDevice::canEditProgramSet (int index) const
{
    return getMidiProgramManager().canEditProgramSet (index);
}

bool MidiOutputDevice::canDeleteProgramSet (int index) const
{
    return getMidiProgramManager().canDeleteProgramSet (index);
}

juce::String MidiOutputDevice::getBankName (int bank)
{
    return getMidiProgramManager().getBankName (getCurrentSetIndex(), bank);
}

int MidiOutputDevice::getBankID (int bank)
{
    return getMidiProgramManager().getBankID (getCurrentSetIndex(), bank);
}

bool MidiOutputDevice::areMidiPatchesZeroBased()
{
    return getMidiProgramManager().isZeroBased (getCurrentSetIndex());
}

MidiOutputDeviceInstance* MidiOutputDevice::createInstance (EditPlaybackContext& c)
{
    return new MidiOutputDeviceInstance (*this, c);
}

//==============================================================================
MidiOutputDeviceInstance::MidiOutputDeviceInstance (MidiOutputDevice& d, EditPlaybackContext& e)
    : OutputDeviceInstance (d, e)
{
    timecodeGenerator = std::make_unique<MidiTimecodeGenerator>();
    midiClockGenerator = std::make_unique<MidiClockGenerator>();
}

MidiOutputDeviceInstance::~MidiOutputDeviceInstance()
{
}

juce::String MidiOutputDeviceInstance::prepareToPlay (TimePosition, bool shouldSendMidiTC)
{
    if (getMidiOutput().outputDevice == nullptr)
        return TRANS("Couldn't open the MIDI port");

    stop();

    shouldSendMidiTimecode = shouldSendMidiTC;
    timecodeGenerator->update (&edit);
    midiClockGenerator->reset (&edit);

    sampleRate = edit.engine.getDeviceManager().getSampleRate();
    isDefaultMidiDevice = (edit.engine.getDeviceManager().getDefaultMidiOutDevice() == &getMidiOutput());

    return {};
}

bool MidiOutputDeviceInstance::start()
{
    if (getMidiOutput().outputDevice != nullptr)
    {
        audioAdjustmentDelay = juce::roundToInt (2.0 * edit.engine.getDeviceManager().getBlockSizeMs());
        playing = true;
        return true;
    }

    return false;
}

void MidiOutputDeviceInstance::stop()
{
    if (playing)
    {
        playing = false;
        getMidiOutput().sendNoteOffMessages();
    }
}

void MidiOutputDeviceInstance::mergeInMidiMessages (const MidiMessageArray& source, TimePosition editTime)
{
    midiMessages.mergeFromWithOffset (source, (editTime + getMidiOutput().getDeviceDelay()).inSeconds());
    midiMessages.sortByTimestamp();
}

void MidiOutputDeviceInstance::addMidiClockMessagesToCurrentBlock (bool isPlaying, bool isDragging, TimeRange editTimeRange)
{
    auto& midiOut = getMidiOutput();

    if (shouldSendMidiTimecode)
    {
        if (midiOut.sendTimecode)
            timecodeGenerator->addMessages (isPlaying, isDragging,
                                            &edit.getTransport(), midiMessages,
                                            editTimeRange.getStart().inSeconds(),
                                            editTimeRange.getEnd().inSeconds());

        if (midiOut.sendMidiClock || midiOut.sendControllerMidiClock)
            midiClockGenerator->addMessages (isPlaying,
                                             &edit.getTransport(), midiMessages,
                                             editTimeRange.getStart(),
                                             editTimeRange.getLength());
    }
}

}} // namespace tracktion { inline namespace engine
