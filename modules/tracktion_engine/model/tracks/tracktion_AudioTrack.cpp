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

//==============================================================================
struct AudioTrack::TrackMuter  : private juce::AsyncUpdater
{
    TrackMuter (AudioTrack& at) : owner (at)        { triggerAsyncUpdate(); }
    ~TrackMuter() override                          { cancelPendingUpdate(); }

    void handleAsyncUpdate() override
    {
        for (int i = 1; i <= 16; ++i)
            owner.injectLiveMidiMessage (juce::MidiMessage::allNotesOff (i),
                                         MidiMessageArray::notMPE);

        owner.trackMuter = nullptr;
    }

    AudioTrack& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackMuter)
};

//==============================================================================
struct AudioTrack::FreezeUpdater : private ValueTreeAllEventListener,
                                   private juce::AsyncUpdater
{
    FreezeUpdater (AudioTrack& at)
        : owner (at), state (owner.state), triggerFreeze (false), updateFreeze (false)
    {
        state.addListener (this);
    }

    ~FreezeUpdater() override
    {
        state.removeListener (this);
        cancelPendingUpdate();
    }

    void freeze()
    {
        markAndUpdate (triggerFreeze);
    }

    AudioTrack& owner;
    juce::ValueTree state;

private:
    bool triggerFreeze, updateFreeze;

    void markAndUpdate (bool& flag)     { flag = true; triggerAsyncUpdate(); }

    bool compareAndReset (bool& flag)
    {
        if (! flag)
            return false;

        flag = false;
        return true;
    }

    void valueTreeChanged() override
    {
        markAndUpdate (updateFreeze);
    }

    void handleAsyncUpdate() override
    {
        if (compareAndReset (triggerFreeze))
        {
            if (! owner.isFrozen (groupFreeze))
                owner.setFrozen (true, Track::individualFreeze);
        }

        if (compareAndReset (updateFreeze))
        {
            if (owner.isFrozen (Track::individualFreeze) && (! owner.hasFreezePointPlugin()))
                owner.setFrozen (false, individualFreeze);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeUpdater)
};

//==============================================================================
AudioTrack::AudioTrack (Edit& ed, const juce::ValueTree& v)
    : ClipTrack (ed, v, 50, 13, 2000)
{
    soloed.referTo (state, IDs::solo, nullptr);
    soloIsolated.referTo (state, IDs::soloIsolate, nullptr);
    muted.referTo (state, IDs::mute, nullptr);
    frozen.referTo (state, IDs::frozen, nullptr);
    frozenIndividually.referTo (state, IDs::frozenIndividually, nullptr);
    ghostTracks.referTo (state, IDs::ghostTracks, nullptr);
    maxInputs.referTo (state, IDs::maxInputs, nullptr, 1);
    compGroup.referTo (state, IDs::compGroup, &edit.getUndoManager(), -1);
    midiVisibleProportion.referTo (state, IDs::midiVProp, nullptr);
    midiVerticalOffset.referTo (state, IDs::midiVOffset, nullptr);
    midiNoteMap.referTo (state, IDs::midiNoteMap, nullptr);

    updateMidiNoteMapCache();

    std::vector<ChannelIndex> channels = { ChannelIndex (0, juce::AudioChannelSet::left),
                                           ChannelIndex (1, juce::AudioChannelSet::right) };

    callBlocking ([this, &channels]
    {
        waveInputDevice = std::make_unique<WaveInputDevice> (edit.engine, itemID.toString(),
                                                             TRANS("Track Wave Input"), channels,
                                                             InputDevice::trackWaveDevice);

        midiInputDevice = std::make_unique<VirtualMidiInputDevice> (edit.engine, itemID.toString(),
                                                                    InputDevice::trackMidiDevice);

        auto& eid = edit.getEditInputDevices();
        waveInputDevice->setEnabled (eid.isInputDeviceAssigned (*waveInputDevice));
        midiInputDevice->setEnabled (eid.isInputDeviceAssigned (*midiInputDevice));
    });

    output = std::make_unique<TrackOutput> (*this);
    freezeUpdater = std::make_unique<FreezeUpdater> (*this);

    if (getMidiVerticalOffset() < 0 || getMidiVerticalOffset() > 0.99
         || getMidiVisibleProportion() < 0.1 || getMidiVisibleProportion() > 1.0)
        setVerticalScaleToDefault();

    asyncCaller.addFunction (updateAutoCrossfadesFlag,
                             [this]
                             {
                                 for (auto c : getClips())
                                     if (auto acb = dynamic_cast<AudioClipBase*> (c))
                                         if (acb->getAutoCrossfade())
                                             acb->updateAutoCrossfadesAsync (false);
                             });
}

AudioTrack::~AudioTrack()
{
    const bool clearWave = waveInputDevice != nullptr && waveInputDevice->isEnabled();
    const bool clearMidi = midiInputDevice != nullptr && midiInputDevice->isEnabled();

    if (clearWave || clearMidi)
    {
        edit.getTransport().freePlaybackContext();

        for (auto at : getTracksOfType<AudioTrack> (edit, true))
        {
            if (clearWave)  edit.getEditInputDevices().clearInputsOfDevice (*at, *waveInputDevice);
            if (clearMidi)  edit.getEditInputDevices().clearInputsOfDevice (*at, *midiInputDevice);
        }
    }

    notifyListenersOfDeletion();
}

void AudioTrack::initialise()
{
    CRASH_TRACER

    ClipTrack::initialise();

    if (frozenIndividually && ! getFreezeFile().existsAsFile())
        setFrozen (false, individualFreeze);

    output->initialise();
}

void AudioTrack::sanityCheckName()
{
    auto n = ClipTrack::getName();

    if ((n.startsWithIgnoreCase ("Track ")
          || n.startsWithIgnoreCase (TRANS("Track") + " "))
         && n.substring (6).trim().containsOnly ("0123456789"))
    {
        // For "Track 1" type names, leave the actual name empty.
        resetName();
        changed();
    }

    // We need to update the alias here as it's the first time the name will be returned correctly upon track creation
    // We also won't get a property change if the track order etc. changes as it will just be empty
    auto devName = getName();

    if (waveInputDevice != nullptr) waveInputDevice->setAlias (devName);
    if (midiInputDevice != nullptr) midiInputDevice->setAlias (devName);
}

juce::String AudioTrack::getName()
{
    auto n = ClipTrack::getName();

    if (n.isEmpty())
        n << TRANS("Track") << ' ' << getAudioTrackNumber();

    return n;
}

int AudioTrack::getAudioTrackNumber() noexcept
{
    return getAudioTracks (edit).indexOf (this) + 1;
}

VolumeAndPanPlugin* AudioTrack::getVolumePlugin()     { return pluginList.getPluginsOfType<VolumeAndPanPlugin>().getLast(); }
LevelMeterPlugin* AudioTrack::getLevelMeterPlugin()   { return pluginList.getPluginsOfType<LevelMeterPlugin>().getLast(); }
EqualiserPlugin* AudioTrack::getEqualiserPlugin()     { return pluginList.getPluginsOfType<EqualiserPlugin>().getLast(); }

AuxSendPlugin* AudioTrack::getAuxSendPlugin (int bus) const
{
    for (auto p : pluginList)
        if (auto f = dynamic_cast<AuxSendPlugin*> (p))
            if (bus < 0 || bus == f->getBusNumber())
                return f;

    return {};
}

//==============================================================================
juce::String AudioTrack::getNameForMidiNoteNumber (int note, int midiChannel, bool preferSharp) const
{
    jassert (midiChannel > 0);

    if (midiNoteMapCache.size() > 0)
    {
        auto itr = midiNoteMapCache.find (note);

        if (itr != midiNoteMapCache.end())
            return itr->second;

        return {};
    }

    juce::String s;

    for (auto af : pluginList)
        if (af->hasNameForMidiNoteNumber (note, midiChannel, s))
            return s;

    if (auto dest = output->getDestinationTrack())
        return dest->getNameForMidiNoteNumber (note, midiChannel, preferSharp);

    // try the master plugins..
    for (auto af : edit.getMasterPluginList())
        if (af->hasNameForMidiNoteNumber (note, midiChannel, s))
            return s;

    if (auto mo = dynamic_cast<MidiOutputDevice*> (getOutput().getOutputDevice (true)))
        return mo->getNameForMidiNoteNumber (note, midiChannel, preferSharp);

    return midiChannel == 10 ? TRANS(juce::MidiMessage::getRhythmInstrumentName (note))
                             : juce::MidiMessage::getMidiNoteName (note, preferSharp, true,
                                                                   edit.engine.getEngineBehaviour().getMiddleCOctave());
}

void AudioTrack::updateMidiNoteMapCache()
{
    midiNoteMapCache.clear();

    auto m = midiNoteMap.get();

    auto lines = juce::StringArray::fromLines (m);
    for (auto l : lines)
    {
        if (l.startsWith ("//"))
            continue;

        int digits = 0;
        for (int i = 0; i < l.length(); i++)
        {
            auto c = l[i];
            
            if (juce::CharacterFunctions::isDigit (c))
                digits++;
            else
                break;
        }

        if (digits > 0)
            midiNoteMapCache[l.substring (0, digits).getIntValue()] = l.substring (digits).trim();
    }
}

bool AudioTrack::areMidiPatchesZeroBased() const
{
    // do something for plugins here
    if (auto dest = output->getDestinationTrack())
        return dest->areMidiPatchesZeroBased();

    // try the master plugins..
    if (auto midiDevice = dynamic_cast<MidiOutputDevice*> (output->getOutputDevice (false)))
        return midiDevice->areMidiPatchesZeroBased();

    return false;
}

juce::String AudioTrack::getNameForBank (int bank) const
{
    juce::String s;

    for (auto p : pluginList)
        if (p->hasNameForMidiBank (bank, s))
            return s;

    if (auto dest = output->getDestinationTrack())
        return dest->getNameForBank (bank);

    // try the master plugins..
    for (auto p : edit.getMasterPluginList())
        if (p->hasNameForMidiBank (bank, s))
            return s;

    if (auto midiDevice = dynamic_cast<MidiOutputDevice*> (output->getOutputDevice (false)))
        return midiDevice->getBankName (bank);

    return edit.engine.getMidiProgramManager().getBankName (0, bank);
}

int AudioTrack::getIdForBank (int bank) const
{
    if (auto dest = output->getDestinationTrack())
        return dest->getIdForBank (bank);

    if (auto midiDevice = dynamic_cast<MidiOutputDevice*> (output->getOutputDevice (false)))
        return midiDevice->getBankID (bank);

    return bank;
}

juce::String AudioTrack::getNameForProgramNumber (int programNumber, int bank) const
{
    juce::String s;

    for (auto p : pluginList)
        if (p->hasNameForMidiProgram (programNumber, bank, s))
            return s;

    if (const AudioTrack* const dest = output->getDestinationTrack())
        return dest->getNameForProgramNumber (programNumber, bank);

    // try the master plugins..
    for (auto p : edit.getMasterPluginList())
        if (p->hasNameForMidiProgram (programNumber, bank, s))
            return s;

    if (auto midiDevice = dynamic_cast<MidiOutputDevice*> (output->getOutputDevice (false)))
        return midiDevice->getProgramName (programNumber, bank);

    return TRANS(juce::MidiMessage::getGMInstrumentName (programNumber));
}

//==============================================================================
void AudioTrack::setMute (bool b)           { muted = b; }
void AudioTrack::setSolo (bool b)           { soloed = b; }
void AudioTrack::setSoloIsolate (bool b)    { soloIsolated = b; }

bool AudioTrack::isMuted (bool includeMutingByDestination) const
{
    if (muted)
        return true;

    if (includeMutingByDestination)
    {
        if (auto p = getParentFolderTrack())
            return p->isMuted (true);
        
        if (auto dest = output->getDestinationTrack())
            return dest->isMuted (true);
    }

    return false;
}

bool AudioTrack::isSolo (bool includeIndirectSolo) const
{
    if (soloed)
        return true;

    if (includeIndirectSolo)
    {
        // If any of the parent tracks are soloed, this needs to be indirectly soloed
        for (auto p = getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
            if (p->isSolo (false))
                return true;

        if (! isPartOfSubmix())
            if (auto dest = output->getDestinationTrack())
                return dest->isSolo (true);
    }

    return false;
}

bool AudioTrack::isSoloIsolate (bool includeIndirectSolo) const
{
    if (soloIsolated)
        return true;

    if (includeIndirectSolo)
    {
        // If any of the parent tracks are solo isolate, this needs to be indirectly solo isolate
        for (auto p = getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
            if (p->isSoloIsolate (false))
                return true;

        if (! isPartOfSubmix())
            if (auto dest = output->getDestinationTrack())
                return dest->isSoloIsolate (true);
    }

    return false;
}

static bool isInputTrackSolo (const Track& track)
{
    for (auto t : track.getInputTracks())
        if (t->isSolo (true))
            return true;

    return false;
}

bool AudioTrack::isTrackAudible (bool areAnyTracksSolo) const
{
    if (areAnyTracksSolo && isInputTrackSolo (*this))
        return true;

    return Track::isTrackAudible (areAnyTracksSolo);
}

//==============================================================================
juce::String AudioTrack::getTrackPlayabilityWarning() const
{
    bool hasMidi = false, hasWave = false;

    for (auto c : getClips())
    {
        auto type = c->type;

        if (! hasMidi && (type == TrackItem::Type::midi || type == TrackItem::Type::step))
            hasMidi = true;

        if (! hasWave && type == TrackItem::Type::wave)
            hasWave = true;

        if (hasMidi && hasWave)
            break;
    }

    if (hasMidi && ! canPlayMidi())
        return TRANS("This track contains MIDI-generating clips which may be inaudible as it doesn't output to a MIDI device or a plugin synthesiser.")
                  + "\n\n" + TRANS("To change a track's destination, select the track and use its destination list.");

    if (hasWave && ! canPlayAudio())
    {
        if (! getOutput().canPlayAudio())
            return TRANS("This track contains wave clips which may be inaudible as it doesn't output to an audio device.")
                        + "\n\n" + TRANS("To change a track's destination, select the track and use its destination list.");

        return TRANS("This track contains wave clips which may be inaudible as the audio will be blocked by some of the track's plugins.");
    }

    return {};
}

bool AudioTrack::canPlayAudio() const
{
    if (! getOutput().canPlayAudio())
        return false;

    for (auto p : pluginList)
        if (! p->takesAudioInput())
            return false;

    return true;
}

bool AudioTrack::canPlayMidi() const
{
    if (getOutput().canPlayMidi())
        return true;

    for (auto p : pluginList)
        if (p->takesMidiInput())
            return getOutput().canPlayAudio();

    if (isPartOfSubmix())
        for (auto ft = getParentFolderTrack(); ft != nullptr; ft = ft->getParentFolderTrack())
            for (auto p : ft->pluginList)
                if (p->takesMidiInput())
                    return getOutput().canPlayAudio();

    return false;
}

void AudioTrack::setMidiVerticalPos (double visibleProp, double offset)
{
    visibleProp             = juce::jlimit (0.0, 1.0, visibleProp);
    midiVerticalOffset      = juce::jlimit (0.0, 1.0 - visibleProp, offset);
    midiVisibleProportion   = juce::jlimit (0.1, 1.0 - midiVerticalOffset, visibleProp);
}

void AudioTrack::setVerticalScaleToDefault()
{
    auto midiNotes = juce::jlimit (1, 128, 12 * static_cast<int> (edit.engine.getPropertyStorage()
                                                                   .getProperty (SettingID::midiEditorOctaves, 3)));
    midiVisibleProportion = midiNotes / 128.0;
    midiVerticalOffset = (1.0 - midiVisibleProportion) * 0.5;
}

void AudioTrack::setTrackToGhost (AudioTrack* track, bool shouldGhost)
{
    if (track == nullptr)
        return;

    auto list = EditItemID::parseStringList (ghostTracks.get());
    bool isGhosted = list.contains (track->itemID);

    if (isGhosted != shouldGhost)
    {
        if (shouldGhost)
            list.add (track->itemID);
        else
            list.removeAllInstancesOf (track->itemID);

        ghostTracks = EditItemID::listToString (list);
    }
}

juce::Array<AudioTrack*> AudioTrack::getGhostTracks() const
{
    juce::Array<AudioTrack*> tracks;

    for (auto& trackID : EditItemID::parseStringList (ghostTracks))
        if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (edit, trackID)))
            tracks.add (at);

    return tracks;
}

void AudioTrack::playGuideNote (int note, MidiChannel midiChannel, int velocity, bool stopOtherFirst, bool forceNote, bool autorelease)
{
    jassert (midiChannel.isValid()); //SysEx?
    jassert (velocity >= 0 && velocity <= 127);

    if (stopOtherFirst)
        turnOffGuideNotes (midiChannel);

    if (note >= 0 && (forceNote || edit.engine.getEngineBehaviour().shouldPlayMidiGuideNotes()))
    {
        const int pitch = juce::jlimit (0, 127, note);

        if (! currentlyPlayingGuideNotes.contains (pitch))
        {
            currentlyPlayingGuideNotes.add (pitch);
            injectLiveMidiMessage (juce::MidiMessage::noteOn (midiChannel.getChannelNumber(),
                                                              pitch, (uint8_t) velocity),
                                   MidiMessageArray::notMPE);
        }

        if (autorelease)
            startTimer (100);
    }
}

void AudioTrack::playGuideNotes (const juce::Array<int>& notes, MidiChannel midiChannel,
                                 const juce::Array<int>& vels, bool stopOthersFirst)
{
    jassert (midiChannel.isValid()); //SysEx?

    if (stopOthersFirst)
        turnOffGuideNotes (midiChannel);

    if (notes.size() < 8 && edit.engine.getEngineBehaviour().shouldPlayMidiGuideNotes())
    {
        for (int i = 0; i < notes.size(); ++i)
        {
            const int pitch = juce::jlimit (0, 127, notes.getUnchecked (i));

            if (! currentlyPlayingGuideNotes.contains (pitch))
            {
                currentlyPlayingGuideNotes.add (pitch);
                injectLiveMidiMessage (juce::MidiMessage::noteOn (midiChannel.getChannelNumber(),
                                                                  pitch, (uint8_t) vels.getUnchecked (i)),
                                       MidiMessageArray::notMPE);
            }
        }
    }
}

void AudioTrack::turnOffGuideNotes()
{
    stopTimer();
    
    for (int ch = 1; ch <= 16; ch++)
        turnOffGuideNotes (MidiChannel (ch));
}

void AudioTrack::turnOffGuideNotes (MidiChannel midiChannel)
{
    jassert (midiChannel.isValid()); //SysEx?
    auto channel = midiChannel.getChannelNumber();

    for (auto note : currentlyPlayingGuideNotes)
        injectLiveMidiMessage (juce::MidiMessage::noteOff (channel, note),
                               MidiMessageArray::notMPE);

    currentlyPlayingGuideNotes.clear();
}

//==============================================================================
void AudioTrack::addListener (Listener* l)
{
    if (listeners.isEmpty())
        edit.restartPlayback();

    listeners.add (l);
}

void AudioTrack::removeListener (Listener* l)
{
    listeners.remove (l);
    // N.B. Don't call restartPlayback here or it will be impossible to clear the audio graph
}

//==============================================================================
void AudioTrack::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::maxInputs)
        {
            maxInputs.forceUpdateOfCachedValue();
            edit.getEditInputDevices().clearAllInputs (*this);
        }
        else if (i == IDs::ghostTracks)
        {
            if (ghostTracks.get().isEmpty())
                ghostTracks.resetToDefault();

            changed();
        }
        else if (i == IDs::compGroup)
        {
            changed();
        }
        else if (i == IDs::frozen)
        {
            frozen.forceUpdateOfCachedValue();
            changed();
        }
        else if (i == IDs::frozenIndividually)
        {
            frozenIndividually.forceUpdateOfCachedValue();

            if (frozenIndividually)
                freezeTrack();
            else
                unFreezeTrack();
        }
        else if (i == IDs::name)
        {
            auto devName = getName();
            
            waveInputDevice->setAlias (devName);
            midiInputDevice->setAlias (devName);
        }
        else if (i == IDs::midiNoteMap)
        {
            updateMidiNoteMapCache();
        }
    }
    else if (Clip::isClipState (v))
    {
        TRACKTION_ASSERT_MESSAGE_THREAD;

        if (i == IDs::start || i == IDs::length)
            asyncCaller.updateAsync (updateAutoCrossfadesFlag);

        if (i == IDs::mute && bool (v.getProperty (i)))
            if (trackMuter == nullptr && ! bool (v.getProperty (IDs::processMidiWhenMuted, false)))
                if (v.hasType (IDs::MIDICLIP))
                    trackMuter = std::make_unique<TrackMuter> (*this);
    }

    ClipTrack::valueTreePropertyChanged (v, i);
}


//==============================================================================
bool AudioTrack::hasAnyLiveInputs()
{
    for (auto in : edit.getAllInputDevices())
        if (in->isRecordingActive (*this) && in->isOnTargetTrack (*this))
            return true;

    return false;
}

bool AudioTrack::hasAnyTracksFeedingIn()
{
    for (auto t : getAudioTracks (edit))
        if (t != this && t->getOutput().feedsInto (this))
            return true;

    return false;
}

//==============================================================================
void AudioTrack::injectLiveMidiMessage (const MidiMessageArray::MidiMessageWithSource& message)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    bool wasUsed = false;
    listeners.call (&Listener::injectLiveMidiMessage, *this, message, wasUsed);

    if (! wasUsed)
        edit.warnOfWastedMidiMessages (nullptr, this);
}

void AudioTrack::injectLiveMidiMessage (const juce::MidiMessage& m, MidiMessageArray::MPESourceID source)
{
    injectLiveMidiMessage ({ m, source });
}

bool AudioTrack::mergeInMidiSequence (const juce::MidiMessageSequence& original, TimePosition startTime,
                                      MidiClip* mc, MidiList::NoteAutomationType automationType)
{
    auto ms = original;
    ms.addTimeToMessages (startTime.inSeconds());

    const auto start = TimePosition::fromSeconds (ms.getStartTime());
    const auto end = TimePosition::fromSeconds (ms.getEndTime());

    if (mc == nullptr)
    {
        for (auto c : getClips())
        {
            if (c->getPosition().time.overlaps ({ start, end }))
            {
                mc = dynamic_cast<MidiClip*> (c);

                if (mc != nullptr)
                    break;
            }
        }
    }

    if (mc != nullptr)
    {
        auto pos = mc->getPosition();

        if (pos.getStart() > start)
            mc->extendStart (std::max (TimePosition(), start - TimeDuration::fromSeconds (0.1)));

        if (pos.getEnd() < end)
            mc->setEnd (end + TimeDuration::fromSeconds (0.1), true);

        mc->mergeInMidiSequence (ms, automationType);
        return true;
    }

    return false;
}

juce::Array<Track*> AudioTrack::getInputTracks() const
{
    juce::Array<Track*> inputTracks;

    for (auto track : getAudioTracks (edit))
        if (! track->isPartOfSubmix() && track != this && track->getOutput().feedsInto (this))
            inputTracks.add (track);

    for (auto track : getTracksOfType<FolderTrack> (edit, true))
        if (! track->isPartOfSubmix() && track->getOutput() != nullptr && track->getOutput()->feedsInto (this))
            inputTracks.add (track);

    return inputTracks;
}

static bool canTrackBeChanged (InputDeviceInstance* idi)
{
    if (idi->isRecording())
    {
        idi->edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't change tracks whilst recording is active"));
        return false;
    }

    return true;
}

void AudioTrack::setMaxNumOfInputs (int n)
{
    for (auto* idi : edit.getEditInputDevices().getDevicesForTargetTrack (*this))
        if (! canTrackBeChanged (idi))
            return;

    maxInputs = n;
}

//==============================================================================
bool AudioTrack::isFrozen (FreezeType t) const
{
    return t == anyFreeze ? (frozen || frozenIndividually)
                          : (t == groupFreeze ? frozen : frozenIndividually);
}

void AudioTrack::setFrozen (bool b, FreezeType type)
{
    if (type == individualFreeze)
    {
        if (frozenIndividually != b)
        {
            if (b && getOutput().getDestinationTrack() != nullptr)
            {
                edit.engine.getUIBehaviour().showWarningMessage (TRANS("Tracks which output to another track can't themselves be frozen; "
                                                                       "instead, you should freeze the track they input into."));
            }
            else
            {
                frozenIndividually = b;
            }
        }
    }
    else
    {
        if (frozen != b)
        {
            if (! edit.isLoading())
            {
                const auto outputsToSubmixTrack = [this]
                {
                    if (auto folder = getParentFolderTrack())
                        return folder->isSubmixFolder();
                        
                    return false;
                };
                
                if (b && (getOutput().getDestinationTrack() != nullptr || outputsToSubmixTrack()))
                {
                    edit.engine.getUIBehaviour().showWarningMessage (TRANS("Tracks which output to another track can't themselves be frozen; "
                                                                           "instead, you should freeze the track they input into."));
                }
                else
                {
                    frozen = b;
                }
            }
        }
    }
}

bool AudioTrack::canContainPlugin (Plugin* p) const
{
    const bool isFreezePoint = dynamic_cast<FreezePointPlugin*> (p) != nullptr;

    return dynamic_cast<VCAPlugin*> (p) == nullptr
            && (! isFreezePoint
                 || (isFreezePoint && (p->getOwnerTrack() == this || ! hasFreezePointPlugin())));
}

//==============================================================================
AudioTrack::FreezePointRemovalInhibitor::FreezePointRemovalInhibitor (AudioTrack& at) : track (at)  { ++track.freezePointRemovalInhibitor; }
AudioTrack::FreezePointRemovalInhibitor::~FreezePointRemovalInhibitor()                             { --track.freezePointRemovalInhibitor; }

void AudioTrack::freezeTrack()
{
    insertFreezePointIfRequired();
    const FreezePointPlugin::ScopedPluginDisabler spd (*this, juce::Range<int> (getIndexOfFreezePoint(),
                                                                                pluginList.size()));

    auto& dm = edit.engine.getDeviceManager();

    const bool shouldBeMuted = isMuted (true);
    setMute (false);
    const FreezePointPlugin::ScopedTrackUnsoloer stu (edit);

    juce::BigInteger trackNum;
    trackNum.setBit (getIndexInEditTrackList());
    auto freezeFile = getFreezeFile();
    freezeFile.deleteFile();

    juce::Array<EditItemID> trackIDs { itemID };
    juce::Array<Clip*> clips (getClips());

    for (auto inputTrack : getInputTracks())
    {
        trackIDs.addIfNotAlreadyThere (inputTrack->itemID);

        if (auto ct = dynamic_cast<ClipTrack*> (inputTrack))
            clips.addArray (ct->getClips());
    }

    Renderer::Parameters r (edit);
    r.tracksToDo = trackNum;
    r.destFile = freezeFile;
    r.audioFormat = edit.engine.getAudioFileFormatManager().getFrozenFileFormat();
    r.blockSizeForAudio = dm.getBlockSize();
    r.sampleRateForAudio = dm.getSampleRate();
    r.time = { {}, getLengthIncludingInputTracks() };
    r.endAllowance = RenderOptions::findEndAllowance (edit, &trackIDs, nullptr);
    r.canRenderInMono = true;
    r.mustRenderInMono = false;
    r.usePlugins = true;
    r.useMasterPlugins = false;
    r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (edit.engine);
    r.category = ProjectItem::Category::frozen;

    const Edit::ScopedRenderStatus srs (edit, true);
    const auto desc = TRANS("Creating track freeze for \"XDVX\"")
                        .replace ("XDVX", getName()) + "...";
    
    if (r.engine->getProjectManager().getProject (edit) != nullptr)
        Renderer::renderToProjectItem (desc, r);
    else
        Renderer::renderToFile (desc, r);

    freezePlugins (juce::Range<int> (0, getIndexOfFreezePoint()));
    setMute (shouldBeMuted);

    if (! r.destFile.existsAsFile())
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Nothing to freeze"));
        setFrozen (false, individualFreeze);
        return;
    }

    changed();
}

int AudioTrack::getIndexOfFreezePoint()
{
    int i = 0;

    for (auto p : pluginList)
    {
        if (dynamic_cast<FreezePointPlugin*> (p) != nullptr)
            return i;

        ++i;
    }

    return -1;
}

void AudioTrack::insertFreezePointAfterPlugin (const Plugin::Ptr& p)
{
    auto& pl = pluginList;

    if (! pl.getPlugins().contains (p.get()))
        return;

    removeFreezePoint();
    pl.insertPlugin (FreezePointPlugin::create(), pl.getPlugins().indexOf (p.get()) + 1);

    edit.dispatchPendingUpdatesSynchronously();
    // need to force the audio device to update before we start the render
}

void AudioTrack::removeFreezePoint()
{
    auto& pl = pluginList;

    for (int i = pl.size(); --i >= 0;)
        if (auto f = dynamic_cast<FreezePointPlugin*> (pl[i]))
            f->deleteFromParent();
}

bool AudioTrack::insertFreezePointIfRequired()
{
    if (getIndexOfFreezePoint() != -1)
        return false;

    if (auto p = pluginList.insertPlugin (FreezePointPlugin::create(), getIndexOfDefaultFreezePoint()))
        auto freezer = FreezePointPlugin::createTrackFreezer (p);

    edit.dispatchPendingUpdatesSynchronously();
    // need to force the audio device to update before we start the render

    return true;
}

void AudioTrack::freezeTrackAsync() const
{
    freezeUpdater->freeze();
}

int AudioTrack::getIndexOfDefaultFreezePoint()
{
    int position = edit.engine.getPropertyStorage().getProperty (SettingID::freezePoint, 1);

    if (position == FreezePointPlugin::beforeAllPlugins)
        return 0;

    const auto& pl = pluginList;

    for (int i = 0; i < pl.size(); ++i)
    {
        if (dynamic_cast<VolumeAndPanPlugin*> (pl[i]) != nullptr)
        {
            if (position == FreezePointPlugin::preFader)
                return i;

            if (position == FreezePointPlugin::postFader)
                return i + 1;
        }
    }

    return -1;
}

void AudioTrack::freezePlugins (juce::Range<int> pluginsToFreeze)
{
    int i = 0;

    for (auto p : pluginList)
        p->setFrozen (pluginsToFreeze.contains (i++));
}

void AudioTrack::unFreezeTrack()
{
    // Remove the freeze point if it's in the default location as it will be put back there anyway
    int defaultPosition = edit.engine.getPropertyStorage().getProperty (SettingID::freezePoint, 0);
    auto defaultIndex = getIndexOfDefaultFreezePoint();
    auto freezePosition = (defaultPosition == FreezePointPlugin::postFader) ? defaultIndex
                                                                            : std::max (0, defaultIndex - 1);

    if (getIndexOfFreezePoint() == freezePosition)
        if (freezePointRemovalInhibitor == 0)
            if (auto f = dynamic_cast<FreezePointPlugin*> (pluginList[freezePosition]))
                f->deleteFromParent();

    // Unfreeze all plugins
    freezePlugins ({});

    changed();
}

juce::File AudioTrack::getFreezeFile() const
{
    return TemporaryFileManager::getFreezeFileForTrack (*this);
}

bool AudioTrack::isSidechainSource() const
{
    for (auto p : edit.getPluginCache().getPlugins())
        if (p->getSidechainSourceID() == itemID)
            return true;

    return false;
}

juce::Array<Track*> AudioTrack::findSidechainSourceTracks() const
{
    juce::Array<Track*> srcTracks;

    for (auto p : getAllPlugins())
    {
        auto srcId = p->getSidechainSourceID();

        if (srcId.isValid())
            if (auto srcTrack = findTrackForID (edit, srcId))
                srcTracks.addIfNotAlreadyThere (srcTrack);
    }

    return srcTracks;
}

}} // namespace tracktion { inline namespace engine
