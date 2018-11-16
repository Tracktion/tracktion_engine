/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct ExternalPlugin::ProcessorChangedManager  : public juce::AudioProcessorListener
{
    ProcessorChangedManager (ExternalPlugin& p) : plugin (p)
    {
        if (auto pi = plugin.getAudioPluginInstance())
            pi->addListener (this);
        else
            jassertfalse;
    }

    ~ProcessorChangedManager()
    {
        if (auto pi = plugin.getAudioPluginInstance())
            pi->removeListener (this);
        else
            jassertfalse;
    }

    void audioProcessorParameterChanged (AudioProcessor*, int, float) override
    {
    }

    void audioProcessorChanged (AudioProcessor* ap) override
    {
        if (plugin.edit.isLoading())
            return;

        if (plugin.getLatencySamples() != ap->getLatencySamples())
            plugin.edit.getTransport().triggerClearDevicesOnStop();

        if (auto pi = plugin.getAudioPluginInstance())
            pi->refreshParameterList();
        else
            jassertfalse;

        plugin.changed();
        plugin.edit.pluginChanged (plugin);
    }

    ExternalPlugin& plugin;

private:
    JUCE_DECLARE_NON_COPYABLE (ProcessorChangedManager)
};

//==============================================================================
struct AsyncPluginDeleter  : private Timer,
                             private DeletedAtShutdown
{
    AsyncPluginDeleter() = default;

    ~AsyncPluginDeleter()
    {
        stopTimer();

        while (plugins.size() > 0)
            timerCallback();

        clearSingletonInstance();
    }

    JUCE_DECLARE_SINGLETON (AsyncPluginDeleter, false)

    void deletePlugin (AudioPluginInstance* p)
    {
        if (p != nullptr)
        {
            plugins.add (p);
            startTimer (100);
        }
    }

    bool releaseNextDanglingPlugin()
    {
        if (plugins.size() > 0)
        {
            timerCallback();
            return true;
        }

        return false;
    }

    void timerCallback() override
    {
        if (plugins.isEmpty())
        {
            stopTimer();
            return;
        }

        if (recursive)
            return;

        CRASH_TRACER_PLUGIN (plugins.getLast()->getName().toUTF8());

        const ScopedValueSetter<bool> setter (recursive, true, false);

        Component modal;
        modal.enterModalState (false);

        plugins.removeLast();
    }

private:
    OwnedArray<AudioPluginInstance> plugins;
    bool recursive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsyncPluginDeleter)
};

JUCE_IMPLEMENT_SINGLETON (AsyncPluginDeleter)

void cleanUpDanglingPlugins()
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    if (auto d = AsyncPluginDeleter::getInstanceWithoutCreating())
    {
        for (int count = 400; --count > 0 && d->releaseNextDanglingPlugin();)
        {
            Component modal;
            modal.enterModalState (false);

            MessageManager::getInstance()->runDispatchLoopUntil (10);
        }
    }
   #endif

    AsyncPluginDeleter::deleteInstance();
}

//==============================================================================
#if JUCE_PLUGINHOST_VST
struct ExtraVSTCallbacks  : public juce::VSTPluginFormat::ExtraFunctions
{
    ExtraVSTCallbacks (Edit& ed) : edit (ed) {}

    int64 getTempoAt (int64 samplePos) override
    {
        auto sampleRate = edit.engine.getDeviceManager().getSampleRate();
        return (int64) (10000.0 * edit.tempoSequence.getTempoAt (samplePos / sampleRate).getBpm());
    }

    // returns 0: not supported, 1: off, 2:read, 3:write, 4:read/write
    int getAutomationState() override
    {
        auto& am = edit.getAutomationRecordManager();

        bool r = am.isReadingAutomation();
        bool w = am.isWritingAutomation();

        return r ? (w ? 4 : 2) : (w ? 3 : 1);
    }

    Edit& edit;

    ExtraVSTCallbacks() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtraVSTCallbacks)
};
#endif // JUCE_PLUGINHOST_VST


//==============================================================================
/** specialised AutomatableParameter for wet/dry.
    Having a subclass just lets it label itself more nicely.
*/
struct PluginWetDryAutomatableParam  : public AutomatableParameter
{
    PluginWetDryAutomatableParam (const String& xmlTag, const String& name, ExternalPlugin& owner)
        : AutomatableParameter (xmlTag, name, owner, { 0.0f, 1.0f })
    {
    }

    ~PluginWetDryAutomatableParam()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override     { return Decibels::toString (Decibels::gainToDecibels (value), 1); }
    float stringToValue (const String& s) override  { return dbStringToDb (s); }

    PluginWetDryAutomatableParam() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWetDryAutomatableParam)
};

//==============================================================================
class ExternalPlugin::PluginPlayHead  : public juce::AudioPlayHead
{
public:
    PluginPlayHead (ExternalPlugin& p, PlayHead& ph)  : plugin (p), playhead (ph)
    {
        currentPos = new TempoSequencePosition (plugin.edit.tempoSequence);
        loopStart  = new TempoSequencePosition (plugin.edit.tempoSequence);
        loopEnd    = new TempoSequencePosition (plugin.edit.tempoSequence);
    }

    /** @warning Because some idiotic plugins call getCurrentPosition on the message thread, we can't keep a
                 reference to the context hanging around; it may well have dissapeared by the time these plugins
                 get around to calling it. The best we can do is keep the last info we got.
    */
    void setCurrentContext (const AudioRenderContext* rc)
    {
        const ScopedLock sl (infoLock);

        if (rc != nullptr)
        {
            time        = rc->getEditTime().editRange1.getStart();
            isPlaying   = rc->playhead.isPlaying();
        }
        else
        {
            time        = 0.0;
            isPlaying   = false;
        }
    }

    bool getCurrentPosition (CurrentPositionInfo& result) override
    {
        zerostruct (result);
        result.frameRate = getFrameRate();

        if (currentPos == nullptr)
            return false;

        double localTime = 0.0;

        {
            const ScopedLock sl (infoLock);
            result.isPlaying = isPlaying;
            localTime = time;
        }

        auto& transport = plugin.edit.getTransport();

        result.isRecording      = transport.isRecording();
        result.editOriginTime   = transport.getTimeWhenStarted();
        result.isLooping        = playhead.isLooping();

        if (result.isLooping)
        {
            loopStart->setTime (playhead.getLoopTimes().start);
            result.ppqLoopStart = loopStart->getPPQTime();

            loopEnd->setTime (playhead.getLoopTimes().end);
            result.ppqLoopEnd = loopEnd->getPPQTime();
        }

        result.timeInSamples    = (int64) (localTime * plugin.sampleRate);
        result.timeInSeconds    = localTime;

        currentPos->setTime (localTime);
        auto& tempo = currentPos->getCurrentTempo();
        result.bpm                  = tempo.bpm;
        result.timeSigNumerator     = tempo.numerator;
        result.timeSigDenominator   = tempo.denominator;

        result.ppqPositionOfLastBarStart = currentPos->getPPQTimeOfBarStart();
        result.ppqPosition = jmax (result.ppqPositionOfLastBarStart, currentPos->getPPQTime());

        return true;
    }

private:
    ExternalPlugin& plugin;
    PlayHead& playhead;
    ScopedPointer<TempoSequencePosition> currentPos, loopStart, loopEnd;
    CriticalSection infoLock;
    double time = 0;
    bool isPlaying = false;

    AudioPlayHead::FrameRateType getFrameRate() const
    {
        switch (plugin.edit.getTimecodeFormat().getFPS())
        {
            case 24:    return AudioPlayHead::fps24;
            case 25:    return AudioPlayHead::fps25;
            case 29:    return AudioPlayHead::fps30drop;
            case 30:    return AudioPlayHead::fps30;
            default:    break;
        }

        return AudioPlayHead::fps30; //Just to cope with it.
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginPlayHead)
};

//==============================================================================
namespace
{
    void readBusLayout (AudioProcessor::BusesLayout& busesLayout, const ValueTree& state, AudioProcessor& plugin, bool isInput)
    {
        jassert (state.hasType (IDs::LAYOUT));
        Array<AudioChannelSet>& targetBuses = (isInput ? busesLayout.inputBuses : busesLayout.outputBuses);
        int maxNumBuses = 0;

        auto buses = state.getChildWithName (isInput ? IDs::INPUTS : IDs::OUTPUTS);

        if (buses.isValid())
        {
            for (auto v : buses)
            {
                if (! v.hasType (IDs::BUS))
                    continue;

                const int busIdx = v[IDs::index];
                maxNumBuses = jmax (maxNumBuses, busIdx + 1);

                // The number of buses on busesLayout may not be in sync with the plugin after adding buses
                // because adding an input bus could also add an output bus
                for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                    if (! plugin.addBus (isInput))
                        break;

                for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                    targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

                const auto layout = v[IDs::layout].toString();

                if (layout.isNotEmpty())
                    targetBuses.getReference (busIdx) = AudioChannelSet::fromAbbreviatedString (layout);
            }
        }

        // If the plugin has more buses than specified in the xml, then try to remove them!
        while (maxNumBuses < targetBuses.size())
        {
            if (! plugin.removeBus (isInput))
                break;

            targetBuses.removeLast();
        }
    }

    AudioProcessor::BusesLayout readBusesLayout (const var& layout, AudioProcessor& plugin)
    {
        AudioProcessor::BusesLayout busesLayout;

        if (auto* mb = layout.getBinaryData())
        {
            auto v = ValueTree::readFromData (mb->getData(), mb->getSize());
            readBusLayout (busesLayout, v, plugin, true);
            readBusLayout (busesLayout, v, plugin, false);
        }

        return busesLayout;
    }

    ValueTree createBusLayout (const AudioProcessor::BusesLayout& layout, bool isInput)
    {
        auto& buses = (isInput ? layout.inputBuses : layout.outputBuses);

        ValueTree v (isInput ? IDs::INPUTS : IDs::OUTPUTS);

        for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
        {
            ValueTree bus (IDs::BUS);
            bus.setProperty (IDs::index, busIdx, nullptr);

            const AudioChannelSet& set = buses.getReference (busIdx);
            const String layoutName = set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString();
            bus.setProperty (IDs::layout, layoutName, nullptr);

            v.addChild (bus, -1, nullptr);
        }

        return v.getNumChildren() > 0 ? v : ValueTree();
    }

    ValueTree createBusesLayout (const AudioProcessor::BusesLayout& layout)
    {
        auto inputs = createBusLayout (layout, true);
        auto outputs = createBusLayout (layout, false);

        if (inputs.getNumChildren() == 0 && outputs.getNumChildren() == 0)
            return {};

        ValueTree v (IDs::LAYOUT);
        v.addChild (inputs, -1, nullptr);
        v.addChild (outputs, -1, nullptr);

        return v;
    }

    MemoryBlock createBusesLayoutProperty (const AudioProcessor::BusesLayout& layout)
    {
        MemoryBlock mb;

        auto v = createBusesLayout (layout);

        if (v.isValid())
        {
            MemoryOutputStream os (mb, false);
            v.writeToStream (os);
        }

        return mb;
    }

    void restoreChannelLayout (ExternalPlugin& plugin)
    {
        auto setDefaultChannelConfig = [] (AudioProcessor& ap, bool input)
        {
            // If the main bus is mono, set to stereo if supported
            const int n = ap.getBusCount (input);
            if (n >= 1)
            {
                auto bestSet = AudioChannelSet::disabled();

                if (auto bus = ap.getBus (input, 0))
                {
                    if (bus->getCurrentLayout().size() < 2)
                    {
                        for (int i = 0; i < AudioChannelSet::maxChannelsOfNamedLayout; i++)
                        {
                            AudioChannelSet set = bus->supportedLayoutWithChannels (i);
                            if (! set.isDisabled() && set.size() == 2)
                            {
                                bestSet = set;
                                break;
                            }
                        }

                        if (bestSet != AudioChannelSet::disabled())
                            bus->setCurrentLayout (bestSet);
                    }
                }
            }
        };

        if (auto ap = plugin.getAudioPluginInstance())
        {
            if (plugin.state.hasProperty (IDs::layout))
            {
                plugin.setBusesLayout (readBusesLayout (plugin.state.getProperty (IDs::layout), *ap));
            }
            else
            {
                // It appears that most AUs don't like to have their channel layout changed so just leave the default here as was the old behaviour
                if (plugin.isAU() || plugin.isVST3())
                    return;

                setDefaultChannelConfig (*ap, true);
                setDefaultChannelConfig (*ap, false);
            }
        }
    }

    void flushBusesLayoutToValueTree (ExternalPlugin& plugin, UndoManager* um)
    {
        // Save buses layout
        if (auto* ap = plugin.getAudioPluginInstance())
        {
            auto mb = createBusesLayoutProperty (ap->getBusesLayout());

            if (mb.getSize() > 0)
                plugin.state.setProperty (IDs::layout, mb, um);
            else
                plugin.state.removeProperty (IDs::layout, um);
        }
    }
}

//==============================================================================
ExternalPlugin::ExternalPlugin (PluginCreationInfo info)  : Plugin (info)
{
    CRASH_TRACER

    auto um = getUndoManager();

    addAutomatableParameter (dryGain = new PluginWetDryAutomatableParam ("dry level", TRANS("Dry Level"), *this));
    addAutomatableParameter (wetGain = new PluginWetDryAutomatableParam ("wet level", TRANS("Wet Level"), *this));

    dryValue.referTo (state, IDs::dry, um);
    wetValue.referTo (state, IDs::wet, um, 1.0f);

    dryGain->attachToCurrentValue (dryValue);
    wetGain->attachToCurrentValue (wetValue);

    desc.uid = (int) state[IDs::uid].toString().getHexValue64();
    desc.fileOrIdentifier = state[IDs::filename];
    setEnabled (state.getProperty (IDs::enabled, true));
    desc.name = state[IDs::name];
    desc.manufacturerName = state[IDs::manufacturer];
    identiferString = desc.createIdentifierString();

    initialiseFully();
}

ValueTree ExternalPlugin::create (Engine& e, const PluginDescription& desc)
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);

    v.setProperty (IDs::uid, String::toHexString (desc.uid), nullptr);
    v.setProperty (IDs::filename, desc.fileOrIdentifier, nullptr);
    v.setProperty (IDs::name, desc.name, nullptr);
    v.setProperty (IDs::manufacturer, desc.manufacturerName, nullptr);

    if (e.getPluginManager().areGUIsLockedByDefault())
        v.setProperty (IDs::windowLocked, true, nullptr);

    return v;
}

const char* ExternalPlugin::xmlTypeName = "vst";

void ExternalPlugin::initialiseFully()
{
    if (! fullyInitialised)
    {
        CRASH_TRACER_PLUGIN (getDebugName());
        fullyInitialised = true;

        doFullInitialisation();
        restorePluginStateFromValueTree (state);
        buildParameterList();
        restoreChannelLayout (*this);
    }
}

void ExternalPlugin::updateDebugName()
{
    debugName = desc.name + " (" + desc.pluginFormatName + ")";
}

void ExternalPlugin::buildParameterList()
{
    CRASH_TRACER_PLUGIN (getDebugName());
    autoParamForParamNumbers.clear();
    std::unordered_map<std::string, int> alreadyUsedParamNames;

    if (pluginInstance)
    {
        auto& parameters = pluginInstance->getParameters();
        jassert (parameters.size() < 80000);
        const int maxAutoParams = jmin (80000, parameters.size());

        for (int i = 0; i < maxAutoParams; ++i)
        {
            auto* parameter = parameters.getUnchecked (i);

            if (parameter->isAutomatable())
            {
                String nm (parameter->getName (1024));

                if (nm.isNotEmpty())
                {
                    int count = 1;

                    if (alreadyUsedParamNames.find (nm.toStdString()) != alreadyUsedParamNames.end())
                    {
                        count = alreadyUsedParamNames[nm.toStdString()] + 1;
                        alreadyUsedParamNames[nm.toStdString()] = count;
                        nm << " (" << count << ")";
                    }
                    else
                    {
                        alreadyUsedParamNames[nm.toStdString()] = count;
                    }

                    // Just use the index for the ID for now until this has been added to JUCE
                    auto parameterID = String (i);

                    if (auto paramWithID = dynamic_cast<AudioProcessorParameterWithID*> (parameter))
                        parameterID = paramWithID->paramID;

                    auto p = new ExternalAutomatableParameter (parameterID, nm, *this, i, { 0.0f, 1.0f });
                    addAutomatableParameter (*p);
                    autoParamForParamNumbers.add (p);
                    p->valueChangedByPlugin();

                    if (count >= 2)
                        p->setDisplayName (nm);
                }
                else
                {
                    autoParamForParamNumbers.add (nullptr);
                }
            }
            else
            {
                autoParamForParamNumbers.add (nullptr);
            }
        }
    }

    restoreChangedParametersFromState();
    buildParameterTree();
}

const PluginDescription* ExternalPlugin::findDescForUID (int uid) const
{
    if (uid != 0)
        for (auto d : engine.getPluginManager().knownPluginList)
            if (d->uid == uid)
                return d;

    return {};
}

const PluginDescription* ExternalPlugin::findDescForFileOrID (const String& fileOrID) const
{
    if (fileOrID.isNotEmpty())
    {
        auto& pm = engine.getPluginManager();

        for (auto d : pm.knownPluginList)
            if (d->fileOrIdentifier == fileOrID)
                return d;

        return engine.getEngineBehaviour().findDescriptionForFileOrID (fileOrID);
    }

    return {};
}

static const PluginDescription* findDescForName (Engine& engine, const String& name)
{
    if (name.isEmpty())
        return {};

    auto& pm = engine.getPluginManager();

    auto findName = [&pm] (const String& nameToFind) -> const PluginDescription*
    {
        for (auto* d : pm.knownPluginList)
            if (d->name == nameToFind)
                return d;

        return {};
    };

    if (auto* p = findName (name))
        return p;

   #if JUCE_64BIT
    if (auto* p = findName (name + " (64 bit)"))
        return p;

    if (auto* p = findName (name + " (64-bit)"))
        return p;
   #endif

    return {};
}

const PluginDescription* ExternalPlugin::findMatchingPlugin() const
{
    CRASH_TRACER
    auto& pm = engine.getPluginManager();

    if (auto* p = pm.knownPluginList.getTypeForIdentifierString (desc.createIdentifierString()))
        return p;

    if (desc.pluginFormatName.isEmpty())
    {
        if (auto* p = pm.knownPluginList.getTypeForIdentifierString ("VST" + desc.createIdentifierString()))
            return p;

        if (auto* p = pm.knownPluginList.getTypeForIdentifierString ("AudioUnit" + desc.createIdentifierString()))
            return p;
    }

    if (auto* p = findDescForFileOrID (desc.fileOrIdentifier))
        return p;

    if (auto* p = findDescForUID (desc.uid))
        return p;

    if (auto* p = findDescForName (engine, desc.name))
        return p;

    for (auto* d : pm.knownPluginList)
        if (d->name == desc.name)
            return d;

    for (auto* d : pm.knownPluginList)
        if (File::createFileWithoutCheckingPath (d->fileOrIdentifier).getFileNameWithoutExtension() == desc.name)
            return d;

    if (desc.uid == 0x4d44416a) // old JX-10: hack to update to JX-16
        if (auto* p = findDescForUID (0x4D44414A))
            return p;

    return {};
}

//==============================================================================
void ExternalPlugin::processingChanged()
{
    Plugin::processingChanged();

    if (processing)
    {
        if (! pluginInstance)
        {
            fullyInitialised = false;
            initialiseFully();
        }
    }
    else
    {
        // Remove all the parameters except the wet/dry as these are created in the constructor
        for (int i = autoParamForParamNumbers.size(); --i >= 0;)
            deleteParameter (autoParamForParamNumbers.getUnchecked (i));

        autoParamForParamNumbers.clear();
        getParameterTree().clear();

        deletePluginInstance();
    }
}

void ExternalPlugin::doFullInitialisation()
{
    if (processing && pluginInstance == nullptr && edit.shouldLoadPlugins())
    {
        if (auto* foundDesc = findMatchingPlugin())
        {
            desc = *foundDesc;
            identiferString = desc.createIdentifierString();
            updateDebugName();

            if (isDisabled())
                return;

            CRASH_TRACER_PLUGIN (getDebugName());
            String error;

            callBlocking ([this, &error, foundDesc]
            {
                CRASH_TRACER_PLUGIN (getDebugName());
                error = createPluginInstance (*foundDesc);
            });

            if (pluginInstance)
            {
               #if JUCE_PLUGINHOST_VST
                if (auto xml = juce::VSTPluginFormat::getVSTXML (pluginInstance.get()))
                    vstXML.reset (VSTXML::createFor (*xml));

                juce::VSTPluginFormat::setExtraFunctions (pluginInstance.get(), new ExtraVSTCallbacks (edit));
               #endif

                pluginInstance->setPlayHead (playhead);
                supportsMPE = pluginInstance->supportsMPE();
            }
            else
            {
                TRACKTION_LOG_ERROR (error);
            }
        }
    }
}

//==============================================================================
ExternalPlugin::~ExternalPlugin()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER_PLUGIN (getDebugName());
    notifyListenersOfDeletion();
    windowState->hideWindowForShutdown();
    deinitialise();

    dryGain->detachFromCurrentValue();
    wetGain->detachFromCurrentValue();

    const ScopedLock sl (lock);
    deletePluginInstance();
}

void ExternalPlugin::selectableAboutToBeDeleted()
{
    for (auto param : autoParamForParamNumbers)
        if (param != nullptr)
            param->unregisterAsListener();

    Plugin::selectableAboutToBeDeleted();
}

//==============================================================================
void ExternalPlugin::flushPluginStateToValueTree()
{
    Plugin::flushPluginStateToValueTree();

    auto* um = getUndoManager();

    if (desc.fileOrIdentifier.isNotEmpty())
    {
        state.setProperty ("manufacturer", desc.manufacturerName, um);
        state.setProperty (IDs::name, desc.name, um);
        itemID.writeID (state, um);
        state.setProperty (IDs::uid, getPluginUID(), um);
        state.setProperty (IDs::filename, desc.fileOrIdentifier, um);
    }

    if (pluginInstance)
    {
        state.setProperty (IDs::programNum, pluginInstance->getCurrentProgram(), um);

        TRACKTION_ASSERT_MESSAGE_THREAD
        MemoryBlock chunk;

        pluginInstance->suspendProcessing (true);
        pluginInstance->getStateInformation (chunk);
        saveChangedParametersToState();
        pluginInstance->suspendProcessing (false);

        engine.getEngineBehaviour().saveCustomPluginProperties (state, *pluginInstance, um);

        if (chunk.getSize() > 0)
            state.setProperty (IDs::state, chunk.toBase64Encoding(), um);
        else
            state.removeProperty (IDs::state, um);

        flushBusesLayoutToValueTree (*this, um);
    }
}

void ExternalPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    String s;

    if (v.hasProperty (IDs::state))
    {
        s = v.getProperty (IDs::state).toString();
    }
    else
    {
        auto vstDataTree = v.getChildWithName (IDs::VSTDATA);

        if (vstDataTree.isValid())
        {
            s = vstDataTree.getProperty (IDs::DATA).toString();

            if (s.isEmpty())
                s = vstDataTree.getProperty (IDs::__TEXT).toString();
        }
    }

    if (pluginInstance && s.isNotEmpty())
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        if (getNumPrograms() > 1)
            setCurrentProgram (v.getProperty (IDs::programNum), false);

        MemoryBlock chunk;
        chunk.fromBase64Encoding (s);

        if (chunk.getSize() > 0)
            callBlocking ([this, &chunk]() { pluginInstance->setStateInformation (chunk.getData(), (int) chunk.getSize()); });
    }
}

void ExternalPlugin::getPluginStateFromTree (MemoryBlock& mb)
{
    auto s = state.getProperty (IDs::state).toString();
    mb.reset();

    if (s.isNotEmpty())
        mb.fromBase64Encoding (s);
}

void ExternalPlugin::updateFromMirroredPluginIfNeeded (Plugin& changedPlugin)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (changedPlugin.itemID == masterPluginID)
    {
        if (auto other = dynamic_cast<ExternalPlugin*> (&changedPlugin))
        {
            if (other->pluginInstance != nullptr && other->desc.isDuplicateOf (desc))
            {
                MemoryBlock chunk;
                other->pluginInstance->getStateInformation (chunk);

                if (chunk.getSize() > 0)
                    pluginInstance->setStateInformation (chunk.getData(), (int) chunk.getSize());
            }
        }
    }
}

//==============================================================================
struct ExternalPlugin::MPEChannelRemapper
{
    static constexpr int lowChannel = 2, highChannel = 16;

    void reset() noexcept
    {
        for (auto& s : sourceAndChannel)
            s = MidiMessageArray::notMPE;
    }

    void clearChannel (int channel) noexcept
    {
        sourceAndChannel[channel] = MidiMessageArray::notMPE;
    }

    void remapMidiChannelIfNeeded (MidiMessageArray::MidiMessageWithSource& m) noexcept
    {
        auto channel = m.getChannel();

        if (channel < lowChannel || channel > highChannel)
            return;

        auto sourceAndChannelID = (((uint32) m.mpeSourceID << 5) | (uint32) (channel - 1));

        if ((*m.getRawData() & 0xf0) != 0xf0)
        {
            ++counter;

            if (applyRemapIfExisting (channel, sourceAndChannelID, m))
                return;

            for (int chan = lowChannel; chan <= highChannel; ++chan)
                if (applyRemapIfExisting (chan, sourceAndChannelID, m))
                    return;

            if (sourceAndChannel[channel] == MidiMessageArray::notMPE)
            {
                lastUsed[channel] = counter;
                sourceAndChannel[channel] = sourceAndChannelID;
                return;
            }

            auto chan = getBestChanToReuse();
            sourceAndChannel[chan] = sourceAndChannelID;
            lastUsed[chan] = counter;
            m.setChannel (chan);
        }
    }

    uint32 sourceAndChannel[17] = {};
    uint32 lastUsed[17] = {};
    uint32 counter = 0;

    int getBestChanToReuse() const noexcept
    {
        for (int chan = lowChannel; chan <= highChannel; ++chan)
            if (sourceAndChannel[chan] == MidiMessageArray::notMPE)
                return chan;

        auto bestChan = lowChannel;
        auto bestLastUse = counter;

        for (int chan = lowChannel; chan <= highChannel; ++chan)
        {
            if (lastUsed[chan] < bestLastUse)
            {
                bestLastUse = lastUsed[chan];
                bestChan = chan;
            }
        }

        return bestChan;
    }

    bool applyRemapIfExisting (int channel, uint32 sourceAndChannelID, MidiMessageArray::MidiMessageWithSource& m) noexcept
    {
        if (sourceAndChannel[channel] == sourceAndChannelID)
        {
            if (m.isNoteOff() || m.isAllNotesOff() || m.isResetAllControllers())
                sourceAndChannel[channel] = MidiMessageArray::notMPE;
            else
                lastUsed[channel] = counter;

            m.setChannel (channel);
            return true;
        }

        return false;
    }
};

//==============================================================================
void ExternalPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    CRASH_TRACER_PLUGIN (getDebugName());

    const ScopedLock sl (lock);

    if (mpeRemapper == nullptr)
        mpeRemapper = new MPEChannelRemapper();

    mpeRemapper->reset();

    if (pluginInstance)
    {
        pluginInstance->releaseResources();
        pluginInstance->prepareToPlay (info.sampleRate, info.blockSizeSamples);
        latencySamples = pluginInstance->getLatencySamples();
        latencySeconds = latencySamples / info.sampleRate;

        if (! desc.hasSharedContainer)
        {
            desc = pluginInstance->getPluginDescription();
            updateDebugName();
        }

        pluginInstance->setPlayHead (nullptr);
        playhead = new PluginPlayHead (*this, info.playhead);
        pluginInstance->setPlayHead (playhead);
    }
    else
    {
        playhead = nullptr;
        latencySamples = 0;
        latencySeconds = 0.0;
    }
}

void ExternalPlugin::deinitialise()
{
    if (pluginInstance)
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        const ScopedLock sl (lock);
        pluginInstance->setPlayHead (nullptr); // must be done first!

        if (playhead != nullptr)
            playhead->setCurrentContext (nullptr);

        pluginInstance->releaseResources();
    }
}

void ExternalPlugin::reset()
{
    if (pluginInstance)
    {
        CRASH_TRACER_PLUGIN (getDebugName());
        const ScopedLock sl (lock);
        pluginInstance->reset();
    }
}

void ExternalPlugin::setEnabled (bool shouldEnable)
{
    Plugin::setEnabled (shouldEnable);

    if (shouldEnable != isEnabled())
    {
        if (pluginInstance)
            pluginInstance->reset();

        propertiesChanged();
    }
}

//==============================================================================
void ExternalPlugin::prepareIncomingMidiMessages (MidiMessageArray& incoming, int numSamples, bool isPlaying)
{
    if (incoming.isAllNotesOff)
    {
        uint32 eventsSentOnChannel = 0;

        activeNotes.iterate ([&eventsSentOnChannel, this, isPlaying] (int chan, int noteNumber)
        {
            midiBuffer.addEvent (MidiMessage::noteOff (chan, noteNumber), 0);

            if ((eventsSentOnChannel & (1 << chan)) == 0)
            {
                eventsSentOnChannel |= (1 << chan);

                if (! supportsMPE)
                {
                    midiBuffer.addEvent (MidiMessage::controllerEvent (chan, 66 /* sustain pedal off */, 0), 0);
                    midiBuffer.addEvent (MidiMessage::controllerEvent (chan, 64 /* hold pedal off */, 0), 0);
                }
                else
                {
                    // MPE standard (and JUCE implementation) now requires this instead of allNotesOff.
                    midiBuffer.addEvent (MidiMessage::allControllersOff (1), 0);
                    midiBuffer.addEvent (MidiMessage::allControllersOff (16), 0);
                }

                // NB: Some buggy plugins seem to fail to respond to note-ons if they are preceded
                // by an all-notes-off, so avoid this if just dragging the cursor around while playing.
                if (! isPlaying)
                    midiBuffer.addEvent (MidiMessage::allNotesOff (chan), 0);
            }
        });

        activeNotes.reset();
        mpeRemapper->reset();

        // Reset MPE zone to match MIDI generated by clip
        if (supportsMPE)
        {
            MPEZoneLayout layout;
            layout.setLowerZone (15);

            auto layoutBuffer = MPEMessages::setZoneLayout (layout);
            MidiBuffer::Iterator iter (layoutBuffer);
            MidiMessage result;

            for (int samplePosition = 0; iter.getNextEvent (result, samplePosition);)
                midiBuffer.addEvent (result, samplePosition);
        }
    }

    for (auto& m : incoming)
    {
        if (supportsMPE)
            mpeRemapper->remapMidiChannelIfNeeded (m);

        auto sample = jlimit (0, numSamples - 1, (int) (m.getTimeStamp() * sampleRate));
        midiBuffer.addEvent (m, sample);

        if (m.isNoteOn())
            activeNotes.startNote (m.getChannel(), m.getNoteNumber());
        else if (m.isNoteOff())
            activeNotes.clearNote (m.getChannel(), m.getNoteNumber());
    }

   #if 0
    if (! incoming.isEmpty())
    {
        const uint8* midiData;
        int numBytes, midiEventPos;

        DBG ("----------");

        for (MidiBuffer::Iterator iter (midiBuffer); iter.getNextEvent (midiData, numBytes, midiEventPos);)
            DBG (String::toHexString (midiData, numBytes) << "   " << midiEventPos);
    }
   #endif

    incoming.clear();
}

void ExternalPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (pluginInstance && isEnabled())
    {
        CRASH_TRACER_PLUGIN (getDebugName());
        const ScopedLock sl (lock);

        if (playhead != nullptr)
            playhead->setCurrentContext (&fc);

        midiBuffer.clear();

        if (fc.bufferForMidiMessages != nullptr)
            prepareIncomingMidiMessages (*fc.bufferForMidiMessages, fc.bufferNumSamples, fc.playhead.isPlaying());

        if (fc.destBuffer != nullptr)
        {
            auto destNumChans = fc.destBuffer->getNumChannels();
            jassert (destNumChans > 0);
            jassert (fc.bufferStartSample == 0);

            auto numInputChannels = pluginInstance->getTotalNumInputChannels();
            auto numOutputChannels = pluginInstance->getTotalNumOutputChannels();
            auto numChansToProcess = jmax (1, numInputChannels, numOutputChannels);

            if (destNumChans == numChansToProcess)
            {
                processPluginBlock (fc);
            }
            else
            {
                AudioScratchBuffer asb (numChansToProcess, fc.bufferNumSamples);
                auto& buffer = asb.buffer;

                // Copy or existing channel or clear data
                for (int i = 0; i < numChansToProcess; ++i)
                {
                    if (i < destNumChans)
                        buffer.copyFrom (i, 0, *fc.destBuffer, i, fc.bufferStartSample, fc.bufferNumSamples);
                    else
                        buffer.clear (i, 0, fc.bufferNumSamples);
                }

                if (destNumChans == 1 && numInputChannels == 2)
                {
                    // If we're getting a mono in and need stereo, dupe the channel..
                    buffer.copyFrom (1, 0, buffer, 0, 0, fc.bufferNumSamples);
                }
                else if (destNumChans == 2 && numInputChannels == 1)
                {
                    // If we're getting a stereo in and need mono, average the input..
                    buffer.addFrom (0, 0, *fc.destBuffer, 1, fc.bufferStartSample, fc.bufferNumSamples);
                    buffer.applyGain (0, 0, fc.bufferNumSamples, 0.5f);
                }

                AudioRenderContext fc2 (fc);
                fc2.destBuffer = &asb.buffer;
                fc2.bufferStartSample = 0;

                processPluginBlock (fc2);

                // Copy sample data back clearing unprocessed channels
                for (int i = 0; i < destNumChans; ++i)
                {
                    if (i < numChansToProcess)
                        fc.destBuffer->copyFrom (i, fc.bufferStartSample, buffer, i, 0, fc.bufferNumSamples);
                    else if (i < 2 && numChansToProcess > 0) // convert mono output to stereo for next plugin
                        fc.destBuffer->copyFrom (i, fc.bufferStartSample, buffer, 0, 0, fc.bufferNumSamples);
                    else
                        fc.destBuffer->clear (i, fc.bufferStartSample, fc.bufferNumSamples);
                }
            }
        }
        else
        {
            AudioScratchBuffer asb (jmax (pluginInstance->getTotalNumInputChannels(),
                                          pluginInstance->getTotalNumOutputChannels()), fc.bufferNumSamples);
            pluginInstance->processBlock (asb.buffer, midiBuffer);
        }

        if (fc.bufferForMidiMessages != nullptr)
        {
            fc.bufferForMidiMessages->clear();

            if (! midiBuffer.isEmpty())
            {
                MidiBuffer::Iterator iter (midiBuffer);

                const uint8* midiData;
                int numBytes, midiEventPos;

                while (iter.getNextEvent (midiData, numBytes, midiEventPos))
                    fc.bufferForMidiMessages->addMidiMessage (MidiMessage (midiData, numBytes, fc.midiBufferOffset + midiEventPos / sampleRate),
                                                              midiSourceID);
            }
        }
    }
}

void ExternalPlugin::processPluginBlock (const AudioRenderContext& fc)
{
    juce::AudioBuffer<float> asb (fc.destBuffer->getArrayOfWritePointers(), fc.destBuffer->getNumChannels(),
                                  fc.bufferStartSample, fc.bufferNumSamples);

    auto dry = dryGain->getCurrentValue();
    auto wet = wetGain->getCurrentValue();

    if (dry <= 0.00004f)
    {
        pluginInstance->processBlock (asb, midiBuffer);
        zeroDenormalisedValuesIfNeeded (asb);

        if (wet < 0.999f)
            asb.applyGain (0, fc.bufferNumSamples, wet);
    }
    else
    {
        auto numChans = asb.getNumChannels();
        AudioScratchBuffer dryAudio (numChans, fc.bufferNumSamples);

        for (int i = 0; i < numChans; ++i)
            dryAudio.buffer.copyFrom (i, 0, asb, i, 0, fc.bufferNumSamples);

        pluginInstance->processBlock (asb, midiBuffer);
        zeroDenormalisedValuesIfNeeded (asb);

        if (wet < 0.999f)
            asb.applyGain (0, fc.bufferNumSamples, wet);

        for (int i = 0; i < numChans; ++i)
            asb.addFrom (i, 0, dryAudio.buffer.getReadPointer (i), fc.bufferNumSamples, dry);
    }
}

//==============================================================================
int ExternalPlugin::getNumOutputChannelsGivenInputs (int)
{
    return jmax (1, getNumOutputs());
}

void ExternalPlugin::getChannelNames (StringArray* ins, StringArray* outs)
{
    if (pluginInstance)
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        auto getChannelName = [](AudioProcessor::Bus* bus, int index)
        {
            return bus != nullptr ? AudioChannelSet::getChannelTypeName (bus->getCurrentLayout().getTypeOfChannel (index)) : String();
        };

        if (ins != nullptr)
        {
            const int num = pluginInstance->getTotalNumInputChannels();

            for (int i = 0; i < num; ++i)
            {
                const String name (getChannelName (pluginInstance->getBus (true, 0), i));
                ins->add (name.isNotEmpty() ? name : TRANS("Unnamed"));
            }
        }

        if (outs != nullptr)
        {
            const int num = pluginInstance->getTotalNumOutputChannels();

            for (int i = 0; i < num; ++i)
            {
                const String name (getChannelName (pluginInstance->getBus (false, 0), i));
                outs->add (name.isNotEmpty() ? name : TRANS("Unnamed"));
            }
        }
    }
}

bool ExternalPlugin::noTail()
{
    CRASH_TRACER_PLUGIN (getDebugName());
    return pluginInstance == nullptr || pluginInstance->getTailLengthSeconds() <= 0.0;
}

double ExternalPlugin::getTailLength() const
{
    CRASH_TRACER_PLUGIN (getDebugName());
    return pluginInstance ? pluginInstance->getTailLengthSeconds() : 0.0;
}

//==============================================================================
File ExternalPlugin::getFile() const
{
    const File f (File::createFileWithoutCheckingPath (desc.fileOrIdentifier));

    if (f.exists())
        return f;

    return {};
}

String ExternalPlugin::getSelectableDescription()
{
    if (desc.pluginFormatName.isNotEmpty())
        return getName() + " (" + desc.pluginFormatName + " " + TRANS("Plugin") + ")";

    return getName();
}

//==============================================================================
int ExternalPlugin::getNumPrograms() const
{
    return pluginInstance ? pluginInstance->getNumPrograms() : 0;
}

int ExternalPlugin::getCurrentProgram() const
{
    return pluginInstance ? pluginInstance->getCurrentProgram() : 0;
}

String ExternalPlugin::getProgramName (int index)
{
    if (index == getCurrentProgram())
        return getCurrentProgramName();

    if (pluginInstance)
        return pluginInstance->getProgramName (index);

    return {};
}

bool ExternalPlugin::hasNameForMidiProgram (int programNum, int bank, String& name)
{
    if (takesMidiInput() && isSynth() && getNumPrograms() > 0)
    {
        programNum += (bank * 128);

        if (programNum >= 0 && programNum < getNumPrograms())
            name = getProgramName (programNum);
        else
            name = TRANS("Unnamed");

        return true;
    }

    return false;
}

String ExternalPlugin::getNumberedProgramName (int i)
{
    String s (getProgramName(i));
    if (s.isEmpty())
        s = "(" + TRANS("Unnamed") + ")";

    return String (i + 1) + " - " + s;
}

String ExternalPlugin::getCurrentProgramName()
{
    return pluginInstance ? pluginInstance->getProgramName (pluginInstance->getCurrentProgram()) : String();
}

void ExternalPlugin::setCurrentProgramName (const String& name)
{
    CRASH_TRACER_PLUGIN (getDebugName());

    if (pluginInstance)
        pluginInstance->changeProgramName (pluginInstance->getCurrentProgram(), name);
}

void ExternalPlugin::setCurrentProgram (int index, bool sendChangeMessage)
{
    if (pluginInstance && getNumPrograms() > 0)
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        index = jlimit (0, getNumPrograms() - 1, index);

        if (index != getCurrentProgram())
        {
            pluginInstance->setCurrentProgram (index);
            state.setProperty (IDs::programNum, index, nullptr);

            if (sendChangeMessage)
            {
                changed();

                for (auto p : autoParamForParamNumbers)
                    if (p != nullptr)
                        p->valueChangedByPlugin();
            }
        }
    }
}

bool ExternalPlugin::takesMidiInput()
{
    return pluginInstance && pluginInstance->acceptsMidi();
}

bool ExternalPlugin::isMissing()
{
    return ! isDisabled() && pluginInstance == nullptr;
}

bool ExternalPlugin::isDisabled()
{
    return engine.getEngineBehaviour().isPluginDisabled (identiferString);
}

int ExternalPlugin::getNumInputs() const     { return pluginInstance ? pluginInstance->getTotalNumInputChannels() : 0; }
int ExternalPlugin::getNumOutputs() const    { return pluginInstance ? pluginInstance->getTotalNumOutputChannels() : 0; }

bool ExternalPlugin::setBusesLayout (juce::AudioProcessor::BusesLayout layout)
{
    if (pluginInstance)
    {
        std::unique_ptr<Edit::ScopedRenderStatus> srs;

        if (! baseClassNeedsInitialising())
            srs = std::make_unique<Edit::ScopedRenderStatus> (edit);

        jassert (baseClassNeedsInitialising());

        if (pluginInstance->setBusesLayout (layout))
        {
            if (! edit.isLoading())
            {
                if (auto r = getOwnerRackType())
                    r->checkConnections();

                const ScopedValueSetter<bool> svs (isFlushingLayoutToState, true);
                flushBusesLayoutToValueTree (*this, getUndoManager());
            }

            return true;
        }
    }

    return false;
}

bool ExternalPlugin::setBusLayout (AudioChannelSet set, bool isInput, int busIndex)
{
    if (pluginInstance)
    {
        if (auto* bus = pluginInstance->getBus (isInput, busIndex))
        {
            std::unique_ptr<Edit::ScopedRenderStatus> srs;

            if (! baseClassNeedsInitialising())
                srs = std::make_unique<Edit::ScopedRenderStatus> (edit);

            jassert (baseClassNeedsInitialising());

            if (bus->setCurrentLayout (set))
            {
                if (! edit.isLoading())
                {
                    if (auto r = getOwnerRackType())
                        r->checkConnections();

                    const ScopedValueSetter<bool> svs (isFlushingLayoutToState, true);
                    flushBusesLayoutToValueTree (*this, getUndoManager());
                }

                return true;
            }
        }
    }

    return false;
}

//==============================================================================
String ExternalPlugin::createPluginInstance (const PluginDescription& description)
{
    jassert (! pluginInstance); // This should have already been deleted!

    auto& dm = engine.getDeviceManager();

    String error;
    pluginInstance.reset (engine.getPluginManager().pluginFormatManager
                            .createPluginInstance (description, dm.getSampleRate(), dm.getBlockSize(), error));

    if (pluginInstance != nullptr)
        processorChangedManager = std::make_unique<ProcessorChangedManager> (*this);

    return error;
}

void ExternalPlugin::deletePluginInstance()
{
    processorChangedManager.reset();
    AsyncPluginDeleter::getInstance()->deletePlugin (pluginInstance.release());
}

//==============================================================================
void ExternalPlugin::buildParameterTree() const
{
    auto& paramTree = getParameterTree();

    if (paramTree.rootNode->subNodes.size() > 0)
        return;

    CRASH_TRACER_PLUGIN (getDebugName());
    paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (getAutomatableParameter (0)));
    paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (getAutomatableParameter (1)));

    SortedSet<int> paramsInTree;

    if (vstXML)
    {
        for (int i = 0; i < vstXML->paramTree.size(); ++i)
        {
            if (auto param = dynamic_cast<const VSTXML::Param*> (vstXML->paramTree[i]))
            {
                paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (autoParamForParamNumbers [param->paramID]));
                paramsInTree.add (param->paramID);
            }

            if (auto group = dynamic_cast<const VSTXML::Group*> (vstXML->paramTree[i]))
            {
                auto treeNode = new AutomatableParameterTree::TreeNode (group->name);
                paramTree.rootNode->addSubNode (treeNode);
                buildParameterTree (group, treeNode, paramsInTree);
            }
        }
    }

    for (int i = 0; i < getNumAutomatableParameters(); ++i)
    {
        if (auto vstParam = dynamic_cast<ExternalAutomatableParameter*> (getAutomatableParameter(i).get()))
            if (! paramsInTree.contains (vstParam->getParameterIndex()))
                paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (autoParamForParamNumbers [vstParam->getParameterIndex()]));
    }
}

void ExternalPlugin::buildParameterTree (const VSTXML::Group* group,
                                         AutomatableParameterTree::TreeNode* treeNode,
                                         SortedSet<int>& paramsInTree) const
{
    for (int i = 0; i < group->paramTree.size(); ++i)
    {
        if (auto param = dynamic_cast<const VSTXML::Param*> (group->paramTree[i]))
        {
            treeNode->addSubNode (new AutomatableParameterTree::TreeNode (autoParamForParamNumbers [param->paramID]));
            paramsInTree.add (param->paramID);
        }

        if (auto subGroup = dynamic_cast<const VSTXML::Group*> (group->paramTree[i]))
        {
            auto subTreeNode = new AutomatableParameterTree::TreeNode (subGroup->name);
            treeNode->addSubNode (subTreeNode);
            buildParameterTree (subGroup, subTreeNode, paramsInTree);
        }
    }
}

void ExternalPlugin::deleteFromParent()
{
    CRASH_TRACER_PLUGIN (getDebugName());
    hideWindowForShutdown();
    Plugin::deleteFromParent();
}

AudioPluginInstance* ExternalPlugin::getAudioPluginInstance() const
{
    return pluginInstance.get();
}

void ExternalPlugin::valueTreePropertyChanged (ValueTree& v, const Identifier& id)
{
    if (v == state && id == IDs::layout)
    {
        if (isFlushingLayoutToState)
            return;

        if (auto* ap = getAudioPluginInstance())
        {
            auto stateLayout = readBusesLayout (v.getProperty(IDs::layout), *ap);

            if (stateLayout != ap->getBusesLayout())
                setBusesLayout (stateLayout);
        }
    }
    else
    {
        Plugin::valueTreePropertyChanged (v, id);
    }
}
