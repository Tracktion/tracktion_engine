/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


InputDevice::InputDevice (Engine& e, const String& t, const String& n)
   : engine (e), type (t), name (n)
{
    alias = e.getPropertyStorage().getPropertyItem (SettingID::invalid, getGlobalPropertyName());
    defaultAlias = n;
}

InputDevice::~InputDevice()
{
}

String InputDevice::getGlobalPropertyName() const
{
    return type + "in_" + name + "_alias";
}

bool InputDevice::isTrackDevice() const
{
    return getDeviceType() == trackWaveDevice
            || getDeviceType() == trackMidiDevice;
}

static String findDefaultAliasNameNotClashingWithInputDevices (Engine& engine, bool isMIDI, const String& originalName, String defaultAlias)
{
    int maxLength = 20;

    if (defaultAlias.length() <= maxLength)
        return defaultAlias;

    String bracketed;

    if (defaultAlias.containsChar ('(') && defaultAlias.trim().endsWithChar (')'))
        bracketed = defaultAlias.fromLastOccurrenceOf ("(", false, false)
                                .upToFirstOccurrenceOf (")", false, false)
                                .trim();

    defaultAlias = defaultAlias.substring (0, jmax (10, maxLength - bracketed.length())).trim();
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

String InputDevice::getAlias() const
{
    if (alias.trim().isNotEmpty())
        return alias;

    return defaultAlias;
}

void InputDevice::setAlias (const String& a)
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

String InputDevice::getSelectableDescription()
{
    return name + " (" + type + ")";
}

void InputDevice::setRetrospectiveLock (Engine& e, const Array<InputDeviceInstance*>& devices, bool lock)
{
    const ScopedLock sl (e.getDeviceManager().deviceManager.getAudioCallbackLock());

    for (auto* idi : devices)
        idi->getInputDevice().retrospectiveRecordLock = lock;
}

//==============================================================================
InputDeviceInstance::InputDeviceInstance (InputDevice& d, EditPlaybackContext& c)
    : state (c.edit.getEditInputDevices().getInstanceStateForInputDevice (d)), owner (d), context (c), edit (c.edit)
{
    recordEnabled.referTo (state, IDs::armed, nullptr, false);
    targetTrack.referTo (state, IDs::targetTrack, nullptr);
    targetIndex.referTo (state, IDs::targetIndex, nullptr, -1);

    trackDeviceEnabler.setFunction ([this]
                                    {
                                        // if we're a track device we also need to disable our owner as there will only
                                        // be one instance and this stops the device getting added to the EditPlaybackContext
                                        if (owner.isTrackDevice())
                                        {
                                            owner.setEnabled (targetIndex >= 0 && targetTrack.get().isValid());

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

AudioTrack* InputDeviceInstance::getTargetTrack() const
{
    WeakReference<InputDeviceInstance> ref (const_cast<InputDeviceInstance*> (this));
    trackDeviceEnabler.handleUpdateNowIfNeeded();

    if (ref.wasObjectDeleted())
        return {};

    return owner.isEnabled() ? dynamic_cast<AudioTrack*> (findTrackForID (edit, targetTrack))
                             : nullptr;
}

int InputDeviceInstance::getTargetIndex() const
{
    WeakReference<InputDeviceInstance> ref (const_cast<InputDeviceInstance*> (this));
    trackDeviceEnabler.handleUpdateNowIfNeeded();

    if (ref.wasObjectDeleted())
        return -1;

    return owner.isEnabled() ? targetIndex : -1;
}

void InputDeviceInstance::setTargetTrack (AudioTrack* track, int index)
{
    if (isRecording())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return;
    }

    jassert (index >= 0);
    targetTrack = track == nullptr ? EditItemID() : track->itemID;
    targetIndex = index;

    trackDeviceEnabler.triggerAsyncUpdate();
}

void InputDeviceInstance::clearFromTrack()
{
    targetTrack.resetToDefault();
    targetIndex.resetToDefault();

    trackDeviceEnabler.triggerAsyncUpdate();
}

bool InputDeviceInstance::isAttachedToTrack() const
{
    return getTargetTrack() != nullptr;
}

bool InputDeviceInstance::isRecordingActive() const
{
    if (recordEnabled)
        if (auto t = getTargetTrack())
            return t->acceptsInput();

    return false;
}

bool InputDeviceInstance::isLivePlayEnabled() const
{
    if (auto t = getTargetTrack())
        return t->acceptsInput();

    return false;
}

void InputDeviceInstance::prepareAndPunchRecord()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (! (isRecordingEnabled() && edit.getTransport().isRecording()))
        return;

    auto& dm = edit.engine.getDeviceManager();
    const double start = context.playhead.getPosition();
    const double sampleRate = dm.getSampleRate();
    const int blockSize = dm.getBlockSize();

    const String error (prepareToRecord (start, start, sampleRate, blockSize, true));

    if (error.isNotEmpty())
        edit.engine.getUIBehaviour().showWarningMessage (error);
    else
        startRecording();
}

void InputDeviceInstance::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state)
        if (id == IDs::armed || id == IDs::targetTrack)
            updateRecordingStatus();
}

void InputDeviceInstance::updateRecordingStatus()
{
    const bool wasRecording = ! stopRecording().isEmpty();
    const bool isLivePlayActive = isLivePlayEnabled();

    if (! wasRecording && wasLivePlayActive != isLivePlayActive)
        edit.restartPlayback();

    wasLivePlayActive = isLivePlayActive;

    if (! wasRecording && recordEnabled)
        prepareAndPunchRecord();
}
