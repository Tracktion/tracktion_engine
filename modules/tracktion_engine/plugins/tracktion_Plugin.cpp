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

PluginRenderContext::PluginRenderContext (juce::AudioBuffer<float>* buffer,
                                          const juce::AudioChannelSet& bufferChannels,
                                          int bufferStart, int bufferSize,
                                          MidiMessageArray* midiBuffer, double midiOffset,
                                          TimeRange editTimeRange, bool playing, bool scrubbing, bool rendering,
                                          bool shouldAllowBypassedProcessing) noexcept
    : destBuffer (buffer), destBufferChannels (bufferChannels),
      bufferStartSample (bufferStart), bufferNumSamples (bufferSize),
      bufferForMidiMessages (midiBuffer), midiBufferOffset (midiOffset),
      editTime (editTimeRange),
      isPlaying (playing), isScrubbing (scrubbing), isRendering (rendering),
      allowBypassedProcessing (shouldAllowBypassedProcessing)
{}

//==============================================================================
Plugin::Wire::Wire (const juce::ValueTree& v, juce::UndoManager* um)  : state (v)
{
    sourceChannelIndex.referTo (state, IDs::srcChan, um);
    destChannelIndex.referTo (state, IDs::dstChan, um);
}

struct Plugin::WireList : public ValueTreeObjectList<Plugin::Wire, juce::CriticalSection>,
                          private juce::AsyncUpdater
{
    WireList (Plugin& p, const juce::ValueTree& parentTree)
       : ValueTreeObjectList<Wire, juce::CriticalSection> (parentTree), plugin (p)
    {
        rebuildObjects();
    }

    ~WireList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override { return v.hasType (IDs::SIDECHAINCONNECTION); }
    Wire* createNewObject (const juce::ValueTree& v) override     { return new Wire (v, plugin.getUndoManager()); }
    void deleteObject (Wire* w) override                          { delete w; }

    void newObjectAdded (Wire*) override                    { triggerAsyncUpdate(); }
    void objectRemoved (Wire*) override                     { triggerAsyncUpdate(); }
    void objectOrderChanged() override                      {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override                       { plugin.changed(); }

    Plugin& plugin;
};


//==============================================================================
Plugin::WindowState::WindowState (Plugin& p) : PluginWindowState (p.edit), plugin (p)  {}

//==============================================================================
Plugin::Plugin (PluginCreationInfo info)
    : AutomatableEditItem (info.edit, info.state),
      MacroParameterElement (info.edit, info.state),
      engine (info.edit.engine),
      state (info.state)
{
    isClipEffect = state.getParent().hasType (IDs::EFFECT);
    windowState = std::make_unique<WindowState> (*this);

    jassert (state.isValid());

    auto um = getUndoManager();

    auto wires = state.getChildWithName (IDs::SIDECHAINCONNECTIONS);

    if (wires.isValid())
        sidechainWireList.reset (new WireList (*this, wires));

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

        juce::MessageManager::callAsync ([=, &e]() mutable
        {
            if (auto plugin = dynamic_cast<Plugin*> (ref.get()))
                if (auto na = e.getExternalControllerManager().getAutomap())
                    na->pluginChanged (plugin);
        });
    }
   #endif

    windowState->windowLocked = state [IDs::windowLocked];

    if (state.hasProperty (IDs::windowX))
        windowState->lastWindowBounds = juce::Rectangle<int> (state[IDs::windowX],
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
    juce::StringArray outs;
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

juce::ValueTree Plugin::getConnectionsTree()
{
    auto p = state.getChildWithName (IDs::SIDECHAINCONNECTIONS);

    if (p.isValid())
        return p;

    p = juce::ValueTree (IDs::SIDECHAINCONNECTIONS);
    state.addChild (p, -2, getUndoManager());
    return p;
}

void Plugin::makeConnection (int srcChannel, int dstChannel, juce::UndoManager* um)
{
    if (sidechainWireList != nullptr)
        for (auto w : sidechainWireList->objects)
            if (w->sourceChannelIndex == srcChannel && w->destChannelIndex == dstChannel)
                return;

    auto w = createValueTree (IDs::SIDECHAINCONNECTION,
                              IDs::srcChan, srcChannel,
                              IDs::dstChan, dstChannel);

    getConnectionsTree().addChild (w, -1, um);
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
        juce::StringArray ins, outs;
        getChannelNames (&ins, &outs);
        return ins.size() > 2 || ins.size() > outs.size();
    }

    return false;
}

juce::StringArray Plugin::getSidechainSourceNames (bool allowNone)
{
    juce::StringArray srcNames;

    if (allowNone)
        srcNames.add (TRANS("<none>"));

    int idx = 0;

    for (auto at : getAudioTracks (edit))
    {
        idx++;

        if (at != getOwnerTrack())
            srcNames.add (juce::String::formatted ("%d. ", idx) + at->getName());
    }

    return srcNames;
}

void Plugin::setSidechainSourceByName (const juce::String& name)
{
    bool found = false;
    int idx = 0;

    for (AudioTrack* at : getAudioTracks (edit))
    {
        if (juce::String::formatted ("%d. ", ++idx) + at->getName() == name)
        {
            sidechainSourceID = at->itemID;

            if (getNumWires() == 0)
                guessSidechainRouting();

            found = true;
            break;
        }
    }

    if (!found)
    {
        sidechainSourceID.resetToDefault();
    }
}

void Plugin::guessSidechainRouting()
{
    juce::StringArray ins;
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

juce::String Plugin::getSidechainSourceName()
{
    if (sidechainSourceID->isValid())
        if (auto t = findTrackForID (edit, sidechainSourceID))
            return t->getName();

    return {};
}

void Plugin::getChannelNames (juce::StringArray* ins, juce::StringArray* outs)
{
    getLeftRightChannelNames (ins, outs);
}

juce::StringArray Plugin::getInputChannelNames()
{
    juce::StringArray ins;
    getChannelNames (&ins, nullptr);

    return ins;
}

juce::UndoManager* Plugin::getUndoManager() const noexcept
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
    return isClipEffect;
}

void Plugin::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& i)
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

void Plugin::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (c.getType() == IDs::SIDECHAINCONNECTIONS)
        sidechainWireList.reset (new WireList (*this, c));

    valueTreeChanged();
}

void Plugin::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (c.getType() == IDs::SIDECHAINCONNECTIONS)
        sidechainWireList = nullptr;

    valueTreeChanged();
}

void Plugin::valueTreeParentChanged (juce::ValueTree& v)
{
    isClipEffect = state.getParent().hasType (IDs::EFFECT);
    
    if (v.hasType (IDs::PLUGIN))
        hideWindowForShutdown();
}

//==============================================================================
void Plugin::changed()
{
    Selectable::changed();
    jassert (Selectable::isSelectableValid (&edit));
    edit.updateMirroredPlugin (*this);
}

//==============================================================================
void Plugin::setEnabled (bool b)
{
    enabled = (b || ! canBeDisabled());
    
    if (! enabled)
        cpuUsageMs = 0.0;
}

void Plugin::setFrozen (bool shouldBeFrozen)
{
    frozen = shouldBeFrozen;

    if (frozen)
        cpuUsageMs = 0.0;
}

juce::String Plugin::getTooltip()
{
    return getName() + "$genericfilter";
}

void Plugin::reset()
{
}

//==============================================================================
void Plugin::baseClassInitialise (const PluginInitialisationInfo& info)
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
        const juce::ScopedLock sl (dm.deviceManager.getAudioCallbackLock());

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
    macroParameterList.hideMacroParametersFromTracks();

    for (auto t : getAllTracks (edit))
        t->hideAutomatableParametersForSource (itemID);
    
    hideWindowForShutdown();
    deselect();
    removeFromParent();
}

Track* Plugin::getOwnerTrack() const
{
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
AutomatableParameter* Plugin::addParam (const juce::String& paramID, const juce::String& name,
                                        juce::NormalisableRange<float> valueRange)
{
    auto p = new AutomatableParameter (paramID, name, *this, valueRange);
    addAutomatableParameter (*p);
    return p;
}

AutomatableParameter* Plugin::addParam (const juce::String& paramID, const juce::String& name,
                                        juce::NormalisableRange<float> valueRange,
                                        std::function<juce::String(float)> valueToStringFn,
                                        std::function<float(const juce::String&)> stringToValueFn)
{
    auto p = addParam (paramID, name, valueRange);
    p->valueToStringFunction = valueToStringFn;
    p->stringToValueFunction = stringToValueFn;
    return p;
}

AutomatableParameter::Ptr Plugin::getQuickControlParameter() const
{
    juce::String currentID = quickParamName;

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
                        // First check macros
                        for (auto param : rf->type->macroParameterList.getAutomatableParameters())
                        {
                            if (param->paramID == currentID)
                            {
                                quickControlParameter = param;
                                break;
                            }
                        }

                        // Then plugins
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

void Plugin::applyToBufferWithAutomation (const PluginRenderContext& pc)
{
    SCOPED_REALTIME_CHECK

    const ScopedCpuMeter cpuMeter (cpuUsageMs, 0.2);

    auto& arm = edit.getAutomationRecordManager();
    jassert (initialiseCount > 0);

    updateLastPlaybackTime();

    if (isAutomationNeeded()
        && (arm.isReadingAutomation() || isClipEffect.load()))
    {
        if (pc.isScrubbing || ! pc.isPlaying)
        {
            SCOPED_REALTIME_CHECK
            auto& tc = edit.getTransport();
            updateParameterStreams (tc.isPlayContextActive() && ! pc.isRendering
                                        ? tc.getPosition()
                                        : pc.editTime.getStart());
            applyToBuffer (pc);
        }
        else
        {
            SCOPED_REALTIME_CHECK
            updateParameterStreams (pc.editTime.getStart());
            applyToBuffer (pc);
        }
    }
    else
    {
        SCOPED_REALTIME_CHECK
        applyToBuffer (pc);
    }
}

//==============================================================================
bool Plugin::hasNameForMidiNoteNumber (int, int midiChannel, juce::String&)
{
    jassert (midiChannel >= 1 && midiChannel <= 16);
    juce::ignoreUnused (midiChannel);
    return false;
}

bool Plugin::hasNameForMidiProgram (int, int, juce::String&)
{
    return false;
}

bool Plugin::hasNameForMidiBank (int, juce::String&)
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

    juce::SortedSet<SelectedPluginIndex> pluginIndex;
    juce::ValueTree lastList;

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

void Plugin::sortPlugins (Plugin::Array& plugins)
{
    if (auto first = plugins.getFirst())
    {
        PluginList list (first->edit);
        list.initialise (first->state.getParent());

        std::sort (plugins.begin(), plugins.end(),
                   [&list] (Plugin* a, Plugin* b)
                   {
                       jassert (a != nullptr && b != nullptr);
                       return list.indexOf (a) < list.indexOf (b);
                   });
    }
}

void Plugin::sortPlugins (std::vector<Plugin*>& plugins)
{
    if (plugins.size() == 0 || plugins[0] == nullptr)
        return;
    
    auto first = plugins[0];

    PluginList list (first->edit);
    list.initialise (first->state.getParent());

    std::sort (plugins.begin(), plugins.end(),
               [&list] (Plugin* a, Plugin* b)
               {
                   jassert (a != nullptr && b != nullptr);
                   return list.indexOf (a) < list.indexOf (b);
               });
}

void Plugin::getLeftRightChannelNames (juce::StringArray* chans)
{
    if (chans != nullptr)
    {
        chans->add (TRANS("Left"));
        chans->add (TRANS("Right"));
    }
}

void Plugin::getLeftRightChannelNames (juce::StringArray* ins,
                                       juce::StringArray* outs)
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

}} // namespace tracktion { inline namespace engine
