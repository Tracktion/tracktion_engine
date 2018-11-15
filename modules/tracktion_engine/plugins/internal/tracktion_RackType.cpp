/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static constexpr int maxRackAudioChans = 64;

static String getDefaultInputName (int i)
{
    if (i == 0)  return TRANS("Left input");
    if (i == 1)  return TRANS("Right input");

    return TRANS("Input") + " " + String (i + 1);
}

static String getDefaultOutputName (int i)
{
    if (i == 0)  return TRANS("Left output");
    if (i == 1)  return TRANS("Right output");

    return TRANS("Output") + " " + String (i + 1);
}

//==============================================================================
RackConnection::RackConnection (const ValueTree& v, UndoManager* um)
    : state (v)
{
    sourceID.referTo (state, IDs::src, um);
    destID.referTo (state, IDs::dst, um);
    sourcePin.referTo (state, IDs::srcPin, um);
    destPin.referTo (state, IDs::dstPin, um);
}

//==============================================================================
struct RackType::PluginRenderingInfo
{
    Plugin::Ptr plugin;
    Modifier::Ptr modifier;
    juce::AudioBuffer<float> pluginOutput;
    MidiMessageArray pluginMidiOutput;
    int numOutputChans = 0, numInputChans = 0;
    bool initialised = false, hasBeenRendered = false;
    bool isConnectedToOutput = false;

    struct InputPluginInfo
    {
        PluginRenderingInfo* renderingInfo;
        int sourceOutputChan, destInputChan;
        bool isMidi;
    };

    OwnedArray<InputPluginInfo> inputPlugins;

    struct OutputConnectionInfo
    {
        bool isMidi = false;
        int sourceOutputChan = -1, destOutputChan = -1;
    };

    OwnedArray<OutputConnectionInfo> outputConnections;

    PluginRenderingInfo (Plugin::Ptr p)  : plugin (p)
    {
        jassert (plugin != nullptr);
        StringArray outs, ins;
        plugin->getChannelNames (&ins, &outs);
        numOutputChans = outs.size();
        numInputChans = ins.size();
    }

    PluginRenderingInfo (Modifier::Ptr mod)
        : modifier (std::move (mod))
    {
        jassert (modifier != nullptr);
        numInputChans = modifier->getAudioInputNames().size();
    }

    PluginRenderingInfo (RackType& owner)
    {
        numInputChans = numOutputChans = jmax (owner.getInputNames().size(),
                                               owner.getOutputNames().size());
    }

    void resetRenderingFlag()
    {
        hasBeenRendered = false;
    }

    void initialise (int bufferSize, RackType& owner,
                     const OwnedArray<PluginRenderingInfo>& renderContexts)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (! initialised)
        {
            initialised = true;

            pluginMidiOutput.clear();
            pluginOutput.setSize (jmax (numInputChans, numOutputChans), bufferSize);
            pluginOutput.clear();
            resetRenderingFlag();

            // rebuild a list of input plugins
            inputPlugins.clear();
            outputConnections.clear();
            isConnectedToOutput = false;

            EditItemID pluginID, modifierID;

            if (plugin != nullptr)
                pluginID = plugin->itemID;
            else if (modifier != nullptr)
                modifierID = modifier->itemID;

            // This logic is complicated:
            //   1. Iterate all the (audio/midi) connections in the Rack
            //   2. If this wrapper is a Modifier and a connection is found to this modifier
            //      create a InputPluginInfo
            //      Additionally, if there are no connections from the Modifier to the Rack output
            //      or another Plugin/Modifier output (which there can't be at the moment),
            //      create a dummy output connection so it get processed. This has the default -1, -1
            //      channel config so the audio buffer doesn't get passed on to the Rack output
            //   3. Otherwise, check to see if this is a connection to a Plugin or the Rack output
            //      and create a InputPluginInfo with the relevant channel setup
            //   4. Finally check to see if it's a connection between a Plugin or the Rack input
            //      and the Rack output and create OutputConnectionInfo for it so that it gets processed

            for (auto conn : owner.getConnections())
            {
                auto source = [&] (EditItemID itemID) -> RackType::PluginRenderingInfo*
                              {
                                  for (auto f : renderContexts)
                                  {
                                      if (f->plugin != nullptr && f->plugin->itemID == itemID)
                                          return f;

                                      if (f->modifier != nullptr && f->modifier->itemID == itemID)
                                          return f;
                                  }

                                  return {};
                              } (conn->sourceID);

                if (modifierID.isValid())
                {
                    if (conn->destID == modifierID)
                    {
                        if (source == this)
                        {
                            jassertfalse;
                            continue;
                        }

                        auto inf = new InputPluginInfo();
                        inf->renderingInfo = source;
                        inf->isMidi = conn->sourcePin == 0;

                        if (inf->isMidi)
                            jassert (conn->destPin <= modifier->getMidiInputNames().size());

                        // Add one here as the audio channel num will have 1 subtracted during processing
                        inf->destInputChan = conn->destPin - modifier->getMidiInputNames().size() + 1;
                        inf->sourceOutputChan = conn->sourcePin;
                        inputPlugins.add (inf);

                        {
                            // If this is not connected to any outputs,
                            // add an OutputConnectionInfo so that it gets processed
                            isConnectedToOutput = true;
                            auto oci = new OutputConnectionInfo();
                            outputConnections.add (oci);
                        }
                    }

                    continue;
                }

                if (conn->destID == pluginID)
                {
                    if (source == this)
                    {
                        jassertfalse;
                        continue;
                    }

                    auto inf = new InputPluginInfo();
                    inf->renderingInfo = source;
                    inf->isMidi = conn->sourcePin == 0;

                    jassert ((conn->sourcePin == 0) == (conn->destPin == 0));
                    inf->sourceOutputChan = conn->sourcePin;

                    if (plugin != nullptr)
                        inf->destInputChan = conn->destPin;
                    else
                        inf->destInputChan = conn->sourcePin; // if this is an End-to-End connector

                    if (inf->renderingInfo == nullptr)
                        jassert (inf->sourceOutputChan < owner.getInputNames().size());

                    inputPlugins.add (inf);
                }

                if (conn->sourceID == pluginID && ! conn->destID.get().isValid())
                {
                    isConnectedToOutput = true;

                    auto oci = new OutputConnectionInfo();

                    oci->isMidi = conn->destPin == 0;
                    oci->destOutputChan = conn->destPin;
                    oci->sourceOutputChan = conn->sourcePin;

                    jassert ((conn->sourcePin == 0) == (conn->destPin == 0));

                    outputConnections.add (oci);
                }
            }
        }
    }

    void addPluginChannelOutput (PlayHead& playhead,
                                 EditTimeRange playheadOutputTime,
                                 juce::AudioBuffer<float>* destBuffer,
                                 int destBufferChannel,
                                 int pluginOutputChannelWanted,
                                 MidiMessageArray* destMidi,
                                 juce::AudioBuffer<float>* rackInputBuffer,
                                 MidiMessageArray* rackInputMidi,
                                 bool isRendering)
    {
        jassert (initialised);

        if (! hasBeenRendered)
        {
            hasBeenRendered = true;

            // fill our buffer from the input plugins..
            pluginOutput.clear();
            pluginMidiOutput.clear();

            if (rackInputMidi != nullptr)
                pluginMidiOutput.isAllNotesOff = rackInputMidi->isAllNotesOff;

            for (int i = inputPlugins.size(); --i >= 0;)
            {
                auto inf = inputPlugins.getUnchecked (i);

                if (inf->renderingInfo != nullptr)
                {
                    inf->renderingInfo->addPluginChannelOutput (playhead, playheadOutputTime,
                                                                &pluginOutput, inf->destInputChan, inf->sourceOutputChan,
                                                                &pluginMidiOutput, rackInputBuffer, rackInputMidi,
                                                                isRendering);
                }
                else
                {
                    // no input plugin - so it's connected to the rack's input
                    if (inf->isMidi)
                    {
                        if (rackInputMidi != nullptr)
                            for (auto& mm : *rackInputMidi)
                                pluginMidiOutput.add (mm);
                    }
                    else
                    {
                        if (inf->sourceOutputChan - 1 < rackInputBuffer->getNumChannels()
                             && inf->destInputChan - 1 < pluginOutput.getNumChannels())
                        {
                            pluginOutput.addFrom (inf->destInputChan - 1, 0,
                                                  *rackInputBuffer, inf->sourceOutputChan - 1, 0,
                                                  pluginOutput.getNumSamples());
                        }
                    }
                }
            }

            auto pluginOutputChannels = AudioChannelSet::canonicalChannelSet (pluginOutput.getNumChannels());

            // and render it..
            if (plugin != nullptr && plugin->isEnabled() && ! plugin->baseClassNeedsInitialising())
                plugin->applyToBufferWithAutomation (AudioRenderContext (playhead, playheadOutputTime,
                                                                         &pluginOutput, pluginOutputChannels, 0,
                                                                         pluginOutput.getNumSamples(),
                                                                         &pluginMidiOutput, 0, true, isRendering));

            if (modifier != nullptr && ! modifier->baseClassNeedsInitialising())
                modifier->applyToBuffer (AudioRenderContext (playhead, playheadOutputTime,
                                                             &pluginOutput, pluginOutputChannels,
                                                             0, pluginOutput.getNumSamples(),
                                                             &pluginMidiOutput, 0, true, isRendering));
        }

        if (destBuffer != nullptr)
        {
            if (pluginOutputChannelWanted > 0 && pluginOutputChannelWanted <= pluginOutput.getNumChannels()
                 && destBufferChannel > 0 && destBufferChannel <= destBuffer->getNumChannels())
            {
                destBuffer->addFrom (destBufferChannel - 1, 0,
                                     pluginOutput, pluginOutputChannelWanted - 1, 0,
                                     pluginOutput.getNumSamples());
            }
        }

        if (destBufferChannel == 0 && destMidi != nullptr)
            for (auto& m : pluginMidiOutput)
                destMidi->add (m);
    }

    void addToRackOutput (PlayHead& playhead, EditTimeRange playheadOutputTime,
                          juce::AudioBuffer<float>& outputBuffer,
                          juce::AudioBuffer<float>& inputBuffer,
                          MidiMessageArray& midiOut,
                          MidiMessageArray& midiIn,
                          bool isRendering)
    {
        for (int i = outputConnections.size(); --i >= 0;)
        {
            auto oci = outputConnections.getUnchecked (i);

            addPluginChannelOutput (playhead,
                                    playheadOutputTime,
                                    &outputBuffer,
                                    oci->destOutputChan,
                                    oci->sourceOutputChan,
                                    &midiOut,
                                    &inputBuffer,
                                    &midiIn,
                                    isRendering);
        }
    }
};

//==============================================================================
struct RackType::RackPluginList  : public ValueTreeObjectList<RackType::PluginInfo>
{
    RackPluginList (RackType& t, const ValueTree& parent)
        : ValueTreeObjectList<PluginInfo> (parent), type (t)
    {
        CRASH_TRACER
        callBlocking ([this] { this->rebuildObjects(); });
    }

    ~RackPluginList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::PLUGININSTANCE);
    }

    PluginInfo* createNewObject (const ValueTree& v) override
    {
        CRASH_TRACER
        auto* i = new PluginInfo();
        i->plugin = type.edit.getPluginCache().getOrCreatePluginFor (v.getChild(0));
        i->state = v;
        return i;
    }

    void deleteObject (PluginInfo* p) override
    {
        jassert (p != nullptr);
        delete p;
    }

    void newObjectAdded (PluginInfo*) override                              { sendChange(); }
    void objectRemoved (PluginInfo*) override                               { sendChange(); }
    void objectOrderChanged() override                                      { sendChange(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { sendChange(); }

    void sendChange()
    {
        // XXX
    }

    RackType& type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackPluginList)
};

struct RackType::ConnectionList  : public ValueTreeObjectList<RackConnection>
{
    ConnectionList (RackType& t, const ValueTree& parent)
        : ValueTreeObjectList<RackConnection> (parent), type (t)
    {
        CRASH_TRACER
        rebuildObjects();
    }

    ~ConnectionList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::CONNECTION);
    }

    RackConnection* createNewObject (const ValueTree& v) override
    {
        if (! type.edit.isLoading())
            TRACKTION_ASSERT_MESSAGE_THREAD

        return new RackConnection (v, type.getUndoManager());
    }

    void deleteObject (RackConnection* t) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (t != nullptr);
        delete t;
    }

    void newObjectAdded (RackConnection*) override                          { sendChange(); }
    void objectRemoved (RackConnection*) override                           { sendChange(); }
    void objectOrderChanged() override                                      { sendChange(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { sendChange(); }

    void sendChange()
    {
        // XXX
    }

    RackType& type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectionList)
};

//==============================================================================
struct RackType::WindowStateList  : public ValueTreeObjectList<WindowState>
{
    WindowStateList (RackType& t)
        : ValueTreeObjectList<WindowState> (t.state), type (t)
    {
        CRASH_TRACER
        callBlocking ([this] { this->rebuildObjects(); });
    }

    ~WindowStateList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::WINDOWSTATE);
    }

    WindowState* createNewObject (const ValueTree& v) override
    {
        CRASH_TRACER
        return new WindowState (type, v);
    }

    void deleteObject (WindowState* t) override
    {
        jassert (t != nullptr);
        delete t;
    }

    void newObjectAdded (WindowState*) override                             {}
    void objectRemoved (WindowState*) override                              {}
    void objectOrderChanged() override                                      {}
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  {}

    RackType& type;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowStateList)
};

//==============================================================================
struct RackType::RenderContext
{
    RenderContext (RackType& type)
    {
        if (type.numActiveInstances.load() == 0)
            return;

        // need to create all the wrappers first, then initialise them..
        for (auto f : type.pluginList->objects)
            renderContexts.add (new PluginRenderingInfo (f->plugin));

        for (auto m : type.getModifierList().getModifiers())
            if (m->getMidiInputNames().size() > 0 || m->getAudioInputNames().size() > 0)
                renderContexts.add (new PluginRenderingInfo (*m));

        bool inToOut = false;

        for (auto conn : type.connectionList->objects)
        {
            if (conn->destID->isInvalid() && conn->sourceID->isInvalid())
            {
                inToOut = true;
                break;
            }
        }

        if (inToOut)
        {
            // dummy one to handle inputs that go directly to outputs
            renderContexts.add (new PluginRenderingInfo (type));
        }

        for (auto fw : renderContexts)
            fw->initialise (type.blockSize, type, renderContexts);
    }

    OwnedArray<PluginRenderingInfo> renderContexts;
};

//==============================================================================
RackType::RackType (const ValueTree& v, Edit& owner)
    : MacroParameterElement (owner, v),
      edit (owner), state (v),
      rackID (EditItemID::fromID (state))
{
    CRASH_TRACER

    auto windowState = state.getChildWithName (IDs::WINDOWSTATE);

    if (! windowState.isValid())
    {
        ValueTree ws (IDs::WINDOWSTATE);
        ws.setProperty (IDs::windowPos, state[IDs::windowPos], nullptr);

        if (state.hasProperty (IDs::windowLocked))
            ws.setProperty (IDs::windowLocked, state[IDs::windowLocked], nullptr);

        state.addChild (ws, -1, nullptr);
    }

    windowStateList = std::make_unique<WindowStateList> (*this);

    rackName.referTo (state, IDs::name, getUndoManager());

    if (rackName.get().isEmpty())
        rackName = TRANS("New Rack");

    pluginList.reset (new RackPluginList (*this, state));
    connectionList.reset (new ConnectionList (*this, state));

    if (getOutputNames().isEmpty())
        addDefaultOutputs();

    if (getInputNames().isEmpty())
        addDefaultInputs();

    loadWindowPosition();
    checkConnections();

    modifierList = std::make_unique<ModifierList> (edit, state.getOrCreateChildWithName (IDs::MODIFIERS, getUndoManager()));

    state.addListener (this);

    renderContextBuilder.setFunction ([this] { std::atomic_exchange (&renderContext, std::make_shared<RenderContext> (*this)); });
}

RackType::~RackType()
{
    CRASH_TRACER
    notifyListenersOfDeletion();
    hideWindowForShutdown();
    state.removeListener (this);
    windowStateList.reset();
}

static ValueTree createInOrOut (const Identifier& type, const String& name)
{
    ValueTree v (type);
    v.setProperty (IDs::name, name, nullptr);
    return v;
}

void RackType::removeAllInputsAndOutputs()
{
    for (int i = state.getNumChildren(); --i >= 0;)
    {
        auto v = state.getChild (i);

        if (v.hasType (IDs::OUTPUT) || v.hasType (IDs::INPUT))
            state.removeChild (i, getUndoManager());
    }
}

void RackType::addDefaultInputs()
{
    state.addChild (createInOrOut (IDs::INPUT, "midi input"), -1, nullptr);
    state.addChild (createInOrOut (IDs::INPUT, "input 1 (left)"), -1, nullptr);
    state.addChild (createInOrOut (IDs::INPUT, "input 2 (right)"), -1, nullptr);
}

void RackType::addDefaultOutputs()
{
    state.addChild (createInOrOut (IDs::OUTPUT, "midi output"), -1, nullptr);
    state.addChild (createInOrOut (IDs::OUTPUT, "output 1 (left)"), -1, nullptr);
    state.addChild (createInOrOut (IDs::OUTPUT, "output 2 (right)"), -1, nullptr);
}

namespace
{
    bool hasAnyModifierAssignmentsRecursive (const ValueTree& vt)
    {
        if (vt.hasType (IDs::MODIFIERASSIGNMENTS) && vt.getNumChildren() > 0)
            return true;

        for (auto v : vt)
            if (hasAnyModifierAssignmentsRecursive (v))
                return true;

        return false;
    }
}

Result RackType::restoreStateFromValueTree (const ValueTree& vt)
{
    // First check to see if the incoming state has any modifier assignments
    if (hasAnyModifierAssignmentsRecursive (vt))
        return Result::fail (TRANS("Unable to apply preset due to Macro or Modifier connections, please create a new Rack from the preset"));

    auto v = vt.createCopy();

    if (v.hasType (IDs::PRESET))
        v = v.getChildWithName (IDs::RACK);

    if (! v.hasType (IDs::RACK))
        return Result::fail (TRANS("Invalid or corrupted preset"));

    rackID.writeID (v, nullptr);

    {
        auto um = getUndoManager();
        state.copyPropertiesFrom (v, um);

        // Remove all children except the window state
        for (int i = state.getNumChildren(); --i >= 0;)
        {
            auto c = state.getChild (i);

            if (! (c.hasType (IDs::WINDOWSTATE) || c.hasType (IDs::MACROPARAMETERS) || c.hasType (IDs::MODIFIERS)))
                state.removeChild (i, um);
        }

        // Add all children except window state, macros and modifiers
        for (int i = v.getNumChildren(); --i >= 0;)
        {
            auto c = v.getChild (i);
            v.removeChild (i, nullptr);

            if (! (c.hasType (IDs::WINDOWSTATE) || c.hasType (IDs::MACROPARAMETERS) || c.hasType (IDs::MODIFIERS)))
                state.addChild (c, 0, um);
        }

        checkConnections();

        for (auto rf : activeRackInstances)
            rf->changed();
    }

    propertiesChanged();
    changed();

    return Result::ok();
}

ValueTree RackType::createStateCopy (bool includeAutomation)
{
    saveWindowPosition();

    for (auto p : getPlugins())
        p->flushPluginStateToValueTree();

    auto v = state.createCopy();

    if (! includeAutomation)
        AutomationCurve::removeAllAutomationCurvesRecursively (v);

    return v;
}

Array<RackType::WindowState*> RackType::getWindowStates() const
{
    return windowStateList->objects;
}

void RackType::loadWindowPosition()
{
    for (auto* ws : getWindowStates())
    {
        if (ws->state.hasProperty (IDs::windowPos))
            ws->lastWindowBounds = Rectangle<int>::fromString (ws->state[IDs::windowPos].toString());

        if (ws->state.hasProperty (IDs::windowLocked))
            ws->windowLocked = ws->state[IDs::windowLocked];
    }
}

void RackType::saveWindowPosition()
{
    for (auto* ws : getWindowStates())
    {
        auto windowState = ws->lastWindowBounds.toString();
        ws->state.setProperty (IDs::windowPos, windowState.isEmpty() ? var() : var (windowState), nullptr);
        ws->state.setProperty (IDs::windowLocked, ws->windowLocked, nullptr);
    }
}

RackType::Ptr RackType::createTypeToWrapPlugins (const Plugin::Array& plugins, Edit& ownerEdit)
{
    auto rack = ownerEdit.getRackList().addNewRack();

    if (plugins.size() == 1)
        rack->rackName = plugins.getFirst()->getName() + " " + TRANS("Wrapper");
    else if (plugins.getFirst()->getOwnerTrack() == nullptr)
        rack->rackName = TRANS("Master Wrapper");
    else
        rack->rackName = plugins.getFirst()->getOwnerTrack()->getName() + " "  + TRANS("Wrapper");

    rack->removeInput (2);
    rack->removeInput (1);
    rack->removeOutput (2);
    rack->removeOutput (1);

    for (int i = 0; i < plugins.size(); ++i)
        if (auto f = plugins[i])
            rack->addPlugin (f, Point<float> (1.0f / (plugins.size() + 1) * (i + 1), 0.5f), false);

    StringArray ins, outs;
    plugins.getFirst()->getChannelNames (&ins, nullptr);
    plugins.getLast()->getChannelNames (nullptr, &outs);

    // connect the left side to the first plugin
    for (int i = 0; i < jmin (maxRackAudioChans, ins.size()); ++i)
    {
        String name (ins[i]);

        if (name.isEmpty() || name.equalsIgnoreCase (TRANS("Unnamed")))
            name = getDefaultInputName (i);

        rack->addInput (-1, name);
        rack->addConnection ({}, i + 1, plugins.getFirst()->itemID, i + 1);
    }

    // connect the right side to the last plugin
    for (int i = 0; i < jmin (maxRackAudioChans, outs.size()); ++i)
    {
        String name (outs[i]);

        if (name.isEmpty() || name.equalsIgnoreCase (TRANS("Unnamed")))
            name = getDefaultOutputName (i);

        rack->addOutput (-1, name);
        rack->addConnection (plugins.getLast()->itemID, i + 1, {}, i + 1);
    }

    // midi connections for the first and last plugin
    rack->addConnection ({}, 0, plugins.getFirst()->itemID, 0);
    rack->addConnection (plugins.getLast()->itemID, 0, {}, 0);

    for (int i = 0; i < plugins.size() - 1; ++i)
    {
        auto fsrc = plugins[i];
        auto fdst = plugins[i + 1];

        StringArray dstIns, srcOuts;
        fsrc->getChannelNames (nullptr, &srcOuts);
        fdst->getChannelNames (&dstIns, nullptr);

        rack->addConnection (fsrc->itemID, 0, fdst->itemID, 0);

        for (int j = 0; j < jmin (srcOuts.size(), dstIns.size()); ++j)
            rack->addConnection (fsrc->itemID, j + 1, fdst->itemID, j + 1);
    }

    return rack;
}

StringArray RackType::getInputNames() const
{
    StringArray s;

    for (const auto& v : state)
        if (v.hasType (IDs::INPUT))
            s.add (v.getProperty (IDs::name));

    return s;
}

StringArray RackType::getOutputNames() const
{
    StringArray s;

    for (const auto& v : state)
        if (v.hasType (IDs::OUTPUT))
            s.add (v.getProperty (IDs::name));

    return s;
}

juce::Array<Plugin*> RackType::getPlugins() const
{
    Array<Plugin*> list;

    for (auto i : pluginList->objects)
        list.add (i->plugin.get());

    return list;
}

bool RackType::isPluginAllowed (const Plugin::Ptr& p)
{
    return p != nullptr && p->canBeAddedToRack();
}

void RackType::addPlugin (const Plugin::Ptr& p, Point<float> pos, bool canAutoConnect)
{
    if (isPluginAllowed (p))
    {
        if (! getPlugins().contains (p.get()))
        {
            edit.getTransport().stopIfRecording();

            bool autoConnect = canAutoConnect && pluginList->objects.isEmpty();

            p->removeFromParent();

            ValueTree v (IDs::PLUGININSTANCE);
            v.setProperty (IDs::x, jlimit (0.0f, 1.0f, pos.x), nullptr);
            v.setProperty (IDs::y, jlimit (0.0f, 1.0f, pos.y), nullptr);
            v.addChild (p->state, -1, getUndoManager());

            state.addChild (v, -1, getUndoManager());

            if (autoConnect)
            {
                StringArray ins, outs;
                p->getChannelNames (&ins, &outs);

                while (outs.size() > getOutputNames().size() - 1)
                    if (addOutput (getOutputNames().size(), TRANS("Output") + " " + String (getOutputNames().size())) == -1)
                        break;

                for (int i = 0; i < ins.size(); ++i)   addConnection ({}, i + 1, p->itemID, i + 1);
                for (int i = 0; i < outs.size(); ++i)  addConnection (p->itemID, i + 1, {}, i + 1);

                // midi connections
                addConnection ({}, 0, p->itemID, 0);
                addConnection (p->itemID, 0, {}, 0);
            }
        }
    }
}

Point<float> RackType::getPluginPosition (const Plugin::Ptr& p) const
{
    for (auto info : pluginList->objects)
        if (info->plugin == p)
            return { info->state[IDs::x],
                     info->state[IDs::y] };

    return {};
}

Point<float> RackType::getPluginPosition (int index) const
{
    if (auto info = pluginList->objects[index])
        return { info->state[IDs::x],
                 info->state[IDs::y] };

    return {};
}

void RackType::setPluginPosition (int index, Point<float> pos)
{
    if (auto info = pluginList->objects[index])
    {
        info->state.setProperty (IDs::x, jlimit (0.0f, 1.0f, pos.x), getUndoManager());
        info->state.setProperty (IDs::y, jlimit (0.0f, 1.0f, pos.y), getUndoManager());
    }
}

//==============================================================================
Array<EditItemID> RackType::getPluginsWhichTakeInputFrom (EditItemID sourceId) const
{
    Array<EditItemID> results;

    if (sourceId.isValid())
        for (auto rc : connectionList->objects)
            if (rc->sourceID == sourceId && rc->destID.get().isValid())
                results.addIfNotAlreadyThere (rc->destID);

    return results;
}

bool RackType::arePluginsConnectedIndirectly (EditItemID src, EditItemID dest, int depth) const
{
    if (depth < 100) // to avoid loops
    {
        auto dests = getPluginsWhichTakeInputFrom (src);

        for (auto& d : dests)
            if (d == dest)
                return true;

        for (auto& d : dests)
            if (arePluginsConnectedIndirectly (d, dest, depth + 1))
                return true;
    }

    return false;
}

bool RackType::isConnectionLegal (EditItemID source, int sourcePin,
                                  EditItemID dest, int destPin) const
{
    if (! source.isValid())
    {
        if (sourcePin >= getInputNames().size())
            return false;
    }
    else
    {
        StringArray ins, outs;

        if (auto p = edit.getPluginCache().getPluginFor (source))
            p->getChannelNames (&ins, &outs);

        if (sourcePin > outs.size())
            return false;
    }

    if (! dest.isValid())
    {
        if (destPin >= getOutputNames().size())
            return false;
    }
    else
    {
        StringArray ins, outs;

        if (auto p = edit.getPluginCache().getPluginFor (dest))
            p->getChannelNames (&ins, &outs);
        else if (auto m = findModifierForID (getModifierList(), dest))
            ins = m->getAudioInputNames();

        if (destPin > ins.size())
            return false;
    }

    if (! (source.isValid() || dest.isValid()))
        return true;

    if (source == dest)
        return false;

    return ! arePluginsConnectedIndirectly (dest, source);
}

void RackType::addConnection (EditItemID srcId, int sourcePin,
                              EditItemID dstId, int destPin)
{
    if (isConnectionLegal (srcId, sourcePin, dstId, destPin))
    {
        for (auto rc : connectionList->objects)
            if (rc->destID == dstId && rc->destPin == destPin
                  && rc->sourceID == srcId && rc->sourcePin == sourcePin)
                return;

        ValueTree v (IDs::CONNECTION);
        srcId.setProperty (v, IDs::src, nullptr);
        dstId.setProperty (v, IDs::dst, nullptr);
        v.setProperty (IDs::srcPin, sourcePin, nullptr);
        v.setProperty (IDs::dstPin, destPin, nullptr);

        state.addChild (v, -1, getUndoManager());
    }
}

void RackType::removeConnection (EditItemID srcId, int sourcePin,
                                 EditItemID dstId, int destPin)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (int i = connectionList->objects.size(); --i >= 0;)
    {
        if (auto rc = connectionList->objects[i])
        {
            if (rc->destID == dstId && rc->sourceID == srcId
                 && rc->destPin == destPin && rc->sourcePin == sourcePin)
            {
                state.removeChild (rc->state, getUndoManager());
                break;
            }
        }
    }
}

void RackType::checkConnections()
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    for (int i = connectionList->objects.size(); --i >= 0;)
    {
        if (auto rc = connectionList->objects[i])
        {
            // Check for connections going to no longer available rack IO pins
            if (((rc->sourcePin < 0 || rc->sourcePin >= getInputNames().size()) && rc->sourceID->isInvalid())
                  || ((rc->destPin < 0 || rc->destPin >= getOutputNames().size()) && rc->destID->isInvalid()))
            {
                state.removeChild (rc->state, getUndoManager());
                continue;
            }

            // Check for connections going to no longer available plugin IO pins
            if (auto* ep = dynamic_cast<ExternalPlugin*> (getPluginForID (rc->sourceID)))
            {
                if (rc->sourcePin < 0 || rc->sourcePin > ep->getNumOutputs())
                {
                    state.removeChild (rc->state, getUndoManager());
                    continue;
                }
            }

            if (auto* ep = dynamic_cast<ExternalPlugin*> (getPluginForID (rc->destID)))
            {
                if (rc->destPin < 0 || rc->destPin > ep->getNumInputs())
                {
                    state.removeChild (rc->state, getUndoManager());
                    continue;
                }
            }
        }
    }
}

Array<const RackConnection*> RackType::getConnections() const noexcept
{
    Array<const RackConnection*> list;

    for (auto rc : connectionList->objects)
        list.add (rc);

    return list;
}

//==============================================================================
static bool findModifierWithID (ValueTree& modifiers, EditItemID itemID)
{
    for (auto m : modifiers)
        if (EditItemID::fromID (m) == itemID)
            return true;

    return false;
}

static bool findPluginOrModifierWithID (ValueTree& rack, EditItemID itemID)
{
    for (int i = rack.getNumChildren(); --i >= 0;)
    {
        auto c = rack.getChild (i);

        if (c.hasType (IDs::PLUGININSTANCE))
            if (EditItemID::fromID (c.getChildWithName (IDs::PLUGIN)) == itemID)
                return true;

        if (c.hasType (IDs::MODIFIERS))
            if (findModifierWithID (c, itemID))
                return true;
    }

    return false;
}

static int countNumConnections (ValueTree& rack, const Identifier& type)
{
    int count = 0;

    for (int i = rack.getNumChildren(); --i >= 0;)
        if (rack.getChild (i).hasType (type))
            ++count;

    return count;
}

static bool connectionIsValid (ValueTree& rack, EditItemID srcID, int srcPin, EditItemID dstID, int dstPin)
{
    if (srcID.isInvalid())
    {
        if (srcPin < 0 || srcPin >= countNumConnections (rack, IDs::INPUT))
            return false;
    }
    else if (! findPluginOrModifierWithID (rack, srcID))
    {
        return false;
    }

    if (dstID.isInvalid())
    {
        if (dstPin < 0 || dstPin >= countNumConnections (rack, IDs::OUTPUT))
            return false;
    }
    else if (! findPluginOrModifierWithID (rack, dstID))
    {
        return false;
    }

    return true;
}

void RackType::removeBrokenConnections (ValueTree& rack, UndoManager* um)
{
    for (int i = rack.getNumChildren(); --i >= 0;)
    {
        auto c = rack.getChild (i);

        if (c.hasType (IDs::CONNECTION)
             && ! connectionIsValid (rack,
                                     EditItemID::fromProperty (c, IDs::src), c[IDs::srcPin],
                                     EditItemID::fromProperty (c, IDs::dst), c[IDs::dstPin]))
            rack.removeChild (i, um);
    }
}

void RackType::createInstanceForSideChain (Track& at, const BigInteger& channelMask,
                                           EditItemID pluginID, int pinIndex)
{
    const bool connectLeft  = channelMask[0];
    const bool connectRight = channelMask[1];
    auto& pl = at.pluginList;
    RackInstance* rack = nullptr;

    for (auto p : pl)
        if (auto rf = dynamic_cast<RackInstance*> (p))
            if (rf->type.get() == this)
                rack = rf;

    if (rack == nullptr)
    {
        if (auto p = edit.getPluginCache().getOrCreatePluginFor (RackInstance::create (*this)))
        {
            pl.insertPlugin (p, 0, nullptr);
            rack = dynamic_cast<RackInstance*> (p.get());
        }
    }

    if (rack != nullptr)
    {
        auto inputs = getInputNames();
        const String leftName ("sidechain input (left)");
        const String rightName ("sidechain input (right)");
        const String noneName (rack->getNoPinName());

        if (connectLeft)
        {
            int srcPinIndex = inputs.indexOf (leftName);

            if (srcPinIndex == -1)
                srcPinIndex = addInput (-1, leftName);

            rack->setInputName (RackInstance::left, leftName);
            rack->setOutputName (RackInstance::left, noneName);

            if (! connectRight)
            {
                rack->setInputName (RackInstance::right, noneName);
                rack->setOutputName (RackInstance::right, noneName);
            }

            if (srcPinIndex >= 0)
                addConnection ({}, srcPinIndex, pluginID, pinIndex);
        }

        if (connectRight)
        {
            int srcPinIndex = inputs.indexOf (rightName);

            if (srcPinIndex == -1)
                srcPinIndex = addInput (-1, rightName);

            rack->setInputName (RackInstance::right, rightName);
            rack->setOutputName (RackInstance::right, noneName);

            if (! connectLeft)
            {
                rack->setInputName (RackInstance::left, noneName);
                rack->setOutputName (RackInstance::left, noneName);
            }

            if (srcPinIndex != -1)
                addConnection ({}, srcPinIndex, pluginID,
                               connectLeft ? pinIndex + 1 : pinIndex);
        }

        rack->wetGain->setParameter (0.0f, dontSendNotification);
        rack->dryGain->setParameter (1.0f, dontSendNotification);

        for (SelectionManager::Iterator sm; sm.next();)
            if (sm->isSelected (rack))
                sm->refreshDetailComponent();
    }
    else
    {
        edit.engine.getUIBehaviour().showWarningAlert (TRANS("Unable to create side-chain"),
                                                       TRANS("Unable to create rack on source track"));
    }
}

//==============================================================================
juce::String RackType::getSelectableDescription()
{
    return TRANS("Plugin Rack");
}

//==============================================================================
static int findIndexOfNthInstanceOf (ValueTree& parent, const Identifier& type, int index)
{
    int count = 0;

    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto v = parent.getChild (i);

        if (v.hasType (type))
            if (count++ == index)
                return i;
    }

    return -1;
}

int RackType::addInput (int index, const String& name)
{
    int numNames = getInputNames().size();

    if (numNames < maxRackAudioChans)
    {
        if (index >= 0)
        {
            for (auto conn : connectionList->objects)
                if (conn->sourceID->isInvalid() && conn->sourcePin >= index)
                    conn->sourcePin = conn->sourcePin + 1;
        }

        state.addChild (createInOrOut (IDs::INPUT, name),
                        index < 0 ? findIndexOfNthInstanceOf (state, IDs::INPUT, numNames - 1) + 1
                                  : findIndexOfNthInstanceOf (state, IDs::INPUT, index),
                        getUndoManager());

        if (index < 0)
            index = numNames;

        jassert (getInputNames()[index] == name);
        return index;
    }

    return -1;
}

int RackType::addOutput (int index, const String& name)
{
    int numNames = getOutputNames().size();

    if (numNames < maxRackAudioChans)
    {
        if (index >= 0)
        {
            for (auto conn : connectionList->objects)
                if (conn->destID->isInvalid() && conn->destPin >= index)
                    conn->destPin = conn->destPin + 1;
        }

        state.addChild (createInOrOut (IDs::OUTPUT, name),
                        index < 0 ? findIndexOfNthInstanceOf (state, IDs::OUTPUT, numNames - 1) + 1
                                  : findIndexOfNthInstanceOf (state, IDs::OUTPUT, index),
                        getUndoManager());

        if (index < 0)
            index = numNames;

        jassert (getOutputNames()[index] == name);
        return index;
    }

    return -1;
}

void RackType::removeInput (int index)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    int toRemove = findIndexOfNthInstanceOf (state, IDs::INPUT, index);

    if (toRemove >= 0)
    {
        for (auto conn : connectionList->objects)
            if (conn->sourceID->isInvalid() && conn->sourcePin > index)
                conn->sourcePin = conn->sourcePin - 1;

        state.removeChild (toRemove, getUndoManager());
        checkConnections();
    }
}

void RackType::removeOutput (int index)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    int toRemove = findIndexOfNthInstanceOf (state, IDs::OUTPUT, index);

    if (toRemove >= 0)
    {
        for (auto conn : connectionList->objects)
            if (conn->destID->isInvalid() && conn->destPin > index)
                conn->destPin = conn->destPin - 1;

        state.removeChild (toRemove, getUndoManager());
        checkConnections();
    }
}

static String checkChannelName (const String& name)
{
    return name.trim().isEmpty() ? TRANS("Unnamed") : name;
}

void RackType::renameInput (int index, const String& name)
{
    int toRename = findIndexOfNthInstanceOf (state, IDs::INPUT, index);

    if (toRename >= 0)
        state.getChild (toRename).setProperty (IDs::name, checkChannelName (name), getUndoManager());
}

void RackType::renameOutput (int index, const String& name)
{
    int toRename = findIndexOfNthInstanceOf (state, IDs::OUTPUT, index);

    if (toRename >= 0)
        state.getChild (toRename).setProperty (IDs::name, checkChannelName (name), getUndoManager());
}

//==============================================================================
UndoManager* RackType::getUndoManager() const
{
    return &edit.getUndoManager();
}

void RackType::countInstancesInEdit()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    numberOfInstancesInEdit = 0;

    for (auto p : getAllPlugins (edit, false))
        if (auto rf = dynamic_cast<RackInstance*> (p))
            if (rf->type.get() == this)
                ++numberOfInstancesInEdit;
}

//==============================================================================
void RackType::registerInstance (RackInstance* instance, const PlaybackInitialisationInfo& info)
{
    CRASH_TRACER
    const ScopedLock sl (renderLock);

    renderContextBuilder.triggerAsyncUpdate();
    isFirstCallbackOfBlock = true;

    activeRackInstances.addIfNotAlreadyThere (instance);
    numActiveInstances.store (activeRackInstances.size());

    blockSize = info.blockSizeSamples;
    sampleRate = info.sampleRate;

    tempBufferOut.setSize (jmax (1, getOutputNames().size()), blockSize);
    tempBufferOut.clear();

    tempBufferIn.setSize (jmax (1, getInputNames().size()), blockSize);
    tempBufferIn.clear();

    tempMidiBufferOut.clear();
    tempMidiBufferIn.clear();

    initialisePluginsIfNeeded (info);

    countInstancesInEdit();
}

void RackType::initialisePluginsIfNeeded (const PlaybackInitialisationInfo& info) const
{
    latencyCalculation.reset();

    for (auto f : getPlugins())
        if (f->baseClassNeedsInitialising())
            f->baseClassInitialise (info);

    for (auto& m : getModifierList().getModifiers())
        if (m->baseClassNeedsInitialising())
            m->baseClassInitialise (info);
}

void RackType::deregisterInstance (RackInstance* instance)
{
    const ScopedLock sl (renderLock);

    renderContextBuilder.triggerAsyncUpdate();

    activeRackInstances.removeAllInstancesOf (instance);
    numActiveInstances.store (activeRackInstances.size());

    if (activeRackInstances.isEmpty())
    {
        tempBufferOut.setSize (1, 64);
        tempBufferIn.setSize (1, 64);
    }

    tempMidiBufferOut.clear();
    tempMidiBufferIn.clear();

    if (activeRackInstances.isEmpty())
    {
        for (auto f : getPlugins())
            if (! f->baseClassNeedsInitialising())
                f->baseClassDeinitialise();

        for (auto m : getModifierList().getModifiers())
            if (! m->baseClassNeedsInitialising())
                m->baseClassDeinitialise();
    }

    countInstancesInEdit();
}

void RackType::updateAutomatableParamPositions (double time)
{
    for (auto f : getPlugins())
        f->updateAutomatableParamPosition (time);

    for (auto m : getModifierList().getModifiers())
        m->updateAutomatableParamPosition (time);
}

//==============================================================================
namespace RackLatency
{
    Array<const RackConnection*> getConnectionsTo (const Array<const RackConnection*>& allConnections, const EditItemID destID)
    {
        Array<const RackConnection*> connections;

        for (auto c : allConnections)
            if (c->destID == destID)
                connections.add (c);

        return connections;
    }

    void visitConnectionsTo (const Array<const RackConnection*>& allConnections, const Array<EditItemID> sourcesStack,
                             const std::function<bool (const Array<EditItemID>& /*currentStack*/)>& visitor)
    {
        // Visit this source
        jassert (visitor); // Must be called with a valid visitor
        if (! visitor (sourcesStack))
            return;

        // Find connections to the last item on the stack
        for (auto c : getConnectionsTo (allConnections, sourcesStack.getLast()))
        {
            // Detect cycles and skip these
            if (sourcesStack.contains (c->sourceID))
                continue;

            // Push the new source on to the stack and visit it
            Array<EditItemID> newStack (sourcesStack);
            newStack.add (c->sourceID);
            visitConnectionsTo (allConnections, newStack, visitor);
        }
    }

    double getLatencyBetweenConnectionAndSource (RackType& type, const RackConnection& connection, EditItemID sourceID)
    {
        double latency = 0.0;
        Array<Array<EditItemID>> pathsBetweenConnectionAndSource;

        auto visitor = [sourceID, &pathsBetweenConnectionAndSource, pathsChecked = Array<Array<EditItemID>>()] (const Array<EditItemID>& currentStack) mutable
        {
            if (pathsChecked.contains (currentStack))
                return false;

            pathsChecked.add (currentStack);

            // If we've hit our source to find, calculate the latency of the stack
            if (currentStack.getLast() == sourceID)
                pathsBetweenConnectionAndSource.add (currentStack);

            return true;
        };

        visitConnectionsTo (type.getConnections(), Array<EditItemID> (connection.sourceID.get()), visitor);

        // Calculate max latency of all paths
        for (const auto& path : pathsBetweenConnectionAndSource)
        {
            double pathLatency = 0.0;

            for (auto item : path)
                if (auto af = type.getPluginForID (item))
                    pathLatency += af->getLatencySeconds();

            latency = jmax (latency, pathLatency);
        }

        return latency;
    }

    Array<const RackConnection*> removeDuplicates (Array<const RackConnection*> connections)
    {
        for (int i = connections.size(); --i >= 0;)
        {
            const auto connection = connections.getUnchecked (i);

            for (int start = i - 1; --start >= 0;)
            {
                const auto c = connections.getUnchecked (i);

                if (connection->sourceID == c->sourceID
                    && connection->destID == c->destID)
                {
                    connections.remove (i);
                    --i;
                }
            }
        }

        return connections;
    }

    double getMaxLatency (RackType& type)
    {
        // Find all latencies between all outputs and all inputs
        double maxLatency = 0.0;

        for (auto c : removeDuplicates (type.getConnections()))
        {
            if (c->destID.get().isInvalid())
            {
                // Looking for connections to the input hence the empty ID
                const double latencyForConnection = getLatencyBetweenConnectionAndSource (type, *c, {});
                maxLatency = jmax (maxLatency, latencyForConnection);
            }
        }

        return maxLatency;
    }
}

struct RackType::LatencyCalculation
{
    LatencyCalculation (RackType& rt, double rate, int buffSize)
        : sampleRate (rate), bufferSize (buffSize),
          latencySeconds (RackLatency::getMaxLatency (rt))
    {
    }

    const double sampleRate;
    const int bufferSize;
    const double latencySeconds = 0.0;
};

double RackType::getLatencySeconds (double rate, int bufferSize)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (latencyCalculation)
        if (latencyCalculation->sampleRate != rate || latencyCalculation->bufferSize != bufferSize)
            latencyCalculation.reset();

    if (! latencyCalculation)
        latencyCalculation = std::make_unique<LatencyCalculation> (*this, rate, bufferSize);

    return latencyCalculation->latencySeconds + (numberOfInstancesInEdit > 1 ? bufferSize / rate : 0.0);
}

//==============================================================================
void RackType::updateTempBuferSizes()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    const ScopedLock sl (renderLock);

    auto outSize = getOutputNames().size();

    if (outSize != tempBufferOut.getNumChannels())
    {
        tempBufferOut.setSize (jmax (1, outSize), tempBufferOut.getNumSamples());
        tempBufferOut.clear();
    }

    auto inSize = getInputNames().size();

    if (inSize != tempBufferIn.getNumChannels())
    {
        tempBufferIn.setSize (jmax (1, inSize), tempBufferIn.getNumSamples());
        tempBufferIn.clear();
    }
}

static void applyDelay (juce::AudioBuffer<float>* buffer, int start, int numSamples,
                        juce::AudioBuffer<float>* delayBuffer, int& delayPos)
{
    if (delayBuffer != nullptr)
    {
        auto delaySize = delayBuffer->getNumSamples();

        float* l = buffer->getWritePointer (0, start);
        float* r = buffer->getNumChannels() > 1 ? buffer->getWritePointer (1, start) : nullptr;
        float* const delayL = delayBuffer->getWritePointer (0);
        int dp = delayPos;

        if (r != nullptr)
        {
            float* const delayR = delayBuffer->getWritePointer (1);

            for (int i = numSamples; --i >= 0;)
            {
                float out = delayL[dp];
                delayL[dp] = *l;
                *l++ = out;

                out = delayR[dp];
                delayR[dp] = *r;
                *r++ = out;

                if (++dp >= delaySize)
                    dp = 0;
            }
        }
        else
        {
            for (int i = numSamples; --i >= 0;)
            {
                float out = delayL[dp];
                delayL[dp] = *l;
                *l++ = out;

                if (++dp >= delaySize)
                    dp = 0;
            }
        }

        delayPos = dp;
    }
}

bool RackType::isReadyToRender() const
{
    auto hasRenderContext = [this] { return std::atomic_load (&renderContext); };
    return numActiveInstances.load() == 0 || hasRenderContext();
}

void RackType::newBlockStarted()
{
    isFirstCallbackOfBlock = true;
}

void RackType::process (const AudioRenderContext& fc,
                        int leftInputGoesTo,   float leftInputGain1,  float leftInputGain2,
                        int rightInputGoesTo,  float rightInputGain1, float rightInputGain2,
                        int leftOutComesFrom,  float leftOutGain1,    float leftOutGain2,
                        int rightOutComesFrom, float rightOutGain1,   float rightOutGain2,
                        float dryGain,
                        juce::AudioBuffer<float>* delayBuffer,
                        int& delayPos)
{
    CRASH_TRACER
    const ScopedLock sl (renderLock);
    auto rrc = std::atomic_load (&renderContext);

    if (numActiveInstances.load() == 0 || rrc == nullptr)
        return;

    if (isFirstCallbackOfBlock)
    {
        CRASH_TRACER

        // starting a new block..
        isFirstCallbackOfBlock = false;

        // reset the flags on all the plugin wrappers..
        for (auto f : rrc->renderContexts)
            f->resetRenderingFlag();

        if (numActiveInstances.load() <= 1)
        {
            // only one plugin using us, so we can render synchronously..
            tempMidiBufferIn.clear();
            tempMidiBufferOut.clear();

            if (fc.destBuffer != nullptr)
            {
                // load our input buffer..
                tempBufferIn.clear();
                tempBufferOut.clear();

                if (leftInputGoesTo > 0)
                    AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, leftInputGoesTo - 1, 0, 0,
                                                      fc.bufferStartSample, fc.bufferNumSamples,
                                                      AudioFadeCurve::linear, leftInputGain1, leftInputGain2);

                if (tempBufferIn.getNumChannels() > 1 && rightInputGoesTo > 0)
                    AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, rightInputGoesTo - 1, 0,
                                                      fc.destBuffer->getNumChannels() > 1 ? 1 : 0,
                                                      fc.bufferStartSample, fc.bufferNumSamples,
                                                      AudioFadeCurve::linear, rightInputGain1, rightInputGain2);
            }

            if (fc.bufferForMidiMessages != nullptr)
                tempMidiBufferIn.mergeFromAndClear (*fc.bufferForMidiMessages);

            // render all the nodes that connect to the output
            for (int i = rrc->renderContexts.size(); --i >= 0;)
            {
                auto fw = rrc->renderContexts.getUnchecked (i);

                if (fw->isConnectedToOutput)
                    fw->addToRackOutput (fc.playhead,
                                         fc.streamTime,
                                         tempBufferOut,
                                         tempBufferIn,
                                         tempMidiBufferOut,
                                         tempMidiBufferIn,
                                         fc.isRendering);
            }

            if (fc.destBuffer != nullptr)
            {
                applyDelay (fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, delayBuffer, delayPos);

                fc.destBuffer->applyGain (fc.bufferStartSample, fc.bufferNumSamples, dryGain);

                // copy out the appropriate output channels
                if (leftOutComesFrom > 0)
                    AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 0, fc.bufferStartSample, leftOutComesFrom - 1,
                                                      0, fc.bufferNumSamples, AudioFadeCurve::linear, leftOutGain1, leftOutGain2);

                if (fc.destBuffer->getNumChannels() > 1
                      && rightOutComesFrom > 0 && tempBufferOut.getNumChannels() > rightOutComesFrom - 1)
                    AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 1, fc.bufferStartSample,
                                                      rightOutComesFrom - 1, 0, fc.bufferNumSamples,
                                                      AudioFadeCurve::linear, rightOutGain1, rightOutGain2);
            }

            if (fc.bufferForMidiMessages != nullptr)
                fc.bufferForMidiMessages->mergeFromAndClear (tempMidiBufferOut);
        }
        else
        {
            CRASH_TRACER

            // first of many plugins to call us for the same block, so render up from the previous
            // input buffer and return the result..
            // render all the nodes that connect to the output
            tempBufferOut.clear();
            tempMidiBufferOut.clear();

            for (int i = rrc->renderContexts.size(); --i >= 0;)
            {
                auto fw = rrc->renderContexts.getUnchecked (i);

                if (fw->isConnectedToOutput)
                    fw->addToRackOutput (fc.playhead,
                                         fc.streamTime,
                                         tempBufferOut,
                                         tempBufferIn,
                                         tempMidiBufferOut,
                                         tempMidiBufferIn,
                                         fc.isRendering);
            }

            // then add the new input to our cleared input buffer
            // and copy out the appropriate output channels
            if (fc.destBuffer != nullptr)
            {
                tempBufferIn.clear();

                if (leftInputGoesTo > 0)
                    AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, leftInputGoesTo - 1, 0, 0,
                                                      fc.bufferStartSample, fc.bufferNumSamples,
                                                      AudioFadeCurve::linear, leftInputGain1, leftInputGain2);

                if (fc.destBuffer->getNumChannels() > 1 && rightInputGoesTo > 0
                      && rightInputGoesTo - 1 < tempBufferIn.getNumChannels())
                    AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, rightInputGoesTo - 1, 0,
                                                      fc.destBuffer->getNumChannels() > 1 ? 1 : 0,
                                                      fc.bufferStartSample, fc.bufferNumSamples,
                                                      AudioFadeCurve::linear, rightInputGain1, rightInputGain2);

                applyDelay (fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, delayBuffer, delayPos);

                fc.destBuffer->applyGain (fc.bufferStartSample, fc.bufferNumSamples, dryGain);

                if (leftOutComesFrom > 0)
                    AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 0,
                                                      fc.bufferStartSample, leftOutComesFrom - 1,
                                                      0, fc.bufferNumSamples, AudioFadeCurve::linear, leftOutGain1, leftOutGain2);

                if (fc.destBuffer->getNumChannels() > 1 && rightOutComesFrom > 0)
                    AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 1,
                                                      fc.bufferStartSample, rightOutComesFrom - 1,
                                                      0, fc.bufferNumSamples, AudioFadeCurve::linear, rightOutGain1, rightOutGain2);
            }

            if (fc.bufferForMidiMessages != nullptr)
            {
                tempMidiBufferIn.clear();
                tempMidiBufferIn.mergeFromAndClear (*fc.bufferForMidiMessages);
                fc.bufferForMidiMessages->mergeFrom (tempMidiBufferOut);
            }
        }
    }
    else
    {
        CRASH_TRACER

        // second call for the same block, so return the last output block..
        if (fc.destBuffer != nullptr)
        {
            // add the new input to our input buffer..
            if (leftInputGoesTo > 0)
                AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, leftInputGoesTo - 1, 0, 0,
                                                  fc.bufferStartSample, fc.bufferNumSamples,
                                                  AudioFadeCurve::linear, leftInputGain1, leftInputGain2);

            if (fc.destBuffer->getNumChannels() > 1 && rightInputGoesTo > 0
                  && ((rightInputGoesTo - 1) < tempBufferIn.getNumChannels()))
                AudioFadeCurve::addWithCrossfade (tempBufferIn, *fc.destBuffer, rightInputGoesTo - 1, 0, 1,
                                                  fc.bufferStartSample, fc.bufferNumSamples,
                                                  AudioFadeCurve::linear, rightInputGain1, rightInputGain2);

            applyDelay (fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, delayBuffer, delayPos);

            fc.destBuffer->applyGain (fc.bufferStartSample, fc.bufferNumSamples, dryGain);

            // copy out the appropriate output channels
            if (leftOutComesFrom > 0)
                AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 0,
                                                  fc.bufferStartSample, leftOutComesFrom - 1,
                                                  0, fc.bufferNumSamples,
                                                  AudioFadeCurve::linear, leftOutGain1, leftOutGain2);

            if (fc.destBuffer->getNumChannels() > 1 && rightOutComesFrom > 0)
                AudioFadeCurve::addWithCrossfade (*fc.destBuffer, tempBufferOut, 1,
                                                  fc.bufferStartSample, rightOutComesFrom - 1,
                                                  0, fc.bufferNumSamples,
                                                  AudioFadeCurve::linear, rightOutGain1, rightOutGain2);
        }

        if (fc.bufferForMidiMessages != nullptr)
        {
            tempMidiBufferIn.mergeFromAndClear (*fc.bufferForMidiMessages);
            fc.bufferForMidiMessages->mergeFrom (tempMidiBufferOut);
        }
    }
}

RackType::Ptr RackType::findRackTypeContaining (const Plugin& plugin)
{
    for (auto p : getAllPlugins (plugin.edit, false))
        if (auto rack = dynamic_cast<RackInstance*> (p))
            if (rack->type != nullptr)
                if (rack->type->getPluginForID (plugin.itemID))
                    return rack->type;

    return {};
}

//==============================================================================
static StringArray stripAndPrependChannels (StringArray&& nms)
{
    StringArray names (nms);
    names.remove (0);
    int i = 0;

    for (auto&& n : names)
        n = String (++i) + ". " + n;

    return names;
}

StringArray RackType::getAudioInputNamesWithChannels() const
{
    return stripAndPrependChannels (getInputNames());
}

StringArray RackType::getAudioOutputNamesWithChannels() const
{
    return stripAndPrependChannels (getOutputNames());
}

//==============================================================================
Plugin* RackType::getPluginForID (EditItemID pluginID)
{
    if (pluginID.isInvalid())
        return {};

    for (auto p : pluginList->objects)
        if (p->plugin->itemID == pluginID)
            return p->plugin.get();

    return {};
}

void RackType::hideWindowForShutdown()
{
    for (auto af : getPlugins())
        if (auto ep = dynamic_cast<ExternalPlugin*> (af))
            ep->hideWindowForShutdown();

    for (auto ws : getWindowStates())
        ws->hideWindowForShutdown();
}

bool RackType::pasteClipboard()
{
    auto sm = edit.engine.getUIBehaviour().getSelectionManagerForRack (*this);

    if (sm == nullptr)
        return false;

    SelectableList pastedItems;

    if (auto plugins = Clipboard::getInstance()->getContentWithType<Clipboard::Plugins>())
    {
        for (auto& pluginState : plugins->plugins)
        {
            auto stateCopy = pluginState.createCopy();
            EditItemID::remapIDs (stateCopy, nullptr, edit);

            if (auto newPlugin = edit.getPluginCache().getOrCreatePluginFor (stateCopy))
            {
                auto selectedPlugin = sm->getFirstItemOfType<Plugin>();
                RackType::Ptr selectedRack (sm->getFirstItemOfType<RackType>());

                if (selectedPlugin != nullptr || selectedRack != nullptr)
                {
                    if (selectedRack == nullptr)
                        selectedRack = edit.getRackList().findRackContaining (*selectedPlugin);

                    if (selectedRack != nullptr)
                    {
                        Point<float> pos (0.1f, 0.1f);

                        if (selectedPlugin != nullptr && selectedRack.get() == this)
                            pos = getPluginPosition (*selectedPlugin).translated (0.1f, 0.1f);

                        for (auto af : getPlugins())
                            if (pos == getPluginPosition (*af))
                                pos = pos.translated (0.1f, 0.1f);

                        selectedRack->addPlugin (newPlugin, pos, false);
                        pastedItems.add (newPlugin.get());
                    }
                }
            }
        }
    }

    sm->select (pastedItems);

    return pastedItems.isNotEmpty();
}

//==============================================================================
ModifierList& RackType::getModifierList() const noexcept
{
    return *modifierList;
}

//==============================================================================
struct RackTypeList::ValueTreeList  : public ValueTreeObjectList<RackType>
{
    ValueTreeList (RackTypeList& l, const ValueTree& parent)
        : ValueTreeObjectList<RackType> (parent), list (l)
    {
        // NB: rebuildObjects() is called after construction so that the edit has a valid
        // list while they're being created
    }

    ~ValueTreeList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::RACK);
    }

    RackType* createNewObject (const ValueTree& v) override
    {
        auto t = new RackType (v, list.edit);
        t->incReferenceCount();
        return t;
    }

    void deleteObject (RackType* t) override
    {
        jassert (t != nullptr);
        t->decReferenceCount();
    }

    void newObjectAdded (RackType*) override                                { sendChange(); }
    void objectRemoved (RackType*) override                                 { sendChange(); }
    void objectOrderChanged() override                                      { sendChange(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { sendChange(); }

    void sendChange()
    {
        // XXX
    }

    RackTypeList& list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueTreeList)
};

//==============================================================================
RackTypeList::RackTypeList (Edit& ed) : edit (ed)
{
}

void RackTypeList::initialise (const ValueTree& v)
{
    CRASH_TRACER

    state = v;
    jassert (state.hasType (IDs::RACKS));

    list = new ValueTreeList (*this, v);
    list->rebuildObjects();
}

RackTypeList::~RackTypeList()
{
    for (auto t : list->objects)
        t->hideWindowForShutdown();

    list = nullptr;
}

const Array<RackType*>& RackTypeList::getTypes() const noexcept
{
    return list->objects;
}

int RackTypeList::size() const
{
    return list->objects.size();
}

RackType::Ptr RackTypeList::getRackType (int index) const
{
    return list->objects[index];
}

RackType::Ptr RackTypeList::getRackTypeForID (EditItemID rackID) const
{
    for (auto r : list->objects)
        if (r->rackID == rackID)
            return *r;

    return {};
}

RackType::Ptr RackTypeList::findRackContaining (Plugin& p) const
{
    for (auto r : list->objects)
        if (r->getPlugins().contains (&p))
            return *r;

    return {};
}

void RackTypeList::removeRackType (const RackType::Ptr& type)
{
    if (list->objects.contains (type.get()))
    {
        for (auto f : getAllPlugins (edit, false))
            if (auto rf = dynamic_cast<RackInstance*> (f))
                if (rf->type == type)
                    rf->removeFromParent();

        type->hideWindowForShutdown();
        state.removeChild (type->state, &edit.getUndoManager());
    }
}

RackType::Ptr RackTypeList::addNewRack()
{
    auto newID = edit.createNewItemID();

    ValueTree v (IDs::RACK);
    newID.writeID (v, nullptr);
    state.addChild (v, -1, &edit.getUndoManager());

    auto p = getRackTypeForID (newID);
    jassert (p != nullptr);
    return p;
}

RackType::Ptr RackTypeList::addRackTypeFrom (const ValueTree& rackType)
{
    if (! rackType.hasType (IDs::RACK))
        return {};

    auto typeID = EditItemID::fromID (rackType);

    if (typeID.isInvalid())
        return {};

    auto type = getRackTypeForID (typeID);

    if (type == nullptr)
    {
        if (rackType.isValid())
        {
            state.addChild (rackType.createCopy(), -1, &edit.getUndoManager());

            type = getRackTypeForID (typeID);
            jassert (type != nullptr);
        }
    }

    return type;
}

void RackTypeList::importRackFiles (const Array<File>& files)
{
    int oldNumRacks = size();

    for (auto& f : files)
    {
        ScopedPointer<XmlElement> xml (XmlDocument::parse (f));

        if (xml != nullptr)
            addRackTypeFrom (ValueTree::fromXml (*xml));
    }

    if (oldNumRacks < size())
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Rack types added!"));
}

//==============================================================================
void RackType::triggerUpdate()
{
    CRASH_TRACER

    if (edit.isLoading())
        return;

    latencyCalculation.reset();
    countInstancesInEdit();
    renderContextBuilder.triggerAsyncUpdate();
}

void RackType::updateRenderContext()
{
    if (edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD
}

//==============================================================================
void RackType::valueTreeChildAdded (ValueTree&, ValueTree& c)
{
    if (matchesAnyOf (c.getType(), { IDs::OUTPUT, IDs::INPUT }))
        updateTempBuferSizes();

    triggerUpdate();
}

void RackType::valueTreeChildRemoved (ValueTree&, ValueTree& c, int)
{
    if (matchesAnyOf (c.getType(), { IDs::OUTPUT, IDs::INPUT }))
        updateTempBuferSizes();

    triggerUpdate();
}

void RackType::valueTreeChildOrderChanged (ValueTree&, int, int)   {}
void RackType::valueTreeParentChanged (ValueTree&)                 { triggerUpdate(); }
void RackType::valueTreeRedirected (ValueTree&)                    { triggerUpdate(); }

void RackType::valueTreePropertyChanged (ValueTree& v, const Identifier& ident)
{
    if (v.hasType (IDs::PLUGININSTANCE) || v.hasType (IDs::CONNECTION))
        if (ident != IDs::x && ident != IDs::y && ident != IDs::windowLocked && ident != IDs::windowPos)
            triggerUpdate();

    if (v == state && ident == IDs::name)
    {
        rackName.forceUpdateOfCachedValue();

        for (auto af : getAllPlugins (edit, false))
            if (auto rf = dynamic_cast<RackInstance*> (af))
                if (rf->type.get() == this)
                    rf->changed();

        changed();
    }

    if (v == state && ident == IDs::id)
        jassertfalse;
}

//==============================================================================
RackType::WindowState::WindowState (RackType& r, juce::ValueTree windowStateTree)
    : PluginWindowState (r.edit.engine), rack (r), state (std::move (windowStateTree))  {}
