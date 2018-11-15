/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct LiveMidiInjectingAudioNode  : public SingleInputAudioNode
{
    LiveMidiInjectingAudioNode (AudioTrack* t, AudioNode* source)
        : SingleInputAudioNode (source), track (t)
    {
    }

    ~LiveMidiInjectingAudioNode()
    {
        removeFromList();
    }

    void removeFromList()
    {
        const ScopedLock sl (activeNodeLock);
        activeNodes.removeAllInstancesOf (this);
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        SingleInputAudioNode::prepareAudioNodeToPlay (info);

        const ScopedLock sl (activeNodeLock);
        activeNodes.addIfNotAlreadyThere (this);

        // if this node is taking over from a previous one, also take over its live message list
        // to avoid these messages getting lost
        for (auto n : activeNodes)
        {
            if (n->track == track && this != n)
            {
                const ScopedLock sl2 (n->liveMidiLock);
                liveMidiMessages.swapWith (n->liveMidiMessages);
                midiSourceID = n->midiSourceID;
            }
        }
    }

    void releaseAudioNodeResources() override
    {
        SingleInputAudioNode::releaseAudioNodeResources();
        removeFromList();
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        if (rc.bufferForMidiMessages != nullptr && ! liveMidiMessages.isEmpty())
            callRenderAdding (rc);
        else
            input->renderOver (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (rc.bufferForMidiMessages != nullptr && ! liveMidiMessages.isEmpty())
        {
            MidiMessageArray messages;

            {
                const ScopedLock sl (liveMidiLock);
                messages.swapWith (liveMidiMessages);
            }

            rc.bufferForMidiMessages->mergeFromAndClear (messages);
        }

        input->renderAdding (rc);
    }

    void injectMessage (MidiMessageArray::MidiMessageWithSource mm)
    {
        mm.setTimeStamp (0.0);

        const ScopedLock sl (liveMidiLock);
        liveMidiMessages.add (std::move (mm));
    }

    static bool injectMidiMessageForTrack (AudioTrack* targetTrack, const MidiMessageArray::MidiMessageWithSource& mm)
    {
        const ScopedLock sl (activeNodeLock);

        for (auto node : activeNodes)
        {
            if (node->track == targetTrack)
            {
                node->injectMessage (mm);
                return true;
            }
        }

        return false;
    }

private:
    AudioTrack* track;

    CriticalSection liveMidiLock;
    MidiMessageArray liveMidiMessages;
    static juce::Array<LiveMidiInjectingAudioNode*> activeNodes;
    static CriticalSection activeNodeLock;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveMidiInjectingAudioNode)
};

juce::Array<LiveMidiInjectingAudioNode*> LiveMidiInjectingAudioNode::activeNodes;
CriticalSection LiveMidiInjectingAudioNode::activeNodeLock;

//==============================================================================
struct AudioTrack::LiveMidiOutputAudioNode  : public SingleInputAudioNode,
                                              private AsyncUpdater
{
    LiveMidiOutputAudioNode (AudioTrack& t, AudioNode* source)
        : SingleInputAudioNode (source), track (t), trackPtr (&t)
    {
        pendingMessages.reserve (50);
        dispatchingMessages.reserve (50);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        if (rc.bufferForMidiMessages != nullptr)
            callRenderAdding (rc);
        else
            input->renderOver (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        input->renderAdding (rc);

        const ScopedLock sl (lock);

        if (rc.bufferForMidiMessages != nullptr)
            for (auto& m : *rc.bufferForMidiMessages)
                pendingMessages.add (m);

        if (! pendingMessages.isEmpty())
            triggerAsyncUpdate();
    }

private:
    AudioTrack& track;
    Track::Ptr trackPtr;

    CriticalSection lock;
    MidiMessageArray pendingMessages, dispatchingMessages;

    void handleAsyncUpdate() override
    {
        {
            const ScopedLock sl (lock);
            pendingMessages.swapWith (dispatchingMessages);
        }

        for (auto& m : dispatchingMessages)
            track.listeners.call (&AudioTrack::Listener::recordedMidiMessageSentToPlugins, track, m);

        dispatchingMessages.clear();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveMidiOutputAudioNode)
};

//==============================================================================
struct AudioTrack::TrackMuter : private AsyncUpdater
{
    TrackMuter (AudioTrack& at) : owner (at)        { triggerAsyncUpdate(); }
    ~TrackMuter()                                   { cancelPendingUpdate(); }

    void handleAsyncUpdate() override
    {
        for (int i = 1; i <= 16; ++i)
            owner.injectLiveMidiMessage (MidiMessage::allNotesOff (i), MidiMessageArray::notMPE);

        owner.trackMuter = nullptr;
    }

    AudioTrack& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackMuter)
};

//==============================================================================
struct AudioTrack::FreezeUpdater : private ValueTreeAllEventListener,
                                   private AsyncUpdater
{
    FreezeUpdater (AudioTrack& at)
        : owner (at), state (owner.state), triggerFreeze (false), updateFreeze (false)
    {
        state.addListener (this);
    }

    ~FreezeUpdater()
    {
        state.removeListener (this);
        cancelPendingUpdate();
    }

    void freeze()
    {
        markAndUpdate (triggerFreeze);
    }

    AudioTrack& owner;
    ValueTree state;

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
AudioTrack::AudioTrack (Edit& edit, const ValueTree& v)
    : ClipTrack (edit, v, 50, 13, 2000)
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

    std::vector<ChannelIndex> channels = { ChannelIndex (0, juce::AudioChannelSet::left),
                                           ChannelIndex (1, juce::AudioChannelSet::right) };

    callBlocking ([this, &edit, &channels]
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
    const String n = ClipTrack::getName();

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
    const String devName (getName());
    if (waveInputDevice != nullptr) waveInputDevice->setAlias (devName);
    if (midiInputDevice != nullptr) midiInputDevice->setAlias (devName);
}

String AudioTrack::getName()
{
    String n = ClipTrack::getName();

    if (n.isEmpty())
        n << TRANS("Track") << ' ' << getAudioTrackNumber();

    return n;
}

int AudioTrack::getAudioTrackNumber() noexcept
{
    return getAudioTracks (edit).indexOf (this) + 1;
}

VolumeAndPanPlugin* AudioTrack::getVolumePlugin()     { return pluginList.findFirstPluginOfType<VolumeAndPanPlugin>(); }
LevelMeterPlugin* AudioTrack::getLevelMeterPlugin()   { return pluginList.findFirstPluginOfType<LevelMeterPlugin>(); }
EqualiserPlugin* AudioTrack::getEqualiserPlugin()     { return pluginList.findFirstPluginOfType<EqualiserPlugin>(); }

AuxSendPlugin* AudioTrack::getAuxSendPlugin (int bus)
{
    for (auto p : pluginList)
        if (auto f = dynamic_cast<AuxSendPlugin*> (p))
            if (bus < 0 || bus == f->getBusNumber())
                return f;

    return {};
}

//==============================================================================
String AudioTrack::getNameForMidiNoteNumber (int note, int midiChannel) const
{
    jassert (midiChannel > 0);
    String s;

    for (auto af : pluginList)
        if (af->hasNameForMidiNoteNumber (note, midiChannel, s))
            return s;

    if (auto dest = output->getDestinationTrack())
        return dest->getNameForMidiNoteNumber (note, midiChannel);

    // try the master plugins..
    for (auto af : edit.getMasterPluginList())
        if (af->hasNameForMidiNoteNumber (note, midiChannel, s))
            return s;

    if (auto mo = dynamic_cast<MidiOutputDevice*> (getOutput().getOutputDevice (true)))
        return mo->getNameForMidiNoteNumber (note, midiChannel);

    return midiChannel == 10 ? TRANS(MidiMessage::getRhythmInstrumentName (note))
                             : MidiMessage::getMidiNoteName (note, true, true,
                                                             edit.engine.getEngineBehaviour().getMiddleCOctave());
}

bool AudioTrack::areMidiPatchesZeroBased() const
{
    String s;

    // do something for plugins here
    if (auto dest = output->getDestinationTrack())
        return dest->areMidiPatchesZeroBased();

    // try the master plugins..
    if (auto midiDevice = dynamic_cast<MidiOutputDevice*> (output->getOutputDevice (false)))
        return midiDevice->areMidiPatchesZeroBased();

    return false;
}

String AudioTrack::getNameForBank (int bank) const
{
    String s;

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

String AudioTrack::getNameForProgramNumber (int programNumber, int bank) const
{
    String s;

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

    return TRANS(MidiMessage::getGMInstrumentName (programNumber));
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
        if (auto p = getParentFolderTrack())
            return p->isMuted (true);

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

        if (auto dest = output->getDestinationTrack())
            return dest->isSoloIsolate (true);
    }

    return false;
}

static bool isInputTrackSolo (const AudioTrack& track)
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
String AudioTrack::getTrackPlayabilityWarning() const
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
    visibleProp = jlimit (0.0, 1.0, visibleProp);
    midiVerticalOffset = jlimit (0.0, 1.0 - visibleProp, offset);
    midiVisibleProportion = jlimit (0.1, 1.0 - midiVerticalOffset, visibleProp);
}

void AudioTrack::setVerticalScaleToDefault()
{
    const int midiNotes = jlimit (1, 128, 12 * static_cast<int> (edit.engine.getPropertyStorage().getProperty (SettingID::midiEditorOctaves, 3)));
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

void AudioTrack::playGuideNote (int note, MidiChannel midiChannel, int velocity, bool stopOtherFirst, bool forceNote)
{
    jassert (midiChannel.isValid()); //SysEx?
    jassert (velocity >= 0 && velocity <= 127);

    if (stopOtherFirst)
        turnOffGuideNotes (midiChannel);

    if (note >= 0 && (forceNote || edit.engine.getEngineBehaviour().shouldPlayMidiGuideNotes()))
    {
        const int pitch = jlimit (0, 127, note);

        if (! currentlyPlayingGuideNotes.contains (pitch))
        {
            currentlyPlayingGuideNotes.add (pitch);
            injectLiveMidiMessage (MidiMessage::noteOn (midiChannel.getChannelNumber(), pitch, (uint8) velocity),
                                   MidiMessageArray::notMPE);
        }
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
            const int pitch = jlimit (0, 127, notes.getUnchecked (i));

            if (! currentlyPlayingGuideNotes.contains (pitch))
            {
                currentlyPlayingGuideNotes.add (pitch);
                injectLiveMidiMessage (MidiMessage::noteOn (midiChannel.getChannelNumber(), pitch, (uint8) vels.getUnchecked (i)),
                                       MidiMessageArray::notMPE);
            }
        }
    }
}

void AudioTrack::turnOffGuideNotes()
{
    for (int ch = 1; ch <= 16; ch++)
        turnOffGuideNotes (MidiChannel (ch));
}

void AudioTrack::turnOffGuideNotes (MidiChannel midiChannel)
{
    jassert (midiChannel.isValid()); //SysEx?
    auto channel = midiChannel.getChannelNumber();

    for (auto note : currentlyPlayingGuideNotes)
        injectLiveMidiMessage (MidiMessage::noteOff (channel, note), MidiMessageArray::notMPE);

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

    if (listeners.isEmpty())
        edit.restartPlayback();
}

//==============================================================================
void AudioTrack::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
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
            const String devName (getName());
            waveInputDevice->setAlias (devName);
            midiInputDevice->setAlias (devName);
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
static CombiningAudioNode* createClipCombiner (const CreateAudioNodeParams& params,
                                               const juce::Array<Clip*>& clips)
{
    auto combiner = new CombiningAudioNode();

    for (auto clip : clips)
        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
            combiner->addInput (clip->getPosition().time, clip->createAudioNode (params));

    return combiner;
}

bool AudioTrack::canUseBufferedAudioNode()
{
    // We can only use a buffered version if we don't have any live inputs or any plugins that share buffers

    if (edit.engine.getDeviceManager().getInternalBufferMultiplier() == 1)
        return false;

    if (waveInputDevice->isEnabled() || midiInputDevice->isEnabled())
        return false;

    return ! pluginList.needsConstantBufferSize();
}

static void addTrackInputs (const CreateAudioNodeParams& params,
                            MixerAudioNode& mixer, const juce::Array<AudioTrack*>& inputTracks)
{
    for (auto t : inputTracks)
        if (t->isProcessing (true))
            mixer.addInput (t->createAudioNode (params));
}

static juce::Array<AudioNode*> createLiveInputs (AudioTrack& track, const CreateAudioNodeParams& params)
{
    juce::Array<AudioNode*> destArray;

    if (! params.forRendering)
        if (auto context = track.edit.getCurrentPlaybackContext())
            for (auto in : context->getAllInputs())
                if (in->isLivePlayEnabled() && in->getTargetTrack() == &track)
                    if (auto node = in->createLiveInputNode())
                        destArray.add (node);

    return destArray;
}

static AudioNode* createFilterAudioNode (AudioTrack& track, AudioNode* node, const CreateAudioNodeParams& params)
{
    auto modList = track.getModifierList().getModifiers();

    for (auto mod : modList)
        node = mod->createPreFXAudioNode (node);

    node = track.pluginList.createAudioNode (node, params.addAntiDenormalisationNoise);

    for (auto mod : modList)
        node = mod->createPostFXAudioNode (node);

    return node;
}

AudioNode* AudioTrack::createAudioNode (const CreateAudioNodeParams& params)
{
    CRASH_TRACER

    if (! params.forRendering && frozenIndividually)
        return createFreezeAudioNode (params.addAntiDenormalisationNoise);

    const auto& clips = getClips();

    std::unique_ptr<CombiningAudioNode> clipCombiner (createClipCombiner (params, clips));
    auto liveNodes = createLiveInputs (*this, params);

    juce::Array<AudioTrack*> inputTracks;

    for (auto t : getAudioTracks (edit))
        if (t != this && t->createsOutput() && t->getOutput().outputsToDestTrack (*this))
            inputTracks.add (t);

    AudioNode* node = nullptr;

    const bool use64Bit = (params.forRendering || edit.engine.getPropertyStorage().getProperty (SettingID::use64Bit, false))
                            && inputTracks.size() > 1; // no point using 64 bit unless there's a lot of tracks to sum up

    const bool shouldUseMultiCPU = inputTracks.size() > 1 && edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() > 1;

    if (clips.size() > 0)
    {
        CRASH_TRACER
        AudioNode* clipsNode = new PlayHeadAudioNode (clipCombiner.release());

        {
            juce::Array<AudioClipBase*> araClips;

            for (int i = clips.size(); --i >= 0;)
                if (auto acb = dynamic_cast<AudioClipBase*> (clips.getUnchecked (i)))
                    if (acb->isUsingMelodyne() && acb->melodyneProxy != nullptr)
                        araClips.add (acb);

            if (araClips.size() > 0)
            {
                auto mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);
                mixer->addInput (clipsNode);

                for (auto acb : araClips)
                    if (auto araNode = acb->createMelodyneAudioNode())
                        mixer->addInput (araNode);

                clipsNode = mixer;
            }
        }

        for (int i = clips.size(); --i >= 0;)
            if (auto pl = clips.getUnchecked (i)->getPluginList())
                clipsNode = pl->attachNodesForPluginsNeedingLivePlay (clipsNode);

        if (compGroup != -1)
            if (auto tc = edit.getTrackCompManager().getTrackComp (this))
                clipsNode = tc->createAudioNode (*this, clipsNode);

        if (! listeners.isEmpty())
            clipsNode = new LiveMidiOutputAudioNode (*this, clipsNode);

        clipsNode = new LiveMidiInjectingAudioNode (this, clipsNode);

        if (liveNodes.size() > 0)
        {
            if (inputTracks.size() > 0)
            {
                if (! params.forRendering)
                    clipsNode = new TrackMutingAudioNode (*this, clipsNode, true);

                auto mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);
                mixer->addInput (clipsNode);

                addTrackInputs (params, *mixer, inputTracks);

                node = new TrackMutingAudioNode (*this, mixer, false);
            }
            else
            {
                node = new TrackMutingAudioNode (*this, clipsNode, ! params.forRendering);
            }

            auto mixer = new MixerAudioNode (use64Bit, false);
            mixer->addInput (node);

            for (int i = liveNodes.size(); --i >= 0;)
                mixer->addInput (liveNodes.getUnchecked (i));

            if (params.includePlugins)
                node = createFilterAudioNode (*this, mixer, params);
            else
                node = mixer;

            if (isSidechainSource())
                node = new SidechainSendAudioNode (node, itemID);

            node = new TrackMutingAudioNode (*this, node, false);
        }
        else
        {
            if (inputTracks.size() > 0)
            {
                if (! params.forRendering)
                    clipsNode = new TrackMutingAudioNode (*this, clipsNode, true);

                auto mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);
                mixer->addInput (clipsNode);

                addTrackInputs (params, *mixer, inputTracks);
                node = mixer;
            }
            else
            {
                node = clipsNode;
            }

            if (params.includePlugins)
                node = createFilterAudioNode (*this, node, params);

            if (isSidechainSource())
                node = new SidechainSendAudioNode (node, itemID);

            // if it's rendering we can't be recording anyway
            node = new TrackMutingAudioNode (*this, node, ! params.forRendering && inputTracks.isEmpty());
        }
    }
    else
    {
        CRASH_TRACER

        if (liveNodes.size() > 0)
        {
            MixerAudioNode* mixer = nullptr;

            if (inputTracks.size() > 0)
            {
                mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);

                addTrackInputs (params, *mixer, inputTracks);

                node = new TrackMutingAudioNode (*this, mixer, false);

                mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);
                mixer->addInput (node);
            }
            else
            {
                mixer = new MixerAudioNode (use64Bit, false);
            }

            mixer->addInput (new PlayHeadAudioNode (clipCombiner.release()));

            for (int i = liveNodes.size(); --i >= 0;)
                mixer->addInput (liveNodes[i]);

            if (params.includePlugins)
                node = createFilterAudioNode (*this, mixer, params);
            else
                node = mixer;

            if (isSidechainSource())
                node = new SidechainSendAudioNode (node, itemID);

            node = new TrackMutingAudioNode (*this, node, false);
        }
        else
        {
            if (inputTracks.size() > 0)
            {
                auto mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);

                addTrackInputs (params, *mixer, inputTracks);

                if (params.includePlugins)
                    node = createFilterAudioNode (*this, mixer, params);
                else
                    node = mixer;

                node = new TrackMutingAudioNode (*this, node, false);
            }
            else
            {
                node = new MixerAudioNode (false, false);

                if (params.includePlugins)
                {
                    node = createFilterAudioNode (*this, node, params);

                    if (isSidechainSource())
                        node = new SidechainSendAudioNode (node, itemID);

                    node = new TrackMutingAudioNode (*this, node, false);
                }
                else
                {
                    if (isSidechainSource())
                        node = new SidechainSendAudioNode (node, itemID);
                }
            }
        }
    }

    if (! params.forRendering)
    {
        if (waveInputDevice->isEnabled())
            node = waveInputDevice->createWaveInputDeviceNode (node);

        if (midiInputDevice->isEnabled())
            node = midiInputDevice->createMidiEventSnifferNode (node);

        if (liveNodes.isEmpty() && inputTracks.isEmpty() && canUseBufferedAudioNode())
            node = new BufferingAudioNode (node, edit.engine.getDeviceManager().getInternalBufferSize());
    }

    return node;
}

//==============================================================================
bool AudioTrack::hasAnyLiveInputs()
{
    for (auto in : edit.getAllInputDevices())
        if (in->isRecordingActive() && this == in->getTargetTrack())
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
    if (! LiveMidiInjectingAudioNode::injectMidiMessageForTrack (this, message))
        edit.warnOfWastedMidiMessages (nullptr, this);
}

void AudioTrack::injectLiveMidiMessage (const juce::MidiMessage& m, MidiMessageArray::MPESourceID source)
{
    injectLiveMidiMessage ({ m, source });
}

bool AudioTrack::mergeInMidiSequence (const MidiMessageSequence& original, double startTime,
                                      MidiClip* mc, MidiList::NoteAutomationType automationType)
{
    auto ms = original;
    ms.addTimeToMessages (startTime);

    auto start = ms.getStartTime();
    auto end = ms.getEndTime();

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
            mc->extendStart (jmax (0.0, start - 0.1));

        if (pos.getEnd() < end)
            mc->setEnd (end + 0.1, true);

        mc->mergeInMidiSequence (ms, automationType);
        return true;
    }

    return false;
}

juce::Array<AudioTrack*> AudioTrack::getInputTracks() const
{
    juce::Array<AudioTrack*> inputTracks;

    for (auto track : getAudioTracks (edit))
        if (track != this && track->getOutput().feedsInto (this))
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
                if (b && getOutput().getDestinationTrack() != nullptr)
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
    const FreezePointPlugin::ScopedPluginDisabler spd (*this, Range<int> (getIndexOfFreezePoint(),
                                                                          pluginList.size()));

    auto& dm = edit.engine.getDeviceManager();

    const bool shouldBeMuted = isMuted (true);
    setMute (false);
    const FreezePointPlugin::ScopedTrackUnsoloer stu (edit);

    BigInteger trackNum;
    trackNum.setBit (getIndexInEditTrackList());
    auto freezeFile = getFreezeFile();
    freezeFile.deleteFile();

    Renderer::Parameters r (edit);
    r.tracksToDo = trackNum;
    r.destFile = freezeFile;
    r.audioFormat = edit.engine.getAudioFileFormatManager().getFrozenFileFormat();
    r.blockSizeForAudio = dm.getBlockSize();
    r.sampleRateForAudio = dm.getSampleRate();
    r.time = { 0.0, getLengthIncludingInputTracks() };
    r.canRenderInMono = true;
    r.mustRenderInMono = false;
    r.usePlugins = true;
    r.useMasterPlugins = false;
    r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (edit.engine);

    const Edit::ScopedRenderStatus srs (edit);
    auto renderedItem = Renderer::renderToProjectItem (TRANS("Creating track freeze for \"XDVX\"")
                                                        .replace ("XDVX", getName()) + "...", r);

    freezePlugins (Range<int> (0, getIndexOfFreezePoint()));
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
        const ScopedPointer<FreezePointPlugin::ScopedTrackFreezer> freezer (FreezePointPlugin::createTrackFreezer (p));

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

void AudioTrack::freezePlugins (Range<int> pluginsToFreeze)
{
    int i = 0;

    for (auto p : pluginList)
        p->setFrozen (pluginsToFreeze.contains (i++));
}

void AudioTrack::unFreezeTrack()
{
    // Remove the freeze point if it's in the default location as it will be put back there anyway
    const int defaultPosition = edit.engine.getPropertyStorage().getProperty (SettingID::freezePoint, 0);
    const int defaultIndex = getIndexOfDefaultFreezePoint();
    const int freezePosition = defaultPosition == FreezePointPlugin::postFader ? defaultIndex
                                                                               : jmax (0, defaultIndex - 1);

    if (getIndexOfFreezePoint() == freezePosition)
        if (freezePointRemovalInhibitor == 0)
            if (auto f = dynamic_cast<FreezePointPlugin*> (pluginList[freezePosition]))
                f->deleteFromParent();

    // Unfreeze all plugins
    freezePlugins ({});

    changed();
}

File AudioTrack::getFreezeFile() const noexcept
{
    // TODO: unify proxy filename handling
    return edit.getTempDirectory (true)
             .getChildFile (getTrackFreezePrefix() + "0_" + itemID.toString() + ".freeze");
}

AudioNode* AudioTrack::createFreezeAudioNode (bool addAntiDenormalisationNoise)
{
    AudioNode* node = new WaveAudioNode (AudioFile (getFreezeFile()),
                                         { 0.0, getLengthIncludingInputTracks() },
                                         0.0, {}, {},
                                         1.0, AudioChannelSet::stereo());

    node = pluginList.createAudioNode (node, addAntiDenormalisationNoise);

    if (isSidechainSource())
        node = new SidechainSendAudioNode (node, itemID);

    node = new TrackMutingAudioNode (*this, node, false);

    return node;
}

bool AudioTrack::isSidechainSource() const
{
    for (auto p : tracktion_engine::getAllPlugins (edit, false))
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
