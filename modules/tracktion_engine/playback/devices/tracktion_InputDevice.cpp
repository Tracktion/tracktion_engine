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
                                            owner.setEnabled (destTracks.size() > 0);

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

juce::Array<AudioTrack*> InputDeviceInstance::getTargetTracks() const
{
    juce::WeakReference<InputDeviceInstance> ref (const_cast<InputDeviceInstance*> (this));
    trackDeviceEnabler.handleUpdateNowIfNeeded();

    if (ref.wasObjectDeleted())
        return {};

    juce::Array<AudioTrack*> tracks;
    
    if (owner.isEnabled())
        for (auto dest : destTracks)
            if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (edit, dest->targetTrack)))
                tracks.add (at);
    
    return tracks;
}

juce::Array<int> InputDeviceInstance::getTargetIndexes() const
{
    juce::WeakReference<InputDeviceInstance> ref (const_cast<InputDeviceInstance*> (this));
    trackDeviceEnabler.handleUpdateNowIfNeeded();

    if (ref.wasObjectDeleted())
        return {};
    
    juce::Array<int> indexes;
    
    if (owner.isEnabled())
        for (auto dest : destTracks)
            indexes.add (dest->targetIndex);

    return indexes;
}

void InputDeviceInstance::setTargetTrack (AudioTrack& track, int index, bool moveToTrack)
{
    if (isRecording())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return;
    }
    
    if (owner.isTrackDevice() || moveToTrack)
    {
        for (auto t : getTargetTracks())
            removeTargetTrack (*t);
    }
    else
    {
        removeTargetTrack (track);
    }

    auto v = juce::ValueTree (IDs::INPUTDEVICEDESTINATION);
    state.addChild (v, -1, &edit.getUndoManager());
    
    auto& dest = *destTracks[destTracks.size() - 1];
    dest.targetTrack = track.itemID;
    dest.targetIndex = index;

    trackDeviceEnabler.triggerAsyncUpdate();
}

void InputDeviceInstance::removeTargetTrack (AudioTrack& track)
{
    if (isRecording())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return;
    }
    
    for (int i = destTracks.size(); --i >= 0;)
    {
        auto& dt = *destTracks[i];
        if (dt.targetTrack == track.itemID)
            state.removeChild (dt.state, &edit.getUndoManager());
    }
}

void InputDeviceInstance::removeTargetTrack (AudioTrack& track, int index)
{
    removeTargetTrack (track.itemID, index);
}

void InputDeviceInstance::removeTargetTrack (EditItemID trackID, int index)
{
    if (isRecording())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return;
    }

    for (int i = destTracks.size(); --i >= 0;)
    {
        auto& dt = *destTracks[i];
        if (dt.targetTrack == trackID && dt.targetIndex == index)
            state.removeChild (dt.state, &edit.getUndoManager());
    }
}

void InputDeviceInstance::clearFromTracks()
{
    if (isRecording())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return;
    }
    
    for (int i = destTracks.size(); --i >= 0;)
    {
        auto& dt = *destTracks[i];
        state.removeChild (dt.state, &edit.getUndoManager());
    }

    trackDeviceEnabler.triggerAsyncUpdate();
}

bool InputDeviceInstance::isAttachedToTrack() const
{
    return getTargetTracks().size() > 0;
}

bool InputDeviceInstance::isOnTargetTrack (const Track& t)
{
    for (auto dest : destTracks)
        if (dest->targetTrack == t.itemID)
            return true;

    return false;
}

bool InputDeviceInstance::isOnTargetTrack (const Track& t, int idx)
{
    for (auto dest : destTracks)
        if (dest->targetTrack == t.itemID && dest->targetIndex == idx)
            return true;

    return false;
}

InputDeviceInstance::InputDeviceDestination* InputDeviceInstance::getDestination (const Track& track, int index)
{
    for (auto dest : destTracks)
        if (dest->targetTrack == track.itemID && dest->targetIndex == index)
            return dest;

    return {};
}

bool InputDeviceInstance::isRecordingActive() const
{
    for (auto dest : destTracks)
        if (dest->recordEnabled)
            if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (edit, dest->targetTrack)))
                return at->acceptsInput();
    
    return false;
}

bool InputDeviceInstance::isRecordingActive (const Track& t) const
{
    for (auto dest : destTracks)
        if (t.itemID == dest->targetTrack)
            if (dest->recordEnabled)
                return t.acceptsInput();
    
    return false;
}

bool InputDeviceInstance::isRecordingEnabled (const Track& t) const
{
    for (auto dest : destTracks)
        if (dest->targetTrack == t.itemID)
            if (dest->recordEnabled)
                return true;
    
    return false;
}

void InputDeviceInstance::setRecordingEnabled (const Track& t, bool b)
{
    if (edit.getTransport().isRecording() && destTracks.size() > 1)
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Punch is only supported for inputs on one track"));
        return;
    }

    for (auto dest : destTracks)
        if (dest->targetTrack == t.itemID)
            dest->recordEnabled = b;
}

bool InputDeviceInstance::isLivePlayEnabled (const Track& t) const
{
    for (auto dest : destTracks)
        if (dest->targetTrack == t.itemID)
            return t.acceptsInput();

    return false;
}

void InputDeviceInstance::prepareAndPunchRecord()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (auto t = getTargetTracks().getFirst())
    {
        if (! (isRecordingEnabled (*t) && edit.getTransport().isRecording()))
            return;

        auto& dm = edit.engine.getDeviceManager();
        const auto start = context.getPosition();
        const double sampleRate = dm.getSampleRate();
        const int blockSize = dm.getBlockSize();

        auto error = prepareToRecord (start, start, sampleRate, blockSize, true);

        if (error.isNotEmpty())
            edit.engine.getUIBehaviour().showWarningMessage (error);
        else
            startRecording();
    }
}

void InputDeviceInstance::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v.getParent() == state)
        if (id == IDs::armed || id == IDs::targetTrack)
            updateRecordingStatus();
}

void InputDeviceInstance::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c)
{
    if (p == state && c.hasType (IDs::INPUTDEVICEDESTINATION))
        updateRecordingStatus();
}

void InputDeviceInstance::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int)
{
    if (p == state && c.hasType (IDs::INPUTDEVICEDESTINATION))
        updateRecordingStatus();
}

void InputDeviceInstance::updateRecordingStatus()
{
    if (auto t = getTargetTracks().getFirst())
    {
        const bool wasRecording = ! stopRecording().isEmpty();
        const bool isLivePlayActive = isLivePlayEnabled (*t);

        if (! wasRecording && wasLivePlayActive != isLivePlayActive)
            edit.restartPlayback();

        wasLivePlayActive = isLivePlayActive;

        if (! wasRecording && isRecordingEnabled (*t))
            prepareAndPunchRecord();
    }
}

}} // namespace tracktion { inline namespace engine
