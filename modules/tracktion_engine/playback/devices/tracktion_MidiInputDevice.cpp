/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


class MidiControllerParser  : private AsyncUpdater
{
public:
    MidiControllerParser() {}

    void processMessage (const MidiMessage& m)
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
            const ScopedLock sl (pendingLock);
            pendingMessages.add ({ number, channel, value / 127.0f });
        }

        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        Array<Message> messages;

        {
            const ScopedLock sl (pendingLock);
            pendingMessages.swapWith (messages);
        }

        if (auto pcm = ParameterControlMappings::getCurrentlyFocusedMappings())
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

    Array<Message> pendingMessages;
    CriticalSection pendingLock;
};

//==============================================================================
struct RetrospectiveMidiBuffer
{
    RetrospectiveMidiBuffer (Engine& e)
    {
        lengthInSeconds = e.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30);
    }

    void addMessage (const MidiMessage& m, double adjust)
    {
        auto cutoffTime = Time::getMillisecondCounterHiRes() * 0.001 + adjust - lengthInSeconds;

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

        SortedSet<int> usedIndexes;

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

    Array<MidiMessage> sequence;
    double lengthInSeconds = 0;
};

//==============================================================================
MidiInputDevice::MidiInputDevice (Engine& e, const String& type, const String& name)
   : InputDevice (e, type, name)
{
    levelMeasurer.setShowMidi (true);

    endToEndEnabled = true;
    zeromem (keysDown, sizeof (keysDown));
    zeromem (keyDownVelocities, sizeof (keyDownVelocities));

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
        MouseCursor::showWaitCursor();

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

        MouseCursor::hideWaitCursor();
    }
}

void MidiInputDevice::loadProps (const XmlElement* n)
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

void MidiInputDevice::saveProps (XmlElement& n)
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

AudioTrack* MidiInputDevice::getDestinationTrack()
{
    if (auto edit = engine.getUIBehaviour().getLastFocusedEdit())
        if (auto in = edit->getCurrentInstanceForInputDevice (this))
            return in->getTargetTrack();

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
    programToUse = jlimit (0, 128, prog);
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
void MidiInputDevice::handleNoteOn (MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    if (eventReceivedFromDevice)
        return;

    handleIncomingMidiMessage (MidiMessage::noteOn (jmax (1, channelToUse.getChannelNumber()), midiNoteNumber, velocity));
}

void MidiInputDevice::handleNoteOff (MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float)
{
    if (eventReceivedFromDevice)
        return;

    handleIncomingMidiMessage (MidiMessage::noteOff (jmax (1, channelToUse.getChannelNumber()), midiNoteNumber));
}

void MidiInputDevice::handleIncomingMidiMessage (MidiInput*, const MidiMessage& m)
{
    const ScopedValueSetter<bool> svs (eventReceivedFromDevice, true, false);
    handleIncomingMidiMessage (m);
    keyboardState.processNextMidiEvent (m);
}

void MidiInputDevice::updateRetrospectiveBufferLength (double length)
{
    if (retrospectiveBuffer != nullptr)
        retrospectiveBuffer->lengthInSeconds = length;
}

//==============================================================================
void MidiInputDevice::addInstance (MidiInputDeviceInstanceBase* i)    { const ScopedLock sl (instanceLock); instances.addIfNotAlreadyThere (i); }
void MidiInputDevice::removeInstance (MidiInputDeviceInstanceBase* i) { const ScopedLock sl (instanceLock); instances.removeAllInstancesOf (i); }

void MidiInputDevice::connectionStateChanged()
{
    if (isTrackDevice() && (! enabled))
        return;

    if (programToUse > 0 && channelToUse.isValid())
    {
        int bankID;

        if (auto destTrack = getDestinationTrack())
            bankID = destTrack->getIdForBank (bankToUse);
        else
            bankID = engine.getMidiProgramManager().getBankID (0, bankToUse);

        auto chan = channelToUse.getChannelNumber();
        handleIncomingMidiMessage (MidiMessage::controllerEvent (chan, 0x00, MidiControllerEvent::bankIDToCoarse (bankID)));
        handleIncomingMidiMessage (MidiMessage::controllerEvent (chan, 0x20, MidiControllerEvent::bankIDToFine (bankID)));
        handleIncomingMidiMessage (MidiMessage::programChange (chan, programToUse - 1));
    }
}

//==============================================================================
void MidiInputDevice::sendNoteOnToMidiKeyListeners (MidiMessage& message)
{
    if (message.isNoteOn())
    {
        if (overrideNoteVels)
            message.setVelocity (1.0f);

        levelMeasurer.processMidiLevel (message.getFloatVelocity());

        const int noteNum = message.getNoteNumber();

        keysDown[noteNum] = true;
        keyDownVelocities[noteNum] = message.getVelocity();

        startTimer (50);
    }
}

void MidiInputDevice::timerCallback()
{
    stopTimer();

    if (auto t = getDestinationTrack())
    {
        Array<int> keys, vels;

        for (int i = 0; i < 128; ++i)
        {
            if (keysDown[i])
            {
                keys.add (i);
                vels.add (keyDownVelocities[i]);
            }
        }

        zeromem (keysDown, sizeof (keysDown));
        zeromem (keyDownVelocities, sizeof (keyDownVelocities));

        midiKeyChangeDispatcher->listeners.call (&MidiKeyChangeDispatcher::Listener::midiKeyStateChanged, t, keys, vels);
    }
}

//==============================================================================
void MidiInputDevice::flipEndToEnd()
{
    endToEndEnabled = ! endToEndEnabled;
    TransportControl::restartAllTransports (engine, false);
    changed();
    saveProps();
}

static bool trackContainsClipWithName (const AudioTrack& track, const String& name)
{
    for (auto& c : track.getClips())
        if (c->getName().equalsIgnoreCase (name))
            return true;

    return false;
}

static String getNameForNewClip (AudioTrack& track)
{
    for (int index = 1; ; ++index)
    {
        auto clipName = track.getName() + " "  + TRANS("Recording") + " " + String (index);

        if (! trackContainsClipWithName (track, clipName))
            return clipName;
    }
}


Clip* MidiInputDevice::addMidiToTrackAsTransaction (Clip* takeClip, AudioTrack& track, juce::MidiMessageSequence& ms,
                                                    EditTimeRange position, MergeMode merge, MidiChannel midiChannel,
                                                    SelectionManager* selectionManagerToUse)
{
    CRASH_TRACER
    Clip* createdClip = nullptr;
    auto& ed = track.edit;
    quantisation.applyQuantisationToSequence (ms, ed, position.start);

    bool needToAddClip = true;
    const auto automationType = recordToNoteAutomation ? MidiList::NoteAutomationType::expression
                                                       : MidiList::NoteAutomationType::none;

    if ((merge == MergeMode::optional && mergeRecordings) || merge == MergeMode::always)
        needToAddClip = ! track.mergeInMidiSequence (ms, position.start, nullptr, automationType);

    if (takeClip != nullptr)
    {
        if (auto midiClip = dynamic_cast<MidiClip*> (takeClip))
        {
            ms.addTimeToMessages (midiClip->getPosition().getStart());
            ms.updateMatchedPairs();
            midiClip->addTake (ms, automationType);

            midiClip->setMidiChannel (midiChannel);

            if (programToUse > 0)
                midiClip->getSequence().addControllerEvent (0.0, MidiControllerEvent::programChangeType,
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
                mc->getSequence().addControllerEvent (0.0, MidiControllerEvent::programChangeType,
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

    ~MidiInputDeviceInstanceBase()
    {
        getMidiInput().removeInstance (this);
    }

    bool isRecordingActive() const override
    {
        return getMidiInput().recordingEnabled
                && InputDeviceInstance::isRecordingActive();
    }

    bool shouldTrackContentsBeMuted() override      { return recording && ! getMidiInput().mergeRecordings; }

    virtual void handleMMCMessage (const MidiMessage&) {}
    virtual bool handleTimecodeMessage (const MidiMessage&) { return false; }

    String prepareToRecord (double start, double, double, int, bool) override
    {
        startTime = start;
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

    double getPunchInTime() override
    {
        return startTime;
    }

    bool isRecording() override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        return recording;
    }

    Clip::Array stopRecording() override
    {
        if (! recording)
            return {};

        return context.stopRecording (*this, { startTime, context.playhead.getUnloopedPosition() }, false);
    }

    bool handleIncomingMidiMessage (const MidiMessage& message)
    {
        if (recording)
            recorded.addEvent (MidiMessage (message, context.playhead.streamTimeToSourceTimeUnlooped (message.getTimeStamp())));

        ScopedLock sl (nodeLock);

        for (auto n : nodes)
            n->handleIncomingMidiMessage (message);

        return recording || nodes.size() > 0;
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

    Clip::Array applyLastRecordingToEdit (EditTimeRange recordedRange,
                                          bool isLooping, EditTimeRange loopRange,
                                          bool discardRecordings,
                                          SelectionManager* selectionManager) override
    {
        CRASH_TRACER

        if (discardRecordings)
            recorded.clear();

        auto track = getTargetTrack();

        if (recorded.getNumEvents() == 0 || track == nullptr)
            return {};

        Clip::Array createdClips;
        auto& mi = getMidiInput();

        recorded.updateMatchedPairs();
        auto channelToApply = mi.recordToNoteAutomation ? mi.getChannelToUse()
                                                        : applyChannel (recorded, mi.getChannelToUse());
        applyTimeAdjustment (recorded, mi.getManualAdjustmentMs());

        auto recordingStart = recordedRange.start;
        auto recordingEnd = recordedRange.end;

        bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

        if (isLooping && recordingEnd > loopRange.end)
        {
            juce::MidiMessageSequence replaceSequence;
            const auto loopLen = loopRange.getLength();
            const auto maxNumLoops = 2 + (int) ((recordingEnd - recordingStart) / loopLen);

            Clip* takeClip = nullptr;

            for (int loopNum = 0; loopNum < maxNumLoops; ++loopNum)
            {
                juce::MidiMessageSequence loopSequence;
                const double thisLoopStart = loopRange.start + loopNum * loopLen;
                const double thisLoopEnd = thisLoopStart + loopLen;

                for (int i = 0; i < recorded.getNumEvents(); ++i)
                {
                    auto& m = recorded.getEventPointer (i)->message;

                    if (m.isNoteOn())
                    {
                        double s = m.getTimeStamp();
                        double e = s;

                        if (auto* noteOff = recorded.getEventPointer (i)->noteOffObject)
                            e = noteOff->message.getTimeStamp();

                        if (e > thisLoopStart && s < thisLoopEnd)
                        {
                            if (s < thisLoopStart)
                                s = 0.0;
                            else
                                s = std::fmod (s - loopRange.start, loopLen);

                            if (e > thisLoopEnd)
                                e = loopLen;
                            else
                                e = std::fmod (e - loopRange.start, loopLen);

                            loopSequence.addEvent (MidiMessage (m, s));
                            loopSequence.addEvent (MidiMessage (MidiMessage::noteOff (m.getChannel(), m.getNoteNumber()), e));
                        }
                    }
                    else if (! m.isNoteOff())
                    {
                        const double t = m.getTimeStamp();

                        if (t >= thisLoopStart && t < thisLoopEnd)
                            loopSequence.addEvent (MidiMessage (m, std::fmod (t - loopRange.start, loopLen)));
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
            double startPos  = recordingStart;
            double endPos    = recordingEnd;
            double maxEndPos = endPos + 0.5;

            if (edit.recordingPunchInOut)
            {
                if (startPos < loopRange.end)
                {
                    // if we didn't get as far as the punch-in
                    if (endPos <= loopRange.start)
                        return createdClips;

                    startPos  = jmax (startPos, loopRange.start);
                    endPos    = jlimit (startPos + 0.1, loopRange.end, endPos);
                    maxEndPos = endPos;
                }
                else if (edit.getNumCountInBeats() > 0)
                {
                    startPos = jmax (startPos, loopRange.start);
                }
            }

            Array<int> eventsToDelete;
            Array<MidiMessage> noteOffMessagesToAdd;
            Array<MidiMessage> mpeMessagesToAddAtStartPos;

            const auto ensureNoteOffIsInsideClip = [&noteOffMessagesToAdd, endPos] (juce::MidiMessageSequence::MidiEventHolder& m)
            {
                jassert (m.message.isNoteOn());

                if (m.noteOffObject == nullptr)
                    noteOffMessagesToAdd.add (MidiMessage (MidiMessage::noteOff (m.message.getChannel(), m.message.getNoteNumber()), endPos));

                else if (m.noteOffObject->message.getTimeStamp() > endPos)
                    m.noteOffObject->message.setTimeStamp (endPos);
            };

            const auto isNoteOnThatEndsAfterClipStart = [startPos] (juce::MidiMessageSequence::MidiEventHolder& m)
            {
                return m.message.isNoteOn() && (m.noteOffObject == nullptr || m.noteOffObject->message.getTimeStamp() > startPos);
            };

            const auto isOutsideClipAndNotNoteOff = [startPos, maxEndPos] (const juce::MidiMessage& m)
            {
                return m.getTimeStamp() < startPos || (m.getTimeStamp() > maxEndPos && ! m.isNoteOff());
            };

            if (mi.recordToNoteAutomation)
            {
                auto clipStartIndex = recorded.getNextIndexAtTime (startPos);

                for (int i = recorded.getNumEvents(); --i >= 0;)
                {
                    auto& m = *recorded.getEventPointer (i);

                    if (m.message.getTimeStamp() < startPos && isNoteOnThatEndsAfterClipStart (m))
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
                    else if (m.message.getTimeStamp() < endPos && m.message.isNoteOn())
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

                    if (m.message.getTimeStamp() < startPos && isNoteOnThatEndsAfterClipStart (m))
                        m.message.setTimeStamp (startPos);

                    if (isOutsideClipAndNotNoteOff (m.message))
                        eventsToDelete.add (i);

                    else if (m.message.getTimeStamp() < endPos && m.message.isNoteOn())
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
                    recorded.addEvent (m, startPos);
            }

            recorded.sort();
            recorded.updateMatchedPairs();
            recorded.addTimeToMessages (-startPos);

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

        recorded.clear();

        return createdClips;
    }

    Clip* applyRetrospectiveRecord (SelectionManager* selectionManager) override
    {
        CRASH_TRACER

        auto& mi = getMidiInput();
        auto retrospective = mi.getRetrospectiveMidiBuffer();

        if (retrospective == nullptr)
            return nullptr;

        auto sequence = retrospective->getMidiMessages();
        auto track = getTargetTrack();

        if (sequence.getNumEvents() == 0 || track == nullptr)
            return nullptr;

        sequence.updateMatchedPairs();
        auto channelToApply = mi.recordToNoteAutomation ? mi.getChannelToUse()
                                                        : applyChannel (sequence, mi.getChannelToUse());
        applyTimeAdjustment (sequence, mi.getManualAdjustmentMs());

        auto clipStart = Time::getMillisecondCounterHiRes() * 0.001 - retrospective->lengthInSeconds + mi.getAdjustSecs();
        sequence.addTimeToMessages (-clipStart);

        double start;
        double length = retrospective->lengthInSeconds;
        double offset = 0;

        if (context.playhead.isPlaying())
        {
            start = jmax (0.0, context.playhead.getPosition()) - length;
        }
        else if (lastEditTime >= 0 && pausedTime < 20)
        {
            start = lastEditTime + pausedTime - length;
            lastEditTime = -1;
        }
        else
        {
            auto position = context.playhead.getPosition();

            if (position >= 5)
                start = position - length;
            else
                start = jmax (0.0, context.playhead.getPosition());
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
                                                        { start, start + length },
                                                        MidiInputDevice::MergeMode::never,
                                                        channelToApply, selectionManager);
            clip->setOffset (offset);
            return clip;
        }

        return nullptr;
    }

    void masterTimeUpdate (double time)
    {
        if (context.playhead.isPlaying())
        {
            pausedTime = 0;
            lastEditTime = context.playhead.streamTimeToSourceTime (time);
        }
        else
        {
            pausedTime += edit.engine.getDeviceManager().getBlockSizeMs() / 1000.0;
        }
    }

    bool isLivePlayEnabled() const override
    {
        return owner.isEndToEndEnabled() && InputDeviceInstance::isLivePlayEnabled();
    }

    AudioNode* createLiveInputNode() override
    {
        return new InputAudioNode (*this, midiSourceID);
    }

    MidiInputDevice& getMidiInput() const   { return static_cast<MidiInputDevice&> (owner); }

    bool volatile recording = false, livePlayOver = false;
    double startTime = 0;
    juce::MidiMessageSequence recorded;

private:
    struct InputAudioNode  : public AudioNode
    {
        InputAudioNode (MidiInputDeviceInstanceBase& m, MidiMessageArray::MPESourceID msi)
            : owner (m), midiSourceID (msi)
        {
            for (int i = 256; --i >= 0;)
                incomingMessages.add (new MidiMessage (0x80, 0, 0));
        }

        ~InputAudioNode()
        {
            releaseAudioNodeResources();
        }

        void getAudioNodeProperties (AudioNodeProperties& info) override
        {
            info.hasAudio = false;
            info.hasMidi = true;
            info.numberOfChannels = 0;
        }

        void visitNodes (const VisitorFn& v) override
        {
            v (*this);
        }

        bool purgeSubNodes (bool, bool keepMidi) override
        {
            return keepMidi;
        }

        void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
        {
            lastPlayheadTime = 0.0;
            numMessages = 0;
            maxExpectedMsPerBuffer = (unsigned int) (((info.blockSizeSamples * 1000) / info.sampleRate) * 2 + 100);
            auto& mi = getMidiInput();

            {
                const ScopedLock sl (bufferLock);

                auto channelToUse = mi.getChannelToUse();
                auto programToUse = mi.getProgramToUse();

                if (channelToUse.isValid() && programToUse > 0)
                    handleIncomingMidiMessage (MidiMessage::programChange (channelToUse.getChannelNumber(), programToUse - 1));
            }

            {
                const ScopedLock sl (bufferLock);
                liveRecordedMessages.clear();
                numLiveMessagesToPlay = 0;
            }

            owner.add (this);
        }

        bool isReadyToRender() override
        {
            return true;
        }

        void releaseAudioNodeResources() override
        {
            owner.remove (this);

            const ScopedLock sl (bufferLock);
            numMessages = 0;
            numLiveMessagesToPlay = 0;
        }

        void createProgramChanges (MidiMessageArray& bufferForMidiMessages)
        {
            auto& mi = getMidiInput();
            auto channelToUse = mi.getChannelToUse();
            auto programToUse = mi.getProgramToUse();

            if (programToUse > 0 && channelToUse.isValid())
                bufferForMidiMessages.addMidiMessage (MidiMessage::programChange (channelToUse.getChannelNumber(), programToUse - 1),
                                                      0, midiSourceID);
        }

        void renderOver (const AudioRenderContext& rc) override
        {
            callRenderAdding (rc);
        }

        void renderAdding (const AudioRenderContext& rc) override
        {
            invokeSplitRender (rc, *this);
        }

        void renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
        {
            if (rc.bufferForMidiMessages != nullptr)
            {
                const auto timeNow = Time::getApproximateMillisecondCounter();

                const ScopedLock sl (bufferLock);

                if (! rc.isContiguousWithPreviousBlock())
                    createProgramChanges (*rc.bufferForMidiMessages);

                // if it's been a long time since the last block, clear the buffer because
                // it means we were muted or glitching
                if (timeNow > lastReadTime + maxExpectedMsPerBuffer)
                {
                    //jassertfalse
                    numMessages = 0;
                }

                lastReadTime = timeNow;

                jassert (numMessages <= incomingMessages.size());

                if (int num = jmin (numMessages, incomingMessages.size()))
                {
                    // not quite right as the first event won't be at the start of the buffer, but near enough for live stuff
                    auto timeAdjust = incomingMessages.getUnchecked (0)->getTimeStamp();

                    for (int i = 0; i < num; ++i)
                    {
                        auto* m = incomingMessages.getUnchecked (i);
                        rc.bufferForMidiMessages->addMidiMessage (*m,
                                                                  jlimit (0.0, jmax (0.0, editTime.getLength()), m->getTimeStamp() - timeAdjust),
                                                                  midiSourceID);
                    }
                }

                numMessages = 0;

                if (lastPlayheadTime > editTime.getStart())
                    // when we loop, we can assume all the messages in here are now from the previous time round, so are playable
                    numLiveMessagesToPlay = liveRecordedMessages.size();

                lastPlayheadTime = editTime.getStart();

                auto& mi = getMidiInput();

                bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

                if ((! createTakes && ! mi.replaceExistingClips)
                     && numLiveMessagesToPlay > 0
                     && rc.playhead.isPlaying())
                {
                    const ScopedLock sl2 (liveInputLock);

                    for (int i = 0; i < numLiveMessagesToPlay; ++i)
                    {
                        auto& m = liveRecordedMessages[i];
                        auto t = m.getTimeStamp();

                        if (editTime.contains (t))
                            rc.bufferForMidiMessages->add (m, t - editTime.getStart());
                    }
                }
            }
        }

        void handleIncomingMidiMessage (const MidiMessage& message)
        {
            auto& mi = getMidiInput();
            auto channelToUse = mi.getChannelToUse().getChannelNumber();

            {
                const ScopedLock sl (bufferLock);

                if (numMessages < incomingMessages.size())
                {
                    auto& m = *incomingMessages.getUnchecked (numMessages);
                    m = message;

                    if (channelToUse > 0)
                        m.setChannel (channelToUse);

                    ++numMessages;
                }
            }

            if (owner.livePlayOver)
            {
                auto& playhead = owner.context.playhead;

                if (playhead.isPlaying())
                {
                    auto sourceTime = playhead.streamTimeToSourceTime (message.getTimeStamp());

                    if (message.isNoteOff())
                        sourceTime = mi.quantisation.roundUp (sourceTime, owner.edit);
                    else if (message.isNoteOn())
                        sourceTime = mi.quantisation.roundToNearest (sourceTime, owner.edit);

                    if (playhead.isLooping())
                        if (sourceTime >= playhead.getLoopTimes().end)
                            sourceTime = playhead.getLoopTimes().start;

                    MidiMessage newMess (message, sourceTime);

                    if (channelToUse > 0)
                        newMess.setChannel (channelToUse);

                    const ScopedLock sl (liveInputLock);

                    liveRecordedMessages.addMidiMessage (newMess, sourceTime, midiSourceID);
                }
            }
        }

        MidiInputDevice& getMidiInput() const noexcept   { return owner.getMidiInput(); }

    private:
        MidiInputDeviceInstanceBase& owner;

        CriticalSection bufferLock;
        OwnedArray<MidiMessage> incomingMessages;
        int numMessages = 0;
        MidiMessageArray liveRecordedMessages;
        int numLiveMessagesToPlay = 0; // the index of the first message that's been recorded in the current loop
        MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::notMPE;
        CriticalSection liveInputLock;
        unsigned int lastReadTime = 0, maxExpectedMsPerBuffer = 0;
        double lastPlayheadTime = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputAudioNode)
    };

    CriticalSection nodeLock;
    Array<InputAudioNode*> nodes;
    double lastEditTime = -1.0;
    double pausedTime = 0;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    void add (InputAudioNode* node)     { ScopedLock sl (nodeLock); nodes.addIfNotAlreadyThere (node); }
    void remove (InputAudioNode* node)  { ScopedLock sl (nodeLock); nodes.removeAllInstancesOf (node); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputDeviceInstanceBase)
};


//==============================================================================
void MidiInputDevice::masterTimeUpdate (double time)
{
    adjustSecs = time - Time::getMillisecondCounterHiRes() * 0.001;

    const ScopedLock sl (instanceLock);

    for (auto instance : instances)
        instance->masterTimeUpdate (time);
}

void MidiInputDevice::sendMessageToInstances (const MidiMessage& message)
{
    bool messageUnused = true;

    {
        const ScopedLock sl (instanceLock);

        for (auto i : instances)
            if (i->handleIncomingMidiMessage (message))
                messageUnused = false;
    }

    if (messageUnused && message.isNoteOn())
        if (auto&& warnOfWasted = engine.getDeviceManager().warnOfWastedMidiMessagesFunction)
            warnOfWasted (this);
}
