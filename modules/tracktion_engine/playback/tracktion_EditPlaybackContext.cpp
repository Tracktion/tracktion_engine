/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


std::function<AudioNode*(AudioNode*)> EditPlaybackContext::insertOptionalLastStageNode = [] (AudioNode* input)    { return input; };

//==============================================================================
EditPlaybackContext::ScopedDeviceListReleaser::ScopedDeviceListReleaser (EditPlaybackContext& e, bool reallocate)
    : owner (e), shouldReallocate (reallocate)
{
    // Do this here to avoid creating the audio nodes upon reallocation causing a deadlock upon plugin initialisation
    if (reallocate)
        owner.isAllocated = false;

    owner.releaseDeviceList();
}

EditPlaybackContext::ScopedDeviceListReleaser::~ScopedDeviceListReleaser()
{
    if (shouldReallocate)
        owner.rebuildDeviceList();
}

//==============================================================================
EditPlaybackContext::EditPlaybackContext (TransportControl& tc)
    : edit (tc.edit), transport (tc)
{
    rebuildDeviceList();

    if (edit.shouldPlay())
        edit.engine.getDeviceManager().addContext (this);
}

EditPlaybackContext::~EditPlaybackContext()
{
    edit.engine.getDeviceManager().removeContext (this);

    clearNodes();
    midiDispatcher.setMidiDeviceList (OwnedArray<MidiOutputDeviceInstance>());
}

void EditPlaybackContext::releaseDeviceList()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    const ScopedValueSetter<bool> alocateSetter (isAllocated, isAllocated);
    clearNodes();
    midiDispatcher.setMidiDeviceList (OwnedArray<MidiOutputDeviceInstance>());

    // Clear the outputs before the inputs as the midi inputs will be referenced by the MidiDeviceInstanceBase::InputAudioNode
    waveOutputs.clear();
    midiOutputs.clear();
    waveInputs.clear();
    midiInputs.clear();
}

void EditPlaybackContext::rebuildDeviceList()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    jassert (waveInputs.isEmpty() && midiInputs.isEmpty()
              && midiOutputs.isEmpty() && waveOutputs.isEmpty());

    auto& dm = edit.engine.getDeviceManager();

    for (auto mo : dm.midiOutputs)
        if (mo->isEnabled())
            midiOutputs.add (mo->createInstance (*this));

    for (auto mi : dm.midiInputs)
        if (mi->isEnabled() && mi->isAvailableToEdit())
            midiInputs.add (mi->createInstance (*this));

    for (auto wo : dm.waveOutputs)
        if (wo->isEnabled())
            waveOutputs.add (wo->createInstance (*this));

    for (auto wi : dm.waveInputs)
        if (wi->isEnabled() && wi->isAvailableToEdit())
            waveInputs.add (wi->createInstance (*this));

    for (auto* at : getAudioTracks (edit))
    {
        auto& twid = at->getWaveInputDevice();
        auto& tmid = at->getMidiInputDevice();

        if (twid.isEnabled())  waveInputs.add (twid.createInstance (*this));
        if (tmid.isEnabled())  midiInputs.add (tmid.createInstance (*this));
    }

    midiDispatcher.setMidiDeviceList (midiOutputs);

    if (isAllocated)
        reallocate();
}

void EditPlaybackContext::removeInstanceForDevice (InputDevice& device)
{
    // Keep the instances alive until after reallocating because the nodes will refer to them
    juce::OwnedArray<InputDeviceInstance> removedInstances;

    for (int i = midiInputs.size(); --i >= 0;)
        if (&midiInputs.getUnchecked (i)->owner == &device)
            removedInstances.add (midiInputs.removeAndReturn (i));

    for (int i = waveInputs.size(); --i >= 0;)
        if (&waveInputs.getUnchecked (i)->owner == &device)
            removedInstances.add (waveInputs.removeAndReturn (i));

    if (! removedInstances.isEmpty() && isAllocated)
        reallocate();
}

void EditPlaybackContext::addWaveInputDeviceInstance (InputDevice& device)
{
    jassert (getInputFor (&device) == nullptr);

    if (waveInputs.add (device.createInstance (*this)) != nullptr && isAllocated)
        reallocate();
}

void EditPlaybackContext::addMidiInputDeviceInstance (InputDevice& device)
{
    jassert (getInputFor (&device) == nullptr);

    if (midiInputs.add (device.createInstance (*this)) != nullptr && isAllocated)
        reallocate();
}

void EditPlaybackContext::clearNodes()
{
    CRASH_TRACER

    for (auto mo : midiOutputs)
        mo->stop();

    for (auto mi : midiInputs)
        mi->stop();

    for (auto wi : waveInputs)
        wi->stop();

    priorityBooster = nullptr;

    // mustn't delete any of the nodes until all have been removed from
    // the graph, because there could be interdependencies between some
    // of them, e.g. aux sends
    OwnedArray<AudioNode> removedNodes;

    for (auto mo : midiOutputs)
        removedNodes.add (mo->replaceAudioNode (nullptr));

    for (auto wo : waveOutputs)
        removedNodes.add (wo->replaceAudioNode (nullptr));

    removedNodes.clear();
    isAllocated = false;
}

static AudioNode* prepareNode (AudioNode* node, bool forMidi)
{
    if (node != nullptr)
    {
        {
            CRASH_TRACER
            node->purgeSubNodes (! forMidi, forMidi);
        }

        {
            CRASH_TRACER
            AudioNodeProperties info;
            node->getAudioNodeProperties (info);
        }
    }

    return node;
}

static void prepareNodesToPlay (Engine& engine, const Array<AudioNode*>& allNodes, double startTime, PlayHead& playhead)
{
    Array<AudioNode*> nonNullNodes (allNodes);
    nonNullNodes.removeAllInstancesOf (nullptr);

    auto& dm = engine.getDeviceManager();

    PlaybackInitialisationInfo info =
    {
        startTime,
        dm.getSampleRate(),
        dm.getBlockSize(),
        &nonNullNodes,
        playhead
    };

    CRASH_TRACER

    for (auto n : allNodes)
        if (n != nullptr)
            n->prepareAudioNodeToPlay (info);
}

static AudioNode* createPlaybackAudioNode (Edit& edit, OutputDeviceInstance& deviceInstance,
                                           std::function<AudioNode*(AudioNode*)> insertOptionalLastStageNode,
                                           bool addAntiDenormalisationNoise)
{
    CRASH_TRACER
    OutputDevice& device = deviceInstance.owner;
    auto& engine = edit.engine;
    MixerAudioNode* mixer = nullptr;
    bool addedFrozenTracksYet = false;

    const bool shouldUse64Bit = engine.getPropertyStorage().getProperty (SettingID::use64Bit, false);
    const bool shouldUseMultiCPU = (engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() > 1);

    CreateAudioNodeParams cnp;
    cnp.audioNodeToBeReplaced = deviceInstance.getAudioNode();
    cnp.forRendering = false;
    cnp.includePlugins = true;
    cnp.addAntiDenormalisationNoise = addAntiDenormalisationNoise;

    for (auto t : getAudioTracks (edit))
    {
        if (t->isProcessing (true)
             && t->createsOutput()
             && (! t->isPartOfSubmix())
             && &device == t->getOutput().getOutputDevice (false))
        {
            if (t->isFrozen (Track::groupFreeze))
            {
                if (! addedFrozenTracksYet)
                {
                    CRASH_TRACER
                    addedFrozenTracksYet = true;

                    for (auto& freezeFile : edit.getFrozenTracksFiles())
                    {
                        const String fn (freezeFile.getFileName());
                        const String outId (fn.fromLastOccurrenceOf ("_", false, false)
                                              .upToFirstOccurrenceOf (".", false, false));

                        if (device.getDeviceID() == outId)
                        {
                            AudioFile af (freezeFile);
                            const double length = af.getLength();

                            if (length > 0)
                            {
                                AudioNode* node = new WaveAudioNode (af,
                                                                     { 0.0, length },
                                                                     0.0,
                                                                     {},
                                                                     LiveClipLevel(),
                                                                     1.0, juce::AudioChannelSet::stereo());

                                node = new TrackMutingAudioNode (edit, node);
                                node = new PlayHeadAudioNode (node);

                                if (mixer == nullptr)
                                    mixer = new MixerAudioNode (shouldUse64Bit, shouldUseMultiCPU);

                                mixer->addInput (node);
                            }
                        }
                    }
                }
            }
            else
            {
                if (mixer == nullptr)
                    mixer = new MixerAudioNode (shouldUse64Bit, shouldUseMultiCPU);

                mixer->addInput (t->createAudioNode (cnp));
            }
        }
    }

    // Create nodes for any submix tracks
    for (auto t : getTracksOfType<FolderTrack> (edit, true))
    {
        if (t->isSubmixFolder() && (! t->isPartOfSubmix()) && t->outputsToDevice (device))
        {
            if (auto n = t->createAudioNode (cnp))
            {
                if (mixer == nullptr)
                    mixer = new MixerAudioNode (shouldUse64Bit, shouldUseMultiCPU);

                mixer->addInput (n);
            }
        }
    }

    // Create nodes for any insert plugins
    bool deviceIsBeingUsedAsInsert = false;

    for (auto p : getAllPlugins (edit, false))
    {
        if (auto ins = dynamic_cast<InsertPlugin*> (p))
        {
            if (ins->isFrozen())
                continue;

            if (auto sendNode = ins->createSendAudioNode (device))
            {
                if (mixer == nullptr)
                    mixer = new MixerAudioNode (shouldUse64Bit, shouldUseMultiCPU);

                mixer->addInput (sendNode);
                deviceIsBeingUsedAsInsert = true;
            }
        }
    }

    AudioNode* finalNode = mixer;

    // add any master plugins..
    if (! deviceIsBeingUsedAsInsert && finalNode != nullptr)
    {
        CRASH_TRACER

        // add any master plugins..
        if (engine.getDeviceManager().getDefaultWaveOutDevice() == &device)
        {
            finalNode = edit.getMasterPluginList().createAudioNode (finalNode, addAntiDenormalisationNoise);
            finalNode = edit.getMasterVolumePlugin()->createAudioNode (finalNode, false);
        }

        finalNode = insertOptionalLastStageNode (finalNode);

        if (finalNode != nullptr)
            finalNode = FadeInOutAudioNode::createForEdit (edit, finalNode);

        if (edit.getIsPreviewEdit() && finalNode != nullptr)
            finalNode = new LevelMeasuringAudioNode (edit.getPreviewLevelMeasurer(), finalNode);
    }

    if (edit.isClickTrackDevice (device))
    {
        CRASH_TRACER

        auto clickNode = new ClickMutingNode (new ClickNode (device.isMidi(), edit, Edit::maximumLength), edit);

        auto finalMixer = dynamic_cast<MixerAudioNode*> (finalNode);

        if (finalMixer == nullptr)
        {
            finalMixer = new MixerAudioNode (false, shouldUseMultiCPU);

            finalMixer->addInput (clickNode);
            finalMixer->addInput (finalNode);

            finalNode = finalMixer;
        }
        else
        {
            finalMixer->addInput (clickNode);
        }
    }

    return finalNode;
}

void EditPlaybackContext::createAudioNodes (double startTime, bool addAntiDenormalisationNoise)
{
    CRASH_TRACER

    isAllocated = true;

    Array<AudioNode*> allNodes;

    for (auto mo : midiOutputs)
        allNodes.add (prepareNode (createPlaybackAudioNode (edit, *mo, insertOptionalLastStageNode,
                                                            addAntiDenormalisationNoise), true));

    for (auto wo : waveOutputs)
        allNodes.add (prepareNode (createPlaybackAudioNode (edit, *wo, insertOptionalLastStageNode,
                                                            addAntiDenormalisationNoise), false));

    prepareNodesToPlay (edit.engine, allNodes, startTime, playhead);

    const auto& tempoSections = edit.tempoSequence.getTempoSections();
    const bool hasTempoChanged = tempoSections.getChangeCount() != lastTempoSections.getChangeCount();

    // mustn't delete any of the nodes until all have been removed from
    // the graph, because there could be interdependencies between some
    // of them, e.g. aux sends
    OwnedArray<AudioNode> removedNodes;
    removedNodes.ensureStorageAllocated (1024);

    {
        int i = 0;
        ScopedLock sl (edit.engine.getDeviceManager().deviceManager.getAudioCallbackLock());

        for (auto mo : midiOutputs)
            removedNodes.add (mo->replaceAudioNode (allNodes.getUnchecked (i++)));

        for (auto wo : waveOutputs)
            removedNodes.add (wo->replaceAudioNode (allNodes.getUnchecked (i++)));

        if (hasTempoChanged && lastTempoSections.size() > 0)
        {
            const double lastBeats = lastTempoSections.timeToBeats (playhead.getPosition());
            const double lastPositionRemapped = tempoSections.beatsToTime (lastBeats);

            playhead.overridePosition (lastPositionRemapped);
        }
    }

    if (hasTempoChanged)
        lastTempoSections = tempoSections;
}

void EditPlaybackContext::createPlayAudioNodes (double startTime)
{
    createAudioNodes (startTime, shouldAddAntiDenormalisationNoise (edit.engine));
    startPlaying (startTime);
}

void EditPlaybackContext::createPlayAudioNodesIfNeeded (double startTime)
{
    if (! isAllocated)
        createPlayAudioNodes (startTime);
}

void EditPlaybackContext::reallocate()
{
    createPlayAudioNodes (playhead.getPosition());
}

void EditPlaybackContext::startPlaying (double start)
{
    prepareOutputDevices (start);

    if (priorityBooster == nullptr)
        priorityBooster = new ProcessPriorityBooster();

    for (auto mo : midiOutputs)
        mo->start();
}

void EditPlaybackContext::startRecording (double start, double punchIn)
{
    auto& dm = edit.engine.getDeviceManager();
    auto sampleRate = dm.getSampleRate();
    auto blockSize  = dm.getBlockSize();

    String error;

    for (int i = waveInputs.size(); --i >= 0 && error.isEmpty();)
        if (auto wi = waveInputs.getUnchecked (i))
            if (wi->isRecordingActive())
                error = wi->prepareToRecord (start, punchIn, sampleRate, blockSize, false);

    for (int i = midiInputs.size(); --i >= 0 && error.isEmpty();)
        if (auto mi = midiInputs.getUnchecked (i))
            if (mi->isRecordingActive())
                error = mi->prepareToRecord (start, punchIn, sampleRate, blockSize, false);

    if (error.isNotEmpty())
    {
        for (auto wi : waveInputs)
            if (wi->isRecordingActive())
                wi->recordWasCancelled();

        for (auto mi : midiInputs)
            if (mi->isRecordingActive())
                mi->recordWasCancelled();

        edit.engine.getUIBehaviour().showWarningAlert (TRANS("Record Error"), error);
    }
    else
    {
        startPlaying (start);

        for (auto wi : waveInputs)
            if (wi->isRecordingActive())
                wi->startRecording();

        for (auto mi : midiInputs)
            if (mi->isRecordingActive())
                mi->startRecording();
    }
}

void EditPlaybackContext::prepareOutputDevices (double start)
{
    CRASH_TRACER
    auto& dm = edit.engine.getDeviceManager();
    double sampleRate = dm.getSampleRate();
    int blockSize = dm.getBlockSize();

    start = playhead.streamTimeToSourceTime (start);

    for (auto wo : waveOutputs)
        wo->prepareToPlay (sampleRate, blockSize);

    for (auto mo : midiOutputs)
        mo->prepareToPlay (start, true);

    midiDispatcher.prepareToPlay (playhead, start);
}

void EditPlaybackContext::prepareForPlaying (double startTime)
{
    createPlayAudioNodesIfNeeded (startTime);
}

void EditPlaybackContext::prepareForRecording (double startTime, double punchIn)
{
    createPlayAudioNodesIfNeeded (startTime);
    startRecording (startTime, punchIn);
}

static SelectionManager* findAppropriateSelectionManager (Edit& ed)
{
    SelectionManager* found = nullptr;

    for (SelectionManager::Iterator iter; iter.next();)
        if (auto sm = iter.get())
            if (sm->edit == &ed)
                if (found == nullptr || found->editViewID == -1)
                    found = sm;

    return found;
}

Clip::Array EditPlaybackContext::stopRecording (InputDeviceInstance& in, EditTimeRange recordedRange, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    const auto loopRange = transport.getLoopRange();
    in.stop();

    return in.applyLastRecordingToEdit (recordedRange,
                                        transport.looping, loopRange,
                                        discardRecordings,
                                        findAppropriateSelectionManager (edit));
}

Clip::Array EditPlaybackContext::recordingFinished (EditTimeRange recordedRange, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    Clip::Array clips;

    for (auto in : getAllInputs())
        clips.addArray (stopRecording (*in, recordedRange, discardRecordings));

    return clips;
}

Result EditPlaybackContext::applyRetrospectiveRecord (Array<Clip*>* clips)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    bool inputAssigned = false;

    for (auto in : getAllInputs())
    {
        if (in->isAttachedToTrack())
        {
            inputAssigned = true;
            break;
        }
    }

    if (! inputAssigned)
        return Result::fail (TRANS("Unable to perform retrospective record, no inputs are assigned to a track"));

    InputDevice::setRetrospectiveLock (edit.engine, getAllInputs(), true);

    bool clipCreated = false;

    for (auto in : getAllInputs())
    {
        if (auto clip = in->applyRetrospectiveRecord (findAppropriateSelectionManager (edit)))
        {
            if (clips != nullptr)
                clips->add (clip);
                
            clipCreated = true;
        }
    }

    InputDevice::setRetrospectiveLock (edit.engine, getAllInputs(), false);

    if (! clipCreated)
        return Result::fail (TRANS("Unable to perform retrospective record, all input buffers are empty"));

    return Result::ok();
}

Array<InputDeviceInstance*> EditPlaybackContext::getAllInputs()
{
    Array<InputDeviceInstance*> allInputs;
    allInputs.addArray (waveInputs);
    allInputs.addArray (midiInputs);

    return allInputs;
}

void EditPlaybackContext::fillNextAudioBlock (EditTimeRange streamTime, float** allChannels, int numSamples)
{
    CRASH_TRACER

    if (edit.isRendering())
        return;

    SCOPED_REALTIME_CHECK

    // update stream time for track inputs
    for (auto in : midiInputs)
        if (in->owner.getDeviceType() == InputDevice::trackMidiDevice)
            in->owner.masterTimeUpdate (streamTime.getStart());

    midiDispatcher.masterTimeUpdate (playhead, streamTime.getStart());

    playhead.deviceManagerPositionUpdate (streamTime.getStart(), streamTime.getEnd());

    // sync this playback context with a master context
    if (contextToSyncTo != nullptr && playhead.isPlaying())
    {
        const auto streamPos      = contextToSyncTo->playhead.getPosition();
        const bool masterPlaying  = contextToSyncTo->playhead.isPlaying();

        if (! hasSynced)
        {
            auto masterPosition = contextToSyncTo->playhead.getPosition();
            auto masterOffset = std::fmod (masterPosition - previousBarTime, syncInterval);
            jassert (masterPosition - previousBarTime >= 0);
            jassert (masterOffset > 0);

            // if the next bar is too far away, start playing now
            auto startPosition = masterOffset - syncInterval;

            if (playhead.isLooping() && startPosition < -0.6)
                startPosition = masterOffset;

            startPosition = std::fmod (startPosition, playhead.getLoopTimes().getLength());

            playhead.setRollInToLoop (startPosition);
            hasSynced = true;
        }
        else if (! masterPlaying || std::abs (lastStreamPos - streamPos) > 0.2)
        {
            const RelativeTime rt = Time::getCurrentTime() - contextToSyncTo->playhead.getLastUserInteractionTime();

            if (! masterPlaying || rt.inSeconds() < 0.2)
            {
                // user has moved  or stopped the playhead -- break the sync
                contextToSyncTo = nullptr;
            }
            else
            {
                auto masterPosition = contextToSyncTo->playhead.getPosition();
                auto masterOffset = std::fmod (masterPosition - previousBarTime, syncInterval);
                auto position = playhead.getPosition();

                auto newPosition = std::floor (position / syncInterval) * syncInterval + masterOffset;
                newPosition = std::fmod (newPosition, playhead.getLoopTimes().getLength());

                playhead.setRollInToLoop (newPosition);
            }
        }

        lastStreamPos = streamPos;
    }

    edit.updateModifierTimers (playhead, streamTime, numSamples);
    midiDispatcher.nextBlockStarted (playhead, streamTime, numSamples);

    for (auto r : edit.getRackList().getTypes())
        r->newBlockStarted();

    for (auto wo : waveOutputs)
        wo->fillNextAudioBlock (playhead, streamTime, allChannels, numSamples);
}

InputDeviceInstance* EditPlaybackContext::getInputFor (InputDevice* d) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto i : waveInputs)
        if (&i->owner == d)
            return i;

    for (auto i : midiInputs)
        if (&i->owner == d)
            return i;

    return {};
}

void EditPlaybackContext::syncToContext (EditPlaybackContext* newContextToSyncTo,
                                         double newPreviousBarTime, double newSyncInterval)
{
    contextToSyncTo = newContextToSyncTo;
    previousBarTime = newPreviousBarTime;
    syncInterval    = newSyncInterval;
    hasSynced       = false;
}

static bool hasCheckedDenormNoise = false;

bool EditPlaybackContext::shouldAddAntiDenormalisationNoise (Engine& e)
{
    static bool shouldAdd;

    if (! hasCheckedDenormNoise)
    {
        shouldAdd = e.getPropertyStorage().getProperty (SettingID::addAntiDenormalNoise, false);
        hasCheckedDenormNoise = true;
    }

    return shouldAdd;
}

void EditPlaybackContext::setAddAntiDenormalisationNoise (Engine& e, bool b)
{
    e.getPropertyStorage().setProperty (SettingID::addAntiDenormalNoise, b);
    hasCheckedDenormNoise = false;
}

//==============================================================================
#if JUCE_WINDOWS
static int numHighPriorityPlayers = 0, numRealtimeDefeaters = 0;

void updateProcessPriority()
{
    int level = 0;

    if (numHighPriorityPlayers > 0)
        level = numRealtimeDefeaters == 0 && Engine::getInstance().getPropertyStorage().getProperty (SettingID::useRealtime, false) ? 2 : 1;

    Engine::getInstance().getEngineBehaviour().setProcessPriority (level);
}

EditPlaybackContext::ProcessPriorityBooster::ProcessPriorityBooster()        { ++numHighPriorityPlayers; updateProcessPriority(); }
EditPlaybackContext::ProcessPriorityBooster::~ProcessPriorityBooster()       { --numHighPriorityPlayers; updateProcessPriority(); }
EditPlaybackContext::RealtimePriorityDisabler::RealtimePriorityDisabler()    { ++numRealtimeDefeaters; updateProcessPriority(); }
EditPlaybackContext::RealtimePriorityDisabler::~RealtimePriorityDisabler()   { --numRealtimeDefeaters; updateProcessPriority(); }

#else

EditPlaybackContext::ProcessPriorityBooster::ProcessPriorityBooster()        {}
EditPlaybackContext::ProcessPriorityBooster::~ProcessPriorityBooster()       {}
EditPlaybackContext::RealtimePriorityDisabler::RealtimePriorityDisabler()    {}
EditPlaybackContext::RealtimePriorityDisabler::~RealtimePriorityDisabler()   {}

#endif
