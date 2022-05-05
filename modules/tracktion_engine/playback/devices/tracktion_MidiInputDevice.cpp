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

class MidiControllerParser  : private juce::AsyncUpdater
{
public:
    MidiControllerParser (Engine& e) : engine (e) {}

    void processMessage (const juce::MidiMessage& m)
    {
        const int channel = m.getChannel();

        if (m.isController())
        {
            const int number = m.getControllerNumber();
            const int value = m.getControllerValue();

            if (number == 98)
            {
                lastNRPNFine = value;
                wasNRPN = true;
            }
            else if (number == 99)
            {
                lastNRPNCoarse = value;
                wasNRPN = true;
            }
            else if (number == 100)
            {
                lastRPNFine = value;
                wasNRPN = false;
            }
            else if (number == 101)
            {
                lastRPNCoarse = value;
                wasNRPN = false;
            }
            else if (number == 6)
            {
                // data entry msb..
                if (wasNRPN)
                    lastParamNumber = 0x20000 + (lastNRPNCoarse << 7) + lastNRPNFine;
                else
                    lastParamNumber = 0x30000 + (lastRPNCoarse << 7) + lastRPNFine;

                if (lastParamNumber != 0)
                {
                    controllerMoved (lastParamNumber, value, channel);
                    lastParamNumber = 0;
                }
            }
            else if (number == 0x26)
            {
                // data entry lsb - ignore this because it doesn't always get sent,
                // so we can't wait for it..
            }
            else
            {
                // not sure if this is going to work yet, shove the channel number in the high byte
                lastParamNumber = 0x10000 + number;
                controllerMoved (lastParamNumber, value, channel);
            }
        }
        else if (m.isChannelPressure())
        {
            lastParamNumber = 0x40000;
            controllerMoved (lastParamNumber, m.getChannelPressureValue(), channel);
        }
    }

private:
    void controllerMoved (int number, int value, int channel)
    {
        {
            const juce::ScopedLock sl (pendingLock);
            pendingMessages.add ({ number, channel, value / 127.0f });
        }

        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        juce::Array<Message> messages;

        {
            const juce::ScopedLock sl (pendingLock);
            pendingMessages.swapWith (messages);
        }

        if (auto pcm = ParameterControlMappings::getCurrentlyFocusedMappings (engine))
            for (const auto& m : messages)
                pcm->sendChange (m.controllerID, m.newValue, m.channel);
    }

    int lastParamNumber = 0;
    int lastRPNCoarse = 0, lastRPNFine = 0, lastNRPNCoarse = 0, lastNRPNFine = 0;
    bool wasNRPN = false;

    struct Message
    {
        int controllerID;
        int channel;
        float newValue;
    };

    Engine& engine;
    juce::Array<Message> pendingMessages;
    juce::CriticalSection pendingLock;
};

//==============================================================================
struct RetrospectiveMidiBuffer
{
    RetrospectiveMidiBuffer (Engine& e)
    {
        lengthInSeconds = e.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30);
    }

    void addMessage (const juce::MidiMessage& m, double adjust)
    {
        auto cutoffTime = juce::Time::getMillisecondCounterHiRes() * 0.001
                            + adjust - lengthInSeconds;

        if (m.getTimeStamp() > cutoffTime)
            sequence.add (m);

        // remove all events that are no longer in the time window
        int unused = 0;
        for (int i = 0; i < sequence.size() && sequence[i].getTimeStamp() < cutoffTime; i++)
            unused = i + 1;

        if (unused > 0)
            sequence.removeRange (0, unused);
    }

    juce::MidiMessageSequence getMidiMessages()
    {
        juce::MidiMessageSequence result;

        for (auto m : sequence)
            result.addEvent (m);

        result.updateMatchedPairs();

        juce::SortedSet<int> usedIndexes;

        // remove all unmatched note on / off from the sequence
        for (int i = 0; i < result.getNumEvents(); ++i)
        {
            auto evt = result.getEventPointer (i);

            if (evt->message.isNoteOn() && evt->noteOffObject != nullptr)
            {
                usedIndexes.add (i);
                usedIndexes.add (result.getIndexOfMatchingKeyUp (i));
            }
        }

        for (int i = result.getNumEvents(); --i >= 0;)
        {
            auto evt = result.getEventPointer (i);

            if (evt->message.isNoteOnOrOff() && ! usedIndexes.contains (i))
                result.deleteEvent (i, false);
        }

        result.updateMatchedPairs();

        return result;
    }

    juce::Array<juce::MidiMessage> sequence;
    double lengthInSeconds = 0;
};

//==============================================================================
MidiInputDevice::MidiInputDevice (Engine& e, const juce::String& deviceType, const juce::String& deviceName)
   : InputDevice (e, deviceType, deviceName)
{
    levelMeasurer.setShowMidi (true);

    endToEndEnabled = true;
    std::memset (keysDown, 0, sizeof (keysDown));
    std::memset (keysUp, 0, sizeof (keysUp));
    std::memset (keyDownVelocities, 0, sizeof (keyDownVelocities));

    keyboardState.addListener (this);
}

MidiInputDevice::~MidiInputDevice()
{
    keyboardState.removeListener (this);
    notifyListenersOfDeletion();
}

void MidiInputDevice::setEnabled (bool b)
{
    if ((b != enabled) || (! isTrackDevice() && firstSetEnabledCall))
    {
        CRASH_TRACER
        enabled = b;
        ScopedWaitCursor waitCursor;
        
        if (b)
        {
            enabled = false;
            saveProps();

            DeadMansPedalMessage dmp (engine.getPropertyStorage(),
                                      TRANS("The last time the app was started, the MIDI input device \"XZZX\" failed to start "
                                            "properly, and has been disabled.").replace ("XZZX", getName())
                                        + "\n\n" + TRANS("Use the settings panel to re-enable it."));

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

        if (! isTrackDevice())
            engine.getExternalControllerManager().midiInOutDevicesChanged();
    }
}

void MidiInputDevice::loadProps (const juce::XmlElement* n)
{
    endToEndEnabled = true;
    recordingEnabled = true;
    mergeRecordings = true;
    replaceExistingClips = false;
    recordToNoteAutomation = isMPEDevice();
    adjustSecs = 0;
    manualAdjustMs = 0;
    channelToUse = {};
    programToUse = 0;
    bankToUse = 0;
    overrideNoteVels = false;
    disallowedChannels.clear();

    if (n != nullptr)
    {
        if (! isTrackDevice())
            enabled = n->getBoolAttribute ("enabled", enabled);

        endToEndEnabled = n->getBoolAttribute ("endToEnd", endToEndEnabled);
        recordingEnabled = n->getBoolAttribute ("recEnabled", recordingEnabled);
        mergeRecordings = n->getBoolAttribute ("mergeRecordings", mergeRecordings);
        replaceExistingClips = n->getBoolAttribute ("replaceExisting", replaceExistingClips);
        recordToNoteAutomation = n->getBoolAttribute ("recordToNoteAutomation", recordToNoteAutomation) || isMPEDevice();
        quantisation.setType (n->getStringAttribute ("quantisation"));
        channelToUse = MidiChannel::fromChannelOrZero (n->getIntAttribute ("channel", channelToUse.getChannelNumber()));
        programToUse = n->getIntAttribute ("program", programToUse);
        bankToUse = n->getIntAttribute ("bank", bankToUse);
        overrideNoteVels = n->getBoolAttribute ("useFullVelocity", overrideNoteVels);
        manualAdjustMs = n->getDoubleAttribute ("manualAdjustMs", manualAdjustMs);
        disallowedChannels.parseString (n->getStringAttribute ("disallowedChannels", disallowedChannels.toString (2)), 2);

        connectionStateChanged();
    }
}

void MidiInputDevice::saveProps (juce::XmlElement& n)
{
    n.setAttribute ("enabled", enabled);
    n.setAttribute ("endToEnd", endToEndEnabled);
    n.setAttribute ("recEnabled", recordingEnabled);
    n.setAttribute ("mergeRecordings", mergeRecordings);
    n.setAttribute ("replaceExisting", replaceExistingClips);
    n.setAttribute ("recordToNoteAutomation", recordToNoteAutomation);
    n.setAttribute ("quantisation", quantisation.getType (false));
    n.setAttribute ("channel", channelToUse.getChannelNumber());
    n.setAttribute ("program", programToUse);
    n.setAttribute ("bank", bankToUse);
    n.setAttribute ("useFullVelocity", overrideNoteVels);
    n.setAttribute ("manualAdjustMs", manualAdjustMs);
    n.setAttribute ("disallowedChannels", disallowedChannels.toString (2));
}

juce::Array<AudioTrack*> MidiInputDevice::getDestinationTracks()
{
    if (auto edit = engine.getUIBehaviour().getLastFocusedEdit())
        if (auto in = edit->getCurrentInstanceForInputDevice (this))
            return in->getTargetTracks();

    return {};
}

void MidiInputDevice::setChannelAllowed (int midiChannel, bool b)
{
    disallowedChannels.setBit (midiChannel - 1, ! b);
    changed();
    saveProps();
}

void MidiInputDevice::setEndToEndEnabled (bool b)
{
    if (endToEndEnabled != b)
    {
        endToEndEnabled = b;
        TransportControl::restartAllTransports (engine, false);
        changed();
        saveProps();
    }
}

void MidiInputDevice::setChannelToUse (int newChan)
{
    auto chan = MidiChannel::fromChannelOrZero (newChan);

    if (channelToUse != chan)
    {
        channelToUse = chan;
        changed();
        saveProps();
    }
}

void MidiInputDevice::setProgramToUse (int prog)
{
    programToUse = juce::jlimit (0, 128, prog);
    changed();
}

void MidiInputDevice::setBankToUse (int bank)
{
    bankToUse = bank;
}

void MidiInputDevice::setOverridingNoteVelocities (bool b)
{
    if (overrideNoteVels != b)
    {
        overrideNoteVels = b;
        changed();
        saveProps();
    }
}

void MidiInputDevice::setManualAdjustmentMs (double ms)
{
    if (manualAdjustMs != ms)
    {
        manualAdjustMs = ms;
        changed();
        saveProps();
    }
}

bool MidiInputDevice::isMPEDevice() const
{
    return getName().contains ("Seaboard");
}

//==============================================================================
void MidiInputDevice::handleNoteOn (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    if (eventReceivedFromDevice)
        return;

    handleIncomingMidiMessage (juce::MidiMessage::noteOn (std::max (1, channelToUse.getChannelNumber()), midiNoteNumber, velocity));
}

void MidiInputDevice::handleNoteOff (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float)
{
    if (eventReceivedFromDevice)
        return;

    handleIncomingMidiMessage (juce::MidiMessage::noteOff (std::max (1, channelToUse.getChannelNumber()), midiNoteNumber));
}

void MidiInputDevice::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const juce::ScopedValueSetter<bool> svs (eventReceivedFromDevice, true, false);
    handleIncomingMidiMessage (m);
    keyboardState.processNextMidiEvent (m);
}

void MidiInputDevice::updateRetrospectiveBufferLength (double length)
{
    if (retrospectiveBuffer != nullptr)
        retrospectiveBuffer->lengthInSeconds = length;
}

//==============================================================================
void MidiInputDevice::addInstance (MidiInputDeviceInstanceBase* i)    { const juce::ScopedLock sl (instanceLock); instances.addIfNotAlreadyThere (i); }
void MidiInputDevice::removeInstance (MidiInputDeviceInstanceBase* i) { const juce::ScopedLock sl (instanceLock); instances.removeAllInstancesOf (i); }

void MidiInputDevice::connectionStateChanged()
{
    if (isTrackDevice() && (! enabled))
        return;

    if (programToUse > 0 && channelToUse.isValid())
    {
        int bankID;

        if (auto destTrack = getDestinationTracks().getFirst())
            bankID = destTrack->getIdForBank (bankToUse);
        else
            bankID = engine.getMidiProgramManager().getBankID (0, bankToUse);

        auto chan = channelToUse.getChannelNumber();
        handleIncomingMidiMessage (juce::MidiMessage::controllerEvent (chan, 0x00, MidiControllerEvent::bankIDToCoarse (bankID)));
        handleIncomingMidiMessage (juce::MidiMessage::controllerEvent (chan, 0x20, MidiControllerEvent::bankIDToFine (bankID)));
        handleIncomingMidiMessage (juce::MidiMessage::programChange (chan, programToUse - 1));
    }
}

//==============================================================================
void MidiInputDevice::sendNoteOnToMidiKeyListeners (juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        if (overrideNoteVels)
            message.setVelocity (1.0f);

        levelMeasurer.processMidiLevel (message.getFloatVelocity());

        const int noteNum = message.getNoteNumber();

        {
            juce::ScopedLock sl (noteLock);
            keysDown[noteNum] = true;
            keyDownVelocities[noteNum] = message.getVelocity();
        }

        startTimer (50);
    }
    else if (message.isNoteOff())
    {
        const int noteNum = message.getNoteNumber();

        {
            juce::ScopedLock sl (noteLock);
            keysUp[noteNum] = true;
        }

        startTimer (25);
    }
}

void MidiInputDevice::timerCallback()
{
    stopTimer();

    juce::Array<int> down, vels, up;

    bool keysDownCopy[128], keysUpCopy[128];
    uint8_t keyDownVelocitiesCopy[128];

    {
        juce::ScopedLock sl (noteLock);

        memcpy (keysUpCopy, keysUp, sizeof (keysUp));
        memcpy (keysDownCopy, keysDown, sizeof (keysDown));
        memcpy (keyDownVelocitiesCopy, keyDownVelocities, sizeof (keyDownVelocities));

        std::memset (keysDown, 0, sizeof (keysDown));
        std::memset (keysUp, 0, sizeof (keysUp));
        std::memset (keyDownVelocities, 0, sizeof (keyDownVelocities));
    }

    for (int i = 0; i < 128; ++i)
    {
        if (keysDownCopy[i])
        {
            down.add (i);
            vels.add (keyDownVelocitiesCopy[i]);
        }
        if (keysUpCopy[i])
        {
            up.add (i);
        }
    }

    if (down.size() > 0 || up.size() > 0)
        for (auto t : getDestinationTracks())
            midiKeyChangeDispatcher->listeners.call (&MidiKeyChangeDispatcher::Listener::midiKeyStateChanged, t, down, vels, up);
}

//==============================================================================
void MidiInputDevice::flipEndToEnd()
{
    endToEndEnabled = ! endToEndEnabled;
    TransportControl::restartAllTransports (engine, false);
    changed();
    saveProps();
}

static bool trackContainsClipWithName (const AudioTrack& track, const juce::String& name)
{
    for (auto& c : track.getClips())
        if (c->getName().equalsIgnoreCase (name))
            return true;

    return false;
}

static juce::String getNameForNewClip (AudioTrack& track)
{
    for (int index = 1; ; ++index)
    {
        auto clipName = track.getName() + " "  + TRANS("Recording") + " " + juce::String (index);

        if (! trackContainsClipWithName (track, clipName))
            return clipName;
    }
}


Clip* MidiInputDevice::addMidiToTrackAsTransaction (Clip* takeClip, AudioTrack& track, juce::MidiMessageSequence& ms,
                                                    TimeRange position, MergeMode merge, MidiChannel midiChannel,
                                                    SelectionManager* selectionManagerToUse)
{
    CRASH_TRACER
    Clip* createdClip = nullptr;
    auto& ed = track.edit;
    quantisation.applyQuantisationToSequence (ms, ed, position.getStart());

    bool needToAddClip = true;
    const auto automationType = recordToNoteAutomation ? MidiList::NoteAutomationType::expression
                                                       : MidiList::NoteAutomationType::none;

    if ((merge == MergeMode::optional && mergeRecordings) || merge == MergeMode::always)
        needToAddClip = ! track.mergeInMidiSequence (ms, position.getStart(), nullptr, automationType);

    if (takeClip != nullptr)
    {
        if (auto midiClip = dynamic_cast<MidiClip*> (takeClip))
        {
            ms.addTimeToMessages (midiClip->getPosition().getStart().inSeconds());
            ms.updateMatchedPairs();
            midiClip->addTake (ms, automationType);

            midiClip->setMidiChannel (midiChannel);

            if (programToUse > 0)
                midiClip->getSequence().addControllerEvent ({}, MidiControllerEvent::programChangeType,
                                                            (programToUse - 1) << 7, &ed.getUndoManager());
        }
    }
    else if (needToAddClip)
    {
        ms.updateMatchedPairs();

        if (replaceExistingClips && merge == MergeMode::optional)
            track.deleteRegion (position, selectionManagerToUse);

        if (auto mc = track.insertMIDIClip (getNameForNewClip (track), position, nullptr))
        {
            track.mergeInMidiSequence (ms, mc->getPosition().getStart(), mc.get(), automationType);

            if (recordToNoteAutomation)
                mc->setMPEMode (true);

            mc->setMidiChannel (midiChannel);

            if (programToUse > 0)
                mc->getSequence().addControllerEvent ({}, MidiControllerEvent::programChangeType,
                                                      (programToUse - 1) << 7, &ed.getUndoManager());

            mc->setLength (position.getLength(), true);

            if (selectionManagerToUse != nullptr)
            {
                selectionManagerToUse->selectOnly (*mc);
                selectionManagerToUse->keepSelectedObjectsOnScreen();
            }

            createdClip = mc.get();
        }
        else
        {
            jassertfalse;
        }
    }

    return createdClip;
}

//==============================================================================
class MidiInputDeviceInstanceBase  : public InputDeviceInstance
{
public:
    MidiInputDeviceInstanceBase (MidiInputDevice& d, EditPlaybackContext& c)
        : InputDeviceInstance (d, c)
    {
        getMidiInput().addInstance (this);
    }

    ~MidiInputDeviceInstanceBase() override
    {
        getMidiInput().removeInstance (this);
    }

    bool isRecordingActive() const override
    {
        return getMidiInput().recordingEnabled && InputDeviceInstance::isRecordingActive();
    }

    bool isRecordingActive (const Track& t) const override
    {
        return getMidiInput().recordingEnabled && InputDeviceInstance::isRecordingActive (t);
    }
    
    bool shouldTrackContentsBeMuted() override      { return recording && ! getMidiInput().mergeRecordings; }

    virtual void handleMMCMessage (const juce::MidiMessage&) {}
    virtual bool handleTimecodeMessage (const juce::MidiMessage&) { return false; }

    juce::String prepareToRecord (TimePosition, TimePosition punchIn, double, int, bool) override
    {
        startTime = punchIn;
        recorded.clear();
        livePlayOver = context.transport.looping;

        return {};
    }

    void stop() override
    {
        recording = false;
        livePlayOver = false;
    }

    void recordWasCancelled() override
    {
        recording = false;
    }

    TimePosition getPunchInTime() override
    {
        return startTime;
    }

    bool isRecording() override
    {
        return recording;
    }

    Clip::Array stopRecording() override
    {
        if (! recording)
            return {};

        return context.stopRecording (*this, { startTime, context.getUnloopedPosition() }, false);
    }

    bool handleIncomingMidiMessage (const juce::MidiMessage& message)
    {
        if (recording)
            recorded.addEvent (juce::MidiMessage (message, context.globalStreamTimeToEditTimeUnlooped (message.getTimeStamp()).inSeconds()));

        juce::ScopedLock sl (consumerLock);

        for (auto c : consumers)
            c->handleIncomingMidiMessage (message);

        return recording || consumers.size() > 0;
    }

    MidiChannel applyChannel (juce::MidiMessageSequence& sequence, MidiChannel channelToApply)
    {
        if (! channelToApply.isValid())
        {
            for (int i = 0; i < sequence.getNumEvents(); ++i)
            {
                auto chan = MidiChannel::fromChannelOrZero (sequence.getEventPointer (i)->message.getChannel());

                if (chan.isValid())
                {
                    channelToApply = chan;
                    break;
                }
            }
        }

        if (channelToApply.isValid())
            for (int i = sequence.getNumEvents(); --i >= 0;)
                sequence.getEventPointer(i)->message.setChannel (channelToApply.getChannelNumber());

        return channelToApply;
    }

    static void applyTimeAdjustment (juce::MidiMessageSequence& sequence, double adjustmentMs)
    {
        if (adjustmentMs != 0)
            sequence.addTimeToMessages (adjustmentMs * 0.001);
    }

    Clip::Array applyLastRecordingToEdit (TimeRange recordedRange,
                                          bool isLooping, TimeRange loopRange,
                                          bool discardRecordings,
                                          SelectionManager* selectionManager) override
    {
        CRASH_TRACER

        if (discardRecordings)
        { 
            for (auto c : consumers)
                c->discardRecordings();

            recorded.clear();
        }

        if (recorded.getNumEvents() == 0)
            return {};

        Clip::Array createdClips;
        auto& mi = getMidiInput();

        recorded.updateMatchedPairs();
        auto channelToApply = mi.recordToNoteAutomation ? mi.getChannelToUse()
                                                        : applyChannel (recorded, mi.getChannelToUse());
        auto timeAdjustMs = mi.getManualAdjustmentMs();
        
        if (context.getNodePlayHead() != nullptr)
            timeAdjustMs -= 1000.0 * tracktion::graph::sampleToTime (context.getLatencySamples(), edit.engine.getDeviceManager().getSampleRate());
        
        applyTimeAdjustment (recorded, timeAdjustMs);

        auto recordingStart = recordedRange.getStart();
        auto recordingEnd = recordedRange.getEnd();

        bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

        for (auto track : getTargetTracks())
        {
            if (! activeTracks.contains (track))
                continue;
                
            if (isLooping && recordingEnd > loopRange.getEnd())
            {
                juce::MidiMessageSequence replaceSequence;
                const auto loopLen = loopRange.getLength();
                const auto maxNumLoops = 2 + (int) ((recordingEnd - recordingStart) / loopLen);

                Clip* takeClip = nullptr;

                for (int loopNum = 0; loopNum < maxNumLoops; ++loopNum)
                {
                    juce::MidiMessageSequence loopSequence;
                    const auto thisLoopStart = loopRange.getStart() + loopLen * loopNum;
                    const auto thisLoopEnd = (thisLoopStart + loopLen).inSeconds();

                    for (int i = 0; i < recorded.getNumEvents(); ++i)
                    {
                        auto& m = recorded.getEventPointer (i)->message;

                        if (m.isNoteOn())
                        {
                            double s = m.getTimeStamp();
                            double e = s;

                            if (auto* noteOff = recorded.getEventPointer (i)->noteOffObject)
                                e = noteOff->message.getTimeStamp();

                            if (e > thisLoopStart.inSeconds() && s < thisLoopEnd)
                            {
                                if (s < thisLoopStart.inSeconds())
                                    s = 0.0;
                                else
                                    s = std::fmod (s - loopRange.getStart().inSeconds(), loopLen.inSeconds());

                                if (e > thisLoopEnd)
                                    e = loopLen.inSeconds();
                                else
                                    e = std::fmod (e - loopRange.getStart().inSeconds(), loopLen.inSeconds());

                                loopSequence.addEvent (juce::MidiMessage (m, s));
                                loopSequence.addEvent (juce::MidiMessage (juce::MidiMessage::noteOff (m.getChannel(),
                                                                                                      m.getNoteNumber()), e));
                            }
                        }
                        else if (! m.isNoteOff())
                        {
                            const double t = m.getTimeStamp();

                            if (t >= thisLoopStart.inSeconds() && t < thisLoopEnd)
                                loopSequence.addEvent (juce::MidiMessage (m, std::fmod (t - loopRange.getStart().inSeconds(), loopLen.inSeconds())));
                        }
                    }

                    if (loopSequence.getNumEvents() > 0)
                    {
                        loopSequence.updateMatchedPairs();

                        if (createTakes)
                        {
                            if (auto* clip = mi.addMidiToTrackAsTransaction (takeClip, *track,
                                                                             loopSequence, loopRange,
                                                                             MidiInputDevice::MergeMode::optional,
                                                                             channelToApply,
                                                                             selectionManager))
                            {
                                takeClip = clip;
                                createdClips.add (clip);
                            }
                        }
                        else if (mi.replaceExistingClips)
                        {
                            replaceSequence = loopSequence;
                        }
                        else
                        {
                            if (auto* clip = mi.addMidiToTrackAsTransaction (nullptr, *track,
                                                                             loopSequence, loopRange,
                                                                             MidiInputDevice::MergeMode::always,
                                                                             channelToApply,
                                                                             selectionManager))
                            {
                                createdClips.add (clip);
                            }
                        }
                    }
                }

                if (mi.replaceExistingClips && replaceSequence.getNumEvents() > 0)
                {
                    if (auto* clip = mi.addMidiToTrackAsTransaction (nullptr, *track,
                                                                     replaceSequence, loopRange,
                                                                     MidiInputDevice::MergeMode::optional,
                                                                     channelToApply, selectionManager))
                    {
                        createdClips.add (clip);
                    }
                }
            }
            else
            {
                auto startPos  = recordingStart;
                auto endPos    = recordingEnd;
                auto maxEndPos = endPos + 0.5s;

                if (edit.recordingPunchInOut)
                {
                    if (startPos < loopRange.getEnd())
                    {
                        // if we didn't get as far as the punch-in
                        if (endPos <= loopRange.getStart())
                            return createdClips;

                        startPos  = std::max (startPos, loopRange.getStart());
                        endPos    = juce::jlimit (startPos + 0.1s, loopRange.getEnd(), endPos);
                        maxEndPos = endPos;
                    }
                    else if (edit.getNumCountInBeats() > 0)
                    {
                        startPos = std::max (startPos, loopRange.getStart());
                    }
                }

                juce::Array<int> eventsToDelete;
                juce::Array<juce::MidiMessage> noteOffMessagesToAdd, mpeMessagesToAddAtStartPos;

                const auto ensureNoteOffIsInsideClip = [&noteOffMessagesToAdd, endPos] (juce::MidiMessageSequence::MidiEventHolder& m)
                {
                    jassert (m.message.isNoteOn());

                    if (m.noteOffObject == nullptr)
                        noteOffMessagesToAdd.add (juce::MidiMessage (juce::MidiMessage::noteOff (m.message.getChannel(),
                                                                                                 m.message.getNoteNumber()), endPos.inSeconds()));

                    else if (m.noteOffObject->message.getTimeStamp() > endPos.inSeconds())
                        m.noteOffObject->message.setTimeStamp (endPos.inSeconds());
                };

                const auto isNoteOnThatEndsAfterClipStart = [startPos] (juce::MidiMessageSequence::MidiEventHolder& m)
                {
                    return m.message.isNoteOn() && (m.noteOffObject == nullptr || m.noteOffObject->message.getTimeStamp() > startPos.inSeconds());
                };

                const auto isOutsideClipAndNotNoteOff = [startPos, maxEndPos] (const juce::MidiMessage& m)
                {
                    return (m.getTimeStamp() < startPos.inSeconds() || m.getTimeStamp() > maxEndPos.inSeconds()) && ! m.isNoteOff();
                };

                if (mi.recordToNoteAutomation)
                {
                    auto clipStartIndex = recorded.getNextIndexAtTime (startPos.inSeconds());

                    for (int i = recorded.getNumEvents(); --i >= 0;)
                    {
                        auto& m = *recorded.getEventPointer (i);

                        if (m.message.getTimeStamp() < startPos.inSeconds() && isNoteOnThatEndsAfterClipStart (m))
                        {
                            MPEStartTrimmer::reconstructExpression (mpeMessagesToAddAtStartPos, recorded, clipStartIndex, m.message.getChannel());

                            ensureNoteOffIsInsideClip (m);
                            eventsToDelete.add (i); // Remove original noteOn, findInitialNoteExpression makes new one in correct order
                            m.noteOffObject = nullptr; // Prevent deletion of original note off, it will be paird with the new note-on later
                        }
                        else if (isOutsideClipAndNotNoteOff (m.message))
                        {
                            eventsToDelete.add (i);
                        }
                        else if (m.message.getTimeStamp() < endPos.inSeconds() && m.message.isNoteOn())
                        {
                            ensureNoteOffIsInsideClip (m);
                        }
                    }
                }
                else
                {
                    for (int i = recorded.getNumEvents(); --i >= 0;)
                    {
                        auto& m = *recorded.getEventPointer (i);

                        if (m.message.getTimeStamp() < startPos.inSeconds() && isNoteOnThatEndsAfterClipStart (m))
                            m.message.setTimeStamp (startPos.inSeconds());

                        if (isOutsideClipAndNotNoteOff (m.message))
                            eventsToDelete.add (i);

                        else if (m.message.getTimeStamp() < endPos.inSeconds() && m.message.isNoteOn())
                            ensureNoteOffIsInsideClip (m);
                    }
                }

                if (! eventsToDelete.isEmpty())
                {
                    // N.B. eventsToDelete is in reverse order so iterate forwards when deleting
                    for (int index : eventsToDelete)
                        recorded.deleteEvent (index, true);
                }

                if (! noteOffMessagesToAdd.isEmpty())
                {
                    for (const auto& m : noteOffMessagesToAdd)
                        recorded.addEvent (m);
                }

                if (! mpeMessagesToAddAtStartPos.isEmpty())
                {
                    for (const auto& m : mpeMessagesToAddAtStartPos)
                        recorded.addEvent (m, startPos.inSeconds());
                }

                recorded.sort();
                recorded.updateMatchedPairs();
                recorded.addTimeToMessages (-startPos.inSeconds());

                if (recorded.getNumEvents() > 0)
                {
                    if (auto* clip = mi.addMidiToTrackAsTransaction (nullptr, *track, recorded,
                                                                     { startPos, endPos },
                                                                     MidiInputDevice::MergeMode::optional,
                                                                     channelToApply, selectionManager))
                    {
                        createdClips.add (clip);
                    }
                }
            }
        }

        recorded.clear();

        return createdClips;
    }

    juce::Array<Clip*> applyRetrospectiveRecord (SelectionManager* selectionManager) override
    {
        CRASH_TRACER
        
        juce::Array<Clip*> clips;

        auto& mi = getMidiInput();
        auto retrospective = mi.getRetrospectiveMidiBuffer();

        if (retrospective == nullptr)
            return {};

        for (auto track : getTargetTracks())
        {
            auto sequence = retrospective->getMidiMessages();

            if (sequence.getNumEvents() == 0)
                return {};

            sequence.updateMatchedPairs();
            auto channelToApply = mi.recordToNoteAutomation ? mi.getChannelToUse()
                                                            : applyChannel (sequence, mi.getChannelToUse());
            auto timeAdjustMs = mi.getManualAdjustmentMs();
            
            if (context.getNodePlayHead() != nullptr)
                timeAdjustMs -= 1000.0 * tracktion::graph::sampleToTime (context.getLatencySamples(), edit.engine.getDeviceManager().getSampleRate());
            
            applyTimeAdjustment (sequence, timeAdjustMs);

            auto clipStart = juce::Time::getMillisecondCounterHiRes() * 0.001
                                - retrospective->lengthInSeconds + mi.getAdjustSecs();
            sequence.addTimeToMessages (-clipStart);

            double start;
            double length = retrospective->lengthInSeconds;
            double offset = 0;

            if (context.isPlaying())
            {
                start = std::max (TimePosition(), context.getPosition()).inSeconds() - length;
            }
            else if (lastEditTime >= 0 && pausedTime < 20)
            {
                start = lastEditTime + pausedTime - length;
                lastEditTime = -1;
            }
            else
            {
                auto position = context.getPosition();

                if (position >= 5s)
                    start = position.inSeconds() - length;
                else
                    start = std::max (TimePosition(), context.getPosition()).inSeconds();
            }

            if (sequence.getNumEvents() > 0)
            {
                auto firstEventTime = std::floor (sequence.getStartTime());
                sequence.addTimeToMessages (-firstEventTime);
                start += firstEventTime;
                length -= firstEventTime;
            }

            if (start < 0)
            {
                offset = -start;
                start = 0;
                length -= offset;
            }

            retrospective->sequence.clear();

            if (sequence.getNumEvents() > 0)
            {
                auto clip = mi.addMidiToTrackAsTransaction (nullptr, *track, sequence,
                                                            { TimePosition::fromSeconds (start), TimePosition::fromSeconds (start + length) },
                                                            MidiInputDevice::MergeMode::never,
                                                            channelToApply, selectionManager);
                clip->setOffset (TimeDuration::fromSeconds (offset));
                clips.add (clip);
            }
        }

        return clips;
    }

    void masterTimeUpdate (double time)
    {
        if (context.isPlaying())
        {
            pausedTime = 0;
            lastEditTime = context.globalStreamTimeToEditTime (time).inSeconds();
        }
        else
        {
            pausedTime += edit.engine.getDeviceManager().getBlockSizeMs() / 1000.0;
        }
    }

    bool isLivePlayEnabled (const Track& t) const override
    {
        return owner.isEndToEndEnabled() && InputDeviceInstance::isLivePlayEnabled (t);
    }

    MidiInputDevice& getMidiInput() const   { return static_cast<MidiInputDevice&> (owner); }

    std::atomic<bool> recording { false }, livePlayOver { false };
    TimePosition startTime;
    juce::MidiMessageSequence recorded;

private:
    juce::CriticalSection consumerLock;
    juce::Array<Consumer*> consumers;
    double lastEditTime = -1.0;
    double pausedTime = 0;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    void addConsumer (Consumer* c) override      { juce::ScopedLock sl (consumerLock); consumers.addIfNotAlreadyThere (c); }
    void removeConsumer (Consumer* c) override   { juce::ScopedLock sl (consumerLock); consumers.removeAllInstancesOf (c); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputDeviceInstanceBase)
};


//==============================================================================
void MidiInputDevice::masterTimeUpdate (double time)
{
    adjustSecs = time - juce::Time::getMillisecondCounterHiRes() * 0.001;

    const juce::ScopedLock sl (instanceLock);

    for (auto instance : instances)
        instance->masterTimeUpdate (time);
}

void MidiInputDevice::sendMessageToInstances (const juce::MidiMessage& message)
{
    bool messageUnused = true;

    {
        const juce::ScopedLock sl (instanceLock);

        for (auto i : instances)
            if (i->handleIncomingMidiMessage (message))
                messageUnused = false;
    }

    if (messageUnused && message.isNoteOn())
        if (auto&& warnOfWasted = engine.getDeviceManager().warnOfWastedMidiMessagesFunction)
            warnOfWasted (this);
}

}} // namespace tracktion { inline namespace engine
