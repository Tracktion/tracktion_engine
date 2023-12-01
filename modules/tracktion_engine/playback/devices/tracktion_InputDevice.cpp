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

InputDevice::InputDevice (Engine& e, const juce::String& t, const juce::String& n)
   : engine (e), type (t), name (n)
{
    alias = e.getPropertyStorage().getPropertyItem (SettingID::invalid, getGlobalPropertyName());
    defaultAlias = n;
}

InputDevice::~InputDevice()
{
}

juce::String InputDevice::getGlobalPropertyName() const
{
    return type + "in_" + name + "_alias";
}

bool InputDevice::isTrackDevice() const
{
    return getDeviceType() == trackWaveDevice
            || getDeviceType() == trackMidiDevice;
}

static juce::String findDefaultAliasNameNotClashingWithInputDevices (Engine& engine, bool isMIDI,
                                                                     const juce::String& originalName,
                                                                     juce::String defaultAlias)
{
    int maxLength = 20;

    if (defaultAlias.length() <= maxLength)
        return defaultAlias;

    juce::String bracketed;

    if (defaultAlias.containsChar ('(') && defaultAlias.trim().endsWithChar (')'))
        bracketed = defaultAlias.fromLastOccurrenceOf ("(", false, false)
                                .upToFirstOccurrenceOf (")", false, false)
                                .trim();

    defaultAlias = defaultAlias.substring (0, std::max (10, maxLength - bracketed.length())).trim();
    defaultAlias = defaultAlias.upToLastOccurrenceOf (" ", false, false).trim();

    if (bracketed.isNotEmpty())
        defaultAlias << " (" << bracketed << ")";

    while (defaultAlias.endsWithChar ('+') || defaultAlias.endsWithChar ('-'))
        defaultAlias = defaultAlias.dropLastCharacters (1).trim();

    auto& dm = engine.getDeviceManager();

    auto totalDevs = isMIDI ? dm.getNumMidiInDevices()
                            : dm.getNumWaveInDevices();

    auto newAlias = defaultAlias;
    defaultAlias = {};

    for (int i = 0; i < totalDevs; ++i)
    {
        auto otherAlias = isMIDI ? dm.getMidiInDevice(i)->getAlias()
                                 : dm.getWaveInDevice(i)->getAlias();

        if (otherAlias == defaultAlias && otherAlias.isNotEmpty())
        {
            // if it's a dupe, just bottle out and use the full name
            newAlias = originalName.substring (0, 40);
            break;
        }
    }

    return newAlias;
}

void InputDevice::initialiseDefaultAlias()
{
    defaultAlias = getName();

    if (isTrackDevice())
        return;

    defaultAlias = findDefaultAliasNameNotClashingWithInputDevices (engine, isMidi(), getName(), defaultAlias);
}

juce::String InputDevice::getAlias() const
{
    if (alias.trim().isNotEmpty())
        return alias;

    return defaultAlias;
}

void InputDevice::setAlias (const juce::String& a)
{
    if (alias != a)
    {
        alias = a.substring (0, 20).trim();

        if (alias == defaultAlias)
            alias = {};
    }
}

bool InputDevice::isEnabled() const
{
    return enabled;
}

void InputDevice::setMonitorMode (MonitorMode newMode)
{
    if (monitorMode == newMode)
        return;

    monitorMode = newMode;
    TransportControl::restartAllTransports (engine, false);
    changed();
    saveProps();
}

juce::String InputDevice::getSelectableDescription()
{
    return name + " (" + type + ")";
}

void InputDevice::setRetrospectiveLock (Engine& e, const juce::Array<InputDeviceInstance*>& devices, bool lock)
{
    const juce::ScopedLock sl (e.getDeviceManager().deviceManager.getAudioCallbackLock());

    for (auto* idi : devices)
        idi->getInputDevice().retrospectiveRecordLock = lock;
}

//==============================================================================
InputDeviceInstance::InputDeviceInstance (InputDevice& d, EditPlaybackContext& c)
    : state (c.edit.getEditInputDevices().getInstanceStateForInputDevice (d)), owner (d), context (c), edit (c.edit)
{
    trackDeviceEnabler.setFunction ([this]
                                    {
                                        // if we're a track device we also need to disable our owner as there will only
                                        // be one instance and this stops the device getting added to the EditPlaybackContext
                                        if (owner.isTrackDevice())
                                        {
                                            owner.setEnabled (destinations.size() > 0);

                                            if (! owner.isEnabled())
                                                state.getParent().removeChild (state, nullptr);
                                        }
                                    });

    state.addListener (this);
}

InputDeviceInstance::~InputDeviceInstance()
{
    jassert (Selectable::isSelectableValid (&edit));
}

juce::Array<EditItemID> InputDeviceInstance::getTargets() const
{
    juce::WeakReference<InputDeviceInstance> ref (const_cast<InputDeviceInstance*> (this));
    trackDeviceEnabler.handleUpdateNowIfNeeded();

    if (ref.wasObjectDeleted())
        return {};

    if (! owner.isEnabled())
        return {};

    juce::Array<EditItemID> targets;

    for (auto dest : destinations)
        if (auto tID = dest->targetID; tID.isValid())
            targets.add (tID);

    return targets;
}

tl::expected<InputDeviceInstance::Destination*, juce::String>
InputDeviceInstance::setTarget (EditItemID targetID, bool move, juce::UndoManager* um,
                                std::optional<int> index)
{
    auto track = findTrackForID (edit, targetID);
    ClipSlot* clipSlot = track != nullptr ? nullptr
                                          : findClipSlotForID (edit, targetID);

    if (! (track || clipSlot))
        return {};

    if (isRecording())
        return tl::unexpected (TRANS("Can't change tracks whilst recording is active"));

    if (owner.isTrackDevice() || move)
    {
        for (auto t : getTargets())
            [[ maybe_unused ]] auto res = removeTarget (t, um);
    }
    else
    {
        [[ maybe_unused ]] auto res = removeTarget (targetID, um);
    }

    auto v = juce::ValueTree (IDs::INPUTDEVICEDESTINATION);
    v.setProperty (IDs::targetID, targetID, nullptr);

    if (index)
        v.setProperty (IDs::targetIndex, 0, nullptr);

    state.addChild (v, -1, um);
    jassert (destinations[destinations.size() - 1]->targetID == targetID);

    trackDeviceEnabler.triggerAsyncUpdate();

    return destinations[destinations.size() - 1];
}

juce::Result InputDeviceInstance::removeTarget (EditItemID targetID, juce::UndoManager* um)
{
    if (isRecording())
        return juce::Result::fail (TRANS("Can't change tracks whilst recording is active"));

    for (int i = destinations.size(); --i >= 0;)
    {
        auto& dt = *destinations[i];

        if (dt.targetID == targetID)
            state.removeChild (dt.state, um);
    }

    return juce::Result::ok();
}

bool InputDeviceInstance::isRecordingActive() const
{
    for (auto dest : destinations)
        if (dest->recordEnabled)
            return true;

    return false;
}

bool InputDeviceInstance::isRecordingActive (EditItemID targetID) const
{
    for (auto dest : destinations)
        if (targetID == dest->targetID)
            return dest->recordEnabled;

    return false;
}

bool InputDeviceInstance::isRecordingEnabled (EditItemID targetID) const
{
    for (auto dest : destinations)
        if (dest->targetID == targetID)
            if (dest->recordEnabled)
                return true;

    return false;
}

void InputDeviceInstance::setRecordingEnabled (EditItemID targetID, bool b)
{
    for (auto dest : destinations)
        if (dest->targetID == targetID)
            dest->recordEnabled = b;
}

bool InputDeviceInstance::isLivePlayEnabled (const Track& t) const
{
    if (! t.acceptsInput())
        return false;

    for (auto dest : destinations)
    {
        if (dest->targetID != t.itemID)
            continue;

        switch (owner.getMonitorMode())
        {
            case InputDevice::MonitorMode::on:          return true;
            case InputDevice::MonitorMode::automatic:   return isRecordingEnabled (t.itemID);
            case InputDevice::MonitorMode::off:         return false;
        };
    }

    return false;
}

void InputDeviceInstance::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v.getParent() == state)
    {
        if (id == IDs::armed || id == IDs::targetID)
        {
            const auto targetID = EditItemID::fromVar (v[IDs::targetID]);

            if (id == IDs::targetID)
                if (auto t = findTrackForID (edit, targetID))
                    t->changed();

            updateRecordingStatus (targetID);
        }
    }
}

void InputDeviceInstance::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c)
{
    if (p == state && c.hasType (IDs::INPUTDEVICEDESTINATION))
    {
        const auto targetID = EditItemID::fromVar (c[IDs::targetID]);

        if (auto t = findTrackForID (edit, targetID))
            t->changed();

        updateRecordingStatus (targetID);
    }
}

void InputDeviceInstance::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int)
{
    if (p == state && c.hasType (IDs::INPUTDEVICEDESTINATION))
    {
        const auto targetID = EditItemID::fromVar (c[IDs::targetID]);

        if (auto t = findTrackForID (edit, targetID))
            t->changed();

        updateRecordingStatus (targetID);
    }
}

void InputDeviceInstance::updateRecordingStatus (EditItemID targetID)
{
    for (auto dest : destinations)
    {
        if (dest->targetID != targetID)
            continue;

        auto track = findTrackForID (edit, targetID);

        if (! track)
            continue;

        InputDeviceInstance::StopRecordingParameters params;
        params.targetsToStop = { targetID };
        params.unloopedTimeToEndRecording = context.getUnloopedPosition();
        params.isLooping = context.transport.looping;
        params.markedRange = context.transport.getLoopRange();
        params.discardRecordings = false;
        auto res = stopRecording (params);
        Clip::Array clips = res.value_or (Clip::Array());

        const bool wasRecording = ! clips.isEmpty();
        const bool isLivePlayActive = isLivePlayEnabled (*track);

        // This logic rebuilds the audio graph if the monitor state changes as it's a static node-type in the graph
        if (! wasRecording && wasLivePlayActive != isLivePlayActive)
            edit.restartPlayback();

        wasLivePlayActive = isLivePlayActive;

        if (! wasRecording && isRecordingEnabled (targetID))
            prepareAndPunchRecord (*this, targetID);
    }
}

//==============================================================================
juce::Array<std::pair<AudioTrack*, int>> getTargetTracksAndIndexes (InputDeviceInstance& instance)
{
    juce::Array<std::pair<AudioTrack*, int>> tracks;

    for (auto dest : instance.destinations)
        if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (instance.edit, dest->targetID)))
            tracks.add ({ at, dest->targetIndex });

    return tracks;
}

juce::Array<AudioTrack*> getTargetTracks (InputDeviceInstance& instance)
{
    juce::Array<AudioTrack*> tracks;

    for (auto [at, idx] : getTargetTracksAndIndexes (instance))
        tracks.add (at);

    return tracks;
}

bool isOnTargetTrack (InputDeviceInstance& instance, const Track& track, int index)
{
    for (auto [at, idx] : getTargetTracksAndIndexes (instance))
        if (at == &track && idx == index)
            return true;

    return false;
}

juce::Result clearFromTargets (InputDeviceInstance& instance, juce::UndoManager* um)
{
    if (instance.isRecording())
        return juce::Result::fail (TRANS("Can't change tracks whilst recording is active"));

    auto result = juce::Result::ok();

    for (auto targetID : instance.getTargets())
        if (auto res = instance.removeTarget (targetID, um); ! res)
            result = res;

    return juce::Result::ok();
}

bool isAttached (InputDeviceInstance& instance)
{
    return ! instance.getTargets().isEmpty();
}

//==============================================================================
InputDeviceInstance::Destination* getDestination (InputDeviceInstance& instance, const Track& track, int index)
{
    for (auto dest : instance.destinations)
        if (dest->targetID == track.itemID && dest->targetIndex == index)
            return dest;

    return {};
}

InputDeviceInstance::Destination* getDestination (InputDeviceInstance& instance, const ClipSlot& cs)
{
    for (auto dest : instance.destinations)
        if (dest->targetID == cs.itemID)
            return dest;

    return {};
}

InputDeviceInstance::Destination* getDestination (InputDeviceInstance& instance, const juce::ValueTree& destinationState)
{
    for (auto dest : instance.destinations)
        if (dest->state == destinationState)
            return dest;

    return {};
}


}} // namespace tracktion { inline namespace engine
