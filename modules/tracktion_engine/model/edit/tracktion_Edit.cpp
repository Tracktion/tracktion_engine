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

Edit::GlobalMacros::GlobalMacros (Edit& e)
    : MacroParameterElement (e, e.state),
      edit (e)
{
}

//==============================================================================
struct Edit::UndoTransactionTimer   : private juce::Timer,
                                      private juce::ChangeListener
{
    UndoTransactionTimer (Edit& e)
        : edit (e)
    {
        // Add the change listener asyncronously to avoid messages coming in
        // from the Edit initialisation phase
        juce::MessageManager::callAsync ([ref = juce::WeakReference<UndoTransactionTimer> (this)]
                                         {
                                             if (ref != nullptr)
                                                 ref->edit.getUndoManager().addChangeListener (ref.get());
                                         });
    }

    ~UndoTransactionTimer() override
    {
        edit.getUndoManager().removeChangeListener (this);
    }

    void timerCallback() override
    {
        if (edit.numUndoTransactionInhibitors > 0)
            return;

        if (! juce::Component::isMouseButtonDownAnywhere())
        {
            stopTimer();
            edit.getUndoManager().beginNewTransaction();
        }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        edit.markAsChanged();
        startTimer (350);
    }

    Edit& edit;

    JUCE_DECLARE_WEAK_REFERENCEABLE (UndoTransactionTimer)
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

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
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
                    if (v[IDs::endToEnd])
                        edit.getTransport().ensureContextAllocated();
                    else
                        edit.getTransport().freePlaybackContext();

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
                         || i == IDs::loopedSequenceType || i == IDs::grooveStrength
                         || i == IDs::proxyAllowed || i == IDs::resamplingQuality || i == IDs::warpTime)
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
                    || i == IDs::velocities || i == IDs::gates || i == IDs::probabilities 
                     || i == IDs::note || i == IDs::velocity || i == IDs::groove
                     || i == IDs::grooveStrength)
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
                const auto type = v[IDs::type].toString();

                if (i == IDs::enabled
                    && edit.engine.getEngineBehaviour().shouldBypassedPluginsBeRemovedFromPlaybackGraph())
                   restart();

                if (type == AuxSendPlugin::xmlTypeName || type == AuxReturnPlugin::xmlTypeName)
                {
                    if (i == IDs::enabled || i == IDs::busNum)
                        restart();
                }
                else if (type == RackInstance::xmlTypeName)
                {
                    if (i == IDs::enabled
                        || i == IDs::leftTo || i == IDs::rightTo || i == IDs::leftFrom || i == IDs::rightFrom)
                       restart();
                }
                else if (i == IDs::outputDevice || i == IDs::inputDevice
                         || i == IDs::manualAdjustMs || i == IDs::sidechainSourceID || i == IDs::ignoreVca || i == IDs::busNum)
                {
                    restart();
                }
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

    void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int) override
    {
        childAddedOrRemoved (p, c);
    }

    void childAddedOrRemoved (juce::ValueTree& p, juce::ValueTree& c)
    {
        if (c.hasType (IDs::NOTE)
             || c.hasType (IDs::CONTROL)
             || c.hasType (IDs::SYSEX)
             || c.hasType (IDs::PLUGIN)
             || c.hasType (IDs::RACK)
             || c.hasType (IDs::PLUGININSTANCE)
             || c.hasType (IDs::CONNECTION)
             || p.hasType (IDs::CHANNELS)
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

    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    void clipMovedOrAdded (const juce::ValueTree& v)
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
            trackStatusUpdater.reset (new TrackStatusUpdater (*this));
    }

    struct TrackStatusUpdater  : private juce::AsyncUpdater
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

    std::unique_ptr<TrackStatusUpdater> trackStatusUpdater;

    //==============================================================================
    void linkedClipsChanged()
    {
        linkedClipsMapDirty = true;
    }

    juce::Array<Clip*> getClipsInLinkGroup (const juce::String& linkGroupid)
    {
        jassert (! edit.isLoading());

        if (linkedClipsMapDirty)
        {
            linkedClipsMap.clear();

            edit.visitAllTracksRecursive ([&] (Track& t)
            {
                if (auto ct = dynamic_cast<ClipTrack*> (&t))
                    for (auto c : ct->getClips())
                        if (c->getLinkGroupID().isNotEmpty())
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
    std::map<juce::String, juce::Array<Clip*>> linkedClipsMap;

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
        for (auto plugin : plugins)
            if (auto p = dynamic_cast<Plugin*> (plugin.get()))
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
    static juce::Atomic<int> nextId;
    return ++nextId;
}

//==============================================================================
Edit::Edit (Options options)
    : engine (options.engine),
      tempoSequence (*this),
      state (options.editState),
      instanceId (getNextInstanceId()),
      editProjectItemID (options.editProjectItemID),
      loadContext (options.loadContext),
      editRole (options.role)
{
    CRASH_TRACER

    jassert (state.isValid());
    jassert (state.hasType (IDs::EDIT));
    jassert (editProjectItemID.load().isValid()); // This must be valid or it won't be able to create temp files etc.

    if (options.editFileRetriever)
        editFileRetriever = std::move (options.editFileRetriever);
    else
        editFileRetriever = [this]() -> juce::File
        {
            if (auto item = engine.getProjectManager().getProjectItem (*this))
                return item->getSourceFile();

            return {};
        };

    if (options.filePathResolver)
        filePathResolver = std::move (options.filePathResolver);
    else
        filePathResolver = [this] (const juce::String& path) -> juce::File
        {
            jassert (path.isNotEmpty());

            if (juce::File::isAbsolutePath (path))
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

    undoManager.setMaxNumberOfStoredUnits (1000 * options.numUndoLevelsToStore, options.numUndoLevelsToStore);

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

Edit::Edit (Engine& e, juce::ValueTree editState, EditRole role,
            LoadContext* sourceLoadContext, int numUndoLevelsToStore)
    : Edit ({ e, editState, ProjectItemID::fromProperty (editState, IDs::projectID),
              role, sourceLoadContext, numUndoLevelsToStore, {}, {} })
{
}

Edit::~Edit()
{
    CRASH_TRACER

    for (auto af : pluginCache->getPlugins())
        af->hideWindowForShutdown();

    for (auto at : getTracksOfType<AudioTrack> (*this, true))
        for (auto c : at->getClips())
            if (auto acb = dynamic_cast<AudioClipBase*> (c))
                acb->hideMelodyneWindow();

    engine.getActiveEdits().edits.removeFirstMatchingValue (this);
    masterReference.clear();
    changeResetterTimer.reset();

    if (transportControl != nullptr)
        transportControl->freePlaybackContext();

    // must only delete an edit with the message thread locked - many things on the message thread may be in the
    // middle of using it at this point..
    jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());
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
juce::String Edit::getName()
{
    if (auto item = engine.getProjectManager().getProjectItem (*this))
        return item->getName();

    return {};
}

void Edit::setProjectItemID (ProjectItemID newID)
{
    editProjectItemID = newID;
    state.setProperty (IDs::projectID, editProjectItemID.load().toString(), nullptr);
}

Edit::ScopedRenderStatus::ScopedRenderStatus (Edit& ed, bool shouldReallocateOnDestruction)
    : edit (ed), reallocateOnDestruction (shouldReallocateOnDestruction)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (edit.performingRenderCount >= 0);
    ++edit.performingRenderCount;
    edit.getTransport().freePlaybackContext();
}

Edit::ScopedRenderStatus::~ScopedRenderStatus()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (edit.performingRenderCount > 0);
    --edit.performingRenderCount;

    if (edit.performingRenderCount == 0 && reallocateOnDestruction && edit.shouldPlay())
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
        state.setProperty (IDs::creationTime, juce::Time::getCurrentTime().toMilliseconds(), nullptr);

    lastSignificantChange.referTo (state, IDs::lastSignificantChange, nullptr,
                                   juce::String::toHexString (juce::Time::getCurrentTime().toMilliseconds()));

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
    TemporaryFileManager::purgeOrphanFreezeAndProxyFiles (*this);

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

                      for (auto effect : getAllClipEffects (*this))
                          effect->initialise();
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
    // Initiliase PitchSequence first as the TempoSequence depends on it
    pitchSequence.initialise (*this, state.getOrCreateChildWithName (IDs::PITCHSEQUENCE, nullptr));

    const bool needToLoadOldTempoData = ! state.getChildWithName (IDs::TEMPOSEQUENCE).isValid();
    tempoSequence.setState (state.getOrCreateChildWithName (IDs::TEMPOSEQUENCE, nullptr), false);

    if (needToLoadOldTempoData)
        loadOldTimeSigInfo();
}

void Edit::initialiseTimecode (juce::ValueTree& transportState)
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
        meta.setProperty (IDs::date, juce::Time::getCurrentTime().getYear(), nullptr);
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
        getMasterSliderPosParameter()->setParameter (masterVolumeFaderPos, juce::dontSendNotification);

        getMasterPanParameter()->getCurve().clear();
        getMasterPanParameter()->setParameter (masterPan, juce::dontSendNotification);
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
            if (c->getPosition().getLength().inSeconds() <= 0.0)
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

    ensureArrangerTrack();
    ensureTempoTrack();
    ensureMarkerTrack();
    ensureChordTrack();
    ensureMasterTrack();
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

    addValueTreeProperties (state,
                            IDs::appVersion, engine.getPropertyStorage().getApplicationVersion(),
                            IDs::modifiedBy, engine.getPropertyStorage().getUserName(),
                            IDs::projectID, editProjectItemID.load().toString());

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

juce::Time Edit::getTimeOfLastChange() const
{
    return juce::Time (lastSignificantChange.get().getHexValue64());
}

void Edit::resetChangedStatus()
{
    // Dispatch pending changes syncronously or hasChanged will always get set to true
    getUndoManager().dispatchPendingMessages();
    hasChanged = false;
}

void Edit::markAsChanged()
{
    lastSignificantChange = juce::String::toHexString (juce::Time::getCurrentTime().toMilliseconds());
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
static juce::Array<SelectionManager*> getSelectionManagers (const Edit& ed)
{
    juce::Array<SelectionManager*> sms;

    for (SelectionManager::Iterator sm; sm.next();)
        if (sm->getEdit() == &ed)
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
        sm->refreshPropertyPanel();
    }
}

void Edit::undo()           { undoOrRedo (true); }
void Edit::redo()           { undoOrRedo (false); }

Edit::UndoTransactionInhibitor::UndoTransactionInhibitor (Edit& e) : edit (&e)                                  { ++e.numUndoTransactionInhibitors; }
Edit::UndoTransactionInhibitor::UndoTransactionInhibitor (const UndoTransactionInhibitor& o) : edit (o.edit)    { if (auto e = dynamic_cast<Edit*> (edit.get())) ++(e->numUndoTransactionInhibitors); }
Edit::UndoTransactionInhibitor::~UndoTransactionInhibitor()                                                     { if (auto e = dynamic_cast<Edit*> (edit.get())) --(e->numUndoTransactionInhibitors); }

//==============================================================================
EditItemID Edit::createNewItemID (const std::vector<EditItemID>& idsToAvoid) const
{
    if (nextID == 0)
    {
        auto existingIDs = EditItemID::findAllIDs (state);

        existingIDs.insert (existingIDs.end(), idsToAvoid.begin(), idsToAvoid.end());

        trackCache.visitItems ([&] (auto i)  { existingIDs.push_back (i->itemID); });
        clipCache.visitItems ([&] (auto i)   { existingIDs.push_back (i->itemID); });

        std::sort (existingIDs.begin(), existingIDs.end());
        nextID = existingIDs.empty() ? 1001 : (existingIDs.back().getRawID() + 1);

       #if JUCE_DEBUG
        usedIDs.insert (existingIDs.begin(), existingIDs.end());
       #endif
    }

    auto newID = EditItemID::fromRawID (nextID++);

   #if JUCE_DEBUG
    jassert (usedIDs.find (newID) == usedIDs.end());
    usedIDs.insert (newID);
   #endif

    return newID;
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

TimePosition Edit::getNextTimeOfInterest (TimePosition t)
{
    if (t < TimePosition())
        return {};

    auto first = toPosition (getLength());

    for (auto ct : getClipTracks (*this))
    {
        auto d = ct->getNextTimeOfInterest (t);

        if (d < first && d > t)
            first = d;
    }

    return first;
}

TimePosition Edit::getPreviousTimeOfInterest (TimePosition t)
{
    if (t < TimePosition())
        return {};

    TimePosition last;

    for (auto ct : getClipTracks (*this))
    {
        auto d = ct->getPreviousTimeOfInterest (t);

        if (d > last)
            last = d;
    }

    return last;
}

//==============================================================================
Track::Ptr Edit::loadTrackFrom (juce::ValueTree& v)
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
static Track::Ptr createAndInitialiseTrack (Edit& ed, const juce::ValueTree& v)
{
    Track::Ptr t = new Type (ed, v);
    t->initialise();
    return t;
}

Track::Ptr Edit::createTrack (const juce::ValueTree& v)
{
    CRASH_TRACER

    if (v.hasType (IDs::TRACK))            return createAndInitialiseTrack<AudioTrack> (*this, v);
    if (v.hasType (IDs::MARKERTRACK))      return createAndInitialiseTrack<MarkerTrack> (*this, v);
    if (v.hasType (IDs::FOLDERTRACK))      return createAndInitialiseTrack<FolderTrack> (*this, v);
    if (v.hasType (IDs::AUTOMATIONTRACK))  return createAndInitialiseTrack<AutomationTrack> (*this, v);
    if (v.hasType (IDs::TEMPOTRACK))       return createAndInitialiseTrack<TempoTrack> (*this, v);
    if (v.hasType (IDs::CHORDTRACK))       return createAndInitialiseTrack<ChordTrack> (*this, v);
    if (v.hasType (IDs::ARRANGERTRACK))    return createAndInitialiseTrack<ArrangerTrack> (*this, v);
    if (v.hasType (IDs::MASTERTRACK))      return createAndInitialiseTrack<MasterTrack> (*this, v);

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

void Edit::setTimecodeOffset (TimeDuration newOffset)
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

    std::unique_ptr<juce::XmlElement> oldInfoXml (oldInfoState.createXml());

    if (auto oldInfo = oldInfoXml.get())
    {
        if (auto sequenceNode = oldInfo->getFirstChildElement())
            if (sequenceNode->hasTagName ("SECTION"))
                oldInfo = sequenceNode;

        if (auto tempo = tempoSequence.getTempo (0))
            tempo->set (BeatPosition(), oldInfo->getDoubleAttribute ("bpm", 120.0), 0, false);

        if (auto timeSig = tempoSequence.getTimeSig (0))
        {
            timeSig->setStringTimeSig (oldInfo->getStringAttribute ("timeSignature", "4/4"));
            timeSig->triplets = oldInfo->getBoolAttribute ("triplets", false);
        }

        state.removeChild (state.getChildWithName ("TIMECODETYPE"), nullptr);
    }
}

void Edit::loadOldVideoInfo (const juce::ValueTree& videoState)
{
    CRASH_TRACER

    const juce::File videoFile (videoState["videoFile"].toString());

    if (videoFile.existsAsFile())
        if (auto proj = engine.getProjectManager().getProject (*this))
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

    for (auto sm : getSelectionManagers (*this))
        if (! sm->containsType<ExternalPlugin>())
            sm->refreshPropertyPanel();

    auto& ecm = engine.getExternalControllerManager();

    if (ecm.isAttachedToEdit (*this))
        ecm.editPositionChanged (this, getTransport().position);
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
    getMasterSliderPosParameter()->setParameter (v, juce::sendNotification);
}

void Edit::setMasterPanPos (float p)
{
    getMasterPanParameter()->setParameter (p, juce::sendNotification);
}

//==============================================================================
juce::String Edit::getAuxBusName (int bus) const
{
    return auxBusses.getChildWithProperty (IDs::bus, bus).getProperty (IDs::name);
}

void Edit::setAuxBusName (int bus, const juce::String& nm)
{
    auto name = nm.substring (0, 20);

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
                b = createValueTree (IDs::NAME,
                                     IDs::bus, bus);
                auxBusses.addChild (b, -1, nullptr);
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
TimeDuration Edit::getLength() const
{
    if (! totalEditLength)
    {
        totalEditLength = TimeDuration();

        for (auto t : getClipTracks (*this))
            totalEditLength = juce::jmax (*totalEditLength, t->getLength());
    }

    return *totalEditLength;
}

TimePosition Edit::getFirstClipTime() const
{
    auto t = TimePosition::fromSeconds (getLength().inSeconds());
    bool gotOne = false;

    for (auto track : getClipTracks (*this))
    {
        if (auto first = track->getClips().getFirst())
        {
            gotOne = true;
            t = std::min (t, first->getPosition().getStart());
        }
    }

    return gotOne ? t : TimePosition();
}

juce::Array<Clip*> Edit::findClipsInLinkGroup (juce::String linkGroupID) const
{
    if (treeWatcher != nullptr)
        return treeWatcher->getClipsInLinkGroup (linkGroupID);

    return {};
}

Track::Ptr Edit::insertTrack (TrackInsertPoint insertPoint, juce::ValueTree v, SelectionManager* sm)
{
    CRASH_TRACER

    if (getAllTracks (*this).size() >= engine.getEngineBehaviour().getEditLimits().maxNumTracks)
        return {};

    auto parent = state;

    if (insertPoint.parentTrackID.isValid())
        if (auto t = findTrackForID (*this, insertPoint.parentTrackID))
            parent = t->state;

    auto preceeding = parent.getChildWithProperty (IDs::id, insertPoint.precedingTrackID);

    return insertTrack (v, parent, preceeding, sm);
}

Track::Ptr Edit::insertTrack (juce::ValueTree v, juce::ValueTree parent,
                              juce::ValueTree preceeding, SelectionManager* sm)
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
    return insertTrack (insertPoint, juce::ValueTree (xmlType), sm);
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

    for (auto sm : getSelectionManagers (*this))
        sm->keepSelectedObjectsOnScreen();
}

void Edit::updateTrackStatuses()
{
    CRASH_TRACER

    sanityCheckTrackNames();
    updateMuteSoloStatuses();

    // Only refresh if this Edit is being shown or it can cancel things like file previews
    for (auto sm : getSelectionManagers (*this))
        sm->refreshPropertyPanel();
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
        // You can't delete global tracks, just hide them instead
        jassert (! (t->isMasterTrack() || t->isMarkerTrack() || t->isTempoTrack() || t->isChordTrack()));

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

void Edit::ensureArrangerTrack()
{
    if (getArrangerTrack() == nullptr)
    {
        auto v = createValueTree (IDs::ARRANGERTRACK,
                                  IDs::name, TRANS("Arranger"));

        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureTempoTrack()
{
    if (getTempoTrack() == nullptr)
    {
        auto v = createValueTree (IDs::TEMPOTRACK,
                                  IDs::name, TRANS("Global"));

        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureMarkerTrack()
{
    if (auto t = getMarkerTrack())
    {
        t->setName (TRANS("Marker"));
    }
    else
    {
        auto v = createValueTree (IDs::MARKERTRACK,
                                  IDs::name, TRANS("Marker"));

        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureChordTrack()
{
    if (auto t = getChordTrack())
    {
        t->setName (TRANS("Chord"));
    }
    else
    {
        auto v = createValueTree (IDs::CHORDTRACK,
                                  IDs::name, TRANS("Chord"));

        state.addChild (v, 0, &getUndoManager());
    }
}

void Edit::ensureMasterTrack()
{
    if (auto t = getMasterTrack())
    {
        t->setName (TRANS("Master"));
    }
    else
    {
        auto v = createValueTree (IDs::MASTERTRACK,
                                  IDs::name, TRANS("Master"));

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
        if (auto p = dynamic_cast<Plugin*> (changed.get()))
            sendMirrorUpdateToAllPlugins (*p);

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

void Edit::addModifierTimer (ModifierTimer& mt)
{
    jassert (! modifierTimers.contains (&mt));
    modifierTimers.add (&mt);
}

void Edit::removeModifierTimer (ModifierTimer& mt)
{
    modifierTimers.removeFirstMatchingValue (&mt);
}

void Edit::updateModifierTimers (TimePosition editTime, int numSamples) const
{
    const juce::ScopedLock sl (modifierTimers.getLock());

    for (auto mt : modifierTimers)
        mt->updateStreamTime (editTime, numSamples);
}

//==============================================================================
void Edit::setLowLatencyMonitoring (bool enabled, const juce::Array<EditItemID>& plugins)
{
    lowLatencyMonitoring = enabled;
    juce::AudioDeviceManager::AudioDeviceSetup setup;

    auto& deviceManager = engine.getDeviceManager().deviceManager;
    deviceManager.getAudioDeviceSetup (setup);

    if (lowLatencyMonitoring)
    {
        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            normalLatencyBufferSizeSeconds = setup.bufferSize / setup.sampleRate;
            setup.bufferSize = juce::roundToInt ((static_cast<double> (engine.getPropertyStorage().getProperty (SettingID::lowLatencyBuffer, 5.8)) / 1000.0) * setup.sampleRate);
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
        setup.bufferSize = juce::roundToInt (normalLatencyBufferSizeSeconds * setup.sampleRate);
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

void Edit::setLowLatencyDisabledPlugins (const juce::Array<EditItemID>& newPlugins)
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

juce::Array<InputDeviceInstance*> Edit::getAllInputDevices() const
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
void Edit::setClickTrackRange (TimeRange newTimes) noexcept
{
    clickMark1Time = newTimes.getStart();
    clickMark2Time = newTimes.getEnd();
}

TimeRange Edit::getClickTrackRange() const noexcept
{
    return TimeRange::between (clickMark1Time, clickMark2Time);
}

juce::String Edit::getClickTrackDevice() const
{
    if (clickTrackDevice == DeviceManager::getDefaultAudioOutDeviceName (false)
         || clickTrackDevice == DeviceManager::getDefaultMidiOutDeviceName (false))
        return clickTrackDevice;

    if (auto out = engine.getDeviceManager().findOutputDeviceWithName (clickTrackDevice))
        return out->getName();

    return DeviceManager::getDefaultAudioOutDeviceName (false);
}

bool Edit::isClickTrackDevice (OutputDevice& dev) const
{
    auto& dm = engine.getDeviceManager();

    if (auto out = dm.findOutputDeviceWithName (clickTrackDevice))
        return out == &dev;

    if (clickTrackDevice == DeviceManager::getDefaultMidiOutDeviceName (false))
        return dm.getDefaultMidiOutDevice() == &dev;

    return dm.getDefaultWaveOutDevice() == &dev;
}

void Edit::setClickTrackOutput (const juce::String& deviceName)
{
    clickTrackDevice = deviceName;
    restartPlayback();
}

void Edit::setClickTrackVolume (float gain)
{
    clickTrackGain = juce::jlimit (0.2f, 1.0f, gain);
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
void Edit::readFrozenTracksFiles()
{
    CRASH_TRACER

    auto freezeFiles = TemporaryFileManager::getFrozenTrackFiles (*this);
    bool resetFrozenness = freezeFiles.isEmpty();

    for (auto& freezeFile : freezeFiles)
    {
        const auto outId = TemporaryFileManager::getDeviceIDFromFreezeFile (*this, freezeFile);

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
        AudioFile::deleteFiles (engine, freezeFiles);

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
    const ScopedRenderStatus srs (*this, true);

    AudioFile::deleteFiles (engine, TemporaryFileManager::getFrozenTrackFiles (*this));

    auto& dm = engine.getDeviceManager();

    for (int j = dm.getNumOutputDevices(); --j >= 0;)
    {
        if (auto outputDevice = dynamic_cast<WaveOutputDevice*> (dm.getOutputDeviceAt (j)))
        {
            juce::BigInteger frozen;
            TimeDuration length;
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
                        length = juce::jmax (length, at->getLengthIncludingInputTracks());
                    }
                }

                ++i;
            }

            if (frozen.countNumberOfSetBits() > 0 && length > TimeDuration())
            {
                length = length + TimeDuration::fromSeconds (5.0);

                for (auto sm : getSelectionManagers (*this))
                    sm->deselectAll();

                juce::StringPairArray metadataFound;

                Renderer::Parameters r (*this);
                r.tracksToDo = frozen;
                r.destFile = TemporaryFileManager::getFreezeFileForDevice (*this, *outputDevice);
                r.audioFormat = engine.getAudioFileFormatManager().getFrozenFileFormat();
                r.blockSizeForAudio = dm.getBlockSize();
                r.sampleRateForAudio = dm.getSampleRate();
                r.time = { {}, length };
                r.canRenderInMono = true;
                r.mustRenderInMono = false;
                r.usePlugins = true;
                r.useMasterPlugins = false;
                r.category = ProjectItem::Category::frozen;
                r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (engine);

                Renderer::renderToProjectItem (TRANS("Updating frozen tracks for output device \"XDVX\"")
                                                  .replace ("XDVX", outputDevice->getName()) + "...", r);

                if (! r.destFile.exists())
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

void Edit::needToUpdateFrozenTracks()
{
    // ignore this until fully loaded
    if (frozenTrackCallback != nullptr && ! isLoadInProgress)
        frozenTrackCallback->triggerAsyncUpdate();
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

juce::File Edit::getTempDirectory (bool createIfNonExistent) const
{
    juce::File result (tempDirectory);

    if (result == juce::File())
        result = tempDirectory = engine.getTemporaryFileManager().getTempDirectory()
                                    .getChildFile ("edit_" + getProjectItemID().toStringSuitableForFilename());

    if (createIfNonExistent && ! result.createDirectory())
        TRACKTION_LOG_ERROR ("Failed to create edit temp folder: " + result.getFullPathName());

    return result;
}

void Edit::setTempDirectory (const juce::File& dir)
{
    tempDirectory = dir;
}

//==============================================================================
void Edit::setVideoFile (const juce::File& f, juce::String importDesc)
{
    CRASH_TRACER
    juce::File currentFile;

    if (auto item = engine.getProjectManager().getProjectItem (videoSource))
        currentFile = item->getSourceFile();

    if (f != currentFile)
    {
        videoSource.resetToDefault();

        if (auto proj = engine.getProjectManager().getProject (*this))
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

juce::File Edit::getVideoFile() const
{
    if (videoSource.get().isValid())
        if (auto item = engine.getProjectManager().getProjectItem (videoSource))
            return item->getSourceFile();

    return {};
}

//==============================================================================
juce::Array<AutomatableParameter*> Edit::getAllAutomatableParams (bool includeTrackParams) const
{
    juce::Array<AutomatableParameter*> list;

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
        // Skip the MasterTrack as that is covered by the masterPluginList above
        auto masterTrack = getMasterTrack();
        
        for (auto t : getAllTracks (*this))
        {
            if (t == masterTrack)
                continue;
            
            list.addArray (t->macroParameterList.getMacroParameters());
            list.addArray (t->getAllAutomatableParams());
        }
    }

    return list;
}

//==============================================================================
ArrangerTrack* Edit::getArrangerTrack() const
{
    return dynamic_cast<ArrangerTrack*> (findTrackForPredicate (*this, [] (Track& t) { return t.isArrangerTrack(); }));
}

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

MasterTrack* Edit::getMasterTrack() const
{
    return dynamic_cast<MasterTrack*> (findTrackForPredicate (*this, [] (Track& t) { return t.isMasterTrack(); }));
}

Edit::Metadata Edit::getEditMetadata()
{
    Metadata metadata;

    auto meta = state.getChildWithName (IDs::ID3VORBISMETADATA);

    metadata.album       = meta[IDs::album];
    metadata.artist      = meta[IDs::artist];
    metadata.comment     = meta[IDs::comment];
    metadata.date        = meta.getProperty (IDs::date, juce::Time::getCurrentTime().getYear());
    metadata.genre       = meta[IDs::genre];
    metadata.title       = meta[IDs::title];
    metadata.trackNumber = meta.getProperty (IDs::trackNumber, 1);

    return metadata;
}

void Edit::setEditMetadata (Metadata metadata)
{
    auto meta = state.getChildWithName (IDs::ID3VORBISMETADATA);

    addValueTreeProperties (meta,
                            IDs::album, metadata.album,
                            IDs::artist, metadata.artist,
                            IDs::comment, metadata.comment,
                            IDs::date, metadata.date,
                            IDs::genre, metadata.genre,
                            IDs::title, metadata.title,
                            IDs::trackNumber, metadata.trackNumber);
}

//==============================================================================
std::unique_ptr<Edit> Edit::createEditForPreviewingPreset (Engine& engine, juce::ValueTree v, const Edit* editToMatch,
                                                           bool tryToMatchTempo, bool* couldMatchTempo, juce::ValueTree midiPreviewPlugin,
                                                           juce::ValueTree midiDrumPreviewPlugin, bool forceMidiToDrums, Edit* editToUpdate)
{
    CRASH_TRACER
    std::unique_ptr<Edit> edit;

    if (editToUpdate != nullptr)
        edit.reset (editToUpdate);
    else
        edit = std::make_unique<Edit> (engine, createEmptyEdit (engine), forEditing, nullptr, 1);

    edit->ensureNumberOfAudioTracks (2);
    edit->isPreviewEdit = true;
    edit->getTransport().setCurrentPosition (0);

    if (couldMatchTempo != nullptr)
        *couldMatchTempo = false;

    auto tracks = getAudioTracks (*edit);
    if (tracks.size() < 2)
        return {};

    for (auto t : tracks)
    {
        auto& clips = t->getClips();
        for (int i = clips.size(); --i >= 0;)
            clips[i]->removeFromParentTrack();

        jassert (t->getClips().size() == 0);
    }

    auto& master = edit->getMasterPluginList();

    while (master.size() > 0)
        master[0]->removeFromParent();

    bool isDrums = false;

    auto midiTrack  = tracks[0];
    auto drumTrack  = tracks[1];

    bool resizeClip = false;
    double clipTempo = 120.0;

    if (Clip::isClipState (v))
    {
        juce::MemoryBlock mb;
        mb.fromBase64Encoding (v[IDs::state].toString());

        auto state = updateLegacyEdit (juce::ValueTree::readFromData (mb.getData(), mb.getSize()));

        if (state.isValid())
        {
            EditItemID::remapIDs (state, nullptr, *edit);
            midiTrack->insertClipWithState (state);
        }

        clipTempo = state[IDs::bpm];

        if (clipTempo < 20.0)
            clipTempo = 120.0;

        resizeClip = true;
    }

    if (auto firstClip = midiTrack->getClips().getFirst())
    {
        if (dynamic_cast<StepClip*> (firstClip) != nullptr)
            isDrums = true;
        else if (auto mc = dynamic_cast<MidiClip*> (firstClip))
            isDrums = forceMidiToDrums || mc->isRhythm();
    }

    auto track = midiTrack;

    if (isDrums)
    {
        track = drumTrack;

        if (auto firstClip = midiTrack->getClips().getFirst())
            firstClip->moveToTrack (*track);
    }

    if (track->pluginList.size() < 3)
    {
        if (isDrums && midiDrumPreviewPlugin.isValid())
            track->pluginList.insertPlugin (midiDrumPreviewPlugin, 0);
        if (! isDrums && midiPreviewPlugin.isValid())
            track->pluginList.insertPlugin (midiPreviewPlugin, 0);
    }

    double songTempo = 120.0;
    auto length = TimeDuration::fromSeconds (1.0);

    // Get original clip length
    if (auto firstClip = track->getClips().getFirst())
        length = firstClip->getPosition().getLength();

    // change tempo to match main edit
    if (tryToMatchTempo && editToMatch != nullptr)
    {
        auto& targetTempo = editToMatch->tempoSequence.getTempoAt (TimePosition::fromSeconds (0.01));
        auto firstTempo = edit->tempoSequence.getTempo (0);
        firstTempo->setBpm (targetTempo.getBpm());

        songTempo = targetTempo.getBpm();

        if (couldMatchTempo != nullptr)
            *couldMatchTempo = true;

        auto& targetPitch = editToMatch->pitchSequence.getPitchAt (TimePosition::fromSeconds (0.01));

        if (auto firstPitch = edit->pitchSequence.getPitch (0))
        {
            firstPitch->setPitch (targetPitch.getPitch());
            firstPitch->setScaleID (targetPitch.getScale());
        }
        else
        {
            jassertfalse;
        }
    }

    if (resizeClip)
    {
        if (auto firstClip = track->getClips().getFirst())
        {
            length = TimeDuration::fromSeconds (length.inSeconds() * clipTempo / songTempo);
            firstClip->setStart ({}, false, true);
            firstClip->setLength (length, true);

            edit->getTransport().setLoopRange ({ TimePosition(), length });
        }
    }

    if (v.hasType (IDs::PROGRESSION))
    {
        auto clipLength = edit->tempoSequence.toTime (BeatPosition::fromBeats (32 * 4));
        edit->getTransport().setLoopRange ({ TimePosition(), clipLength });

        if (auto mc = track->insertMIDIClip ({ TimePosition(), clipLength }, nullptr))
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
        auto clipLength = edit->tempoSequence.toTime (BeatPosition::fromBeats (32 * 4));
        edit->getTransport().setLoopRange ({ TimePosition(), clipLength });

        if (auto mc = track->insertMIDIClip ({ TimePosition(), clipLength }, nullptr))
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
        auto clipLength = edit->tempoSequence.toTime (BeatPosition::fromBeats (32 * 4));
        edit->getTransport().setLoopRange ({ TimePosition(), clipLength });

        if (auto mc = track->insertMIDIClip ({ TimePosition(), clipLength }, nullptr))
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

    return edit;
}

std::unique_ptr<Edit> Edit::createEditForPreviewingFile (Engine& engine, const juce::File& file, const Edit* editToMatch,
                                                         bool tryToMatchTempo, bool tryToMatchPitch, bool* couldMatchTempo,
                                                         juce::ValueTree midiPreviewPlugin,
                                                         juce::ValueTree midiDrumPreviewPlugin,
                                                         bool forceMidiToDrums,
                                                         Edit* editToUpdate)
{
    CRASH_TRACER
    std::unique_ptr<Edit> edit;
    if (editToUpdate != nullptr)
        edit.reset (editToUpdate);
    else
        edit = std::make_unique<Edit> (engine, createEmptyEdit (engine), forEditing, nullptr, 1);

    edit->ensureNumberOfAudioTracks (3);
    edit->isPreviewEdit = true;
    edit->getTransport().setCurrentPosition (0);

    if (couldMatchTempo != nullptr)
        *couldMatchTempo = false;

    auto tracks = getAudioTracks (*edit);
    if (tracks.size() < 3)
        return {};

    for (auto t : tracks)
    {
        auto& clips = t->getClips();
        for (int i = clips.size(); --i >= 0;)
            clips[i]->removeFromParentTrack();

        jassert (t->getClips().size() == 0);
    }

    auto& master = edit->getMasterPluginList();
    while (master.size() > 0)
        master[0]->removeFromParent();

    auto audioTrack = tracks[0];
    auto midiTrack  = tracks[1];
    auto drumTrack  = tracks[2];

    bool isMidi = file.hasFileExtension (juce::String (midiFileWildCard).removeCharacters ("*"));

    if (isMidi)
    {
        if (auto fin = file.createInputStream())
        {
            juce::MidiFile mf;
            mf.readFrom (*fin);
            mf.convertTimestampTicksToSeconds();

            juce::MidiMessageSequence sequence;

            for (int i = 0; i < mf.getNumTracks(); ++i)
                sequence.addSequence (*mf.getTrack (i), 0.0, 0.0, 1.0e6);

            sequence.updateMatchedPairs();

            int ch10Count = 0, allCount = 0;

            for (auto m : sequence)
            {
                auto msg = m->message;

                if (msg.isNoteOn())
                {
                    allCount++;

                    if (msg.getChannel() == 10)
                        ch10Count++;
                }
            }

            bool isDrums = forceMidiToDrums || (float (ch10Count) / allCount > 90.0f);
            auto track = isDrums ? drumTrack : midiTrack;

            if (auto mc = track->insertMIDIClip ({ TimePosition(), TimePosition::fromSeconds (1.0) }, nullptr))
            {
                if (track->pluginList.size() < 3)
                {
                    if (isDrums && midiDrumPreviewPlugin.isValid())
                        track->pluginList.insertPlugin (midiDrumPreviewPlugin, 0);
                    if (! isDrums && midiPreviewPlugin.isValid())
                        track->pluginList.insertPlugin (midiPreviewPlugin, 0);
                }

                // Find bpm of source midi file
                double srcBpm = 120.0;
                juce::MidiMessageSequence tempos;
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
                    auto& targetTempo = editToMatch->tempoSequence.getTempoAt (TimePosition::fromSeconds (0.01));
                    auto firstTempo = edit->tempoSequence.getTempo (0);
                    firstTempo->setBpm (targetTempo.getBpm());

                    if (couldMatchTempo != nullptr)
                        *couldMatchTempo = true;
                }

                auto dstBpm = edit->tempoSequence.getTempo (0)->getBpm();

                length *= srcBpm / dstBpm;

                if (length < 0.001)
                    length = 2;

                const TimeRange timeRange (TimePosition(), TimeDuration::fromSeconds (length));
                mc->setPosition ({ timeRange, TimeDuration() });
                edit->getTransport().setLoopRange (timeRange);
            }
        }
    }
    else
    {
        const AudioFile af (engine, file);
        auto length = af.getLength();

        if (auto wc = dynamic_cast<WaveAudioClip*> (audioTrack->insertNewClip (TrackItem::Type::wave, { TimePosition(), TimePosition::fromSeconds (1.0) }, nullptr)))
        {
            wc->setUsesProxy (false);
            wc->getSourceFileReference().setToDirectFileReference (file, false);

            if (editToMatch != nullptr)
            {
                if (tryToMatchTempo || tryToMatchPitch)
                {
                   #if ! TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
                    wc->setUsesTimestretchedPreview (true);
                   #endif
                    wc->setLoopInfo (af.getInfo().loopInfo);
                    wc->setTimeStretchMode (TimeStretcher::defaultMode);
                }

                auto& targetTempo = editToMatch->tempoSequence.getTempoAt (TimePosition::fromSeconds (0.01));
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

            const TimeRange timeRange (TimePosition(), TimeDuration::fromSeconds (length));
            wc->setPosition ({ timeRange, TimeDuration() });
            edit->getTransport().setLoopRange (timeRange);
        }
    }

    return edit;
}

std::unique_ptr<Edit> Edit::createEditForPreviewingClip (Clip& clip)
{
    CRASH_TRACER
    auto edit = std::make_unique<Edit> (clip.edit.engine, createEmptyEdit (clip.edit.engine), forEditing, nullptr, 1);
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
    auto edit = std::make_unique<Edit> (engine, createEmptyEdit (engine), forEditing, nullptr, 1);

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

}} // namespace tracktion { inline namespace engine
