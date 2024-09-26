/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
   Polls a set of targets to see if they should be stopped.
   Used by MidiInputDeviceInstance and WaveInputDeviceInstance.
*/
class MidiInputDevice::NoteDispatcher : public juce::HighResolutionTimer
{
public:
    NoteDispatcher (MidiInputDevice& o) : owner (o)
    {
        startTimer (1);
        items.reserve (20);
    }

    ~NoteDispatcher() override
    {
        stopTimer();
    }

    void hiResTimerCallback() override
    {
        juce::ScopedLock sl (lock);

        if (items.size() == 0)
            return;

        auto now = juce::Time::getMillisecondCounterHiRes();

        int sent = 0;
        for (size_t i = 0; i < items.size(); i++)
        {
            if (items[i].when > now)
                break;

            auto m = items[i].m;
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);

            owner.handleIncomingMidiMessage (m, owner.getMPESourceID());

            sent++;
        }

        if (sent > 0)
            items.erase (items.begin(), items.begin() + sent);
    }

    void enqueue (double tm, const juce::MidiMessage& m)
    {
        juce::ScopedLock sl (lock);

        items.push_back ({tm, m});
    }

    void clear (int channel, int note)
    {
        juce::ScopedLock sl (lock);

        items.erase (std::remove_if (items.begin(), items.end(),
                                     [&](const Item& i) -> bool { return i.m.getChannel() == channel && i.m.getNoteNumber() == note; }),
                     items.end());
    }

private:
    struct Item
    {
        double when;
        juce::MidiMessage m;
    };

    juce::CriticalSection lock;
    std::vector<Item> items;
    MidiInputDevice& owner;
};

//==============================================================================
//==============================================================================
/**
   Polls a set of targets to see if they should be stopped.
   Used by MidiInputDeviceInstance and WaveInputDeviceInstance.
*/
class RecordStopper
{
public:
    enum class HasFinished { no, yes };

    RecordStopper (std::function<HasFinished (EditItemID)> checkTargetFinishedCallback)
        : callback (std::move (checkTargetFinishedCallback))
    {
        assert (callback);
    }

    void addTargetToStop (EditItemID targetID)
    {
        if (contains_v (targetIDs, targetID))
            return;

        targetIDs.push_back (targetID);
        timer.startTimerHz (10);
    }

    bool isQueued (EditItemID targetID) const
    {
        return contains_v (targetIDs, targetID);
    }

private:
    LambdaTimer timer { [this] { checkForTargetsThatHaveFinished(); } };
    std::vector<EditItemID> targetIDs;
    std::function<HasFinished (EditItemID)> callback;

    void checkForTargetsThatHaveFinished()
    {
        std::vector<EditItemID> targetsToErase;

        for (auto targetID : targetIDs)
            if (callback (targetID) == HasFinished::yes)
                targetsToErase.push_back (targetID);

        targetIDs.erase (std::remove_if (targetIDs.begin(), targetIDs.end(),
                                         [&] (auto tID) { return contains_v (targetsToErase, tID); }),
                         targetIDs.end());

        if (targetIDs.empty())
            timer.stopTimer();
    }
};


//==============================================================================
//==============================================================================
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
        if (lock.try_lock())
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

            lock.unlock();
        }
    }

    juce::MidiMessageSequence takeMidiMessages()
    {
        juce::MidiMessageSequence result;

        {
            lock.lock();

            for (auto m : sequence)
                result.addEvent (m);

            sequence.clear();

            lock.unlock();
        }

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
    RealTimeSpinLock lock;
};

//==============================================================================
MidiInputDevice::MidiInputDevice (Engine& e, juce::String deviceType, juce::String deviceName, juce::String deviceIDToUse)
   : InputDevice (e, std::move (deviceType), std::move (deviceName), std::move (deviceIDToUse))
{
    enabled = true;
    levelMeasurer.setShowMidi (true);

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
    if (b != enabled)
    {
        enabled = b;
        saveProps();
        engine.getDeviceManager().rescanMidiDeviceList();
    }
}

void MidiInputDevice::loadMidiProps (const juce::XmlElement* n)
{
    monitorMode = defaultMonitorMode;
    recordingEnabled = true;
    mergeRecordings = true;
    replaceExistingClips = false;
    recordToNoteAutomation = isMPEDevice();
    adjustSecs = 0;
    manualAdjustMs = 0;
    minimumLengthMs = 0;
    channelToUse = {};
    programToUse = 0;
    bankToUse = 0;
    overrideNoteVels = false;
    disallowedChannels.clear();

    if (n != nullptr)
    {
        if (! isTrackDevice())
            enabled = n->getBoolAttribute ("enabled", enabled);

        monitorMode = magic_enum::enum_cast<MonitorMode> (n->getStringAttribute ("monitorMode").toStdString()).value_or (monitorMode);
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
        minimumLengthMs = n->getDoubleAttribute ("minimumLengthMs", minimumLengthMs);
        disallowedChannels.parseString (n->getStringAttribute ("disallowedChannels", disallowedChannels.toString (2)), 2);
        noteFilterRange.set (n->getIntAttribute ("noteStart", 0),
                             n->getIntAttribute ("noteEnd", 128));

        connectionStateChanged();
    }

    if (minimumLengthMs > 0 && noteDispatcher == nullptr)
        noteDispatcher = std::make_unique<NoteDispatcher> (*this);
    else if (minimumLengthMs <= 0)
        noteDispatcher = nullptr;

    lastNoteOns.resize (minimumLengthMs > 0 ? 128 * 16 : 0);
}

void MidiInputDevice::saveMidiProps (juce::XmlElement& n)
{
    n.setAttribute ("enabled", enabled);
    n.setAttribute ("monitorMode", std::string (magic_enum::enum_name (monitorMode)));
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
    n.setAttribute ("minimumLengthMs", minimumLengthMs);

    if (! disallowedChannels.isZero())
        n.setAttribute ("disallowedChannels", disallowedChannels.toString (2));

    if (! noteFilterRange.isAllNotes())
    {
        n.setAttribute ("noteStart", noteFilterRange.startNote);
        n.setAttribute ("noteEnd", noteFilterRange.endNote);
    }
}

juce::Array<AudioTrack*> MidiInputDevice::getDestinationTracks()
{
    if (auto edit = engine.getUIBehaviour().getLastFocusedEdit())
        if (auto in = edit->getCurrentInstanceForInputDevice (this))
            return getTargetTracks (*in);

    return {};
}

void MidiInputDevice::setChannelAllowed (int midiChannel, bool allowed)
{
    if (allowed != isChannelAllowed (midiChannel))
    {
        disallowedChannels.setBit (midiChannel - 1, ! allowed);
        changed();
        saveProps();
    }
}

void MidiInputDevice::setNoteFilterRange (NoteFilterRange newRange)
{
    if (noteFilterRange.startNote != newRange.startNote
         || noteFilterRange.endNote != newRange.endNote)
    {
        noteFilterRange = newRange;
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

void MidiInputDevice::setMinimumLengthMs (double ms)
{
    if (minimumLengthMs != ms)
    {
        minimumLengthMs = ms;
        changed();
        saveProps();

        if (minimumLengthMs > 0 && noteDispatcher == nullptr)
            noteDispatcher = std::make_unique<NoteDispatcher> (*this);
        else if (minimumLengthMs <= 0)
            noteDispatcher = nullptr;

        lastNoteOns.resize (minimumLengthMs > 0 ? 128 * 16 : 0);
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

    handleIncomingMidiMessage (juce::MidiMessage::noteOn (std::max (1, channelToUse.getChannelNumber()), midiNoteNumber, velocity), midiSourceID);
}

void MidiInputDevice::handleNoteOff (juce::MidiKeyboardState*, int /*midiChannel*/, int midiNoteNumber, float)
{
    if (eventReceivedFromDevice)
        return;

    handleIncomingMidiMessage (juce::MidiMessage::noteOff (std::max (1, channelToUse.getChannelNumber()), midiNoteNumber), midiSourceID);
}

void MidiInputDevice::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const juce::ScopedValueSetter<bool> svs (eventReceivedFromDevice, true, false);
    handleIncomingMidiMessage (m, midiSourceID);
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
        handleIncomingMidiMessage (juce::MidiMessage::controllerEvent (chan, 0x00, MidiControllerEvent::bankIDToCoarse (bankID)), midiSourceID);
        handleIncomingMidiMessage (juce::MidiMessage::controllerEvent (chan, 0x20, MidiControllerEvent::bankIDToFine (bankID)), midiSourceID);
        handleIncomingMidiMessage (juce::MidiMessage::programChange (chan, programToUse - 1), midiSourceID);
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

static juce::String getNameForNewClip (ClipOwner& owner)
{
    if (auto slot = dynamic_cast<ClipSlot*> (&owner))
        if (auto at = dynamic_cast<AudioTrack*> (&slot->track))
            return getNameForNewClip (*at);

    if (auto at = dynamic_cast<AudioTrack*> (&owner))
        return getNameForNewClip (*at);

    return {};
}


Clip* MidiInputDevice::addMidiAsTransaction (Edit& ed, EditItemID targetID,
                                             Clip* takeClip, juce::MidiMessageSequence ms,
                                             TimeRange position, MergeMode merge, MidiChannel midiChannel)
{
    CRASH_TRACER
    Clip* createdClip = nullptr;
    auto track = findAudioTrackForID (ed, targetID);
    auto clipSlot = track != nullptr ? nullptr : findClipSlotForID (ed, targetID);
    auto clipOwner = track != nullptr ? static_cast<ClipOwner*> (track)
                                      : static_cast<ClipOwner*> (clipSlot);

    if (! clipOwner)
    {
        jassertfalse;
        return nullptr;
    }

    quantisation.applyQuantisationToSequence (ms, ed, position.getStart());

    bool needToAddClip = true;
    const auto automationType = recordToNoteAutomation ? MidiList::NoteAutomationType::expression
                                                       : MidiList::NoteAutomationType::none;

    if (track)
        if ((merge == MergeMode::optional && mergeRecordings) || merge == MergeMode::always)
            needToAddClip = ! track->mergeInMidiSequence (ms, position.getStart(), nullptr, automationType);

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

        if ((replaceExistingClips && merge == MergeMode::optional)
            || clipSlot)
        {
            if (track)
                track->deleteRegion (position, nullptr);
            else
                clipSlot->setClip (nullptr);
        }

        if (auto mc = insertMIDIClip (*clipOwner, getNameForNewClip (*clipOwner), position))
        {
            if (track)
            {
                track->mergeInMidiSequence (std::move (ms), mc->getPosition().getStart(), mc.get(), automationType);
                mc->setLength (position.getLength(), true); // Reset the length here as it migh thave been changed by mergeInMidiSequence above
            }
            else if (clipSlot)
            {
                mergeInMidiSequence (*mc, std::move (ms), toDuration (mc->getPosition().getStart()), automationType);
            }

            if (recordToNoteAutomation)
                mc->setMPEMode (true);

            mc->setMidiChannel (midiChannel);

            if (programToUse > 0)
                mc->getSequence().addControllerEvent ({}, MidiControllerEvent::programChangeType,
                                                      (programToUse - 1) << 7, &ed.getUndoManager());

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

    bool isRecordingActive (EditItemID targetID) const override
    {
        return getMidiInput().recordingEnabled && InputDeviceInstance::isRecordingActive (targetID);
    }

    bool isRecordingQueuedToStop (EditItemID targetID) override
    {
        return getRecordStopper().isQueued (targetID);
    }

    bool shouldTrackContentsBeMuted (const Track& t) override
    {
        return getContextForID (t.itemID) != nullptr
            && ! getMidiInput().mergeRecordings;
    }

    virtual void handleMMCMessage (const juce::MidiMessage&) {}
    virtual bool handleTimecodeMessage (const juce::MidiMessage&) { return false; }

    std::shared_ptr<choc::fifo::SingleReaderSingleWriterFIFO<juce::MidiMessage>> getRecordingNotes (EditItemID targetID) const override
    {
        const std::shared_lock sl (contextLock);

        if (auto rc = getContextForID (targetID))
            return rc->liveNotes;

        return {};
    }

    class MidiRecordingContext : public RecordingContext
    {
    public:
        MidiRecordingContext (EditPlaybackContext& epc, EditItemID target, TimeRange punchRange_)
            : RecordingContext (target),
              scopedActiveRecordingDevice (epc),
              punchRange (punchRange_)
        {
            liveNotes = std::make_shared<choc::fifo::SingleReaderSingleWriterFIFO<juce::MidiMessage>>();
            liveNotes->reset (100);
        }

        detail::ScopedActiveRecordingDevice scopedActiveRecordingDevice;
        const TimeRange punchRange;
        juce::MidiMessageSequence recorded;
        TimePosition unloopedStopTime;

        std::shared_ptr<choc::fifo::SingleReaderSingleWriterFIFO<juce::MidiMessage>> liveNotes;

        std::function<void (tl::expected<Clip::Array, juce::String>)> stopCallback;
        StopRecordingParameters stopParams;
    };

    std::vector<tl::expected<std::unique_ptr<RecordingContext>, juce::String>> prepareToRecord (RecordingParameters params) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        std::vector<tl::expected<std::unique_ptr<RecordingContext>, juce::String>> results;

        if (params.targets.empty())
            for (auto dest : destinations)
                if (dest->recordEnabled)
                    params.targets.push_back (dest->targetID);

        for (auto targetID : params.targets)
        {
            auto recContext = std::make_unique<MidiRecordingContext> (context,
                                                                      targetID,
                                                                      params.punchRange);
            results.emplace_back (std::move (recContext));
        }

        return results;
    }

    std::vector<std::unique_ptr<RecordingContext>> startRecording (std::vector<std::unique_ptr<RecordingContext>> newContexts) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        bool hasAddedContexts = false;

        for (auto& recContext : newContexts)
        {
            if (auto midiContext = dynamic_cast<MidiRecordingContext*> (recContext.get()))
            {
                const auto targetID = midiContext->targetID;

                {
                    const std::unique_lock sl (contextLock);
                    recordingContexts.push_back (std::unique_ptr<MidiRecordingContext> (midiContext));
                }

                hasAddedContexts = true;
                recContext.release();
                context.transport.callRecordingAboutToStartListeners (*this, targetID);
            }
        }

        if (hasAddedContexts && ! edit.getTransport().isPlaying())
            edit.getTransport().play (false);

        // Remove now empty contents and return the rest
        return std::move (erase_if_null (newContexts));
    }

    void prepareToStopRecording (std::vector<EditItemID>) override
    {
        // We don't really need to do this as MIDI is event based
        // However, if we do get extra events after recording is
        // stopped, we might want to handle this the same way as
        // audio by bypassing input
    }

    tl::expected<Clip::Array, juce::String> stopRecording (StopRecordingParameters params) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        // Reserve to avoid allocating whilst
        const auto numContextsRecording = [this]
                                          {
                                              const std::shared_lock sl (contextLock);
                                              return recordingContexts.size();
                                          }();

        std::vector<std::unique_ptr<MidiRecordingContext>> contextsToStop;
        contextsToStop.reserve (numContextsRecording);

        // Stop the relevant contexts
        {
            const std::unique_lock sl (contextLock);

            for (auto& recContext : recordingContexts)
            {
                if (! params.targetsToStop.empty())
                    if (! contains_v (params.targetsToStop, recContext->targetID))
                        continue;

                recContext->unloopedStopTime = params.unloopedTimeToEndRecording;
                contextsToStop.push_back (std::move (recContext));
            }

            erase_if_null (recordingContexts);
            assert ((recordingContexts.size() + contextsToStop.size()) == numContextsRecording);
        }

        // Now apply those stop contexts whilst not holding the lock
        Clip::Array clips;

        for (auto& recContext : contextsToStop)
        {
            const auto targetID = recContext->targetID;
            auto stopCallback = std::move (recContext->stopCallback);
            context.transport.callRecordingAboutToStopListeners (*this, targetID);
            auto contextClips = applyRecording (std::move (recContext),
                                                params.unloopedTimeToEndRecording,
                                                params.isLooping, params.markedRange,
                                                params.discardRecordings);
            context.transport.callRecordingFinishedListeners (*this, targetID, contextClips);

            if (stopCallback)
                stopCallback (contextClips);

            clips.addArray (std::move (contextClips));
        }

        return clips;
    }

    void stopRecording (StopRecordingParameters params,
                        std::function<void (tl::expected<Clip::Array, juce::String>)> callback) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        // Reserve to avoid allocating whilst the lock is held
        const auto getNumContextsRecording = [this]
                                             {
                                                const std::shared_lock sl (contextLock);
                                                return recordingContexts.size();
                                             };

        if (params.targetsToStop.empty())
        {
            params.targetsToStop.reserve (getNumContextsRecording());

            const std::shared_lock sl (contextLock);

            for (auto& recContext : recordingContexts)
                params.targetsToStop.push_back (recContext->targetID);
        }

        // Set the punch out time for the contexts
        {
            const std::unique_lock sl (contextLock);

            for (auto& recContext : recordingContexts)
            {
                if (! contains_v (params.targetsToStop, recContext->targetID))
                    continue;

                recContext->unloopedStopTime = params.unloopedTimeToEndRecording;

                // Unlock whilst doing potentially allocating ops to avoid priority inversion
                {
                    contextLock.unlock();
                    recContext->stopCallback = callback;
                    recContext->stopParams = params;
                    recContext->stopParams.targetsToStop = { recContext->targetID };
                    contextLock.lock();
                }
            }
        }

        // Add the rec context to a timer list to poll if the recording can be stopped
        for (auto targetID : params.targetsToStop)
            getRecordStopper().addTargetToStop (targetID);
    }

    TimePosition getPunchInTime (EditItemID targetID) override
    {
        const std::shared_lock sl (contextLock);

        for (auto& recContext : recordingContexts)
            if (recContext->targetID == targetID)
                return recContext->punchRange.getStart();

        return edit.getTransport().getTimeWhenStarted();
    }

    bool isRecording (EditItemID targetID) override
    {
        return getContextForID (targetID) != nullptr;
    }

    bool isRecording() override
    {
        const std::shared_lock sl (contextLock);
        return ! recordingContexts.empty();
    }

    bool handleIncomingMidiMessage (const juce::MidiMessage& message, MPESourceID sourceID)
    {
        {
            juce::ScopedLock sl (activeNotesLock);

            if (message.isNoteOn())
                activeNotes.startNote (message.getChannel(), message.getNoteNumber());
            else if (message.isNoteOff())
                activeNotes.clearNote (message.getChannel(), message.getNoteNumber());
        }

        const bool recording = isRecording();

        if (recording)
        {
            auto m1 = juce::MidiMessage (message, context.globalStreamTimeToEditTime (message.getTimeStamp()).inSeconds());
            auto m2 = juce::MidiMessage (message, context.globalStreamTimeToEditTimeUnlooped (message.getTimeStamp()).inSeconds());

            for (auto& recContext : recordingContexts)
                recContext->liveNotes->push (m1);

            const std::shared_lock sl (contextLock);

            for (auto& recContext : recordingContexts)
                recContext->recorded.addEvent (m2);
        }

        juce::ScopedLock sl (consumerLock);

        for (auto c : consumers)
            c->handleIncomingMidiMessage (message, sourceID);

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

    Clip::Array applyRecording (std::unique_ptr<MidiRecordingContext> recContext,
                                TimePosition unloopedEndTime,
                                bool isLooping, TimeRange loopRange,
                                bool discardRecordings)
    {
        CRASH_TRACER
        if (! recContext || discardRecordings)
        {
            for (juce::ScopedLock sl (consumerLock); auto c : consumers)
                c->discardRecordings (recContext ? recContext->targetID : EditItemID());

            return {};
        }

        if (recContext->recorded.getNumEvents() == 0)
            return {};

        auto track = findAudioTrackForID (edit, recContext->targetID);
        auto clipSlot = track ? nullptr : findClipSlotForID (edit, recContext->targetID);

        if (! track && ! clipSlot)
            return {};

        // Never add takes for slot recordings
        if (clipSlot)
            isLooping = false;

        auto clipOwner = track != nullptr ? static_cast<ClipOwner*> (track)
                                          : static_cast<ClipOwner*> (clipSlot);
        Clip::Array createdClips;
        auto& mi = getMidiInput();
        auto& recorded = recContext->recorded;
        const bool wasPunchRecording = clipSlot ? false : edit.recordingPunchInOut;

        recorded.updateMatchedPairs();
        auto channelToApply = mi.recordToNoteAutomation ? mi.getChannelToUse()
                                                        : applyChannel (recorded, mi.getChannelToUse());
        auto timeAdjustMs = mi.getManualAdjustmentMs();

        if (context.getNodePlayHead() != nullptr)
            timeAdjustMs -= 1000.0 * tracktion::graph::sampleToTime (context.getLatencySamples(), edit.engine.getDeviceManager().getSampleRate());

        applyTimeAdjustment (recorded, timeAdjustMs);

        TimeRange recordedRange (recContext->punchRange.getStart(), unloopedEndTime);
        auto recordingStart = recordedRange.getStart();
        auto recordingEnd = recordedRange.getEnd();

        const bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

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

                        if (auto noteOff = recorded.getEventPointer (i)->noteOffObject)
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
                        if (auto clip = mi.addMidiAsTransaction (edit, clipOwner->getClipOwnerID(), takeClip,
                                                                 std::move (loopSequence), loopRange,
                                                                 MidiInputDevice::MergeMode::optional,
                                                                 channelToApply))
                        {
                            takeClip = clip;
                            createdClips.add (clip);
                        }
                    }
                    else if (mi.replaceExistingClips)
                    {
                        replaceSequence = std::move (loopSequence);
                    }
                    else
                    {
                        if (auto clip = mi.addMidiAsTransaction (edit, clipOwner->getClipOwnerID(), nullptr,
                                                                 std::move (loopSequence), loopRange,
                                                                 MidiInputDevice::MergeMode::always,
                                                                 channelToApply))
                        {
                            createdClips.add (clip);
                        }
                    }
                }
            }

            if (mi.replaceExistingClips && replaceSequence.getNumEvents() > 0)
            {
                if (auto clip = mi.addMidiAsTransaction (edit, clipOwner->getClipOwnerID(), nullptr,
                                                         std::move (replaceSequence), loopRange,
                                                         MidiInputDevice::MergeMode::optional,
                                                         channelToApply))
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

            if (wasPunchRecording)
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
                if (auto clip = mi.addMidiAsTransaction (edit, clipOwner->getClipOwnerID(), nullptr,
                                                         std::move (recorded), { startPos, endPos },
                                                         MidiInputDevice::MergeMode::optional,
                                                         channelToApply))
                {
                    createdClips.add (clip);
                }
            }
        }

        for (juce::ScopedLock sl (consumerLock); auto c : consumers)
            c->discardRecordings (recContext->targetID);

        return createdClips;
    }

    juce::Array<Clip*> applyRetrospectiveRecord (bool armedOnly) override
    {
        CRASH_TRACER

        juce::Array<Clip*> clips;

        auto& mi = getMidiInput();
        auto retrospective = mi.getRetrospectiveMidiBuffer();

        if (retrospective == nullptr)
            return {};

        for (auto track : getTargetTracks (*this))
        {
            if (armedOnly && ! isRecordingActive (track->itemID))
                continue;

            auto sequence = retrospective->takeMidiMessages();

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

            if (sequence.getNumEvents() > 0)
            {
                auto clip = mi.addMidiAsTransaction (edit, track->getClipOwnerID(), nullptr, std::move (sequence),
                                                     { TimePosition::fromSeconds (start), TimePosition::fromSeconds (start + length) },
                                                     MidiInputDevice::MergeMode::never,
                                                     channelToApply);
                clip->setOffset (TimeDuration::fromSeconds (offset));
                clips.add (clip);

                if (auto mc = dynamic_cast<MidiClip*> (clip))
                {
                    if (track->playSlotClips.get())
                    {
                        if (auto slot = getFreeSlot (*track))
                        {
                            mc->setUsesProxy (false);
                            mc->setStart (0_tp, false, true);

                            if (! mc->isLooping ())
                                mc->setLoopRangeBeats (mc->getEditBeatRange());

                            mc->removeFromParent();
                            slot->setClip (mc);
                        }
                    }
                }
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

    MidiInputDevice& getMidiInput() const   { return static_cast<MidiInputDevice&> (owner); }

    mutable std::shared_mutex contextLock;
    std::vector<std::unique_ptr<MidiRecordingContext>> recordingContexts;

private:
    juce::CriticalSection consumerLock, activeNotesLock;
    juce::Array<Consumer*> consumers;
    double lastEditTime = -1.0;
    double pausedTime = 0;
    MPESourceID midiSourceID = createUniqueMPESourceID();
    ActiveNoteList activeNotes;
    std::unique_ptr<RecordStopper> recordStopper;

    void addConsumer (Consumer* c) override      { juce::ScopedLock sl (consumerLock); consumers.addIfNotAlreadyThere (c); }
    void removeConsumer (Consumer* c) override   { juce::ScopedLock sl (consumerLock); consumers.removeAllInstancesOf (c); }

    void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int index) override
    {
        if (p == state && c.hasType (IDs::INPUTDEVICEDESTINATION))
            injectNoteOffsToTrack();

        InputDeviceInstance::valueTreeChildRemoved (p, c, index);
    }

    void injectNoteOffsToTrack()
    {
        ActiveNoteList notes;

        // The lock isn't great here but it's mostly uncontended and when it is, it's between the MIDI and message threads
        // The lock time is also constant so low-risk
        {
            const juce::ScopedLock sl (activeNotesLock);
            notes = activeNotes;
        }

        for (auto t : getTargetTracks (*this))
        {
            notes.iterate ([&] (auto channel, auto noteNumber)
                           {
                               t->injectLiveMidiMessage (juce::MidiMessage::noteOff (channel, noteNumber), midiSourceID);
                           });
        }
    }

    MidiRecordingContext* getContextForID (EditItemID targetID) const
    {
        const std::shared_lock sl (contextLock);

        for (auto& recContext : recordingContexts)
            if (recContext->targetID == targetID)
                return recContext.get();

        return nullptr;
    }

    RecordStopper& getRecordStopper()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (! recordStopper)
        {
            recordStopper = std::make_unique<RecordStopper> ([this] (auto targetID)
            {
                const auto unloopedTimeNow = context.getUnloopedPosition();
                const std::shared_lock sl (contextLock);

                if (auto recContext = getContextForID (targetID))
                {
                    if (unloopedTimeNow >= recContext->unloopedStopTime)
                    {
                        auto stopParams = recContext->stopParams;

                        // Temp unlock as stopRecording takes a unique lock
                        contextLock.unlock_shared();
                        auto res = stopRecording (stopParams);
                        contextLock.lock_shared();

                        return RecordStopper::HasFinished::yes;
                    }

                    return RecordStopper::HasFinished::no;
                }

                 return RecordStopper::HasFinished::yes;
            });
        }

        return *recordStopper;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputDeviceInstanceBase)
};


//==============================================================================
bool MidiInputDevice::handleIncomingMessage (juce::MidiMessage& m)
{
    if (m.isActiveSense())
        return false;

    if (disallowedChannels[m.getChannel() - 1])
        return false;

    if (m.isNoteOnOrOff())
    {
        auto note = m.getNoteNumber();

        if (note < noteFilterRange.startNote || note >= noteFilterRange.endNote)
            return false;
    }

    if (m.getTimeStamp() == 0 || (! engine.getEngineBehaviour().isMidiDriverUsedForIncommingMessageTiming()))
        m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);

    m.addToTimeStamp (adjustSecs);

    if (overrideNoteVels && m.isNoteOn())
        m.setVelocity (1.0f);

    if (! retrospectiveRecordLock && retrospectiveBuffer != nullptr)
        retrospectiveBuffer->addMessage (m, adjustSecs);

    sendNoteOnToMidiKeyListeners (m);
    return true;
}

void MidiInputDevice::masterTimeUpdate (double time)
{
    adjustSecs = time - juce::Time::getMillisecondCounterHiRes() * 0.001;

    const juce::ScopedLock sl (instanceLock);

    for (auto instance : instances)
        instance->masterTimeUpdate (time);
}

void MidiInputDevice::sendMessageToInstances (const juce::MidiMessage& message, MPESourceID sourceID)
{
    bool messageUnused = true;

    {
        const juce::ScopedLock sl (instanceLock);

        for (auto i : instances)
            if (i->handleIncomingMidiMessage (message, sourceID))
                messageUnused = false;
    }

    if (messageUnused && message.isNoteOn())
        if (auto& warnOfWasted = engine.getDeviceManager().warnOfWastedMidiMessagesFunction)
            warnOfWasted (this);
}

}} // namespace tracktion { inline namespace engine
