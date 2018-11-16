/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


Edit::GlobalMacros::GlobalMacros (Edit& e)
    : MacroParameterElement (e, e.state),
      edit (e)
{
}

//==============================================================================
struct Edit::UndoTransactionTimer   : private Timer,
                                      private ChangeListener
{
    UndoTransactionTimer (Edit& e)
        : edit (e)
    {
        callBlocking ([&] { edit.getUndoManager().addChangeListener (this); });
    }

    ~UndoTransactionTimer()
    {
        edit.getUndoManager().removeChangeListener (this);
    }

    void timerCallback() override
    {
        if (edit.numUndoTransactionInhibitors > 0)
            return;

        if (! Component::isMouseButtonDownAnywhere())
        {
            stopTimer();
            edit.getUndoManager().beginNewTransaction();
        }
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        if (Time::getCurrentTime() > timeToStartTransactions)
        {
            edit.markAsChanged();
            startTimer (350);
        }
    }

    Edit& edit;
    const Time timeToStartTransactions { Time::getCurrentTime() + RelativeTime::seconds (5.0) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoTransactionTimer)
};

//==============================================================================
struct Edit::EditChangeResetterTimer  : private Timer
{
    EditChangeResetterTimer (Edit& ed)  : edit (ed)
    {
        startTimer (200);
    }

    void timerCallback() override
    {
        stopTimer();
        edit.resetChangedStatus();
        edit.changeResetterTimer.reset();
    }

    Edit& edit;
};

//==============================================================================
struct Edit::TreeWatcher   : public juce::ValueTree::Listener
{
    TreeWatcher (Edit& ed, const juce::ValueTree& v)  : edit (ed), state (v)
    {
        state.addListener (this);
    }

    Edit& edit;
    juce::ValueTree state;

    void valueTreePropertyChanged (juce::ValueTree& v, const Identifier& i) override
    {
        if (v.hasType (IDs::TRANSPORT))
        {
            if (i == IDs::recordPunchInOut)
            {
                auto& ecm = edit.engine.getExternalControllerManager();

                if (ecm.isAttachedToEdit (edit))
                    ecm.updatePunchLights();
            }
            else if (i == IDs::endToEnd)
            {
                if (! edit.getTransport().isPlaying())
                {
                    edit.getTransport().ensureContextAllocated();

                    auto& ecm = edit.engine.getExternalControllerManager();

                    if (ecm.isAttachedToEdit (edit))
                        ecm.updateAllDevices();
                }
            }
        }
        else
        {
            if (TrackList::isTrack (v))
            {
                if (i == IDs::frozenIndividually || i == IDs::compGroup)
                {
                    restart();
                }
                else if (i == IDs::frozen)
                {
                    edit.needToUpdateFrozenTracks();
                }
                else if (i == IDs::process)
                {
                    restart();
                }
                else if (i == IDs::mute || i == IDs::solo || i == IDs::soloIsolate)
                {
                    edit.updateMuteSoloStatuses();
                    edit.markAsChanged();
                }
            }
            else if (Clip::isClipState (v))
            {
                if (i == IDs::start || i == IDs::length || i == IDs::offset)
                {
                    clipMovedOrAdded (v);
                }
                else if (i == IDs::linkID)
                {
                    linkedClipsChanged();
                }
                else if (i == IDs::speed || i == IDs::source  || i == IDs::mpeMode
                         || i == IDs::fadeInType || i == IDs::fadeOutType
                         || i == IDs::fadeInBehaviour || i == IDs::fadeOutBehaviour
                         || i == IDs::fadeIn || i == IDs::fadeOut
                         || i == IDs::loopStart || i == IDs::loopLength
                         || i == IDs::loopStartBeats || i == IDs::loopLengthBeats
                         || i == IDs::transpose || i == IDs::pitchChange
                         || i == IDs::elastiqueMode || i == IDs::elastiqueOptions
                         || i == IDs::autoPitch || i == IDs::autoTempo
                         || i == IDs::channels || i == IDs::isReversed
                         || i == IDs::currentTake || i == IDs::sequence || i == IDs::repeatSequence
                         || i == IDs::loopedSequenceType)
                {
                    restart();
                }
            }
            else if (v.hasType (IDs::COMPSECTION))
            {
                restart();
            }
            else if (v.hasType (IDs::WARPMARKER))
            {
                if (i == IDs::sourceTime || i == IDs::warpTime)
                    restart();
            }
            else if (v.hasType (IDs::QUANTISATION))
            {
                if (i == IDs::type || i == IDs::amount)
                    restart();
            }
            else if (v.hasType (IDs::NOTE) || v.hasType (IDs::CONTROL) || v.hasType (IDs::SYSEX))
            {
                if (i != IDs::c)
                    restart();
            }
            else if (MidiExpression::isExpression (v.getType()))
            {
                if (i == IDs::b || i == IDs::v)
                    restart();
            }
            else if (v.hasType (IDs::CHANNEL))
            {
                if (i == IDs::pattern || i == IDs::channel
                     || i == IDs::velocities || i == IDs::gates
                     || i == IDs::note || i == IDs::velocity || i == IDs::groove)
                    restart();
            }
            else if (v.hasType (IDs::PATTERN))
            {
                if (i == IDs::noteLength || i == IDs::numNotes)
                    restart();
            }
            else if (v.hasType (IDs::SEQUENCE))
            {
                if (i == IDs::channelNumber)
                    restart();
            }
            else if (v.hasType (IDs::GROOVE))
            {
                if (i == IDs::current)
                    restart();
            }
            else if (v.hasType (IDs::TAKES))
            {
                if (i == IDs::currentTake)
                    restart();
            }
            else if (v.hasType (IDs::SIDECHAINCONNECTION))
            {
                restart();
            }
            else if (v.hasType (IDs::CLICKTRACK))
            {
                if (i == IDs::active)
                {
                    auto& ecm = edit.engine.getExternalControllerManager();

                    if (ecm.isAttachedToEdit (edit))
                        ecm.clickChanged (edit.clickTrackEnabled);
                }
            }
            else if (v.hasType (IDs::TEMPO) || v.hasType (IDs::TIMESIG))
            {
                restart();
                edit.invalidateStoredLength();
            }
            else if (v.hasType (IDs::PLUGIN))
            {
                if (i == IDs::outputDevice || i == IDs::manualAdjustMs || i == IDs::sidechainSourceID || i == IDs::ignoreVca)
                    restart();
            }
            else if (v.hasType (IDs::MASTERVOLUME))
            {
                if (i == IDs::fadeIn || i == IDs::fadeOut
                     || i == IDs::fadeInType || i == IDs::fadeOutType)
                    restart();
            }
        }
    }

    void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c) override
    {
        childAddedOrRemoved (p, c);
    }

    void valueTreeChildRemoved (ValueTree& p, ValueTree& c, int) override
    {
        childAddedOrRemoved (p, c);
    }

    void childAddedOrRemoved (ValueTree& p, ValueTree& c)
    {
        if (c.hasType (IDs::NOTE)
             || c.hasType (IDs::CONTROL)
             || c.hasType (IDs::SYSEX)
             || c.hasType (IDs::PLUGIN)
             || c.hasType (IDs::RACK)
             || c.hasType (IDs::PLUGININSTANCE)
             || c.hasType (IDs::CONNECTION)
             || c.hasType (IDs::CHANNELS)
             || c.hasType (IDs::SIDECHAINCONNECTION)
             || c.hasType (IDs::MACROPARAMETERS)
             || c.hasType (IDs::MACROPARAMETER)
             || p.hasType (IDs::MAPPEDPARAMETER)
             || c.hasType (IDs::LFO)
             || c.hasType (IDs::BREAKPOINTOSCILLATOR)
             || c.hasType (IDs::STEP)
             || c.hasType (IDs::ENVELOPEFOLLOWER)
             || c.hasType (IDs::RANDOM)
             || c.hasType (IDs::MIDITRACKER)
             || p.hasType (IDs::LOOPINFO)
             || p.hasType (IDs::PATTERN))
        {
            restart();
        }
        else if (Clip::isClipState (c))
        {
            clipMovedOrAdded (c);
            linkedClipsChanged();
        }
        else if (TrackList::isTrack (c))
        {
            restart();
            edit.invalidateStoredLength();
        }
        else if (c.hasType (IDs::WARPMARKER))
        {
            restart();
        }
        else if (p.hasType (IDs::TRACKCOMP))
        {
            restart();
        }
        else if (p.hasType (IDs::TEMPOSEQUENCE))
        {
            restart();
            edit.invalidateStoredLength();
        }
        else if (p.hasType (IDs::NOTE))
        {
            restart();
        }
    }

    void valueTreeChildOrderChanged (ValueTree&, int, int) override {}
    void valueTreeParentChanged (ValueTree&) override {}

    void clipMovedOrAdded (const ValueTree& v)
    {
        edit.invalidateStoredLength();

        if (v.hasType (IDs::AUDIOCLIP)
             || v.hasType (IDs::MIDICLIP)
             || v.hasType (IDs::STEPCLIP)
             || v.hasType (IDs::EDITCLIP)
             || v.hasType (IDs::CHORDCLIP))
            restart();
    }

    void restart()
    {
        edit.restartPlayback();
    }

    void updateTrackStatusesAsync()
    {
        if (trackStatusUpdater == nullptr)
            trackStatusUpdater = new TrackStatusUpdater (*this);
    }

    struct TrackStatusUpdater  : private AsyncUpdater
    {
        TrackStatusUpdater (TreeWatcher& o) : owner (o) { triggerAsyncUpdate(); }

        void handleAsyncUpdate() override
        {
            if (! owner.edit.isLoading())
                owner.edit.updateTrackStatuses();

            owner.trackStatusUpdater = nullptr;
        }

        TreeWatcher& owner;
    };

    ScopedPointer<TrackStatusUpdater> trackStatusUpdater;

    //==============================================================================
    void linkedClipsChanged()
    {
        linkedClipsMapDirty = true;
    }

    Array<Clip*> getClipsInLinkGroup (EditItemID linkGroupid)
    {
        jassert (! edit.isLoading());

        if (linkedClipsMapDirty)
        {
            linkedClipsMap.clear();

            edit.visitAllTracksRecursive ([&] (Track& t)
            {
                if (auto ct = dynamic_cast<ClipTrack*> (&t))
                    for (auto c : ct->getClips())
                        if (c->getLinkGroupID().isValid())
                            linkedClipsMap[c->getLinkGroupID()].add (c);

                return true;
            });

            linkedClipsMapDirty = false;
        }

        return linkedClipsMap[linkGroupid];
    }

    void messageThreadAssertIfLoaded() const
    {
        if (! edit.isLoading())
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
        }
    }

    bool linkedClipsMapDirty = true;
    std::map<EditItemID, Array<Clip*>> linkedClipsMap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TreeWatcher)
};

//==============================================================================
struct Edit::ChangedPluginsList
{
    ChangedPluginsList() = default;

    /** Call to indicate a plugin's state has changed and needs saving. */
    void pluginChanged (Plugin& p)
    {
        plugins.addIfNotAlreadyThere (Plugin::WeakRef (&p));
    }

    /** Flushes the plugin state to the Edit if it has changed. */
    void flushPluginStateIfNeeded (Plugin& p)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        const int index = plugins.indexOf (&p);

        if (index != -1)
        {
            p.flushPluginStateToValueTree();
            plugins.remove (index);
        }
    }

    /** Flushes all changed plugins to the Edit. */
    void flushAll()
    {
        for (auto p : plugins)
            if (p != nullptr)
                p->flushPluginStateToValueTree();

        plugins.clear();
    }

private:
    friend class Edit;
    juce::Array<Plugin::WeakRef> plugins;
};

//==============================================================================
struct Edit::FrozenTrackCallback : public juce::AsyncUpdater
{
    FrozenTrackCallback (Edit& ed) : edit (ed) {}
    void handleAsyncUpdate() override  { edit.updateFrozenTracks(); }
    Edit& edit;
};

struct Edit::PluginChangeTimer : public Timer
{
    PluginChangeTimer (Edit& ed) : edit (ed) {}
    void pluginChanged() noexcept       { startTimer (500); }

    void timerCallback() override
    {
        edit.markAsChanged();
        stopTimer();
    }

    Edit& edit;
};

struct Edit::MirroredPluginUpdateTimer  : public Timer
{
    MirroredPluginUpdateTimer (Edit& ed) : edit (ed) {}
    void timerCallback() override       { edit.updateMirroredPlugins(); }

    Edit& edit;
    juce::Array<Plugin::WeakRef> changedPlugins;
};

//==============================================================================
static int getNextInstanceId() noexcept
{
    static Atomic<int> nextId;
    return ++nextId;
}

//==============================================================================
Edit::Edit (Engine& e, ValueTree editState, EditRole role,
            LoadContext* sourceLoadContext, int numUndoLevelsToStore)
    : engine (e),
      tempoSequence (*this),
      state (editState),
      instanceId (getNextInstanceId()),
      editProjectItemID (ProjectItemID::fromProperty (editState, IDs::projectID)),
      loadContext (sourceLoadContext),
      editRole (role)
{
    CRASH_TRACER

    jassert (state.isValid());
    jassert (state.hasType (IDs::EDIT));
    jassert (editProjectItemID.isValid()); // is it ever OK for this to be invalid?

    editFileRetriever = [this]() -> File
    {
        if (auto item = ProjectManager::getInstance()->getProjectItem (*this))
            return item->getSourceFile();

        return {};
    };

    filePathResolver = [this] (const String& path) -> File
    {
        jassert (path.isNotEmpty());

        if (File::isAbsolutePath (path))
            return path;

        if (editFileRetriever)
            return editFileRetriever().getSiblingFile (path);

        return {};
    };

    pluginCache                 = std::make_unique<PluginCache> (*this);
    mirroredPluginUpdateTimer   = std::make_unique<MirroredPluginUpdateTimer> (*this);
    transportControl            = std::make_unique<TransportControl> (*this, state.getOrCreateChildWithName (IDs::TRANSPORT, nullptr));
    abletonLink                 = std::make_unique<AbletonLink> (*transportControl);
    automationRecordManager     = std::make_unique<AutomationRecordManager> (*this);
    markerManager               = std::make_unique<MarkerManager> (*this, state.getOrCreateChildWithName (IDs::MARKERTRACK, nullptr));
    pluginChangeTimer           = std::make_unique<PluginChangeTimer> (*this);
    frozenTrackCallback         = std::make_unique<FrozenTrackCallback> (*this);
    masterPluginList            = std::make_unique<PluginList> (*this);
    parameterChangeHandler      = std::make_unique<ParameterChangeHandler> (*this);
    parameterControlMappings    = std::make_unique<ParameterControlMappings> (*this);
    rackTypes                   = std::make_unique<RackTypeList> (*this);
    trackCompManager            = std::make_unique<TrackCompManager> (*this);
    changedPluginsList          = std::make_unique<ChangedPluginsList>();

    undoManager.setMaxNumberOfStoredUnits (1000 * numUndoLevelsToStore, numUndoLevelsToStore);

    initialise();

    undoTransactionTimer = std::make_unique<UndoTransactionTimer> (*this);

    if (loadContext != nullptr && ! loadContext->shouldExit)
    {
        loadContext->completed = true;
        loadContext = nullptr;
    }

    // Don't spam logs with preview Edits
    if (getProjectItemID().getProjectID() != 0)
        TRACKTION_LOG ("Loaded edit: " + getName());

    jassert (! engine.getActiveEdits().edits.contains (this));
    engine.getActiveEdits().edits.add (this);

    isFullyConstructed.store (true, std::memory_order_relaxed);
}

Edit::~Edit()
{
    CRASH_TRACER

    for (auto af : pluginCache->getPlugins())
        if (auto ep = dynamic_cast<ExternalPlugin*> (af))
            ep->hideWindowForShutdown();

    engine.getActiveEdits().edits.removeFirstMatchingValue (this);
    masterReference.clear();
    changeResetterTimer.reset();

    if (transportControl != nullptr)
        transportControl->freePlaybackContext();

    // must only delete an edit with the message thread locked - many things on the message thread may be in the
    // middle of using it at this point..
    jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager());
    jassert (! getTransport().isPlayContextActive());
    jassert (numUndoTransactionInhibitors == 0);

    cancelAllProxyGeneratorJobs();
    changedPluginsList.reset();
    editInputDevices.reset();
    treeWatcher.reset();

    notifyListenersOfDeletion();

    engine.getExternalControllerManager().detachFromEdit (this);

    pluginChangeTimer.reset();

    undoManager.clearUndoHistory();

    for (auto rt : rackTypes->getTypes())
        rt->hideWindowForShutdown();

    globalMacros.reset();
    masterVolumePlugin.reset();
    masterPluginList->releaseObjects();
    masterPluginList.reset();
    trackList.reset();
    mirroredPluginUpdateTimer.reset();
    rackTypes.reset();
    undoTransactionTimer.reset();
    markerManager.reset();
    araDocument.reset();
    frozenTrackCallback.reset();

    pitchSequence.freeResources();
    tempoSequence.freeResources();

    pluginCache.reset();
}

//==============================================================================
String Edit::getName()
{
    if (auto item = ProjectManager::getInstance()->getProjectItem (*this))
        return item->getName();

    return {};
}

void Edit::setProjectItemID (ProjectItemID newID)
{
    editProjectItemID = newID;
    state.setProperty (IDs::projectID, editProjectItemID, nullptr);
}

Edit::ScopedRenderStatus::ScopedRenderStatus (Edit& ed)  : edit (ed)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    edit.performingRender = true;
    edit.getTransport().freePlaybackContext();
}

Edit::ScopedRenderStatus::~ScopedRenderStatus()
{
    edit.performingRender = false;

    if (edit.shouldPlay())
        edit.getTransport().ensureContextAllocated();
}

//==============================================================================
void Edit::initialise()
{
    CRASH_TRACER
    const StopwatchTimer loadTimer;

    if (loadContext != nullptr)
        loadContext->progress = 0.0f;

    treeWatcher = std::make_unique<TreeWatcher> (*this, state);

    isLoadInProgress = true;
    tempDirectory = juce::File();

    if (! state.hasProperty (IDs::creationTime))
        state.setProperty (IDs::creationTime, Time::getCurrentTime().toMilliseconds(), nullptr);

    lastSignificantChange.referTo (state, IDs::lastSignificantChange, nullptr,
                                   String::toHexString (Time::getCurrentTime().toMilliseconds()));

    globalMacros = std::make_unique<GlobalMacros> (*this);
    initialiseTempoAndPitch();
    initialiseTransport();
    initialiseVideo();
    initialiseAutomap();
    initialiseClickTrack();
    initialiseMetadata();
    initialiseMasterVolume();
    initialiseRacks();
    initialiseMasterPlugins();
    initialiseAuxBusses();
    initialiseAudioDevices();
    loadTracks();

    if (loadContext != nullptr)
        loadContext->progress = 1.0f;

    initialiseTracks();
    initialiseARA();
    updateMuteSoloStatuses();
    readFrozenTracksFiles();

    getLength(); // forcibly update the length before the isLoadInProgress is disabled.

    for (auto t : getAllTracks (*this))
        t->cancelAnyPendingUpdates();

    initialiseControllerMappings();
    purgeOrphanFreezeAndProxyFiles();

    callBlocking ([this]
                  {
                      // Must be set to false before curve updates
                      // but set inside here to give the message loop some time to dispatch async updates
                      isLoadInProgress = false;

                      for (auto mpl : getAllMacroParameterLists (*this))
                          for (auto mp : mpl->getMacroParameters())
                              mp->initialise();

                      for (auto ap : getAllAutomatableParams (true))
                          ap->updateStream();
                  });

    cancelAnyPendingUpdates();

    // reset the change status asynchronously to take into account deferred updates
    changeResetterTimer = std::make_unique<EditChangeResetterTimer> (*this);

   #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
    if (shouldPlay())
        if (auto na = engine.getExternalControllerManager().getAutomap())
            na->load (*this);
   #endif

    getUndoManager().clearUndoHistory();

    DBG ("Edit loaded in: " << loadTimer.getDescription());
}

void Edit::initialiseTempoAndPitch()
{
    const bool needToLoadOldTempoData = ! state.getChildWithName (IDs::TEMPOSEQUENCE).isValid();
    tempoSequence.setState (state.getOrCreateChildWithName (IDs::TEMPOSEQUENCE, nullptr));

    if (needToLoadOldTempoData)
        loadOldTimeSigInfo();

    pitchSequence.initialise (*this, state.getOrCreateChildWithName (IDs::PITCHSEQUENCE, nullptr));
}

void Edit::initialiseTimecode (ValueTree& transportState)
{
    timecodeFormat.referTo (state, IDs::timecodeFormat, nullptr);

    recordingPunchInOut.referTo (transportState, IDs::recordPunchInOut, nullptr);
    playInStopEnabled.referTo (transportState, IDs::endToEnd, nullptr, true);

    timecodeOffset.referTo (transportState, IDs::midiTimecodeOffset, nullptr);
    midiTimecodeSourceDeviceEnabled.referTo (transportState, IDs::midiTimecodeEnabled, nullptr);
    midiTimecodeIgnoringHours.referTo (transportState, IDs::midiTimecodeIgnoringHours, nullptr);
    midiTimecodeSourceDevice.referTo (transportState, IDs::midiTimecodeSourceDevice, nullptr);
    midiMachineControlSourceDevice.referTo (transportState, IDs::midiMachineControlSourceDevice, nullptr);
    midiMachineControlDestDevice.referTo (transportState, IDs::midiMachineControlDestDevice, nullptr);
}

void Edit::initialiseTransport()
{
    auto transportState = state.getOrCreateChildWithName (IDs::TRANSPORT, nullptr);
    initialiseTimecode (transportState);
}

void Edit::initialiseMetadata()
{
    auto meta = state.getOrCreateChildWithName (IDs::ID3VORBISMETADATA, nullptr);

    if (meta.getNumProperties() == 0)
    {
        meta.setProperty (IDs::trackNumber, 1, nullptr);
        meta.setProperty (IDs::date, Time::getCurrentTime().getYear(), nullptr);
    }
}

void Edit::initialiseMasterVolume()
{
    auto* um = &undoManager;

    auto mvTree = state.getOrCreateChildWithName (IDs::MASTERVOLUME, nullptr);
    auto masterVolState = mvTree.getChildWithName (IDs::PLUGIN);
    auto oldMasterVolState = state.getChildWithName (IDs::PLUGIN);

    if (oldMasterVolState.isValid())
    {
        // To fix a bug with the T6 launch version we need to copy the incorrect master state tree
        copyValueTree (masterVolState, oldMasterVolState, nullptr);
        state.removeChild (oldMasterVolState, nullptr);
        mvTree.addChild (masterVolState, -1, nullptr);
    }

    if (! masterVolState.isValid())
    {
        masterVolState = VolumeAndPanPlugin::create();
        masterVolState.setProperty (IDs::volume, decibelsToVolumeFaderPosition (-3.0f), nullptr);
        mvTree.addChild (masterVolState, -1, nullptr);
    }

    masterVolumePlugin = new VolumeAndPanPlugin (*this, masterVolState, true);

    masterFadeIn.referTo (mvTree, IDs::fadeIn, um);
    masterFadeOut.referTo (mvTree, IDs::fadeOut, um);
    masterFadeInType.referTo (mvTree, IDs::fadeInType, um, AudioFadeCurve::concave);
    masterFadeOutType.referTo (mvTree, IDs::fadeOutType, um, AudioFadeCurve::concave);

    if (mvTree.hasProperty (IDs::left))
    {
        // support for loading legacy edits
        const float masterVolL = (float) mvTree.getProperty (IDs::left,  decibelsToVolumeFaderPosition (0.0f));
        const float masterVolR = (float) mvTree.getProperty (IDs::right, decibelsToVolumeFaderPosition (0.0f));

        const float leftGain  = volumeFaderPositionToGain (masterVolL);
        const float rightGain = volumeFaderPositionToGain (masterVolR);
        const float volGain = (leftGain + rightGain) * 0.5f;

        const float masterVolumeFaderPos = gainToVolumeFaderPosition (volGain);
        const float masterPan = (rightGain - volGain) / volGain;

        getMasterSliderPosParameter()->getCurve().clear();
        getMasterSliderPosParameter()->setParameter (masterVolumeFaderPos, dontSendNotification);

        getMasterPanParameter()->getCurve().clear();
        getMasterPanParameter()->setParameter (masterPan, dontSendNotification);
    }
}

void Edit::initialiseClickTrack()
{
    auto click = state.getOrCreateChildWithName (IDs::CLICKTRACK, nullptr);

    clickTrackGain.referTo (click, IDs::level, nullptr);

    if (clickTrackGain.isUsingDefault())
        clickTrackGain = engine.getPropertyStorage().getProperty (SettingID::lastClickTrackLevel, 0.6);

    clickTrackEnabled.referTo (click, IDs::active, nullptr);
    clickTrackEmphasiseBars.referTo (click, IDs::emphasiseBars, nullptr);
    clickTrackRecordingOnly.referTo (click, IDs::onlyRecording, nullptr);

    clickTrackDevice.referTo (click, IDs::outputDevice, nullptr);
}

void Edit::loadTracks()
{
    trackCompManager->initialise (state.getOrCreateChildWithName (IDs::TRACKCOMPS, nullptr));

    // Make sure tempo + marker tracks are first (their order in the XML may be wrong so sort them now)
    TrackList::sortTracksByType (state, nullptr);

    trackList = std::make_unique<TrackList> (*this, state);
    treeWatcher->linkedClipsChanged();
    updateTrackStatuses();
}

void Edit::removeZeroLengthClips()
{
    Clip::Array clipsToRemove;

    for (auto t : getClipTracks (*this))
        for (auto& c : t->getClips())
            if (c->getPosition().getLength() <= 0.0)
                clipsToRemove.add (c);

    for (auto& c : clipsToRemove)
        c->removeFromParentTrack();
}

void Edit::initialiseTracks()
{
    // If the tempo track hasn't been created yet this is a new Edit
    if (getTempoTrack() == nullptr)
    {
        ensureNumberOfAudioTracks (getProjectItemID().getProjectID() == 0 ? 1 : 8);
        updateTrackStatuses();
    }

    ensureTempoTrack();
    ensureMarkerTrack();
    ensureChordTrack();
    removeZeroLengthClips();
    TrackList::sortTracksByType (state, nullptr);

    // some tracks may have referred to others that were created after them, so give
    // their outputs a chance to catch up now..
    for (auto t : getAudioTracks (*this))
    {
        t->initialise();
        t->getOutput().updateOutput();
    }

    for (auto t : getAllTracks (*this))
        t->refreshCurrentAutoParam();
}

void Edit::initialiseAudioDevices()
{
    inputDeviceState = state.getOrCreateChildWithName (IDs::INPUTDEVICES, nullptr);
    editInputDevices.reset (new EditInputDevices (*this, inputDeviceState));
}

void Edit::initialiseRacks()
{
    rackTypes->initialise (state.getOrCreateChildWithName (IDs::RACKS, nullptr));
}

void Edit::initialiseMasterPlugins()
{
    masterPluginList->initialise (state.getOrCreateChildWithName (IDs::MASTERPLUGINS, nullptr));
}

void Edit::initialiseAuxBusses()
{
    auxBusses = state.getOrCreateChildWithName ("AUXBUSNAMES", nullptr);
}

void Edit::initialiseVideo()
{
    auto* um = &undoManager;

    auto videoState = state.getOrCreateChildWithName (IDs::VIDEO, nullptr);
    videoOffset.referTo (videoState, IDs::videoOffset, um);
    videoMuted.referTo (videoState, IDs::videoMuted, nullptr);
    videoSource.referTo (videoState, IDs::videoSource, nullptr);

    if (videoSource.isUsingDefault())  // fallback for old tracktion edits that just stored filename
        loadOldVideoInfo (videoState);
}

void Edit::initialiseControllerMappings()
{
    controllerMappings = state.getOrCreateChildWithName (IDs::CONTROLLERMAPPINGS, nullptr);
    parameterControlMappings->loadFrom (controllerMappings);
}

void Edit::initialiseAutomap()
{
    automapState = state.getOrCreateChildWithName (IDs::AUTOMAPXML, nullptr);
}

void Edit::initialiseARA()
{
    araDocument.reset (new ARADocumentHolder (*this, state.getOrCreateChildWithName (IDs::ARADOCUMENT, nullptr)));

    auto areAnyClipsUsingMelodyne = [this]()
    {
        for (auto at : getTracksOfType<AudioTrack> (*this, true))
            for (auto c : at->getClips())
                if (auto acb = dynamic_cast<AudioClipBase*> (c))
                    if (acb->isUsingMelodyne())
                        return true;

        return false;
    };

    if (areAnyClipsUsingMelodyne())
        araDocument->getPimpl();
}

void Edit::flushState()
{
   #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
    if (shouldPlay())
        if (auto na = engine.getExternalControllerManager().getAutomap())
            na->save (*this);
   #endif

    jassert (state.getProperty (IDs::appVersion).toString().isNotEmpty());

    state.setProperty (IDs::appVersion, engine.getPropertyStorage().getApplicationVersion(), nullptr);
    state.setProperty (IDs::modifiedBy, engine.getPropertyStorage().getUserName(), nullptr);
    state.setProperty (IDs::projectID, editProjectItemID, nullptr);

    for (auto p : getAllPlugins (*this, true))
        p->flushPluginStateToValueTree();

    for (auto m : getAllModifiers (*this))
        m->flushPluginStateToValueTree();

    for (auto at : getAudioTracks (*this))
    {
        at->flushStateToValueTree();
        at->getOutput().flushStateToValueTree();
    }

    changedPluginsList->plugins.clear();

    if (araDocument != nullptr)
        araDocument->flushStateToValueTree();
}

void Edit::flushPluginStateIfNeeded (Plugin& p)
{
    changedPluginsList->flushPluginStateIfNeeded (p);
}

Time Edit::getTimeOfLastChange() const
{
    return Time (lastSignificantChange.get().getHexValue64());
}

void Edit::resetChangedStatus()
{
    // Dispatch pending changes syncronously or hasChanged will always get set to true
    getUndoManager().dispatchPendingMessages();
    hasChanged = false;
}

void Edit::markAsChanged()
{
    lastSignificantChange = String::toHexString (Time::getCurrentTime().toMilliseconds());
    hasChanged = true;
}

bool Edit::hasChangedSinceSaved() const
{
    return hasChanged;
}

void Edit::restartPlayback()
{
    shouldRestartPlayback = true;

    if (! isTimerRunning())
        startTimer (1);
}

EditPlaybackContext* Edit::getCurrentPlaybackContext() const
{
    return transportControl->getCurrentPlaybackContext();
}

//==============================================================================
void Edit::addWastedMidiMessagesListener (WastedMidiMessagesListener* l)
{
    wastedMidiMessagesListeners.add (l);
}

void Edit::removeWastedMidiMessagesListener (WastedMidiMessagesListener* l)
{
    wastedMidiMessagesListeners.remove (l);
}

void Edit::warnOfWastedMidiMessages (InputDevice* d, Track* t)
{
    wastedMidiMessagesListeners.call (&WastedMidiMessagesListener::warnOfWastedMidiMessages, d, t);
}

//==============================================================================
static Array<SelectionManager*> getSelectionManagers (const Edit& ed)
{
    Array<SelectionManager*> sms;

    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->edit == &ed)
            sms.add (sm.get());

    return sms;
}

//==============================================================================
void Edit::undoOrRedo (bool isUndo)
{
    if (getTransport().isRecording())
        getTransport().stop (false, false, true);

    if (isUndo)
        getUndoManager().undo();
    else
        getUndoManager().redo();

    for (auto sm : getSelectionManagers (*this))
    {
        sm->keepSelectedObjectsOnScreen();
        sm->refreshDetailComponent();
    }
}

void Edit::undo()           { undoOrRedo (true); }
void Edit::redo()           { undoOrRedo (false); }

Edit::UndoTransactionInhibitor::UndoTransactionInhibitor (Edit& e) : edit (&e)                                  { ++e.numUndoTransactionInhibitors; }
Edit::UndoTransactionInhibitor::UndoTransactionInhibitor (const UndoTransactionInhibitor& o) : edit (o.edit)    { if (edit != nullptr) ++(edit->numUndoTransactionInhibitors); }
Edit::UndoTransactionInhibitor::~UndoTransactionInhibitor()                                                     { if (edit != nullptr) --(edit->numUndoTransactionInhibitors); }

//==============================================================================
EditItemID Edit::createNewItemID (const std::vector<EditItemID>& idsToAvoid) const
{
    // TODO: This *may* be slow under heavy load - keep an eye open for this
    // in case a smarter caching system is needed
    auto existingIDs = EditItemID::findAllIDs (state);

    existingIDs.insert (existingIDs.end(), idsToAvoid.begin(), idsToAvoid.end());

    trackCache.visitItems ([&] (auto i)  { existingIDs.push_back (i->itemID); });
    clipCache.visitItems ([&] (auto i)   { existingIDs.push_back (i->itemID); });

    std::sort (existingIDs.begin(), existingIDs.end());

    return EditItemID::findFirstIDNotIn (existingIDs);
}

EditItemID Edit::createNewItemID() const
{
    return createNewItemID ({});
}

void Edit::visitAllTracksRecursive (std::function<bool(Track&)> f) const
{
    if (trackList != nullptr)
        trackList->visitAllRecursive (f);
}

void Edit::visitAllTopLevelTracks (std::function<bool(Track&)> f) const
{
    if (trackList != nullptr)
        trackList->visitAllTopLevel (f);
}

void Edit::visitAllTracks (std::function<bool(Track&)> f, bool recursive) const
{
    if (trackList != nullptr)
        trackList->visitAllTracks (f, recursive);
}

template <typename Predicate>
static Track* findTrackForPredicate (const Edit& edit, Predicate&& f)
{
    Track* result = nullptr;

    edit.visitAllTracksRecursive ([&] (Track& t)
    {
        if (f (t))
        {
            result = &t;
            return false;
        }

        return true;
    });

    return result;
}

double Edit::getNextTimeOfInterest (double t)
{
    if (t < 0)
        return 0;

    auto first = getLength();

    for (auto ct : getClipTracks (*this))
    {
        auto d = ct->getNextTimeOfInterest (t);

        if (d < first && d > t)
            first = d;
    }

    return first;
}

double Edit::getPreviousTimeOfInterest (double t)
{
    if (t < 0)
        return 0;

    double last = 0.0;

    for (auto* ct : getClipTracks (*this))
    {
        auto d = ct->getPreviousTimeOfInterest (t);

        if (d > last)
            last = d;
    }

    return last;
}

//==============================================================================
Track::Ptr Edit::loadTrackFrom (ValueTree& v)
{
    bool needsSanityCheck = false;

    if (findTrackForID (*this, EditItemID::fromID (v)) != nullptr)
    {
        createNewItemID().writeID (v, nullptr);
        needsSanityCheck = true;
    }

    if (auto tr = createTrack (v))
    {
        if (needsSanityCheck)
            tr->sanityCheckName();

        return tr;
    }

    jassertfalse;
    return {};
}

template <typename Type>
static Track::Ptr createAndInitialiseTrack (Edit& ed, const ValueTree& v)
{
    Track::Ptr t = new Type (ed, v);
    t->initialise();
    return t;
}

Track::Ptr Edit::createTrack (const ValueTree& v)
{
    CRASH_TRACER

    if (v.hasType (IDs::TRACK))            return createAndInitialiseTrack<AudioTrack> (*this, v);
    if (v.hasType (IDs::MARKERTRACK))      return createAndInitialiseTrack<MarkerTrack> (*this, v);
    if (v.hasType (IDs::FOLDERTRACK))      return createAndInitialiseTrack<FolderTrack> (*this, v);
    if (v.hasType (IDs::AUTOMATIONTRACK))  return createAndInitialiseTrack<AutomationTrack> (*this, v);
    if (v.hasType (IDs::TEMPOTRACK))       return createAndInitialiseTrack<TempoTrack> (*this, v);
    if (v.hasType (IDs::CHORDTRACK))       return createAndInitialiseTrack<ChordTrack> (*this, v);

    jassertfalse;
    return {};
}

//==============================================================================
bool Edit::areAnyTracksMuted() const
{
    return findTrackForPredicate (*this, [] (Track& t) { return t.isMuted (true); }) != nullptr;
}

bool Edit::areAnyTracksSolo() const
{
    return findTrackForPredicate (*this, [] (Track& t) { return t.isSolo (true); }) != nullptr;
}

bool Edit::areAnyTracksSoloIsolate() const
{
    return findTrackForPredicate (*this, [] (Track& t) { return t.isSoloIsolate (true); }) != nullptr;
}

void Edit::updateMuteSoloStatuses()
{
    const bool anySolo = areAnyTracksSolo();

    visitAllTracksRecursive ([anySolo] (Track& t)
    {
        t.updateAudibility (anySolo);
        return true;
    });

    auto& ecm = engine.getExternalControllerManager();

    if (ecm.isAttachedToEdit (this))
        ecm.updateMuteSoloLights (false);
}

//==============================================================================
MidiInputDevice* Edit::getCurrentMidiTimecodeSource() const
{
    auto& dm = engine.getDeviceManager();

    for (int i = dm.getNumMidiInDevices(); --i >= 0;)
        if (auto min = dm.getMidiInDevice (i))
            if (min->getName() == midiTimecodeSourceDevice)
                return min;

    return dm.getMidiInDevice (0);
}

void Edit::setCurrentMidiTimecodeSource (MidiInputDevice* newDevice)
{
    if (newDevice == nullptr)
        midiTimecodeSourceDevice.resetToDefault();
    else
        midiTimecodeSourceDevice = newDevice->getName();

    updateMidiTimecodeDevices();
    restartPlayback();
}

void Edit::enableTimecodeSync (bool b)
{
    if (b != midiTimecodeSourceDeviceEnabled)
    {
        midiTimecodeSourceDeviceEnabled = b;

        if (b)
        {
            if (auto mi = getCurrentMidiTimecodeSource())
            {
                setCurrentMidiTimecodeSource (mi);
                return; // no need to restart, as setCurrentMidiTimecodeSource does this
            }

            engine.getUIBehaviour().showWarningMessage (TRANS("No MIDI input was selected to be the timecode source"));
        }

        updateMidiTimecodeDevices();
        restartPlayback();
    }
}

void Edit::setTimecodeOffset (double newOffset)
{
    if (timecodeOffset != newOffset)
    {
        timecodeOffset = newOffset;

        updateMidiTimecodeDevices();
        restartPlayback();
    }
}

MidiInputDevice* Edit::getCurrentMidiMachineControlSource() const
{
    auto& dm = engine.getDeviceManager();

    for (int i = dm.getNumMidiInDevices(); --i >= 0;)
        if (auto min = dm.getMidiInDevice (i))
            if (min->getName() == midiMachineControlSourceDevice)
                return min;

    return {};
}

void Edit::setCurrentMidiMachineControlSource (MidiInputDevice* newDevice)
{
    if (newDevice == nullptr)
        midiMachineControlSourceDevice.resetToDefault();
    else
        midiMachineControlSourceDevice = newDevice->getName();

    updateMidiTimecodeDevices();
    restartPlayback();
}

MidiOutputDevice* Edit::getCurrentMidiMachineControlDest() const
{
    auto& dm = engine.getDeviceManager();

    for (int i = dm.getNumMidiOutDevices(); --i >= 0;)
        if (auto mout = dm.getMidiOutDevice (i))
            if (mout->getName() == midiMachineControlDestDevice)
                return mout;

    return {};
}

void Edit::setCurrentMidiMachineControlDest (MidiOutputDevice* newDevice)
{
    if (newDevice == nullptr)
        midiMachineControlDestDevice.resetToDefault();
    else
        midiMachineControlDestDevice = newDevice->getName();

    updateMidiTimecodeDevices();
    restartPlayback();
}

void Edit::setMidiTimecodeIgnoringHours (bool b)
{
    midiTimecodeIgnoringHours = b;
    updateMidiTimecodeDevices();
    restartPlayback();
}

void Edit::updateMidiTimecodeDevices()
{
    auto& dm = engine.getDeviceManager();

    for (int i = dm.getNumMidiInDevices(); --i >= 0;)
    {
        if (auto min = dynamic_cast<PhysicalMidiInputDevice*> (dm.getMidiInDevice(i)))
        {
            min->setReadingMidiTimecode (midiTimecodeSourceDeviceEnabled
                                           && min->getName() == midiTimecodeSourceDevice);
            min->setAcceptingMMC (min->getName() == midiMachineControlSourceDevice);
            min->setIgnoresHours (midiTimecodeIgnoringHours);
        }
    }

    for (int i = dm.getNumMidiOutDevices(); --i >= 0;)
    {
        if (auto mo = dm.getMidiOutDevice (i))
        {
            mo->setSendingMMC (mo->getName() == midiMachineControlDestDevice);

            if (mo->isEnabled())
                mo->updateMidiTC (this);
        }
    }
}

//==============================================================================
void Edit::loadOldTimeSigInfo()
{
    CRASH_TRACER

    auto oldInfoState = state.getChildWithName ("TIMECODETYPE");

    if (! oldInfoState.isValid())
        return;

    std::unique_ptr<XmlElement> oldInfoXml (oldInfoState.createXml());

    if (auto oldInfo = oldInfoXml.get())
    {
        if (auto sequenceNode = oldInfo->getFirstChildElement())
            if (sequenceNode->hasTagName ("SECTION"))
                oldInfo = sequenceNode;

        if (auto tempo = tempoSequence.getTempo (0))
            tempo->set (0, oldInfo->getDoubleAttribute ("bpm", 120.0), 0, false);

        if (auto timeSig = tempoSequence.getTimeSig (0))
        {
            timeSig->setStringTimeSig (oldInfo->getStringAttribute ("timeSignature", "4/4"));
            timeSig->triplets = oldInfo->getBoolAttribute ("triplets", false);
        }

        state.removeChild (state.getChildWithName ("TIMECODETYPE"), nullptr);
    }
}

void Edit::loadOldVideoInfo (const ValueTree& videoState)
{
    CRASH_TRACER

    const File videoFile (videoState["videoFile"].toString());

    if (videoFile.existsAsFile())
        if (auto proj = ProjectManager::getInstance()->getProject (*this))
            if (auto newItem = proj->createNewItem (videoFile, ProjectItem::videoItemType(),
                                                    videoFile.getFileNameWithoutExtension(),
                                                    {}, ProjectItem::Category::video, false))
                videoSource = newItem->getID();
}

TimecodeDisplayFormat Edit::getTimecodeFormat() const
{
    return timecodeFormat;
}

void Edit::setTimecodeFormat (TimecodeDisplayFormat newFormat)
{
    timecodeFormat = newFormat;
}

void Edit::toggleTimecodeMode()
{
    auto f = getTimecodeFormat();

    switch (f.type)
    {
        case TimecodeType::millisecs:       f = TimecodeDisplayFormat (TimecodeType::barsBeats); break;
        case TimecodeType::barsBeats:       f = TimecodeDisplayFormat (TimecodeType::fps24); break;
        case TimecodeType::fps24:           f = TimecodeDisplayFormat (TimecodeType::fps25); break;
        case TimecodeType::fps25:           f = TimecodeDisplayFormat (TimecodeType::fps30); break;
        case TimecodeType::fps30:           f = TimecodeDisplayFormat (TimecodeType::millisecs); break;
        default: jassertfalse;              break;
    }

    setTimecodeFormat (f);

    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->edit == this)
            if (! sm->containsType<ExternalPlugin>())
                sm->refreshDetailComponent();
}

//==============================================================================
void Edit::sendTempoOrPitchSequenceChangedUpdates()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    // notify clips of tempo changes
    for (auto t : getClipTracks (*this))
        for (auto& c : t->getClips())
            c->pitchTempoTrackChanged();
}

void Edit::sendSourceFileUpdate()
{
    CRASH_TRACER

    for (auto t : getAudioTracks (*this))
        for (auto& c : t->getClips())
            c->sourceMediaChanged();

    for (auto p : getAllPlugins (*this, true))
        p->sourceMediaChanged();
}

//==============================================================================
void Edit::setMasterVolumeSliderPos (float v)
{
    getMasterSliderPosParameter()->setParameter (v, sendNotification);
}

void Edit::setMasterPanPos (float p)
{
    getMasterPanParameter()->setParameter (p, sendNotification);
}

//==============================================================================
String Edit::getAuxBusName (int bus) const
{
    return auxBusses.getChildWithProperty (IDs::bus, bus).getProperty (IDs::name);
}

void Edit::setAuxBusName (int bus, const String& nm)
{
    const String name (nm.substring (0, 20));

    if (getAuxBusName (bus) != name)
    {
        auto b = auxBusses.getChildWithProperty (IDs::bus, bus);

        if (name.isEmpty())
        {
            if (b.isValid())
                auxBusses.removeChild (b, nullptr);
        }
        else
        {
            if (! b.isValid())
            {
                b = ValueTree (IDs::NAME);
                auxBusses.addChild (b, -1, nullptr);
                b.setProperty (IDs::bus, bus, nullptr);
            }

            b.setProperty (IDs::name, name, nullptr);
        }
    }
}

//==============================================================================
void Edit::dispatchPendingUpdatesSynchronously()
{
    if (isTimerRunning())
        timerCallback();
}

void Edit::timerCallback()
{
    if (! isFullyConstructed.load (std::memory_order_relaxed))
        return;

    if (shouldRestartPlayback && shouldPlay())
    {
        shouldRestartPlayback = false;
        parameterControlMappings->checkForDeletedParams();

        getTransport().editHasChanged();
    }

    stopTimer();
}

//==============================================================================
double Edit::getLength() const
{
    if (totalEditLength < 0)
    {
        totalEditLength = 0;

        for (auto t : getClipTracks (*this))
            totalEditLength = jmax (totalEditLength, t->getLength());
    }

    return totalEditLength;
}

double Edit::getFirstClipTime() const
{
    auto t = getLength();
    bool gotOne = false;

    for (auto track : getClipTracks (*this))
    {
        if (auto first = track->getClips().getFirst())
        {
            gotOne = true;
            t = jmin (t, first->getPosition().getStart());
        }
    }

    return gotOne ? t : 0.0;
}

Array<Clip*> Edit::findClipsInLinkGroup (EditItemID linkGroupID) const
{
    if (treeWatcher != nullptr)
        return treeWatcher->getClipsInLinkGroup (linkGroupID);

    return {};
}

AudioTrack::Ptr Edit::getOrInsertAudioTrackAt (int trackIndex)
{
    int i = 0;

    // find the next audio track on or after the given index..
    for (auto t : getAllTracks (*this))
    {
        if (i >= trackIndex)
            if (auto at = dynamic_cast<AudioTrack*> (t))
                return at;

        ++i;
    }

    return insertNewAudioTrack (TrackInsertPoint (nullptr, getAllTracks (*this).getLast()), nullptr);
}

Track::Ptr Edit::insertTrack (TrackInsertPoint insertPoint, ValueTree v, SelectionManager* sm)
{
    CRASH_TRACER

    if (getAllTracks (*this).size() >= maxNumTracks)
        return {};

    auto parent = state;

    if (insertPoint.parentTrackID.isValid())
        if (auto t = findTrackForID (*this, insertPoint.parentTrackID))
            parent = t->state;

    auto preceeding = parent.getChildWithProperty (IDs::id, insertPoint.precedingTrackID.toVar());

    return insertTrack (v, parent, preceeding, sm);
}

Track::Ptr Edit::insertTrack (ValueTree v, ValueTree parent, ValueTree preceeding, SelectionManager* sm)
{
    if (! parent.isValid())
        parent = state;

    parent.addChild (v, parent.indexOf (preceeding) + 1, &undoManager);
    auto newTrack = findTrackForState (*this, v);

    if (newTrack != nullptr)
    {
        engine.getUIBehaviour().newTrackCreated (*newTrack);

        if (sm != nullptr)
        {
            sm->addToSelection (newTrack);
            sm->keepSelectedObjectsOnScreen();
        }
    }

    return newTrack;
}

AudioTrack::Ptr Edit::insertNewAudioTrack (TrackInsertPoint insertPoint, SelectionManager* sm)
{
    if (auto newTrack = insertNewTrack (insertPoint, IDs::TRACK, sm))
    {
        newTrack->pluginList.addDefaultTrackPlugins (false);
        return dynamic_cast<AudioTrack*> (newTrack.get());
    }

    return {};
}

FolderTrack::Ptr Edit::insertNewFolderTrack (TrackInsertPoint insertPoint, SelectionManager* sm, bool asSubmix)
{
    if (auto newTrack = insertNewTrack (insertPoint, IDs::FOLDERTRACK, sm))
    {
        newTrack->pluginList.addDefaultTrackPlugins (! asSubmix);
        return dynamic_cast<FolderTrack*> (newTrack.get());
    }

    return {};
}

AutomationTrack::Ptr Edit::insertNewAutomationTrack (TrackInsertPoint insertPoint, SelectionManager* sm)
{
    if (auto t = insertNewTrack (insertPoint, IDs::AUTOMATIONTRACK, sm))
        if (auto at = dynamic_cast<AutomationTrack*> (t.get()))
            return *at;

    return {};
}

Track::Ptr Edit::insertNewTrack (TrackInsertPoint insertPoint, const juce::Identifier& xmlType, SelectionManager* sm)
{
    return insertTrack (insertPoint, ValueTree (xmlType), sm);
}

void Edit::moveTrack (Track::Ptr t, TrackInsertPoint destination)
{
    CRASH_TRACER

    if (t == nullptr)
        return;

    juce::ValueTree newParent (state), preceedingTrack;

    if (destination.parentTrackID.isValid())
        if (auto parentTrack = findTrackForID (*this, destination.parentTrackID))
            newParent = parentTrack->state;

    if (destination.precedingTrackID.isValid())
        if (auto p = findTrackForID (*this, destination.precedingTrackID))
            preceedingTrack = p->state;

    auto oldParent = t->state.getParent();
    auto currentIndex = oldParent.indexOf (t->state);
    auto precedingIndex = preceedingTrack.isValid() ? newParent.indexOf (preceedingTrack) : 0;

    if (newParent.isAChildOf (t->state) || newParent == t->state)
        return;

    if (oldParent != newParent)
    {
        oldParent.removeChild (currentIndex, &undoManager);
        newParent.addChild (t->state, precedingIndex + 1, &undoManager);
    }
    else
    {
        auto newIndex = precedingIndex < currentIndex ? precedingIndex + 1 : precedingIndex;

        if (currentIndex < 0 || newIndex < 0)
            return;

        newParent.moveChild (currentIndex, newIndex, &undoManager);
    }

    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->edit == this)
            sm->keepSelectedObjectsOnScreen();
}

void Edit::updateTrackStatuses()
{
    CRASH_TRACER

    sanityCheckTrackNames();
    updateMuteSoloStatuses();

    // Only refresh if this Edit is being shown or it can cancel things like file previews
    for (auto sm : getSelectionManagers (*this))
        sm->refreshDetailComponent();
}

void Edit::updateTrackStatusesAsync()
{
    if (treeWatcher != nullptr && ! isLoading())
        treeWatcher->updateTrackStatusesAsync();
}

void Edit::deleteTrack (Track* t)
{
    if (t != nullptr && containsTrack (*this, *t))
    {
        CRASH_TRACER

        t->deselect();
        t->setFrozen (false, Track::groupFreeze);

        // Remove any modifier assignments
        for (auto mod : t->getModifierList().getModifiers())
            mod->remove();

        // this is needed in case we're undoing, and things in the track, like midi notes might be selected
        for (auto sm : getSelectionManagers (*this))
            sm->deselectAll();

        auto audioTracks = getAudioTracks (*this);

        auto clearEnabledTrackDevices = [this, &audioTracks] (AudioTrack* trackBeingDeleted)
        {
            // If the track we're deleting is the target track for a device, disable that device
            for (auto* idi : editInputDevices->getDevicesForTargetTrack (*trackBeingDeleted))
                if (idi->owner.isTrackDevice())
                    idi->owner.setEnabled (false);

            auto& wid = trackBeingDeleted->getWaveInputDevice();
            auto& mid = trackBeingDeleted->getMidiInputDevice();

            const bool clearWave = wid.isEnabled();
            const bool clearMidi = mid.isEnabled();

            if (clearWave || clearMidi)
            {
                getTransport().freePlaybackContext();

                for (auto at : audioTracks)
                {
                    if (clearWave)  editInputDevices->clearInputsOfDevice (*at, wid);
                    if (clearMidi)  editInputDevices->clearInputsOfDevice (*at, mid);
                }
            }
        };

        // Remove track from all tracks that might be ghosting it
        // (There's got to be a better way of doing this?)
        if (auto trackAsAudio = dynamic_cast<AudioTrack*> (t))
        {
            for (auto at : audioTracks)
                at->setTrackToGhost (trackAsAudio, false);

            clearEnabledTrackDevices (trackAsAudio);
        }

        if (auto ft = dynamic_cast<FolderTrack*> (t))
        {
            for (auto at : ft->getAllAudioSubTracks (true))
            {
                for (auto f : at->pluginList)
                    if (auto ep = dynamic_cast<ExternalPlugin*> (f))
                        ep->hideWindowForShutdown();

                clearEnabledTrackDevices (at);
                editInputDevices->clearAllInputs (*at);
            }
        }
        else
        {
            for (auto f : t->pluginList)
                if (auto ep = dynamic_cast<ExternalPlugin*> (f))
                    ep->hideWindowForShutdown();

            if (auto at = dynamic_cast<AudioTrack*> (t))
            {
                clearEnabledTrackDevices (at);
                editInputDevices->clearAllInputs (*at);
            }
        }

        t->state.getParent().removeChild (t->state, &undoManager);
    }
}

void Edit::sanityCheckTrackNames()
{
    visitAllTracksRecursive ([&] (Track& t)
    {
        t.sanityCheckName();
        return true;
    });
}

void Edit::ensureTempoTrack()
{
    if (getTempoTrack() == nullptr)
    {
        ValueTree v (IDs::TEMPOTRACK);
        v.setProperty (IDs::name, TRANS("Global"), nullptr);
        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureMarkerTrack()
{
    if (getMarkerTrack() == nullptr)
    {
        ValueTree v (IDs::MARKERTRACK);
        v.setProperty (IDs::name, TRANS("Marker"), nullptr);
        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureChordTrack()
{
    if (getChordTrack() == nullptr)
    {
        ValueTree v (IDs::CHORDTRACK);
        v.setProperty (IDs::name, TRANS("Chords"), nullptr);
        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureNumberOfAudioTracks (int minimumNumTracks)
{
    while (getAudioTracks (*this).size() < minimumNumTracks)
    {
        getTransport().stopIfRecording();

        insertNewAudioTrack (TrackInsertPoint (nullptr, getTopLevelTracks (*this).getLast()), nullptr);
    }
}

//==============================================================================
PluginCache& Edit::getPluginCache() noexcept
{
    return *pluginCache;
}

//==============================================================================
void Edit::sendMirrorUpdateToAllPlugins (Plugin& plugin) const
{
    CRASH_TRACER

    for (auto t : getAllTracks (*this))
        t->sendMirrorUpdateToAllPlugins (plugin);

    for (auto r : getRackList().getTypes())
        for (auto p : r->getPlugins())
            p->updateFromMirroredPluginIfNeeded (plugin);

    masterPluginList->sendMirrorUpdateToAllPlugins (plugin);
}

void Edit::updateMirroredPlugins()
{
    if (isLoading())
        return;

    mirroredPluginUpdateTimer->stopTimer();

    for (auto changed : mirroredPluginUpdateTimer->changedPlugins)
        if (changed != nullptr)
            sendMirrorUpdateToAllPlugins (*changed);

    mirroredPluginUpdateTimer->changedPlugins.clear();
}

void Edit::updateMirroredPlugin (Plugin& p)
{
    if (mirroredPluginUpdateTimer != nullptr)
    {
        mirroredPluginUpdateTimer->changedPlugins.addIfNotAlreadyThere (&p);
        mirroredPluginUpdateTimer->startTimer (500);
    }
}

void Edit::sendStartStopMessageToPlugins()
{
    CRASH_TRACER

    for (auto p : getAllPlugins (*this, true))
        p->playStartedOrStopped();
}

void Edit::muteOrUnmuteAllPlugins()
{
    auto allPlugins = getAllPlugins (*this, true);

    int numEnabled = 0;

    for (auto p : allPlugins)
        if (p->isEnabled())
            ++numEnabled;

    for (auto p : allPlugins)
        p->setEnabled (numEnabled == 0);
}

void Edit::addModifierTimer (ModifierTimer& mt)
{
    jassert (! modifierTimers.contains (&mt));
    modifierTimers.add (&mt);
}

void Edit::removeModifierTimer (ModifierTimer& mt)
{
    modifierTimers.removeFirstMatchingValue (&mt);
}

void Edit::updateModifierTimers (PlayHead& ph, EditTimeRange streamTime, int numSamples) const
{
    const ScopedLock sl (modifierTimers.getLock());

    for (auto mt : modifierTimers)
        mt->updateStreamTime (ph, streamTime, numSamples);
}

//==============================================================================
void Edit::setLowLatencyMonitoring (bool enabled, const Array<EditItemID>& plugins)
{
    lowLatencyMonitoring = enabled;
    AudioDeviceManager::AudioDeviceSetup setup;

    auto& deviceManager = engine.getDeviceManager().deviceManager;
    deviceManager.getAudioDeviceSetup (setup);

    if (lowLatencyMonitoring)
    {
        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            normalLatencyBufferSizeSeconds = setup.bufferSize / setup.sampleRate;
            setup.bufferSize = roundToInt ((static_cast<double> (engine.getPropertyStorage().getProperty (SettingID::lowLatencyBuffer, 5.8)) / 1000.0) * setup.sampleRate);
        }
        else
        {
            normalLatencyBufferSizeSeconds = 0.0f;
        }

        setLowLatencyDisabledPlugins (plugins);
    }
    else
    {
        setLowLatencyDisabledPlugins ({});
        setup.bufferSize = roundToInt (normalLatencyBufferSizeSeconds * setup.sampleRate);
    }

    if (auto dev = deviceManager.getCurrentAudioDevice())
    {
        if (! enabled || (enabled && setup.bufferSize < dev->getCurrentBufferSizeSamples()))
        {
            if (enabled)
            {
                for (auto bufferSize : dev->getAvailableBufferSizes())
                {
                    if (bufferSize >= setup.bufferSize)
                    {
                        setup.bufferSize = bufferSize;
                        break;
                    }
                }
            }

            const bool isPlaying = getTransport().isPlaying();
            getTransport().freePlaybackContext();
            deviceManager.setAudioDeviceSetup (setup, false);
            getTransport().ensureContextAllocated();

            if (isPlaying)
                getTransport().play (true);
        }
    }
}

void Edit::setLowLatencyDisabledPlugins (const Array<EditItemID>& newPlugins)
{
    for (auto p : getAllPlugins (*this, false))
    {
        if (newPlugins.contains (p->itemID))
            p->setEnabled (false);          // disable new ones
        else if (lowLatencyDisabledPlugins.contains (p->itemID))
            p->setEnabled (true);           // re-enable any previosuly disabled ones
    }

    lowLatencyDisabledPlugins = newPlugins;
}

//==============================================================================
void Edit::initialiseAllPlugins()
{
    CRASH_TRACER

    for (auto p : getAllPlugins (*this, true))
        p->initialiseFully();
}

//==============================================================================
InputDeviceInstance* Edit::getCurrentInstanceForInputDevice (InputDevice* d) const
{
    if (auto context = getCurrentPlaybackContext())
        return context->getInputFor (d);

    return {};
}

Array<InputDeviceInstance*> Edit::getAllInputDevices() const
{
    if (auto context = getCurrentPlaybackContext())
        return context->getAllInputs();

    return {};
}

EditInputDevices& Edit::getEditInputDevices() noexcept
{
    return *editInputDevices;
}

//==============================================================================
void Edit::pluginChanged (Plugin& p) noexcept
{
    if (isLoadInProgress)
        return;

    changedPluginsList->pluginChanged (p);
    pluginChangeTimer->pluginChanged();
}

//==============================================================================
void Edit::setClickTrackRange (EditTimeRange newTimes) noexcept
{
    clickMark1Time = newTimes.getStart();
    clickMark2Time = newTimes.getEnd();
}

EditTimeRange Edit::getClickTrackRange() const noexcept
{
    return EditTimeRange::between (clickMark1Time, clickMark2Time);
}

String Edit::getClickTrackDevice() const
{
    if (clickTrackDevice == DeviceManager::getDefaultAudioDeviceName (false)
         || clickTrackDevice == DeviceManager::getDefaultMidiDeviceName (false))
        return clickTrackDevice;

    if (auto out = engine.getDeviceManager().findOutputDeviceWithName (clickTrackDevice))
        return out->getName();

    return DeviceManager::getDefaultAudioDeviceName (false);
}

bool Edit::isClickTrackDevice (OutputDevice& dev) const
{
    auto& dm = engine.getDeviceManager();

    if (auto out = dm.findOutputDeviceWithName (clickTrackDevice))
        return out == &dev;

    if (clickTrackDevice == DeviceManager::getDefaultMidiDeviceName (false))
        return dm.getDefaultMidiOutDevice() == &dev;

    return dm.getDefaultWaveOutDevice() == &dev;
}

void Edit::setClickTrackOutput (const String& deviceName)
{
    clickTrackDevice = deviceName;
    restartPlayback();
}

void Edit::setClickTrackVolume (float gain)
{
    clickTrackGain = jlimit (0.2f, 1.0f, gain);
    engine.getPropertyStorage().setProperty (SettingID::lastClickTrackLevel, gain);
}


//==============================================================================
void Edit::setCountInMode (CountIn mode)
{
    engine.getPropertyStorage().setProperty (SettingID::countInMode, static_cast<int> (mode));
}

Edit::CountIn Edit::getCountInMode() const
{
    return static_cast<CountIn> (static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::countInMode)));
}

int Edit::getNumCountInBeats() const
{
    switch (getCountInMode())
    {
        case CountIn::none:       return 0;
        case CountIn::oneBar:     return tempoSequence.getTimeSig(0)->numerator;
        case CountIn::twoBar:     return tempoSequence.getTimeSig(0)->numerator * 2;
        case CountIn::oneBeat:    return 1;
        case CountIn::twoBeat:    return 2;
        default:                  jassertfalse; break;
    }

    return 0;
}

//==============================================================================
static bool deleteFrozenTrackFiles (const Array<File>& freezeFiles)
{
    bool allOK = true;

    for (auto& freezeFile : freezeFiles)
        if (! AudioFile (freezeFile).deleteFile())
            allOK = false;

    return allOK;
}

void Edit::readFrozenTracksFiles()
{
    CRASH_TRACER

    auto freezeFiles = getFrozenTracksFiles();
    bool resetFrozenness = freezeFiles.isEmpty();

    for (auto& freezeFile : freezeFiles)
    {
        auto fn = freezeFile.getFileName();
        auto outId = fn.fromFirstOccurrenceOf ("_", false, false)
                       .fromFirstOccurrenceOf ("_", false, false)
                       .upToFirstOccurrenceOf (".", false, false);

        if (engine.getDeviceManager().findOutputDeviceForID (outId) == nullptr)
        {
            resetFrozenness = true;
            break;
        }
    }

    // also reset frozenness if no tracks are frozen, to clear out dodgy old freeze files
    bool anyFrozen = false;

    for (auto t : getAllTracks (*this))
        anyFrozen = anyFrozen || t->isFrozen (Track::groupFreeze);

    if (resetFrozenness || ! anyFrozen)
    {
        deleteFrozenTrackFiles (freezeFiles);

        for (auto t : getAllTracks (*this))
        {
            if (t->isFrozen (Track::groupFreeze))
            {
                if (auto at = dynamic_cast<AudioTrack*> (t))
                {
                    at->frozen = false;
                    at->changed();
                }
            }
        }

        if (! isTimerRunning())
            startTimer (1);
    }
}

void Edit::updateFrozenTracks()
{
    if (isRendering())
        return;

    CRASH_TRACER
    TransportControl::stopAllTransports (engine, false, true);
    const ScopedRenderStatus srs (*this);

    deleteFrozenTrackFiles (getFrozenTracksFiles());

    auto& dm = engine.getDeviceManager();

    for (int j = dm.getNumOutputDevices(); --j >= 0;)
    {
        if (auto outputDevice = dynamic_cast<WaveOutputDevice*> (dm.getOutputDeviceAt (j)))
        {
            BigInteger frozen;
            double length = 0;
            int i = 0;

            for (auto t : getAllTracks (*this))
            {
                if (auto at = dynamic_cast<AudioTrack*> (t))
                {
                    if (at->isFrozen (Track::groupFreeze)
                         && at->getOutput().outputsToDevice (outputDevice->getName(), true)
                         && ! at->isMuted (true))
                    {
                        frozen.setBit (i, true);
                        length = jmax (length, at->getLengthIncludingInputTracks());
                    }
                }

                ++i;
            }

            if (frozen.countNumberOfSetBits() > 0 && length > 0)
            {
                length += 5.0;

                for (auto sm : getSelectionManagers (*this))
                    sm->deselectAll();

                auto f = getTempDirectory (true)
                           .getChildFile (getFreezeFilePrefix()
                                           + String (outputDevice->getDeviceID())
                                           + ".freeze");

                StringPairArray metadataFound;

                Renderer::Parameters r (*this);
                r.tracksToDo = frozen;
                r.destFile = f;
                r.audioFormat = engine.getAudioFileFormatManager().getFrozenFileFormat();
                r.blockSizeForAudio = dm.getBlockSize();
                r.sampleRateForAudio = dm.getSampleRate();
                r.time = { 0.0, length };
                r.canRenderInMono = true;
                r.mustRenderInMono = false;
                r.usePlugins = true;
                r.useMasterPlugins = false;
                r.category = ProjectItem::Category::frozen;
                r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (engine);

                Renderer::renderToProjectItem (TRANS("Updating frozen tracks for output device \"XDVX\"")
                                                  .replace ("XDVX", outputDevice->getName()) + "...", r);

                if (! f.exists())
                {
                    // failed to render properly - calling this should unfreeze all the tracks
                    readFrozenTracksFiles();
                }

                engine.getAudioFileManager().checkFilesForChanges();
            }

            // once we've frozen the tracks we need to un-solo them or they will block other playing tracks
            i = 0;

            for (auto t : getAllTracks (*this))
            {
                if (frozen[i++])
                {
                    if (auto at = dynamic_cast<AudioTrack*> (t))
                    {
                        at->setSolo (false);
                        at->state.sendPropertyChangeMessage (IDs::height);
                    }
                }
            }
        }
    }

    SelectionManager::refreshAllPropertyPanels();
}

juce::String Edit::getFreezeFilePrefix() const
{
    return "freeze_" + getProjectItemID().toStringSuitableForFilename() + "_";
}

Array<File> Edit::getFrozenTracksFiles() const
{
    // freeze filename format: freeze_(edit id)_(output device id).freeze

    Array<File> files;
    getTempDirectory (false).findChildFiles (files, File::findFiles, false, getFreezeFilePrefix() + "*");
    return files;
}

void Edit::needToUpdateFrozenTracks()
{
    // ignore this until fully loaded
    if (frozenTrackCallback != nullptr && ! isLoadInProgress)
        frozenTrackCallback->triggerAsyncUpdate();
}

// TODO: move logic for creating and parsing these filenames into one place - see
// also AudioClipBase::getProxyFileToCreate
static EditItemID getEditItemIDFromFilename (const String& name)
{
   auto tokens = StringArray::fromTokens (name.fromFirstOccurrenceOf ("_", false, false), "_", {});

    if (tokens.isEmpty())
        return {};

    // TODO: think about backwards-compatibility for these strings - was a two-part
    // ID separated by an underscore
    return EditItemID::fromVar (tokens[0] + tokens[1]);
}

bool Edit::areAnyClipsUsingFile (const AudioFile& af)
{
    for (auto t : getClipTracks (*this))
        if (t->areAnyClipsUsingFile (af))
            return true;

    return false;
}

void Edit::cancelAllProxyGeneratorJobs() const
{
    auto& apg = engine.getAudioFileManager().proxyGenerator;

    for (auto t : getClipTracks (*this))
    {
        for (auto& c : t->getClips())
        {
            if (auto acb = dynamic_cast<AudioClipBase*> (c))
            {
                auto af = acb->getPlaybackFile();

                if (apg.isProxyBeingGenerated (af))
                    apg.deleteProxy (af);
            }
        }
    }
}

void Edit::purgeOrphanFreezeAndProxyFiles()
{
    CRASH_TRACER
    Array<File> filesToDelete;

    for (DirectoryIterator i (getTempDirectory (false), false, "*"); i.next();)
    {
        auto name = i.getFile().getFileName();
        auto itemID = getEditItemIDFromFilename (name);

        if (itemID.isValid())
        {
            if (name.startsWith (AudioClipBase::getClipProxyPrefix())
                 || name.startsWith (WaveAudioClip::getCompPrefix()))
            {
                auto clip = findClipForID (*this, itemID);

                if (auto acb = dynamic_cast<AudioClipBase*> (clip))
                {
                    if (! acb->isUsingFile (AudioFile (i.getFile())))
                        filesToDelete.add (i.getFile());
                }
                else if (clip == nullptr)
                {
                    filesToDelete.add (i.getFile());
                }
            }
            else if (name.startsWith (AudioClipBase::getFileProxyPrefix()))
            {
                // XXX
            }
            else if (name.startsWith (AudioTrack::getTrackFreezePrefix()))
            {
                if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (*this, itemID)))
                    if (! at->isFrozen (Track::individualFreeze))
                        filesToDelete.add (i.getFile());
            }
        }
        else if (name.startsWith (RenderManager::getFileRenderPrefix()))
        {
            if (! areAnyClipsUsingFile (AudioFile (i.getFile())))
                filesToDelete.add (i.getFile());
        }
    }

    for (int i = filesToDelete.size(); --i >= 0;)
    {
        DBG ("Purging file: " << filesToDelete.getReference (i).getFileName());
        AudioFile (filesToDelete.getReference (i)).deleteFile();
    }
}

File Edit::getTempDirectory (bool createIfNonExistent) const
{
    File result (tempDirectory);

    if (result == File())
        result = tempDirectory = engine.getTemporaryFileManager().getTempDirectory()
                                    .getChildFile ("edit_" + getProjectItemID().toStringSuitableForFilename());

    if (createIfNonExistent && ! result.createDirectory())
        TRACKTION_LOG_ERROR ("Failed to create edit temp folder: " + result.getFullPathName());

    return result;
}

void Edit::setTempDirectory (const File& dir)
{
    tempDirectory = dir;
}

//==============================================================================
void Edit::setVideoFile (const File& f, String importDesc)
{
    CRASH_TRACER
    File currentFile;

    if (auto item = ProjectManager::getInstance()->getProjectItem (videoSource))
        currentFile = item->getSourceFile();

    if (f != currentFile)
    {
        videoSource.resetToDefault();

        if (auto proj = ProjectManager::getInstance()->getProject (*this))
        {
            auto item = proj->getProjectItemForFile (f);

            if (item == nullptr)
                item = proj->createNewItem (f, ProjectItem::videoItemType(), f.getFileNameWithoutExtension(),
                                            importDesc, ProjectItem::Category::video, false);

            if (item != nullptr)
                videoSource = item->getID();
        }
    }
}

File Edit::getVideoFile() const
{
    if (videoSource.get().isValid())
        if (auto item = ProjectManager::getInstance()->getProjectItem (videoSource))
            return item->getSourceFile();

    return {};
}

//==============================================================================
Array<AutomatableParameter*> Edit::getAllAutomatableParams (bool includeTrackParams) const
{
    Array<AutomatableParameter*> list;

    list.add (getMasterSliderPosParameter().get());
    list.add (getMasterPanParameter().get());

    list.addArray (globalMacros->macroParameterList.getMacroParameters());

    for (auto f : *masterPluginList)
    {
        list.addArray (f->macroParameterList.getMacroParameters());

        for (int j = 0; j < f->getNumAutomatableParameters(); ++j)
            list.add (f->getAutomatableParameter (j).get());
    }

    for (auto rft : getRackList().getTypes())
    {
        for (auto mod : rft->getModifierList().getModifiers())
            list.addArray (mod->getAutomatableParameters());

        list.addArray (rft->macroParameterList.getMacroParameters());

        for (auto p : rft->getPlugins())
        {
            list.addArray (p->macroParameterList.getMacroParameters());
            list.addArray (p->getAutomatableParameters());
        }
    }

    if (includeTrackParams)
    {
        for (auto t : getAllTracks (*this))
        {
            list.addArray (t->macroParameterList.getMacroParameters());
            list.addArray (t->getAllAutomatableParams());
        }
    }

    return list;
}

//==============================================================================
MarkerTrack* Edit::getMarkerTrack() const
{
    return dynamic_cast<MarkerTrack*> (findTrackForPredicate (*this, [] (Track& t) { return t.isMarkerTrack(); }));
}

TempoTrack* Edit::getTempoTrack() const
{
    return dynamic_cast<TempoTrack*> (findTrackForPredicate (*this, [] (Track& t) { return t.isTempoTrack(); }));
}

ChordTrack* Edit::getChordTrack() const
{
    return dynamic_cast<ChordTrack*> (findTrackForPredicate (*this, [] (Track& t) { return t.isChordTrack(); }));
}

Edit::Metadata Edit::getEditMetadata()
{
    Metadata metadata;

    auto meta = state.getChildWithName (IDs::ID3VORBISMETADATA);

    metadata.album       = meta[IDs::album];
    metadata.artist      = meta[IDs::artist];
    metadata.comment     = meta[IDs::comment];
    metadata.date        = meta.getProperty (IDs::date, Time::getCurrentTime().getYear());
    metadata.genre       = meta[IDs::genre];
    metadata.title       = meta[IDs::title];
    metadata.trackNumber = meta.getProperty (IDs::trackNumber, 1);

    return metadata;
}

void Edit::setEditMetadata (Metadata metadata)
{
    auto meta = state.getChildWithName (IDs::ID3VORBISMETADATA);

    meta.setProperty (IDs::album, metadata.album, nullptr);
    meta.setProperty (IDs::artist, metadata.artist, nullptr);
    meta.setProperty (IDs::comment, metadata.comment, nullptr);
    meta.setProperty (IDs::date, metadata.date, nullptr);
    meta.setProperty (IDs::genre, metadata.genre, nullptr);
    meta.setProperty (IDs::title, metadata.title, nullptr);
    meta.setProperty (IDs::trackNumber, metadata.trackNumber, nullptr);
}

//==============================================================================
std::unique_ptr<Edit> Edit::createEditForPreviewingPreset (Engine& engine, juce::ValueTree v, const Edit* editToMatch,
                                                           bool tryToMatchTempo, bool* couldMatchTempo, juce::ValueTree midiPreviewPlugin)
{
    CRASH_TRACER
    auto edit = std::make_unique<Edit> (engine, createEmptyEdit(), forEditing, nullptr, 1);
    edit->ensureNumberOfAudioTracks (1);
    edit->isPreviewEdit = true;

    if (couldMatchTempo != nullptr)
        *couldMatchTempo = false;

    if (auto track = getFirstAudioTrack (*edit))
    {
        bool resizeClip = false;
        double clipTempo = 120.0;

        if (Clip::isClipState (v))
        {
            MemoryBlock mb;
            mb.fromBase64Encoding (v[IDs::state].toString());

            auto state = updateLegacyEdit (juce::ValueTree::readFromData (mb.getData(), mb.getSize()));

            if (state.isValid())
                track->state.addChild (state, -1, nullptr);

            clipTempo = state[IDs::bpm];

            if (clipTempo < 20.0)
                clipTempo = 120.0;

            resizeClip = true;
        }

        if (midiPreviewPlugin.isValid())
            track->pluginList.insertPlugin (midiPreviewPlugin, 0);

        double songTempo = 120.0;
        double length = 1.0;

        if (auto firstClip = track->getClips().getFirst())
            length = firstClip->getPosition().getLength();

        // change tempo to match main edit
        if (tryToMatchTempo && editToMatch != nullptr)
        {
            auto& targetTempo = editToMatch->tempoSequence.getTempoAt (0.01);
            auto firstTempo = edit->tempoSequence.getTempo (0);
            firstTempo->setBpm (targetTempo.getBpm());

            songTempo = targetTempo.getBpm();

            if (couldMatchTempo != nullptr)
                *couldMatchTempo = true;

            auto& targetPitch = editToMatch->pitchSequence.getPitchAt (0.01);
            auto firstPitch = edit->pitchSequence.getPitch (0);

            firstPitch->setPitch (targetPitch.getPitch());
            firstPitch->setScaleID (targetPitch.getScale());
        }

        if (resizeClip)
        {
            if (auto firstClip = track->getClips().getFirst())
            {
                length = length * clipTempo / songTempo;
                firstClip->setStart (0.0, false, true);
                firstClip->setLength (length, true);

                edit->getTransport().setLoopRange ({ 0.0, length });
            }
        }

        if (v.hasType (IDs::PROGRESSION))
        {
            auto clipLength = edit->tempoSequence.beatsToTime (32 * 4);
            edit->getTransport().setLoopRange ({ 0.0, clipLength });

            if (auto mc = track->insertMIDIClip ({ 0, clipLength }, nullptr))
            {
                if (auto pg = mc->getPatternGenerator())
                {
                    pg->mode = PatternGenerator::Mode::chords;
                    pg->octave = 6;
                    pg->setChordProgression (v.createCopy());
                    pg->generatePattern();
                }
            }
        }
        else if (v.hasType (IDs::BASSPATTERN))
        {
            auto clipLength = edit->tempoSequence.beatsToTime (32 * 4);
            edit->getTransport().setLoopRange ({ 0.0, clipLength });

            if (auto mc = track->insertMIDIClip ({ 0, clipLength }, nullptr))
            {
                if (auto pg = mc->getPatternGenerator())
                {
                    pg->mode = PatternGenerator::Mode::bass;
                    pg->octave = 6;
                    pg->setChordProgressionFromChordNames ({"i"});
                    pg->setBassPattern (v.createCopy());
                    pg->generatePattern();
                }
            }
        }
        else if (v.hasType (IDs::CHORDPATTERN))
        {
            auto clipLength = edit->tempoSequence.beatsToTime (32 * 4);
            edit->getTransport().setLoopRange ({ 0.0, clipLength });

            if (auto mc = track->insertMIDIClip ({ 0, clipLength }, nullptr))
            {
                if (auto pg = mc->getPatternGenerator())
                {
                    pg->mode = PatternGenerator::Mode::chords;
                    pg->octave = 6;
                    pg->setChordProgressionFromChordNames ({"i"});
                    pg->setChordPattern (v.createCopy());
                    pg->generatePattern();
                }
            }
        }
    }

    return edit;
}

std::unique_ptr<Edit> Edit::createEditForPreviewingFile (Engine& engine, const File& file, const Edit* editToMatch,
                                                         bool tryToMatchTempo, bool tryToMatchPitch, bool* couldMatchTempo,
                                                         juce::ValueTree midiPreviewPlugin)
{
    CRASH_TRACER
    auto edit = std::make_unique<Edit> (engine, createEmptyEdit(), forEditing, nullptr, 1);
    edit->ensureNumberOfAudioTracks (1);
    edit->isPreviewEdit = true;

    if (couldMatchTempo != nullptr)
        *couldMatchTempo = false;

    if (auto track = getFirstAudioTrack (*edit))
    {
        bool isMidi = file.hasFileExtension (String (midiFileWildCard).removeCharacters ("*"));

        if (isMidi)
        {
            if (ScopedPointer<FileInputStream> fin = file.createInputStream())
            {
                MidiFile mf;
                mf.readFrom (*fin);
                mf.convertTimestampTicksToSeconds();

                MidiMessageSequence sequence;

                for (int i = 0; i < mf.getNumTracks(); ++i)
                    sequence.addSequence (*mf.getTrack (i), 0.0, 0.0, 1.0e6);

                sequence.updateMatchedPairs();

                if (auto mc = track->insertMIDIClip ({ 0.0, 1.0 }, nullptr))
                {
                    if (midiPreviewPlugin.isValid())
                        track->pluginList.insertPlugin (midiPreviewPlugin, 0);

                    // Find bpm of source midi file
                    double srcBpm = 120.0;
                    MidiMessageSequence tempos;
                    mf.findAllTempoEvents (tempos);

                    if (auto* evt = tempos.getEventPointer (0))
                        if (evt->message.isTempoMetaEvent())
                            srcBpm = 4.0 * 60.0 / (4 * evt->message.getTempoSecondsPerQuarterNote());

                    // set tempo to that of midi file
                    edit->tempoSequence.getTempo (0)->setBpm (srcBpm);

                    // add midi notes
                    mc->mergeInMidiSequence (sequence, MidiList::NoteAutomationType::none);
                    auto length = sequence.getEndTime();

                    // change tempo to match main edit
                    if (tryToMatchTempo && editToMatch != nullptr)
                    {
                        auto& targetTempo = editToMatch->tempoSequence.getTempoAt (0.01);
                        auto firstTempo = edit->tempoSequence.getTempo (0);
                        firstTempo->setBpm (targetTempo.getBpm());

                        if (couldMatchTempo != nullptr)
                            *couldMatchTempo = true;
                    }

                    auto dstBpm = edit->tempoSequence.getTempo (0)->getBpm();

                    length *= srcBpm / dstBpm;

                    if (length < 0.001)
                        length = 2;

                    mc->setPosition ({ { 0.0, length }, 0.0 });
                    edit->getTransport().setLoopRange ({ 0.0, length });
                }
            }
        }
        else
        {
            const AudioFile af (file);
            auto length = af.getLength();

            if (auto wc = dynamic_cast<WaveAudioClip*> (track->insertNewClip (TrackItem::Type::wave, { 0.0, 1.0 }, nullptr)))
            {
                wc->setUsesProxy (false);
                wc->getSourceFileReference().setToDirectFileReference (file, false);

                if (editToMatch != nullptr)
                {
                    if (tryToMatchTempo || tryToMatchPitch)
                    {
                        wc->setUsesTimestretchedPreview (true);
                        wc->setLoopInfo (af.getInfo().loopInfo);
                        wc->setTimeStretchMode (TimeStretcher::defaultMode);
                    }

                    auto& targetTempo = editToMatch->tempoSequence.getTempoAt (0.01);
                    auto targetPitch = editToMatch->pitchSequence.getPitch (0);

                    if (tryToMatchTempo)
                    {
                        auto firstTempo = edit->tempoSequence.getTempo (0);
                        firstTempo->setBpm (targetTempo.getBpm());

                        edit->setTimecodeFormat (editToMatch->getTimecodeFormat());
                        AudioFileInfo wi = wc->getWaveInfo();

                        if (wi.sampleRate > 0.0)
                        {
                            if (wc->getLoopInfo().getNumBeats() > 0)
                            {
                                length *= wc->getLoopInfo().getBpm (wi) / targetTempo.getBpm();
                                wc->setAutoTempo (true);

                                if (couldMatchTempo != nullptr)
                                    *couldMatchTempo = true;
                            }
                        }
                    }

                    if (tryToMatchPitch)
                    {
                        auto firstPitch = edit->pitchSequence.getPitch (0);
                        firstPitch->setPitch (targetPitch->getPitch());

                        edit->pitchSequence.copyFrom (editToMatch->pitchSequence);
                        wc->setAutoPitch (wc->getLoopInfo().getRootNote() != -1);
                    }
                }

                wc->setPosition ({ { 0.0, length }, 0.0 });
                edit->getTransport().setLoopRange ({ 0.0, length });
            }
        }
    }

    return edit;
}

std::unique_ptr<Edit> Edit::createEditForPreviewingClip (Clip& clip)
{
    CRASH_TRACER
    auto edit = std::make_unique<Edit> (clip.edit.engine, createEmptyEdit(), forEditing, nullptr, 1);
    edit->setTempDirectory (clip.edit.getTempDirectory (false));
    edit->ensureNumberOfAudioTracks (1);
    edit->tempoSequence.copyFrom (clip.edit.tempoSequence);
    edit->pitchSequence.copyFrom (clip.edit.pitchSequence);
    edit->setTimecodeFormat (clip.edit.getTimecodeFormat());
    edit->isPreviewEdit = true;

    if (auto track = getFirstAudioTrack (*edit))
    {
        if (auto c = track->insertClipWithState (clip.state.createCopy()))
        {
            CRASH_TRACER
            jassert (c->getPosition() == clip.getPosition());
            jassert (c->getLoopStart() == clip.getLoopStart());
            jassert (c->getLoopLength() == clip.getLoopLength());
            jassert (c->itemID == clip.itemID);

            // The idea here is that the new clip should be bodged to share the same proxy
            // file as the old one, to avoid having to render a new one.. If this assertion
            // fails, then something has gone wrong with that plan
            jassert (dynamic_cast<AudioClipBase*> (c) == nullptr
                       || ((AudioClipBase*) c)->getPlaybackFile() == ((AudioClipBase&) clip).getPlaybackFile()
                       || ((AudioClipBase*) c)->isUsingMelodyne());

            if (c->isMidi())
                track->getOutput().setOutputToDefaultDevice (true);
        }
        else
        {
            jassertfalse;
        }
    }

    return edit;
}

std::unique_ptr<Edit> Edit::createSingleTrackEdit (Engine& engine)
{
    CRASH_TRACER
    auto edit = std::make_unique<Edit> (engine, createEmptyEdit(), forEditing, nullptr, 1);

    edit->ensureNumberOfAudioTracks (1);

    if (auto track = getFirstAudioTrack (*edit))
        track->getOutput().setOutputToDefaultDevice (false);

    return edit;
}

//==============================================================================
EditDeleter::EditDeleter() = default;
EditDeleter::~EditDeleter() { TRACKTION_ASSERT_MESSAGE_THREAD }

void EditDeleter::deleteEdit (std::unique_ptr<Edit> ed)
{
    const juce::ScopedLock sl (listLock);
    editsToDelete.add (ed.release());
    triggerAsyncUpdate();
}

void EditDeleter::handleAsyncUpdate()
{
    const juce::ScopedLock sl (listLock);
    editsToDelete.clear();
}

//==============================================================================
ActiveEdits::ActiveEdits() = default;

juce::Array<Edit*> ActiveEdits::getEdits() const
{
    juce::Array<Edit*> eds;
    eds.addArray (edits);
    return eds;
}
