/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


Plugin::Wire::Wire (const ValueTree& v, UndoManager* um)  : state (v)
{
    sourceChannelIndex.referTo (state, IDs::srcChan, um);
    destChannelIndex.referTo (state, IDs::dstChan, um);
}

struct Plugin::WireList : public ValueTreeObjectList<Plugin::Wire, CriticalSection>,
                          private AsyncUpdater
{
    WireList (Plugin& p, const ValueTree& parent)
       : ValueTreeObjectList<Wire, CriticalSection> (parent), plugin (p)
    {
        rebuildObjects();
    }

    ~WireList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override { return v.hasType (IDs::SIDECHAINCONNECTION); }
    Wire* createNewObject (const ValueTree& v) override     { return new Wire (v, plugin.getUndoManager()); }
    void deleteObject (Wire* w) override                    { delete w; }

    void newObjectAdded (Wire*) override                    { triggerAsyncUpdate(); }
    void objectRemoved (Wire*) override                     { triggerAsyncUpdate(); }
    void objectOrderChanged() override                      {}
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override                       { plugin.changed(); }

    Plugin& plugin;
};

//==============================================================================
class PluginAudioNode   : public AudioNode
{
public:
    PluginAudioNode (const Plugin::Ptr& p, AudioNode* in, bool denormalisationNoise)
        : plugin (p), input (in), applyAntiDenormalisationNoise (denormalisationNoise)
    {
        jassert (input != nullptr);
        jassert (plugin != nullptr);
    }

    ~PluginAudioNode()
    {
        input = nullptr;
        releaseAudioNodeResources();
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        if (input != nullptr)
        {
            input->getAudioNodeProperties (info);
        }
        else
        {
            info.hasAudio = false;
            info.hasMidi = false;
            info.numberOfChannels = 0;
        }

        info.numberOfChannels = jmax (info.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (info.numberOfChannels));
        info.hasAudio = info.hasAudio || plugin->producesAudioWhenNoAudioInput();
        info.hasMidi  = info.hasMidi  || plugin->takesMidiInput();
        hasAudioInput = info.hasAudio;
        hasMidiInput  = info.hasMidi;
    }

    void visitNodes (const VisitorFn& v) override
    {
        v (*this);

        if (input != nullptr)
            input->visitNodes (v);
    }

    Plugin::Ptr getPlugin() const override    { return plugin; }

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override
    {
        const bool pluginWantsMidi = plugin->takesMidiInput() || plugin->isSynth();

        return (input != nullptr && input->purgeSubNodes (keepAudio, keepMidi || pluginWantsMidi))
                || pluginWantsMidi
                || ! plugin->noTail();
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        if (! hasInitialised)
        {
            hasInitialised = true;
            plugin->baseClassInitialise (info);
            latencySeconds = plugin->getLatencySeconds();

            if (input != nullptr)
                input->prepareAudioNodeToPlay (info);
        }
    }

    bool isReadyToRender() override
    {
        if (auto rf = dynamic_cast<RackInstance*> (plugin.get()))
            if (rf->type != nullptr)
                return rf->type->isReadyToRender();

        if (input != nullptr)
            return input->isReadyToRender();

        return true;
    }

    double getLatencySeconds() const noexcept
    {
        return latencySeconds;
    }

    void releaseAudioNodeResources() override
    {
        if (hasInitialised)
        {
            hasInitialised = false;

            if (input != nullptr)
                input->releaseAudioNodeResources();

            plugin->baseClassDeinitialise();
        }
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (plugin->isEnabled() && (rc.isRendering || (! plugin->isFrozen())))
        {
            callRenderOver (rc);
        }
        else
        {
            if (rc.didPlayheadJump())
                plugin->reset();

            if (input != nullptr)
                input->renderAdding (rc);
        }
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        if (rc.didPlayheadJump())
            plugin->reset();

        if (plugin->isEnabled() && (rc.isRendering || (! plugin->isFrozen())))
        {
            if (latencySeconds > 0)
            {
                AudioRenderContext rc2 (rc);
                rc2.streamTime = rc2.streamTime + latencySeconds;

                input->renderOver (rc2);
                renderPlugin (rc2);
            }
            else
            {
                input->renderOver (rc);
                renderPlugin (rc);
            }
        }
        else
        {
            plugin->cpuUsageMs = 0.0;
            input->renderOver (rc);
        }
    }

    virtual void renderPlugin (const AudioRenderContext& rc)
    {
        SCOPED_REALTIME_CHECK

        if (applyAntiDenormalisationNoise)
            rc.addAntiDenormalisationNoise();

        if (! rc.isContiguousWithPreviousBlock())
            plugin->updateParameterStreams (rc.getEditTime().editRange1.getStart());

        plugin->applyToBufferWithAutomation (rc);
    }

    void prepareForNextBlock (const AudioRenderContext& rc) override
    {
        plugin->prepareForNextBlock (rc);
        input->prepareForNextBlock (rc);
    }

protected:
    Plugin::Ptr plugin;
    ScopedPointer<AudioNode> input;

    bool hasAudioInput = false, hasMidiInput = false, applyAntiDenormalisationNoise = false, hasInitialised = false;
    double latencySeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioNode)
};

//==============================================================================
class FineGrainPluginAudioNode  : public PluginAudioNode
{
public:
    FineGrainPluginAudioNode (const Plugin::Ptr& p, AudioNode* in, bool denormalisationNoise)
        : PluginAudioNode (p, in, denormalisationNoise)
    {
    }

    static bool needsFineGrainAutomation (Plugin& p)
    {
        if (! p.isAutomationNeeded())
            return false;

        if (auto pl = p.getOwnerList())
            return ! pl->needsConstantBufferSize();

        return true;
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        PluginAudioNode::prepareAudioNodeToPlay (info);

        blockSizeToUse = jmax (128, 128 * roundToInt (info.sampleRate / 44100.0));
    }

    void renderPlugin (const AudioRenderContext& rc) override
    {
        if (rc.bufferNumSamples == blockSizeToUse)
            return PluginAudioNode::renderPlugin (rc);

        SCOPED_REALTIME_CHECK

        if (applyAntiDenormalisationNoise)
            rc.addAntiDenormalisationNoise();

        const double editTime = rc.getEditTime().editRange1.getStart();
        const double streamTimePerSample = rc.streamTime.getLength() / rc.bufferNumSamples;

        if (! rc.isContiguousWithPreviousBlock())
            plugin->updateParameterStreams (editTime);

        int numSamplesLeft = rc.bufferNumSamples;
        int numSamplesDone = 0;
        int midiIndex = 0;
        double timeOffset = 0;

        midiInputScratch.clear();
        midiOutputScratch.clear();

        AudioRenderContext rc2 (rc);
        rc2.bufferForMidiMessages = rc.bufferForMidiMessages != nullptr ? &midiInputScratch : nullptr;
        rc2.midiBufferOffset = 0;

        if (rc.bufferForMidiMessages != nullptr)
            midiInputScratch.isAllNotesOff = rc.bufferForMidiMessages->isAllNotesOff;

        while (numSamplesLeft > 0)
        {
            const int numThisTime = jmin (blockSizeToUse, numSamplesLeft);
            auto timeForBlock = streamTimePerSample * numThisTime;
            auto endTimeOfBlock = editTime + timeOffset + timeForBlock;

            // Create a new MIDI buffer with appropriate timings
            if (auto* sourceMidi = rc.bufferForMidiMessages)
            {
                while (midiIndex < sourceMidi->size())
                {
                    auto& m = (*sourceMidi)[midiIndex++];
                    auto timeStamp = m.getTimeStamp();

                    if (timeStamp < rc.midiBufferOffset)
                        continue;

                    midiInputScratch.add (m, timeStamp - timeOffset);

                    if (m.getTimeStamp() > endTimeOfBlock)
                        break;
                }
            }

            juce::AudioBuffer<float> asb;

            if (rc.destBuffer != nullptr)
            {
                asb.setDataToReferTo (rc.destBuffer->getArrayOfWritePointers(), rc.destBuffer->getNumChannels(),
                                      rc.bufferStartSample + numSamplesDone, numThisTime);
                rc2.destBuffer = &asb;
            }
            else
            {
                rc2.destBuffer = nullptr;
            }

            rc2.bufferStartSample = 0;
            rc2.bufferNumSamples = numThisTime;
            rc2.streamTime = EditTimeRange (Range<double>::withStartAndLength (rc.streamTime.getStart() + timeOffset, timeForBlock));

            plugin->applyToBufferWithAutomation (rc2);

            midiOutputScratch.mergeFromAndClearWithOffset (midiInputScratch, timeOffset);
            midiInputScratch.clear();

            timeOffset += timeForBlock;
            numSamplesLeft -= numThisTime;
            numSamplesDone += numThisTime;
        }

        if (rc.bufferForMidiMessages != nullptr)
            rc.bufferForMidiMessages->swapWith (midiOutputScratch);
    }

private:
    MidiMessageArray midiInputScratch, midiOutputScratch;
    int blockSizeToUse = 128;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FineGrainPluginAudioNode)
};

//==============================================================================
Plugin::WindowState::WindowState (Plugin& p) : PluginWindowState (p.edit.engine), plugin (p)  {}

//==============================================================================
Plugin::Plugin (PluginCreationInfo info)
    : AutomatableEditItem (info.edit, info.state),
      MacroParameterElement (info.edit, info.state),
      engine (info.edit.engine),
      state (info.state)
{
    windowState = std::make_unique<WindowState> (*this);

    jassert (state.isValid());

    auto um = getUndoManager();

    auto wires = state.getChildWithName (IDs::SIDECHAINCONNECTIONS);

    if (wires.isValid())
        sidechainWireList = new WireList (*this, wires);

    enabled.referTo (state, IDs::enabled, um, true);

    if (enabled.isUsingDefault())
        enabled = enabled.getDefault();

    processing.referTo (state, IDs::process, um, true);
    frozen.referTo (state, IDs::frozen, um);
    quickParamName.referTo (state, IDs::quickParamName, um);
    masterPluginID.referTo (state, IDs::masterPluginID, um);
    sidechainSourceID.referTo (state, IDs::sidechainSourceID, um);

    state.addListener (this);

   #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
    if (! edit.isLoading())
    {
        Plugin::WeakRef ref (this);
        auto& e = engine;

        MessageManager::callAsync ([=, &e]() mutable
        {
            if (ref != nullptr)
                if (auto na = e.getExternalControllerManager().getAutomap())
                    na->pluginChanged (ref);
        });
    }
   #endif

    windowState->windowLocked = state [IDs::windowLocked];

    if (state.hasProperty (IDs::windowX))
        windowState->lastWindowBounds = Rectangle<int> (state[IDs::windowX],
                                                        state[IDs::windowY], 1, 1);
}

Plugin::~Plugin()
{
    CRASH_TRACER
    windowState->hideWindowForShutdown();

   #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
    if (auto na = engine.getExternalControllerManager().getAutomap())
        na->removePlugin (this);
   #endif
}

void Plugin::selectableAboutToBeDeleted()
{
    // Remove ths listener in case the tree gets modified during descruction
    state.removeListener (this);

    quickControlParameter = nullptr;
    deleteAutomatableParameters();
}

int Plugin::getNumOutputChannelsGivenInputs (int)
{
    StringArray outs;
    getChannelNames (nullptr, &outs);
    return outs.size();
}

int Plugin::getNumWires() const
{
    if (sidechainWireList != nullptr)
        return sidechainWireList->objects.size();

    return 0;
}

Plugin::Wire* Plugin::getWire (int index) const
{
    if (sidechainWireList != nullptr)
        return sidechainWireList->objects[index];

    return {};
}

ValueTree Plugin::getConnectionsTree()
{
    auto p = state.getChildWithName (IDs::SIDECHAINCONNECTIONS);

    if (p.isValid())
        return p;

    p = ValueTree (IDs::SIDECHAINCONNECTIONS);
    state.addChild (p, -2, getUndoManager());
    return p;
}

void Plugin::makeConnection (int srcChannel, int dstChannel, UndoManager* um)
{
    if (sidechainWireList != nullptr)
        for (auto w : sidechainWireList->objects)
            if (w->sourceChannelIndex == srcChannel && w->destChannelIndex == dstChannel)
                return;

    ValueTree w (IDs::SIDECHAINCONNECTION);
    w.setProperty (IDs::srcChan, srcChannel, nullptr);
    w.setProperty (IDs::dstChan, dstChannel, nullptr);

    auto p = getConnectionsTree();

    p.addChild (w, -1, um);
}

void Plugin::breakConnection (int srcChannel, int dstChannel)
{
    auto p = getConnectionsTree();

    if (sidechainWireList != nullptr)
    {
        for (auto w : sidechainWireList->objects)
        {
            if (w->sourceChannelIndex == srcChannel && w->destChannelIndex == dstChannel)
            {
                p.removeChild (w->state, getUndoManager());
                break;
            }
        }
    }

    if (p.getNumChildren() == 0)
        state.removeChild (p, getUndoManager());
}

bool Plugin::canSidechain()
{
    if (! isInRack())
    {
        StringArray ins, outs;
        getChannelNames (&ins, &outs);
        return ins.size() > 2 || ins.size() > outs.size();
    }

    return false;
}

StringArray Plugin::getSidechainSourceNames (bool allowNone)
{
    StringArray srcNames;

    if (allowNone)
        srcNames.add (TRANS("<none>"));

    int idx = 0;

    for (auto at : getAudioTracks (edit))
    {
        idx++;

        if (at != getOwnerTrack())
            srcNames.add (String::formatted ("%d. ", idx) + at->getName());
    }

    return srcNames;
}

void Plugin::setSidechainSourceByName (const String& name)
{
    bool found = false;
    int idx = 0;

    for (AudioTrack* at : getAudioTracks (edit))
    {
        if (String::formatted ("%d. ", ++idx) + at->getName() == name)
        {
            sidechainSourceID = at->itemID;

            if (getNumWires() == 0)
                guessSidechainRouting();

            found = true;
            break;
        }
    }

    if (! found)
        sidechainSourceID.resetToDefault();
}

void Plugin::guessSidechainRouting()
{
    StringArray ins;
    getChannelNames (&ins, nullptr);

    auto* um = getUndoManager();

    if (ins.size() == 1)
    {
        makeConnection (0, 0, um);
        makeConnection (1, 0, um);
    }
    else if (ins.size() == 2)
    {
        makeConnection (0, 0, um);
        makeConnection (1, 0, um);

        makeConnection (2, 1, um);
        makeConnection (3, 1, um);
    }
    else if (ins.size() == 3)
    {
        makeConnection (0, 0, um);
        makeConnection (1, 1, um);

        makeConnection (2, 2, um);
        makeConnection (3, 2, um);
    }
    else
    {
        makeConnection (0, 0, um);
        makeConnection (1, 1, um);

        makeConnection (2, 2, um);
        makeConnection (3, 3, um);
    }
}

String Plugin::getSidechainSourceName()
{
    if (sidechainSourceID->isValid())
        if (auto t = findTrackForID (edit, sidechainSourceID))
            return t->getName();

    return {};
}

void Plugin::getChannelNames (StringArray* ins, StringArray* outs)
{
    getLeftRightChannelNames (ins, outs);
}

StringArray Plugin::getInputChannelNames()
{
    StringArray ins;
    getChannelNames (&ins, nullptr);

    return ins;
}

UndoManager* Plugin::getUndoManager() const noexcept
{
    return &edit.getUndoManager();
}

void Plugin::playStartedOrStopped()
{
    resetRecordingStatus();
}

void Plugin::initialiseFully()
{
    restoreChangedParametersFromState();
}

void Plugin::removeFromParent()
{
    macroParameterList.hideMacroParametersFromTracks();

    for (auto t : getAllTracks (edit))
        t->hideAutomatableParametersForSource (itemID);

    auto* um = getUndoManager();

    auto parent = state.getParent();

    if (parent.hasType (IDs::PLUGININSTANCE)) // If it's in a rack..
    {
        auto rack = parent.getParent();
        rack.removeChild (parent, um);
        RackType::removeBrokenConnections (rack, um);
    }

    parent.removeChild (state, um);
}

bool Plugin::isInRack() const
{
    return state.getParent().hasType (IDs::PLUGININSTANCE);
}

RackType::Ptr Plugin::getOwnerRackType() const
{
    if (isInRack())
        return RackType::findRackTypeContaining (*this);

    return {};
}

bool Plugin::isClipEffectPlugin() const
{
    return state.getParent().hasType (IDs::EFFECT);
}

void Plugin::valueTreePropertyChanged (ValueTree&, const Identifier& i)
{
    if (i == IDs::process)
        processingChanged();
    else
        valueTreeChanged();
}

void Plugin::valueTreeChanged()
{
    changed();
}

void Plugin::valueTreeChildAdded (ValueTree&, ValueTree& c)
{
    if (c.getType() == IDs::SIDECHAINCONNECTIONS)
        sidechainWireList = new WireList (*this, c);

    valueTreeChanged();
}

void Plugin::valueTreeChildRemoved (ValueTree&, ValueTree& c, int)
{
    if (c.getType() == IDs::SIDECHAINCONNECTIONS)
        sidechainWireList = nullptr;

    valueTreeChanged();
}

//==============================================================================
AudioNode* Plugin::createAudioNode (AudioNode* input, bool applyAntiDenormalisationNoise)
{
    jassert (input != nullptr);

    if (isDisabled())
        return input;

    const bool usesSidechain = ! isMissing() && sidechainSourceID->isValid();
    AudioNode* node = usesSidechain ? new SidechainReceiveAudioNode (this, input) : input;

    if (FineGrainPluginAudioNode::needsFineGrainAutomation (*this))
        node = new FineGrainPluginAudioNode (*this, node, applyAntiDenormalisationNoise);
    else
        node = new PluginAudioNode (*this, node, applyAntiDenormalisationNoise);

    if (usesSidechain)
        node = new SidechainReceiveWrapperAudioNode (node);

    return node;
}

void Plugin::changed()
{
    Selectable::changed();

    if (Selectable::isSelectableValid (&edit))
        edit.updateMirroredPlugin (*this);
}

//==============================================================================
void Plugin::setEnabled (bool b)
{
    enabled = (b || ! canBeDisabled());
}

String Plugin::getTooltip()
{
    return getName() + "$genericfilter";
}

void Plugin::reset()
{
}

//==============================================================================
void Plugin::baseClassInitialise (const PlaybackInitialisationInfo& info)
{
    const bool sampleRateOrBlockSizeChanged = (sampleRate != info.sampleRate) || (blockSizeSamples != info.blockSizeSamples);
    bool isUpdatingWithoutStopping = false;
    sampleRate = info.sampleRate;
    blockSizeSamples = info.blockSizeSamples;
    cpuUsageMs = 0.0;

    {
        const double msPerBlock = (sampleRate > 0.0) ? (1000.0 * (blockSizeSamples / sampleRate)) : 0.0;
        timeToCpuScale = (msPerBlock > 0.0) ? (1.0 / msPerBlock) : 0.0;
    }

    {
        auto& dm = engine.getDeviceManager();
        const ScopedLock sl (dm.deviceManager.getAudioCallbackLock());

        if (initialiseCount++ == 0 || sampleRateOrBlockSizeChanged)
        {
            CRASH_TRACER
            initialise (info);
        }
        else
        {
            CRASH_TRACER
            initialiseWithoutStopping (info);
            isUpdatingWithoutStopping = true;
        }
    }

    {
        CRASH_TRACER
        resetRecordingStatus();
    }

    if (! isUpdatingWithoutStopping)
    {
        CRASH_TRACER
        setAutomatableParamPosition (info.startTime);
    }

    if (sampleRateOrBlockSizeChanged)
    {
        CRASH_TRACER
        reset();
    }
}

void Plugin::baseClassDeinitialise()
{
    jassert (initialiseCount > 0);

    if (initialiseCount > 0 && --initialiseCount == 0)
    {
        CRASH_TRACER
        deinitialise();
        resetRecordingStatus();

        timeToCpuScale = 0.0;
        cpuUsageMs = 0.0;
    }
}

//==============================================================================
void Plugin::deleteFromParent()
{
    deselect();
    removeFromParent();
}

Track* Plugin::getOwnerTrack() const
{
    jassert (getReferenceCount() > 0);
    return getTrackContainingPlugin (edit, this);
}

Clip* Plugin::getOwnerClip() const
{
    auto parent = state.getParent();

    if (Clip::isClipState (parent))
        return findClipForID (edit, EditItemID::fromID (parent));

    return {};
}

static PluginList* getListContaining (const Plugin& p)
{
    if (auto c = p.getOwnerClip())
        return c->getPluginList();

    if (auto t = p.getOwnerTrack())
        return &t->pluginList;

    return &p.edit.getMasterPluginList();
}

Plugin::Ptr Plugin::findPluginThatFeedsIntoThis() const
{
    if (auto l = getListContaining (*this))
        return l->getPlugins()[l->indexOf (this) - 1];

    return {};
}

Plugin::Ptr Plugin::findPluginThatThisFeedsInto() const
{
    if (auto l = getListContaining (*this))
        return l->getPlugins()[l->indexOf (this) + 1];

    return {};
}

PluginList* Plugin::getOwnerList() const
{
    return getListContaining (*this);
}

//==============================================================================
AutomatableParameter* Plugin::addParam (const String& paramID, const String& name, Range<float> valueRange)
{
    auto p = new AutomatableParameter (paramID, name, *this, valueRange);
    addAutomatableParameter (*p);
    return p;
}

AutomatableParameter* Plugin::addParam (const String& paramID, const String& name, Range<float> valueRange,
                                        std::function<String(float)> valueToStringFn,
                                        std::function<float(const String&)> stringToValueFn)
{
    auto p = addParam (paramID, name, valueRange);
    p->valueToStringFunction = valueToStringFn;
    p->stringToValueFunction = stringToValueFn;
    return p;
}

AutomatableParameter::Ptr Plugin::getQuickControlParameter() const
{
    String currentID (quickParamName);

    if (currentID.isEmpty())
    {
        quickControlParameter = nullptr;
    }
    else
    {
        if (quickControlParameter == nullptr || currentID != quickControlParameter->paramID)
        {
            quickControlParameter = getAutomatableParameterByID (currentID);

            if (quickControlParameter == nullptr)
            {
                // if this is a rack, dig around trying to get the name
                if (auto rf = dynamic_cast<const RackInstance*> (this))
                {
                    if (rf->type != nullptr)
                    {
                        for (auto p : rf->type->getPlugins())
                        {
                            for (int j = 0; j < p->getNumAutomatableParameters(); j++)
                            {
                                auto param = p->getAutomatableParameter(j);

                                if (param->paramID == currentID)
                                {
                                    quickControlParameter = param;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return quickControlParameter;
}

void Plugin::setQuickControlParameter (AutomatableParameter* param)
{
    if (param == nullptr)
        state.removeProperty (IDs::quickParamName, getUndoManager());
    else
        quickParamName = param->paramID;
}

void Plugin::applyToBufferWithAutomation (const AudioRenderContext& fc)
{
    SCOPED_REALTIME_CHECK

    const ScopedCpuMeter cpuMeter (cpuUsageMs, 0.2);

    auto& arm = edit.getAutomationRecordManager();
    jassert (initialiseCount > 0);

    updateLastPlaybackTime();

    if (isAutomationNeeded() && arm.isReadingAutomation())
    {
        if (fc.playhead.isUserDragging() || ! fc.playhead.isPlaying())
        {
            SCOPED_REALTIME_CHECK
            auto& tc = edit.getTransport();
            updateParameterStreams (tc.isPlayContextActive() && ! fc.isRendering
                                        ? tc.getCurrentPosition()
                                        : fc.getEditTime().editRange1.getStart());
            applyToBuffer (fc);
        }
        else
        {
            SCOPED_REALTIME_CHECK
            updateParameterStreams (fc.getEditTime().editRange1.getEnd());
            applyToBuffer (fc);
        }
    }
    else
    {
        SCOPED_REALTIME_CHECK
        applyToBuffer (fc);
    }
}

//==============================================================================
bool Plugin::hasNameForMidiNoteNumber (int, int midiChannel, String&)
{
    jassert (midiChannel >= 1 && midiChannel <= 16);
    juce::ignoreUnused (midiChannel);
    return false;
}

bool Plugin::hasNameForMidiProgram (int, int, String&)
{
    return false;
}

bool Plugin::hasNameForMidiBank (int, String&)
{
    return false;
}

//==============================================================================
juce::Array<Exportable::ReferencedItem> Plugin::getReferencedItems()  { return {}; }
void Plugin::reassignReferencedItem (const ReferencedItem&, ProjectItemID, double) {}

//==============================================================================
static bool mirrorPluginIsRecursive (Plugin& p, int depth)
{
    if (depth > 20)
        return true;

    if (auto mirrored = p.getMirroredPlugin())
        return mirrorPluginIsRecursive (*mirrored, depth + 1);

    return false;
}

bool Plugin::setPluginToMirror (const Plugin::Ptr& newMaster)
{
    if (newMaster != nullptr)
    {
        if (getName() != newMaster->getName())
            return false;

        auto p1 = dynamic_cast<ExternalPlugin*> (this);
        auto p2 = dynamic_cast<ExternalPlugin*> (newMaster.get());

        if (p1 != nullptr || p2 != nullptr)
        {
            if (p1 == nullptr || p2 == nullptr)
                return false;

            if (! p1->desc.isDuplicateOf (p2->desc))
                return false;
        }
    }

    auto newID = newMaster != nullptr ? newMaster->itemID : EditItemID();

    if (newID != masterPluginID)
    {
        auto oldID = masterPluginID.get();
        masterPluginID = newID;

        if (mirrorPluginIsRecursive (*this, 0))
        {
            masterPluginID = oldID;
            return false;
        }

        if (newMaster != nullptr)
            updateFromMirroredPluginIfNeeded (*newMaster);
    }

    return true;
}

Plugin::Ptr Plugin::getMirroredPlugin() const
{
    if (masterPluginID.get().isValid())
        return edit.getPluginCache().getPluginFor (masterPluginID.get());

    return {};
}

struct SelectedPluginIndex
{
    int index;
    Plugin* plugin;

    bool operator== (const SelectedPluginIndex& other) const     { return index == other.index; }
    bool operator<  (const SelectedPluginIndex& other) const     { return index < other.index; }
};

static Plugin::Array getRackablePlugins (SelectionManager& selectionManager)
{
    Plugin::Array result;

    SortedSet<SelectedPluginIndex> pluginIndex;
    ValueTree lastList;

    for (auto plugin : selectionManager.getItemsOfType<Plugin>())
    {
        if (plugin->canBeAddedToRack() && ! plugin->isInRack())
        {
            if (! lastList.isValid() || lastList == plugin->state.getParent())
            {
                lastList = plugin->state.getParent();

                PluginList list (plugin->edit);
                list.initialise (lastList);

                auto index = list.indexOf (plugin);

                if (index >= 0)
                {
                    SelectedPluginIndex sf = { index, plugin };
                    pluginIndex.add (sf);
                    continue;
                }
            }
        }

        break;
    }

    for (int i = 0; i < pluginIndex.size() - 1; ++i)
        if (pluginIndex[i].index != pluginIndex[i + 1].index - 1)
            return result;

    for (auto& i : pluginIndex)
        result.add (i.plugin);

    return result;
}

bool Plugin::areSelectedPluginsRackable (SelectionManager& selectionManager)
{
    return getRackablePlugins (selectionManager).size() > 0;
}

RackInstance* Plugin::wrapSelectedPluginsInRack (SelectionManager& selectionManager)
{
    auto plugins = getRackablePlugins (selectionManager);

    if (auto first = plugins.getFirst())
    {
        auto& ed = first->edit;
        ed.getTransport().stop (false, true);

        if (auto list = getListContaining (*first))
        {
            auto insertIndex = list->indexOf (first.get());

            if (auto newRackType = RackType::createTypeToWrapPlugins (plugins, ed))
                return dynamic_cast<RackInstance*> (list->insertPlugin (RackInstance::create (*newRackType), insertIndex).get());
        }
    }

    return {};
}

struct PluginSorter
{
    int compareElements (Plugin* first, Plugin* second)
    {
        jassert (first != nullptr && second != nullptr);

        PluginList list (first->edit);
        list.initialise (first->state.getParent());
        return list.indexOf (first) - list.indexOf (second);
    }
};

void Plugin::sortPlugins (Plugin::Array& plugins)
{
    PluginSorter sorter;
    plugins.sort (sorter);
}

void Plugin::getLeftRightChannelNames (StringArray* chans)
{
    if (chans != nullptr)
    {
        chans->add (TRANS("Left"));
        chans->add (TRANS("Right"));
    }
}

void Plugin::getLeftRightChannelNames (StringArray* ins, StringArray* outs)
{
    getLeftRightChannelNames (ins);
    getLeftRightChannelNames (outs);
}

void Plugin::showWindowExplicitly()
{
    windowState->showWindowExplicitly();
}

void Plugin::hideWindowForShutdown()
{
    windowState->hideWindowForShutdown();
}

void Plugin::processingChanged()
{
    if (! processing)
        windowState->hideWindowForShutdown();
}

void Plugin::flushPluginStateToValueTree()
{
    AutomatableEditItem::flushPluginStateToValueTree();

    if (! windowState->lastWindowBounds.isEmpty())
    {
        auto um = getUndoManager();

        state.setProperty (IDs::windowX, windowState->lastWindowBounds.getX(), um);
        state.setProperty (IDs::windowY, windowState->lastWindowBounds.getY(), um);
        state.setProperty (IDs::windowLocked, windowState->windowLocked, um);
    }
}
