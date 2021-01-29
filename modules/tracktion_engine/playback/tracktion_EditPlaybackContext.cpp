/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
 struct EditPlaybackContext::NodePlaybackContext
 {
     NodePlaybackContext (size_t numThreads, size_t maxNumThreadsToUse)
        : player (processState, getPoolCreatorFunction (static_cast<tracktion_graph::ThreadPoolStrategy> (getThreadPoolStrategy()))),
          maxNumThreads (maxNumThreadsToUse)
     {
         setNumThreads (numThreads);
     }
     
     void setNumThreads (size_t numThreads)
     {
         CRASH_TRACER
         player.setNumThreads (std::min (numThreads, maxNumThreads));
     }
     
     void setNode (std::unique_ptr<Node> node, double sampleRate, int blockSize)
     {
         blockSize = juce::roundToInt (blockSize * (1.0 + (10.0 * 0.01))); // max speed comp
         player.setNode (std::move (node), sampleRate, blockSize);
         
         if (auto currentNode = player.getNode())
             latencySamples = currentNode->getNodeProperties().latencyNumSamples;
     }
     
     void clearNode()
     {
         player.clearNode();
     }
     
     int getLatencySamples() const
     {
         return latencySamples;
     }
     
     void postPosition (double newPosition)
     {
         pendingPosition.store (newPosition, std::memory_order_release);
         positionUpdatePending = true;
     }
     
     void setSpeedCompensation (double plusOrMinus)
     {
         speedCompensation = jlimit (-10.0, 10.0, plusOrMinus);
     }
     
     void updateReferenceSampleRange (int numSamples)
     {
         if (speedCompensation != 0.0)
             numSamples = juce::roundToInt (numSamples * (1.0 + (speedCompensation * 0.01)));

         referenceSampleRange = juce::Range<int64_t>::withStartAndLength (referenceSampleRange.getEnd(), (int64_t) numSamples);
         playHead.setReferenceSampleRange (referenceSampleRange);
     }
     
     void process (float** allChannels, int numChannels, int destNumSamples)
     {
         if (positionUpdatePending.exchange (false))
             playHead.setPosition (timeToSample (pendingPosition.load (std::memory_order_acquire), getSampleRate()));
         
         const auto numSamples = referenceSampleRange.getLength();
         scratchMidiBuffer.clear();

         if (isUsingInterpolator || destNumSamples != numSamples)
         {
             // Initialise interpolators
             isUsingInterpolator = true;
             ensureNumInterpolators (numChannels);
             
             // Process required num samples
             scratchAudioBuffer.setSize (numChannels, (int) numSamples, false, false, true);
             scratchAudioBuffer.clear();
             
             tracktion_graph::Node::ProcessContext pc { referenceSampleRange, { tracktion_graph::toBufferView (scratchAudioBuffer), scratchMidiBuffer } };
             player.process (pc);
             
             // Then resample them to the dest num samples
             const double ratio = numSamples / (double) destNumSamples;
             
             for (int channel = 0; channel < numChannels; ++channel)
             {
                 const auto src = scratchAudioBuffer.getReadPointer (channel);
                 const auto dest = allChannels[channel];

                 interpolators[(size_t) channel]->processAdding (ratio, src, dest, destNumSamples, 1.0f);
             }
         }
         else
         {
             auto audioView = choc::buffer::createChannelArrayView (allChannels,
                                                                    (choc::buffer::ChannelCount) numChannels,
                                                                    (choc::buffer::FrameCount) numSamples);
             tracktion_graph::Node::ProcessContext pc { referenceSampleRange, { audioView, scratchMidiBuffer } };
             player.process (pc);
         }
     }
     
     double getSampleRate() const
     {
         return player.getSampleRate();
     }
     
     tracktion_graph::PlayHead playHead;
     tracktion_graph::PlayHeadState playHeadState { playHead };
     ProcessState processState { playHeadState };
     
 private:
     AudioBuffer<float> scratchAudioBuffer;
     MidiMessageArray scratchMidiBuffer;
     TracktionNodePlayer player;
     const size_t maxNumThreads;
     
     int latencySamples = 0;
     juce::Range<int64_t> referenceSampleRange;
     std::atomic<double> pendingPosition { 0.0 };
     std::atomic<bool> positionUpdatePending { false };
     double speedCompensation = 0.0;
     std::vector<std::unique_ptr<juce::LagrangeInterpolator>> interpolators;
     bool isUsingInterpolator = false;
     
     void ensureNumInterpolators (int numRequired)
     {
         for (size_t i = interpolators.size(); i < (size_t) numRequired; ++i)
             interpolators.push_back (std::make_unique<juce::LagrangeInterpolator>());
     }
};
#endif

//==============================================================================
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

    if (edit.isRendering())
    {
        jassertfalse;
        TRACKTION_LOG_ERROR("EditPlaybackContext created whilst rendering");
    }

    if (edit.shouldPlay())
    {
        #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
        if (isExperimentalGraphProcessingEnabled())
            nodePlaybackContext = std::make_unique<NodePlaybackContext> (edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio(),
                                                                         size_t (edit.getIsPreviewEdit() ? 0 : juce::SystemStats::getNumCpus() - 1));
        #endif

        // This ensures the referenceSampleRange of the new context has been synced
        edit.engine.getDeviceManager().addContext (this);
    }
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

   #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
    // Because the nodePlaybackContext is lock-free, it doesn't immediately delete its current node
    // so we need to explicity call clearNode
    if (nodePlaybackContext)
    {
        nodePlaybackContext->playHead.stop();
        nodePlaybackContext->clearNode();
        nodePlaybackContext->setNumThreads (0);
    }
   #endif
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

                    for (auto& freezeFile : TemporaryFileManager::getFrozenTrackFiles (edit))
                    {
                        const String fn (freezeFile.getFileName());
                        const String outId (fn.fromLastOccurrenceOf ("_", false, false)
                                              .upToFirstOccurrenceOf (".", false, false));

                        if (device.getDeviceID() == outId)
                        {
                            AudioFile af (edit.engine, freezeFile);
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
        if (t->isSubmixFolder() && (! t->isPartOfSubmix())
            && t->getOutput() != nullptr && &device == t->getOutput()->getOutputDevice (false))
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

        auto clickNode = new ClickMutingNode (new ClickAudioNode (device.isMidi(), edit, Edit::maximumLength), edit);

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

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
void EditPlaybackContext::createNode()
{
    CRASH_TRACER
    // Reset this until it's updated by the play graph
    audiblePlaybackTime = transport.getCurrentPosition();
    
    isAllocated = true;
    
    auto& dm = edit.engine.getDeviceManager();
    CreateNodeParams cnp { nodePlaybackContext->processState };
    cnp.sampleRate = dm.getSampleRate();
    cnp.blockSize = dm.getBlockSize();
    cnp.includeBypassedPlugins = ! edit.engine.getEngineBehaviour().shouldBypassedPluginsBeRemovedFromPlaybackGraph();
    auto editNode = createNodeForEdit (*this, audiblePlaybackTime, cnp);

    const auto& tempoSections = edit.tempoSequence.getTempoSections();
    const bool hasTempoChanged = tempoSections.getChangeCount() != lastTempoSections.getChangeCount();

    nodePlaybackContext->setNode (std::move (editNode), cnp.sampleRate, cnp.blockSize);
    updateNumCPUs();

    if (hasTempoChanged && lastTempoSections.size() > 0)
    {
        const auto sampleRate = cnp.sampleRate;
        const auto lastTime = sampleToTime (nodePlaybackContext->playHead.getPosition(), sampleRate);
        const auto lastBeats = lastTempoSections.timeToBeats (lastTime);
        const auto lastPositionRemapped = tempoSections.beatsToTime (lastBeats);

        const auto lastSampleRemapped = timeToSample (lastPositionRemapped, sampleRate);
        nodePlaybackContext->playHead.overridePosition (lastSampleRemapped);
    }

    if (hasTempoChanged)
        lastTempoSections = tempoSections;
}
#endif

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
            removedNodes.add (mo->replaceAudioNode (std::unique_ptr<AudioNode> (allNodes.getUnchecked (i++))));

        for (auto wo : waveOutputs)
            removedNodes.add (wo->replaceAudioNode (std::unique_ptr<AudioNode> (allNodes.getUnchecked (i++))));

        if (hasTempoChanged && lastTempoSections.size() > 0)
        {
            auto lastBeats = lastTempoSections.timeToBeats (playhead.getPosition());
            auto lastPositionRemapped = tempoSections.beatsToTime (lastBeats);

            playhead.overridePosition (lastPositionRemapped);
        }
    }

    if (hasTempoChanged)
        lastTempoSections = tempoSections;
}

void EditPlaybackContext::createPlayAudioNodes (double startTime)
{
   #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
    if (isExperimentalGraphProcessingEnabled())
        createNode();
    else
   #endif
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
        priorityBooster.reset (new ProcessPriorityBooster (edit.engine));

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

    midiDispatcher.prepareToPlay (start);
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
            if (sm->getEdit() == &ed)
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
        for (auto clip : in->applyRetrospectiveRecord (findAppropriateSelectionManager (edit)))
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

    playhead.deviceManagerPositionUpdate (streamTime.getStart(), streamTime.getEnd());

    // Update the dispatcher after the playhead reference time
    midiDispatcher.masterTimeUpdate (playhead.streamTimeToSourceTime (streamTime.getStart()));

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

    edit.updateModifierTimers (playhead.streamTimeToSourceTime (streamTime.getStart()), numSamples);
    midiDispatcher.renderDevices (playhead, streamTime, numSamples);

    for (auto r : edit.getRackList().getTypes())
        r->newBlockStarted();

    for (auto wo : waveOutputs)
        wo->fillNextAudioBlock (playhead, streamTime, allChannels, numSamples);
}

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
void EditPlaybackContext::fillNextNodeBlock (float** allChannels, int numChannels, int numSamples)
{
    CRASH_TRACER

    if (edit.isRendering())
        return;

    SCOPED_REALTIME_CHECK
    if (! nodePlaybackContext)
        return;

    nodePlaybackContext->updateReferenceSampleRange (numSamples);

    // Sync this playback context with a master context
    if (nodeContextToSyncTo != nullptr && nodePlaybackContext->playHead.isPlaying())
    {
        const double sampleRate     = nodeContextToSyncTo->getSampleRate();
        const auto timelinePosition = nodeContextToSyncTo->getNodePlayHead()->getPosition();
        const bool masterPlaying    = nodeContextToSyncTo->getNodePlayHead()->isPlaying();
        const auto loopTimes        = transport.getLoopRange();

        if (! nodeHasSynced)
        {
            auto masterPosition = tracktion_graph::sampleToTime (timelinePosition, sampleRate);
            auto masterOffset = std::fmod (masterPosition - previousBarTime, syncInterval);
            jassert (masterPosition - previousBarTime >= 0);
            jassert (masterOffset > 0);

            // if the next bar is too far away, start playing now
            auto startPosition = masterOffset - syncInterval;

            if (nodePlaybackContext->playHead.isLooping() && startPosition < -0.6)
                startPosition = masterOffset;

            startPosition = std::fmod (startPosition, loopTimes.getLength());

            nodePlaybackContext->playHead.setRollInToLoop (tracktion_graph::timeToSample (startPosition, sampleRate));
            nodeHasSynced = true;
        }
        else if (! masterPlaying || std::abs (tracktion_graph::sampleToTime (lastTimelinePos - timelinePosition, sampleRate)) > 0.2)
        {
            const auto rt = std::chrono::system_clock::now() - nodeContextToSyncTo->getNodePlayHead()->getLastUserInteractionTime();
            const auto numSecondsSinceUserInteraction = std::chrono::duration_cast<std::chrono::seconds> (rt).count();
            
            if (! masterPlaying || numSecondsSinceUserInteraction < 0.2)
            {
                // User has moved  or stopped the playhead -- break the sync
                nodeContextToSyncTo = nullptr;
            }
            else
            {
                auto masterPosition = tracktion_graph::sampleToTime (nodeContextToSyncTo->playhead.getPosition(), sampleRate);
                auto masterOffset = std::fmod (masterPosition - previousBarTime, syncInterval);
                auto editTime = tracktion_graph::sampleToTime (nodePlaybackContext->playHead.getPosition(), sampleRate);

                auto newEditTime = std::floor (editTime / syncInterval) * syncInterval + masterOffset;
                newEditTime = std::fmod (newEditTime, loopTimes.getLength());

                nodePlaybackContext->playHead.setRollInToLoop (tracktion_graph::timeToSample (newEditTime, sampleRate));
            }
        }

        lastTimelinePos = timelinePosition;
    }

    nodePlaybackContext->process (allChannels, numChannels, numSamples);
    
    // Dispatch any MIDI messages that have been injected in to the MidiOutputDeviceInstances by the Node
    const double editTime = tracktion_graph::sampleToTime (nodePlaybackContext->playHead.getPosition(), nodePlaybackContext->getSampleRate());
    midiDispatcher.dispatchPendingMessagesForDevices (editTime);
}
#endif

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

OutputDeviceInstance* EditPlaybackContext::getOutputFor (OutputDevice* d) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto i : waveOutputs)
        if (&i->owner == d)
            return i;

    for (auto i : midiOutputs)
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

   #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
    if (newContextToSyncTo != nullptr && newContextToSyncTo->getNodePlayHead() != nullptr)
    {
        nodeContextToSyncTo = newContextToSyncTo;
        nodeHasSynced = false;
    }
   #endif
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
namespace EditPlaybackContextInternal
{
    std::atomic<bool>& getExperimentalGraphProcessingFlag()
    {
        static std::atomic<bool> enabled { false };
        return enabled;
    }

   #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
    int& getThreadPoolStrategyType()
    {
        static int type = static_cast<int> (tracktion_graph::ThreadPoolStrategy::realTime);
        return type;
    }
   #endif
}

void EditPlaybackContext::enableExperimentalGraphProcessing (bool enable)
{
    EditPlaybackContextInternal::getExperimentalGraphProcessingFlag() = enable;
}

bool EditPlaybackContext::isExperimentalGraphProcessingEnabled()
{
   #if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
    return EditPlaybackContextInternal::getExperimentalGraphProcessingFlag();
   #else
    return false;
   #endif
}

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
tracktion_graph::PlayHead* EditPlaybackContext::getNodePlayHead() const
{
    if (! isExperimentalGraphProcessingEnabled())
        return nullptr;
    
    return nodePlaybackContext ? &nodePlaybackContext->playHead
                               : nullptr;
}

int EditPlaybackContext::getLatencySamples() const
{
    if (! isExperimentalGraphProcessingEnabled())
        return 0;
    
    return nodePlaybackContext ? nodePlaybackContext->getLatencySamples()
                               : 0;
}

double EditPlaybackContext::getAudibleTimelineTime()
{
    return nodePlaybackContext ? audiblePlaybackTime.load()
                               : transport.getCurrentPosition();
}

double EditPlaybackContext::getSampleRate() const
{
    return nodePlaybackContext ? nodePlaybackContext->getSampleRate()
                               : 44100.0;
}

void EditPlaybackContext::updateNumCPUs()
{
    if (nodePlaybackContext)
        nodePlaybackContext->setNumThreads ((size_t) edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() - 1);
}

void EditPlaybackContext::setSpeedCompensation (double plusOrMinus)
{
    if (nodePlaybackContext)
        nodePlaybackContext->setSpeedCompensation (plusOrMinus);
}

void EditPlaybackContext::postPosition (double newPosition)
{
    if (nodePlaybackContext)
        nodePlaybackContext->postPosition (newPosition);
}

void EditPlaybackContext::setThreadPoolStrategy (int type)
{
    type = jlimit (static_cast<int> (tracktion_graph::ThreadPoolStrategy::conditionVariable),
                   static_cast<int> (tracktion_graph::ThreadPoolStrategy::lightweightSemHybrid),
                   type);
    EditPlaybackContextInternal::getThreadPoolStrategyType() = type;
}

int EditPlaybackContext::getThreadPoolStrategy()
{
    const int type = jlimit (static_cast<int> (tracktion_graph::ThreadPoolStrategy::conditionVariable),
                             static_cast<int> (tracktion_graph::ThreadPoolStrategy::lightweightSemHybrid),
                             EditPlaybackContextInternal::getThreadPoolStrategyType());
    
    return type;
}

#endif

//==============================================================================
#if JUCE_WINDOWS
static int numHighPriorityPlayers = 0, numRealtimeDefeaters = 0;

void updateProcessPriority (Engine& engine)
{
    int level = 0;

    if (numHighPriorityPlayers > 0)
        level = numRealtimeDefeaters == 0 && engine.getPropertyStorage().getProperty (SettingID::useRealtime, false) ? 2 : 1;

    engine.getEngineBehaviour().setProcessPriority (level);
}

EditPlaybackContext::ProcessPriorityBooster::ProcessPriorityBooster (Engine& e) : engine (e)        { ++numHighPriorityPlayers; updateProcessPriority (engine); }
EditPlaybackContext::ProcessPriorityBooster::~ProcessPriorityBooster()                              { --numHighPriorityPlayers; updateProcessPriority (engine); }
EditPlaybackContext::RealtimePriorityDisabler::RealtimePriorityDisabler (Engine& e) : engine (e)    { ++numRealtimeDefeaters; updateProcessPriority (engine); }
EditPlaybackContext::RealtimePriorityDisabler::~RealtimePriorityDisabler()                          { --numRealtimeDefeaters; updateProcessPriority (engine); }

#else

EditPlaybackContext::ProcessPriorityBooster::ProcessPriorityBooster (Engine& e) : engine (e)        {}
EditPlaybackContext::ProcessPriorityBooster::~ProcessPriorityBooster()                              {}
EditPlaybackContext::RealtimePriorityDisabler::RealtimePriorityDisabler (Engine& e) : engine (e)    {}
EditPlaybackContext::RealtimePriorityDisabler::~RealtimePriorityDisabler()                          {}

#endif

}
