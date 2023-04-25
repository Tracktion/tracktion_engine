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
//==============================================================================
namespace EditPlaybackContextInternal
{
    inline int& getThreadPoolStrategyType()
    {
        static int type = static_cast<int> (tracktion::graph::ThreadPoolStrategy::lightweightSemHybrid);
        return type;
    }

    inline bool& getPooledMemoryFlag()
    {
        static bool usePool = false;
        return usePool;
    }
}


//==============================================================================
//==============================================================================
struct EditPlaybackContext::ContextSyncroniser
{
    //==============================================================================
    ContextSyncroniser() = default;
    
    //==============================================================================
    enum class SyncAction
    {
        none,           /**< Take no action. */
        rollInToLoop,   /**< Set the dest playhead to roll in to the loop. */
        breakSync       /**< Break the sync and don't call this again. */
    };
    
    struct SyncAndPosition
    {
        SyncAction action;
        TimePosition position;
    };
    
    //==============================================================================
    SyncAndPosition getSyncAction (tracktion::graph::PlayHead& sourcePlayHead, tracktion::graph::PlayHead& destPlayHead,
                                   double sampleRate)
    {
        if (! isValid)
            return  { SyncAction::none, {} };
        
        const auto sourceTimelineTime = TimePosition::fromSamples (sourcePlayHead.getPosition(), sampleRate);
        const auto millisecondsSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds> (sourcePlayHead.getLastUserInteractionTime().time_since_epoch()).count();
        const juce::Time sourceLastInteractionTime (static_cast<int64_t> (millisecondsSinceEpoch));

        const auto destTimelineTime = TimePosition::fromSamples (destPlayHead.getPosition(), sampleRate);
        const auto destLoopDuration = TimeDuration::fromSamples (destPlayHead.getLoopRange().getLength(), sampleRate);

        return getSyncAction (sourceTimelineTime, sourcePlayHead.isPlaying(), sourceLastInteractionTime,
                              destTimelineTime, destPlayHead.isLooping(), destLoopDuration);
    }
    
    void reset (TimePosition previousBarTime_, TimeDuration syncInterval_)
    {
        hasSynced = false;
        previousBarTime = previousBarTime_;
        syncInterval = syncInterval_;
        isValid = true;
    }
        
private:
    //==============================================================================
    TimePosition previousBarTime;
    TimeDuration syncInterval;
    TimePosition lastSourceTimelineTime;
    bool hasSynced = false, isValid = false;

    SyncAndPosition getSyncAction (const TimePosition sourceTimelineTime, const bool sourceIsPlaying, const juce::Time sourceLastInteractionTime,
                                   const TimePosition destTimelineTime, const bool destIsLooping, const TimeDuration destLoopDuration)
    {
        const juce::ScopedValueSetter<TimePosition> lastSourceTimelineTimeUpdater (lastSourceTimelineTime, lastSourceTimelineTime,
                                                                                   sourceTimelineTime);

        if (! hasSynced)
        {
            const auto sourceDurationSinceLastBarStart = TimeDuration::fromSeconds (std::fmod ((sourceTimelineTime - previousBarTime).inSeconds(), syncInterval.inSeconds()));
            jassert (sourceTimelineTime - previousBarTime >= TimeDuration());
            jassert (sourceDurationSinceLastBarStart > TimeDuration());
            
            auto newTimelineTime = sourceDurationSinceLastBarStart - syncInterval;

            // If the next bar is too far away, start playing now
            if (destIsLooping && newTimelineTime < -syncInterval)
                newTimelineTime = sourceDurationSinceLastBarStart;

            newTimelineTime = TimeDuration::fromSeconds (std::fmod (newTimelineTime.inSeconds(), destLoopDuration.inSeconds()));
            hasSynced = true;

            return { SyncAction::rollInToLoop, toPosition (newTimelineTime) };
        }
        
        if (! sourceIsPlaying || std::abs (lastSourceTimelineTime.inSeconds() - sourceTimelineTime.inSeconds()) > 0.2)
        {
            auto rt = juce::Time::getCurrentTime() - sourceLastInteractionTime;

            if (! sourceIsPlaying || rt.inSeconds() < 0.2)
            {
                // user has moved  or stopped the playhead -- break the sync
                isValid = false;
                
                return { SyncAction::breakSync, {} };
            }
            else
            {
                // This +0.1 is here to prevent rounding errors causing the playhead to jump back by a sync interval each loop iteration
                const auto sourceDurationSinceLastBarStart = TimeDuration::fromSeconds (std::fmod ((sourceTimelineTime - previousBarTime).inSeconds(), syncInterval.inSeconds()));
                auto newTimelineTime = syncInterval * std::floor ((destTimelineTime.inSeconds() + 0.1) / syncInterval.inSeconds()) + sourceDurationSinceLastBarStart;
                newTimelineTime = TimeDuration::fromSeconds (std::fmod (newTimelineTime.inSeconds(), destLoopDuration.inSeconds()));

                return { SyncAction::rollInToLoop, toPosition (newTimelineTime) };
            }
        }

        return { SyncAction::none, {} };
    }
};


//==============================================================================
//==============================================================================
 struct EditPlaybackContext::NodePlaybackContext
 {
     NodePlaybackContext (const TempoSequence& ts, size_t numThreads, size_t maxNumThreadsToUse)
        : tempoSequence (ts),
          player (processState, getPoolCreatorFunction (static_cast<tracktion::graph::ThreadPoolStrategy> (getThreadPoolStrategy()))),
          maxNumThreads (maxNumThreadsToUse)
     {
         setNumThreads (numThreads);
         player.enablePooledMemoryAllocations (EditPlaybackContextInternal::getPooledMemoryFlag());
     }
     
     void setNumThreads (size_t numThreads)
     {
         CRASH_TRACER
         player.setNumThreads (std::min (numThreads, maxNumThreads));
     }
     
     void setNode (std::unique_ptr<Node> node, double sampleRate, int blockSize)
     {
         jassert (sampleRate > 0.0);
         jassert (blockSize > 0);
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
     
     void postPosition (TimePosition newPosition)
     {
         pendingPosition.store (newPosition.inSeconds(), std::memory_order_release);
         pendingRollInToLoop.store (false, std::memory_order_release);
         positionUpdatePending = true;
     }

     void postRollInToLoop (double newPosition)
     {
         pendingPosition.store (newPosition, std::memory_order_release);
         pendingRollInToLoop.store (true, std::memory_order_release);
         positionUpdatePending = true;
     }

     void setSpeedCompensation (double plusOrMinus)
     {
         speedCompensation = juce::jlimit (-10.0, 10.0, plusOrMinus);
     }

     void setTempoAdjustment (double plusOrMinusProportion)
     {
         blockLengthScaleFactor = 1.0 + std::clamp (plusOrMinusProportion, -0.5, 0.5);
     }
     
     void updateReferenceSampleRange (int numSamples)
     {
         if (speedCompensation != 0.0)
             numSamples = juce::roundToInt (numSamples * (1.0 + (speedCompensation * 0.01)));

         double sampleDuration = static_cast<double> (numSamples);

         if (blockLengthScaleFactor != 1.0)
             sampleDuration *= blockLengthScaleFactor;

         referenceStreamRange = juce::Range<double>::withStartAndLength (referenceStreamRange.getEnd(), sampleDuration);
         playHead.setReferenceSampleRange (getReferenceSampleRange());
         numSamplesToProcess = static_cast<choc::buffer::FrameCount> (numSamples);
         processState.setPlaybackSpeedRatio (blockLengthScaleFactor);
     }
     
     void resyncToReferenceSampleRange (juce::Range<int64_t> newReferenceSampleRange)
     {
         const double sampleRate = getSampleRate();
         const auto currentPos = tracktion::graph::sampleToTime (playHead.getPosition(), sampleRate);
         referenceStreamRange = juce::Range<double> (static_cast<double> (newReferenceSampleRange.getStart()),
                                                     static_cast<double> (newReferenceSampleRange.getEnd()));
         playHead.setReferenceSampleRange (getReferenceSampleRange());
         playHead.setPosition (tracktion::graph::timeToSample (currentPos, sampleRate));
     }
     
     void process (float* const* allChannels, int numChannels, int destNumSamples)
     {
         if (positionUpdatePending.exchange (false))
         {
             const auto samplePos = timeToSample (pendingPosition.load (std::memory_order_acquire), getSampleRate());
             
             if (pendingRollInToLoop.load (std::memory_order_acquire))
                 playHead.setRollInToLoop (samplePos);
             else
                 playHead.setPosition (samplePos);
         }

         const auto referenceSampleRange = getReferenceSampleRange();
         scratchMidiBuffer.clear();

         if (isUsingInterpolator || destNumSamples != (int) numSamplesToProcess)
         {
             // Initialise interpolators
             isUsingInterpolator = true;
             ensureNumInterpolators (numChannels);
             
             // Process required num samples
             scratchAudioBuffer.setSize (numChannels, (int) numSamplesToProcess, false, false, true);
             scratchAudioBuffer.clear();
             
             tracktion::graph::Node::ProcessContext pc { numSamplesToProcess, referenceSampleRange, { tracktion::graph::toBufferView (scratchAudioBuffer), scratchMidiBuffer } };
             player.process (pc);
             
             // Then resample them to the dest num samples
             const double ratio = numSamplesToProcess / (double) destNumSamples;
             
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
                                                                    numSamplesToProcess);
             tracktion::graph::Node::ProcessContext pc { numSamplesToProcess, referenceSampleRange, { audioView, scratchMidiBuffer } };
             player.process (pc);
         }
     }
     
     double getSampleRate() const
     {
         return player.getSampleRate();
     }

     const TempoSequence& tempoSequence;
     tracktion::graph::PlayHead playHead;
     tracktion::graph::PlayHeadState playHeadState { playHead };
     ProcessState processState { playHeadState, tempoSequence };
     
 private:
     juce::AudioBuffer<float> scratchAudioBuffer;
     MidiMessageArray scratchMidiBuffer;
     TracktionNodePlayer player;
     const size_t maxNumThreads;
     
     int latencySamples = 0;
     choc::buffer::FrameCount numSamplesToProcess = 0;
     juce::Range<double> referenceStreamRange;
     std::atomic<double> pendingPosition { 0.0 };
     std::atomic<bool> positionUpdatePending { false }, pendingRollInToLoop { false };
     double speedCompensation = 0.0, blockLengthScaleFactor = 1.0;
     std::vector<std::unique_ptr<juce::LagrangeInterpolator>> interpolators;
     bool isUsingInterpolator = false;

     juce::Range<int64_t> getReferenceSampleRange() const
     {
         return { static_cast<int64_t> (std::llround (referenceStreamRange.getStart())),
                  static_cast<int64_t> (std::llround (referenceStreamRange.getEnd())) };
     }
     
     void ensureNumInterpolators (int numRequired)
     {
         for (size_t i = interpolators.size(); i < (size_t) numRequired; ++i)
             interpolators.push_back (std::make_unique<juce::LagrangeInterpolator>());
     }
};

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
    if (edit.isRendering())
    {
        jassertfalse;
        TRACKTION_LOG_ERROR("EditPlaybackContext created whilst rendering");
    }

    if (edit.shouldPlay())
    {
        nodePlaybackContext = std::make_unique<NodePlaybackContext> (edit.tempoSequence,
                                                                     edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio(),
                                                                     size_t (edit.getIsPreviewEdit() ? 0 : juce::SystemStats::getNumCpus() - 1));
        contextSyncroniser = std::make_unique<ContextSyncroniser>();

        // This ensures the referenceSampleRange of the new context has been synced
        edit.engine.getDeviceManager().addContext (this);
    }

    rebuildDeviceList();
}

EditPlaybackContext::~EditPlaybackContext()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    releaseDeviceList();
    edit.engine.getDeviceManager().removeContext (this);
}

void EditPlaybackContext::releaseDeviceList()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    const juce::ScopedValueSetter<bool> alocateSetter (isAllocated, isAllocated);
    clearNodes();
    midiDispatcher.setMidiDeviceList (juce::OwnedArray<MidiOutputDeviceInstance>());

    // Clear the outputs before the inputs as the midi inputs will be referenced by the MidiDeviceInstanceBase::Consumer
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
    isAllocated = false;

    // Because the nodePlaybackContext is lock-free, it doesn't immediately delete its current node
    // so we need to explicity call clearNode
    if (nodePlaybackContext)
    {
        nodePlaybackContext->playHead.stop();
        nodePlaybackContext->clearNode();
        nodePlaybackContext->setNumThreads (0);
    }
}

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
    
    if (cnp.sampleRate <= 0.0 || cnp.blockSize <= 0)
    {
        clearNodes();
        return;
    }
    
    cnp.includeBypassedPlugins = ! edit.engine.getEngineBehaviour().shouldBypassedPluginsBeRemovedFromPlaybackGraph();
    auto editNode = createNodeForEdit (*this, audiblePlaybackTime, cnp);

    const auto& tempoSequence = edit.tempoSequence.getInternalSequence();
    const bool hasTempoChanged = tempoSequence.hash() != lastTempoSequence.hash();

    nodePlaybackContext->setNode (std::move (editNode), cnp.sampleRate, cnp.blockSize);
    updateNumCPUs();

    if (hasTempoChanged)
    {
        const auto sampleRate = cnp.sampleRate;
        const auto lastTime = TimePosition::fromSamples (nodePlaybackContext->playHead.getPosition(), sampleRate);
        const auto lastBeats = lastTempoSequence.toBeats (lastTime);
        const auto lastPositionRemapped = tempoSequence.toTime (lastBeats);

        const auto lastSampleRemapped = toSamples (lastPositionRemapped, sampleRate);
        nodePlaybackContext->playHead.overridePosition (lastSampleRemapped);
    }

    if (hasTempoChanged)
        lastTempoSequence = tempoSequence;
}

void EditPlaybackContext::createPlayAudioNodes (TimePosition startTime)
{
    createNode();
    startPlaying (startTime);
}

void EditPlaybackContext::createPlayAudioNodesIfNeeded (TimePosition startTime)
{
    if (! isAllocated)
        createPlayAudioNodes (startTime);
}

void EditPlaybackContext::reallocate()
{
    createPlayAudioNodes (getPosition());
}

void EditPlaybackContext::startPlaying (TimePosition start)
{
    prepareOutputDevices (start);

    if (priorityBooster == nullptr)
        priorityBooster.reset (new ProcessPriorityBooster (edit.engine));

    for (auto mo : midiOutputs)
        mo->start();
}

void EditPlaybackContext::startRecording (TimePosition start, TimePosition punchIn)
{
    auto& dm = edit.engine.getDeviceManager();
    auto sampleRate = dm.getSampleRate();
    auto blockSize  = dm.getBlockSize();

    juce::String error;

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

void EditPlaybackContext::prepareOutputDevices (TimePosition start)
{
    CRASH_TRACER
    auto& dm = edit.engine.getDeviceManager();
    double sampleRate = dm.getSampleRate();
    int blockSize = dm.getBlockSize();

    // TODO: This feels wrong now as the global stream time should be a TimePosition...
    // It looks like start already is an EditTime
    // It also looks like MidiOutputDeviceInstance::prepareToPlay doesn't actually use the
    // time so is likely to be a wrong but unused step
    start = globalStreamTimeToEditTime (start.inSeconds());

    for (auto wo : waveOutputs)
        wo->prepareToPlay (sampleRate, blockSize);

    for (auto mo : midiOutputs)
        mo->prepareToPlay (start, true);

    midiDispatcher.prepareToPlay (start);
}

void EditPlaybackContext::prepareForPlaying (TimePosition startTime)
{
    createPlayAudioNodesIfNeeded (startTime);
}

void EditPlaybackContext::prepareForRecording (TimePosition startTime, TimePosition punchIn)
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

Clip::Array EditPlaybackContext::stopRecording (InputDeviceInstance& in, TimeRange recordedRange, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    const auto loopRange = transport.getLoopRange();
    in.stop();

    auto clips = in.applyLastRecordingToEdit (recordedRange,
                                              transport.looping, loopRange,
                                              discardRecordings,
                                              findAppropriateSelectionManager (edit));
    transport.callRecordingFinishedListeners (in, clips);
    
    return clips;
}

Clip::Array EditPlaybackContext::recordingFinished (TimeRange recordedRange, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    Clip::Array clips;

    for (auto in : getAllInputs())
        clips.addArray (stopRecording (*in, recordedRange, discardRecordings));

    return clips;
}

juce::Result EditPlaybackContext::applyRetrospectiveRecord (juce::Array<Clip*>* clips)
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
        return juce::Result::fail (TRANS("Unable to perform retrospective record, no inputs are assigned to a track"));

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
        return juce::Result::fail (TRANS("Unable to perform retrospective record, all input buffers are empty"));

    return juce::Result::ok();
}

juce::Array<InputDeviceInstance*> EditPlaybackContext::getAllInputs()
{
    juce::Array<InputDeviceInstance*> allInputs;
    allInputs.addArray (waveInputs);
    allInputs.addArray (midiInputs);

    return allInputs;
}

//==============================================================================
void EditPlaybackContext::fillNextNodeBlock (float* const* allChannels, int numChannels, int numSamples)
{
    CRASH_TRACER

    if (edit.isRendering())
        return;

    SCOPED_REALTIME_CHECK
    if (! nodePlaybackContext)
        return;

    nodePlaybackContext->updateReferenceSampleRange (numSamples);
    
    // Sync this playback context with a master context
    if (nodeContextToSyncTo && nodePlaybackContext->playHead.isPlaying() && nodeContextToSyncTo->getNodePlayHead() != nullptr)
    {
        jassert (contextSyncroniser);
        jassert (nodeContextToSyncTo->getNodePlayHead() != nullptr);
        const auto[action, newTimelineTime] = contextSyncroniser->getSyncAction (*nodeContextToSyncTo->getNodePlayHead(),
                                                                                 nodePlaybackContext->playHead,
                                                                                 nodeContextToSyncTo->getSampleRate());
        
        switch (action)
        {
            case ContextSyncroniser::SyncAction::none:
            {
                break;
            }
            case ContextSyncroniser::SyncAction::rollInToLoop:
            {
                nodePlaybackContext->postRollInToLoop (newTimelineTime.inSeconds());
                break;
            }
            case ContextSyncroniser::SyncAction::breakSync:
            {
                nodeContextToSyncTo = nullptr;
                break;
            }
        }
    }

    const auto editTime = TimePosition::fromSamples (nodePlaybackContext->playHead.getPosition(), nodePlaybackContext->getSampleRate());
    edit.updateModifierTimers (editTime, numSamples);
    midiDispatcher.masterTimeUpdate (editTime);

   #if TRACKTION_ENABLE_ABLETON_LINK
    edit.getAbletonLink().syncronise (editTime);
   #endif

    nodePlaybackContext->process (allChannels, numChannels, numSamples);
    
    // Dispatch any MIDI messages that have been injected in to the MidiOutputDeviceInstances by the Node
    midiDispatcher.dispatchPendingMessagesForDevices (editTime);
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
                                         TimePosition newPreviousBarTime, TimeDuration newSyncInterval)
{
    contextToSyncTo = newContextToSyncTo;
    previousBarTime = newPreviousBarTime;
    syncInterval    = newSyncInterval;
    hasSynced       = false;

    if (newContextToSyncTo != nullptr && newContextToSyncTo->getNodePlayHead() != nullptr)
    {
        nodeContextToSyncTo = newContextToSyncTo;
        contextSyncroniser->reset (previousBarTime, syncInterval);
    }
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
tracktion::graph::PlayHead* EditPlaybackContext::getNodePlayHead() const
{
    //TODO  can this be removed?
    return nodePlaybackContext ? &nodePlaybackContext->playHead
                               : nullptr;
}

bool EditPlaybackContext::isPlaying() const
{
    return nodePlaybackContext ? nodePlaybackContext->playHead.isPlaying()
                               : false;
}

bool EditPlaybackContext::isLooping() const
{
    return nodePlaybackContext->playHead.isLooping();
}

bool EditPlaybackContext::isDragging() const
{
    return nodePlaybackContext->playHead.isUserDragging();
}

TimePosition EditPlaybackContext::getPosition() const
{
    return TimePosition::fromSamples (nodePlaybackContext->playHead.getPosition(),
                                      nodePlaybackContext->getSampleRate());
}

TimePosition EditPlaybackContext::getUnloopedPosition() const
{
    return TimePosition::fromSamples (nodePlaybackContext->playHead.getUnloopedPosition(),
                                      nodePlaybackContext->getSampleRate());
}

TimeRange EditPlaybackContext::getLoopTimes() const
{
    return tracktion::timeRangeFromSamples (nodePlaybackContext->playHead.getLoopRange(),
                                                  nodePlaybackContext->getSampleRate());
}

int EditPlaybackContext::getLatencySamples() const
{
    return nodePlaybackContext ? nodePlaybackContext->getLatencySamples()
                               : 0;
}

TimePosition EditPlaybackContext::getAudibleTimelineTime()
{
    return nodePlaybackContext ? TimePosition::fromSeconds (audiblePlaybackTime.load())
                               : transport.getPosition();
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

void EditPlaybackContext::setTempoAdjustment (double plusOrMinusProportion)
{
    if (nodePlaybackContext)
        nodePlaybackContext->setTempoAdjustment (plusOrMinusProportion);
}

void EditPlaybackContext::postPosition (TimePosition newPosition)
{
    if (nodePlaybackContext)
        nodePlaybackContext->postPosition (newPosition);
}

void EditPlaybackContext::play()
{
    if (nodePlaybackContext)
        nodePlaybackContext->playHead.play();
}

void EditPlaybackContext::stop()
{
    if (nodePlaybackContext)
        nodePlaybackContext->playHead.stop();
}

TimePosition EditPlaybackContext::globalStreamTimeToEditTime (double globalStreamTime) const
{
    if (! nodePlaybackContext)
        return TimePosition();
    
    const auto sampleRate = getSampleRate();
    const auto globalSamplePos = tracktion::graph::timeToSample (globalStreamTime, sampleRate);
    const auto timelinePosition = nodePlaybackContext->playHead.referenceSamplePositionToTimelinePosition (globalSamplePos);
    
    return TimePosition::fromSamples (timelinePosition, sampleRate);
}

TimePosition EditPlaybackContext::globalStreamTimeToEditTimeUnlooped (double globalStreamTime) const
{
    if (! nodePlaybackContext)
        return TimePosition();
    
    const auto sampleRate = getSampleRate();
    const auto globalSamplePos = tracktion::graph::timeToSample (globalStreamTime, sampleRate);
    const auto timelinePosition = nodePlaybackContext->playHead.referenceSamplePositionToTimelinePositionUnlooped (globalSamplePos);
    
    return TimePosition::fromSamples (timelinePosition, sampleRate);
}

void EditPlaybackContext::resyncToGlobalStreamTime (juce::Range<double> globalStreamTime, double sampleRate)
{
    if (! nodePlaybackContext)
        return;
    
    const auto globalSampleRange = tracktion::graph::timeToSample (globalStreamTime, sampleRate);
    nodePlaybackContext->resyncToReferenceSampleRange (globalSampleRange);
}

void EditPlaybackContext::setThreadPoolStrategy (int type)
{
    type = juce::jlimit (static_cast<int> (tracktion::graph::ThreadPoolStrategy::conditionVariable),
                         static_cast<int> (tracktion::graph::ThreadPoolStrategy::lightweightSemHybrid),
                         type);

    EditPlaybackContextInternal::getThreadPoolStrategyType() = type;
}

int EditPlaybackContext::getThreadPoolStrategy()
{
    const int type = juce::jlimit (static_cast<int> (tracktion::graph::ThreadPoolStrategy::conditionVariable),
                                   static_cast<int> (tracktion::graph::ThreadPoolStrategy::lightweightSemHybrid),
                                   EditPlaybackContextInternal::getThreadPoolStrategyType());
    
    return type;
}

void EditPlaybackContext::enablePooledMemory (bool enable)
{
    EditPlaybackContextInternal::getPooledMemoryFlag() = enable;
}

//==============================================================================
static int numHighPriorityPlayers = 0, numRealtimeDefeaters = 0;

inline void updateProcessPriority (Engine& engine)
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

}} // namespace tracktion { inline namespace engine
