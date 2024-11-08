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

    inline bool& getNodeMemorySharingFlag()
    {
        static bool useSharing = false;
        return useSharing;
    }

    inline bool& getAudioWorkgroupFlag()
    {
        static bool useAudioWorkgroup = false;
        return useAudioWorkgroup;
    }

    inline juce::AudioWorkgroup getAudioWorkgroupIfEnabled (Engine& e)
    {
        if (! getAudioWorkgroupFlag())
            return {};

        return e.getDeviceManager().deviceManager.getDeviceAudioWorkgroup();
    }

    inline size_t getMaxNumThreadsToUse (Edit& edit)
    {
        if (edit.getIsPreviewEdit())
            return 0;

        auto wg = getAudioWorkgroupIfEnabled (edit.engine);
        return wg ? wg.getMaxParallelThreadCount() - 1
                  : static_cast<size_t> (juce::SystemStats::getNumCpus() - 1);
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
     NodePlaybackContext (EditPlaybackContext& epc, size_t numThreads, size_t maxNumThreadsToUse)
         : editPlaybackContext (epc),
           player (processState,
                   getPoolCreatorFunction (static_cast<tracktion::graph::ThreadPoolStrategy> (getThreadPoolStrategy())),
                   EditPlaybackContextInternal::getAudioWorkgroupIfEnabled (tempoSequence.edit.engine)),
           maxNumThreads (maxNumThreadsToUse)
     {
         processState.onContinuityUpdated = [this]
             {
                 const auto syncRange = processState.getSyncRange();
                 const auto editTime = syncRange.start.time;
                 editPlaybackContext.edit.updateModifierTimers (editTime, static_cast<int> (getNumSamples (syncRange)));
                 editPlaybackContext.midiDispatcher.masterTimeUpdate (editTime);

                #if TRACKTION_ENABLE_ABLETON_LINK
                 editPlaybackContext.edit.getAbletonLink().syncronise (editTime);
                #endif
             };

         setNumThreads (numThreads);
         player.enablePooledMemoryAllocations (EditPlaybackContextInternal::getPooledMemoryFlag());
         player.enableNodeMemorySharing (EditPlaybackContextInternal::getNodeMemorySharingFlag());
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

     void postPlay()
     {
         playPending.store (true, std::memory_order_release);
     }

     bool isPlayPending() const
     {
         return playPending.load (std::memory_order_acquire);
     }

     void postPosition (TimePosition positionToJumpTo, std::optional<TimePosition> whenToJump)
     {
         pendingPosition.store (positionToJumpTo.inSeconds(), std::memory_order_release);

         if (whenToJump)
         {
             pendingPositionJumpTime.store (whenToJump->inSeconds(), std::memory_order_release);
             pendingPositionJumpTimeValid.store (true, std::memory_order_release);
         }
         else
         {
             pendingPositionJumpTimeValid.store (false, std::memory_order_release);
         }

         pendingRollInToLoop.store (false, std::memory_order_release);
         positionUpdatePending = true;
     }

     std::optional<TimePosition> getPendingPositionChange() const
     {
         if (! positionUpdatePending.load (std::memory_order_relaxed))
             return {};

         return TimePosition::fromSeconds (pendingPosition.load (std::memory_order_relaxed));
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

     void checkForTempoSequenceChanges()
     {
         const auto& internalSequence = tempoSequence.getInternalSequence();

         if (internalSequence.hash() == tempoState.hash)
             return;

         const auto lastPositionRemapped = internalSequence.toTime (tempoState.lastBeatPosition);
         const auto lastSampleRemapped = toSamples (lastPositionRemapped, getSampleRate());
         playHead.overridePosition (lastSampleRemapped);
     }

     void nextBlockStarted()
     {
         // Dispatch pending play
         if (playPending.exchange (false, std::memory_order_acquire))
             playHead.play();
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

         // This needs to be called after the playhead reference range has been set above
         checkForTempoSequenceChanges();
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
         const auto referenceSampleRange = getReferenceSampleRange();         // Distpatch pending positions

         if (positionUpdatePending.load (std::memory_order_acquire))
         {
             const double sampleRate = getSampleRate();
             bool shouldPerformPositionChange = true;

             if (pendingPositionJumpTimeValid)
             {
                 const auto currentTimeSeconds = sampleToTime (juce::Range<int64_t>::withStartAndLength (playHead.getPosition(), referenceSampleRange.getLength()), sampleRate);
                 const auto jumpTimeSeconds = pendingPositionJumpTime.load (std::memory_order_acquire);

                 // Check if loop end time is in this block and if it is, cancel the jump
                 const bool loopEndIsInThisBlock = playHead.isLooping()
                    && currentTimeSeconds.contains (sampleToTime (playHead.getLoopRange().getEnd(), sampleRate));

                 if (loopEndIsInThisBlock)
                 {
                     pendingPositionJumpTimeValid = false;
                     pendingPositionJumpTime = 0.0;

                     pendingPosition = 0.0;
                     positionUpdatePending = false;

                     shouldPerformPositionChange = false;
                 }

                 if (currentTimeSeconds.contains (jumpTimeSeconds))
                 {
                     pendingPositionJumpTimeValid = false;
                     pendingPositionJumpTime = 0.0;
                 }
                 else
                 {
                     shouldPerformPositionChange = false;
                 }
             }

             if (shouldPerformPositionChange)
             {
                 if (positionUpdatePending.exchange (false))
                 {
                     const auto samplePos = timeToSample (pendingPosition.load (std::memory_order_acquire), sampleRate);

                     if (pendingRollInToLoop.load (std::memory_order_acquire))
                         playHead.setRollInToLoop (samplePos);
                     else
                         playHead.setPosition (samplePos);
                 }
             }
         }

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

         tempoState = { tempoSequence.getInternalSequence().hash(),
                        processState.editBeatRange.getEnd() };
     }

     double getSampleRate() const
     {
         return player.getSampleRate();
     }

     SyncPoint getSyncPoint() const
     {
         return processState.getSyncPoint();
     }

     EditPlaybackContext& editPlaybackContext;
     const TempoSequence& tempoSequence { editPlaybackContext.edit.tempoSequence };
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
     std::atomic<double> pendingPosition { 0.0 }, pendingPositionJumpTime { 0.0 };
     std::atomic<bool> positionUpdatePending { false }, pendingRollInToLoop { false }, pendingPositionJumpTimeValid { false }, playPending { false };
     double speedCompensation = 0.0, blockLengthScaleFactor = 1.0;
     std::vector<std::unique_ptr<juce::LagrangeInterpolator>> interpolators;
     bool isUsingInterpolator = false;

     struct TempoState
     {
         size_t hash = 0;
         BeatPosition lastBeatPosition;
     };

     TempoState tempoState;

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
        nodePlaybackContext = std::make_unique<NodePlaybackContext> (*this,
                                                                     edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio(),
                                                                     EditPlaybackContextInternal::getMaxNumThreadsToUse (edit));
        contextSyncroniser = std::make_unique<ContextSyncroniser>();

        // This ensures the referenceSampleRange of the new context has been synced
        edit.engine.getDeviceManager().addContext (this);

        // Set the playhead position as early as possible so it doesn't revert to 0 in the TransportControl
        nodePlaybackContext->playHead.setPosition (toSamples (transport.getPosition(),
                                                              edit.engine.getDeviceManager().getSampleRate()));
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

    {
        InputDeviceInstance::StopRecordingParameters params;
        params.discardRecordings = true;

        for (auto mi : midiInputs)
            mi->prepareToStopRecording (params.targetsToStop);

        for (auto mi : midiInputs)
            mi->stopRecording (params);

        for (auto wi : waveInputs)
            wi->prepareToStopRecording (params.targetsToStop);

        for (auto wi : waveInputs)
            wi->stopRecording (params);
    }

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
    audiblePlaybackTime = transport.getPosition().inSeconds();

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

    auto& engineBehaviour = edit.engine.getEngineBehaviour();
    cnp.includeBypassedPlugins = ! engineBehaviour.shouldBypassedPluginsBeRemovedFromPlaybackGraph();
    cnp.allowClipSlots = engineBehaviour.areClipSlotsEnabled();
    cnp.readAheadTimeStretchNodes = engineBehaviour.enableReadAheadForTimeStretchNodes();
    auto editNode = createNodeForEdit (*this, audiblePlaybackTime, cnp);

    nodePlaybackContext->setNode (std::move (editNode), cnp.sampleRate, cnp.blockSize);
    updateNumCPUs();
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
        priorityBooster = std::make_unique<ProcessPriorityBooster> (edit.engine);

    for (auto mo : midiOutputs)
        mo->start();
}

juce::Result EditPlaybackContext::startRecording (TimePosition start, TimePosition punchIn)
{
    struct InputAndContext
    {
        InputDeviceInstance* input = nullptr;
        InputDeviceInstance::PreparedContext preparedContext;
    };

    std::vector<InputAndContext> inputAndContexts;

    auto anyContextHasErrors = [&inputAndContexts]
    {
        for (auto& inputAndContext : inputAndContexts)
            if (hasErrors (inputAndContext.preparedContext))
                return true;

        return false;
    };

    for (int i = waveInputs.size(); --i >= 0 && ! anyContextHasErrors();)
        if (auto wi = waveInputs.getUnchecked (i))
            if (wi->isRecordingActive())
                inputAndContexts.push_back ({ wi, wi->prepareToRecord (getDefaultRecordingParameters (*this, start, punchIn)) });

    for (int i = midiInputs.size(); --i >= 0 && ! anyContextHasErrors();)
        if (auto mi = midiInputs.getUnchecked (i))
            if (mi->isRecordingActive())
                inputAndContexts.push_back ({ mi, mi->prepareToRecord (getDefaultRecordingParameters (*this, start, punchIn)) });

    // Check if any devices started
    {
        bool anyContexts = false;

        for (auto& inputAndContext : inputAndContexts)
            if (! inputAndContext.preparedContext.empty())
                anyContexts = true;

        if (! anyContexts)
            return juce::Result::fail (TRANS("Failed to start recording: No input devices"));
    }

    // Check if any had errors
    for (auto& inputAndContext : inputAndContexts)
        for (auto& res : inputAndContext.preparedContext)
            if (! res)
                return juce::Result::fail (res.error());

    // Now start the recordings
    startPlaying (start);

    for (auto& inputAndContext : inputAndContexts)
    {
        if (! inputAndContext.input->isRecordingActive())
            continue;

        [[ maybe_unused ]] auto [preparedContext, error] = extract (std::move (inputAndContext.preparedContext));
        [[ maybe_unused ]] auto contextsLeft = inputAndContext.input->startRecording (std::move (preparedContext));
        jassert (contextsLeft.empty());
    }

    return juce::Result::ok();
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

tl::expected<Clip::Array, juce::String> EditPlaybackContext::stopRecording (InputDeviceInstance& in, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    InputDeviceInstance::StopRecordingParameters params;
    params.unloopedTimeToEndRecording = getUnloopedPosition();
    params.isLooping = transport.looping;
    params.markedRange = transport.getLoopRange();
    params.discardRecordings = discardRecordings;

    in.prepareToStopRecording (params.targetsToStop);

    return in.stopRecording (params);
}

tl::expected<Clip::Array, juce::String> EditPlaybackContext::stopRecording (TimePosition unloopedEnd, bool discardRecordings)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    Clip::Array clips;
    juce::String error;

    InputDeviceInstance::StopRecordingParameters params;
    params.unloopedTimeToEndRecording = unloopedEnd;
    params.isLooping = transport.looping;
    params.markedRange = transport.getLoopRange();
    params.discardRecordings = discardRecordings;

    auto allInputs = getAllInputs();

    // Prepare all to stop first to avoid extra audio blocks
    for (auto in : allInputs)
        in->prepareToStopRecording (params.targetsToStop);

    for (auto in : allInputs)
        in->stopRecording (params)
            .map ([&] (auto c) { clips.addArray (std::move (c)); })
            .map_error ([&] (auto err) { error = err; });

    if (! error.isEmpty())
        return tl::unexpected (error);

    return clips;
}

juce::Result EditPlaybackContext::applyRetrospectiveRecord (juce::Array<Clip*>* clips, bool armedOnly)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    bool inputAssigned = false;

    for (auto in : getAllInputs())
    {
        if (isAttached (*in) && (! armedOnly || in->isRecordingActive()))
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
        for (auto clip : in->applyRetrospectiveRecord (armedOnly))
        {
            if (clips != nullptr)
                clips->add (clip);

            clipCreated = true;
        }
    }

    InputDevice::setRetrospectiveLock (edit.engine, getAllInputs(), false);

    if (! clipCreated)
        return juce::Result::fail (TRANS("Unable to perform retrospective record, all input buffers are empty"));

    if (clips != nullptr)
    {
        if (auto sm = findAppropriateSelectionManager (edit))
        {
            sm->select (*clips);
            sm->keepSelectedObjectsOnScreen();
        }
    }

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
void EditPlaybackContext::nextBlockStarted()
{
    CRASH_TRACER

    if (edit.isRendering())
        return;

    SCOPED_REALTIME_CHECK
    if (! nodePlaybackContext)
        return;

    nodePlaybackContext->nextBlockStarted();
}

void EditPlaybackContext::fillNextNodeBlock (float* const* allChannels, int numChannels, int numSamples)
{
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

    nodePlaybackContext->process (allChannels, numChannels, numSamples);

    // Dispatch any MIDI messages that have been injected in to the MidiOutputDeviceInstances by the Node
    auto editTime = nodePlaybackContext->processState.getSyncRange().start.time;
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

//==============================================================================
tracktion::graph::PlayHead* EditPlaybackContext::getNodePlayHead() const
{
    //TODO  can this be removed?
    return nodePlaybackContext ? &nodePlaybackContext->playHead
                               : nullptr;
}

void EditPlaybackContext::blockUntilSyncPointChange()
{
    if (const auto startSyncPoint = getSyncPoint())
    {
        for (const auto startTime = std::chrono::steady_clock::now();;)
        {
            const auto syncPointNow = getSyncPoint();

            if (! syncPointNow)
                break;

            if (syncPointNow->referenceSamplePosition != startSyncPoint->referenceSamplePosition)
                break;

            // This probably means something has gone wrong with the audio device and it's not playing
            // back anymore but appears valid so we don't want to block indefinitely
            if ((std::chrono::steady_clock::now() - startTime) > 100ms)
                break;

            std::this_thread::sleep_for (1us);
        }
    }
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

void EditPlaybackContext::postPosition (TimePosition positionToJumpTo, std::optional<TimePosition> whenToJump)
{
    if (nodePlaybackContext)
    {
        if (whenToJump && *whenToJump == positionToJumpTo)
            nodePlaybackContext->postPosition (positionToJumpTo, {});
        else
            nodePlaybackContext->postPosition (positionToJumpTo, whenToJump);
    }
}

std::optional<TimePosition> EditPlaybackContext::getPendingPositionChange() const
{
    if (nodePlaybackContext)
        return nodePlaybackContext->getPendingPositionChange();

    return {};
}

void EditPlaybackContext::play()
{
    if (nodePlaybackContext)
        nodePlaybackContext->playHead.play();
}

void EditPlaybackContext::postPlay()
{
    if (nodePlaybackContext)
        nodePlaybackContext->postPlay();
}

bool EditPlaybackContext::isPlayPending() const
{
    if (nodePlaybackContext)
        return nodePlaybackContext->isPlayPending();

    return false;
}

void EditPlaybackContext::stop()
{
    if (nodePlaybackContext)
        nodePlaybackContext->playHead.stop();
}

std::optional<SyncPoint> EditPlaybackContext::getSyncPoint() const
{
    if (nodePlaybackContext)
        return nodePlaybackContext->getSyncPoint();

    return {};
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

void EditPlaybackContext::enableNodeMemorySharing (bool enable)
{
    EditPlaybackContextInternal::getNodeMemorySharingFlag() = enable;
}

void EditPlaybackContext::enableAudioWorkgroup (bool enable)
{
    EditPlaybackContextInternal::getAudioWorkgroupFlag() = enable;
}

int EditPlaybackContext::getNumActivelyRecordingDevices() const
{
    return activelyRecordingInputDevices.load (std::memory_order_acquire);
}

void EditPlaybackContext::incrementNumActivelyRecordingDevices()
{
    activelyRecordingInputDevices.fetch_add (1, std::memory_order_acq_rel);
}

void EditPlaybackContext::decrementNumActivelyRecordingDevices()
{
    activelyRecordingInputDevices.fetch_sub (1, std::memory_order_acq_rel);
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
