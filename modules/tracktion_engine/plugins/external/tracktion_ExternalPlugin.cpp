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

static juce::String getDeprecatedPluginDescSuffix (const juce::PluginDescription& d)
{
    return "-" + juce::String::toHexString (d.fileOrIdentifier.hashCode())
         + "-" + juce::String::toHexString (d.deprecatedUid);
}

juce::String createIdentifierString (const juce::PluginDescription& d)
{
    return d.pluginFormatName + "-" + d.name + getDeprecatedPluginDescSuffix (d);
}

class ExternalPlugin::ProcessorChangedManager   : public juce::AudioProcessorListener,
                                                  private juce::AsyncUpdater
{
public:
    ProcessorChangedManager (ExternalPlugin& p, juce::AudioPluginInstance& i)
        : plugin (p), instance (i)
    {
        instance.addListener (this);
    }

    ~ProcessorChangedManager() override
    {
        cancelPendingUpdate();

        instance.removeListener (this);
    }

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override
    {
        if (plugin.edit.isLoading())
            return;

        paramChanged = true;
        triggerAsyncUpdate();
    }

    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override
    {
        if (plugin.edit.isLoading())
            return;

        processorChanged = true;
        triggerAsyncUpdate();
    }

private:
    ExternalPlugin& plugin;
    juce::AudioPluginInstance& instance;
    JUCE_DECLARE_NON_COPYABLE (ProcessorChangedManager)

    static bool hasAnyModifiers (AutomatableEditItem& item)
    {
        for (auto param : item.getAutomatableParameters())
            if (! getModifiersOfType<Modifier> (*param).isEmpty())
                return true;

        return false;
    }

    void updateFromPlugin()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        bool wasLatencyChange = false;

        if (auto pi = plugin.getAudioPluginInstance())
        {
            if (plugin.latencySamples != pi->getLatencySamples())
            {
                wasLatencyChange = true;

                if (plugin.isInstancePrepared)
                {
                    plugin.latencySamples = pi->getLatencySamples();
                    plugin.latencySeconds = plugin.latencySamples / plugin.sampleRate;
                }

                plugin.edit.restartPlayback(); // Restart playback to rebuild audio graph for the new latency to take effect

                plugin.edit.getTransport().triggerClearDevicesOnStop(); // This will fully re-initialise plugins
            }

            pi->refreshParameterList();

            // refreshParameterList can delete the AudioProcessorParameter that our ExternalAutomatableParameters
            // are listening too so re-attach any possibly deleted listeners here
            for (auto p : plugin.autoParamForParamNumbers)
            {
                if (p != nullptr)
                {
                    p->unregisterAsListener();
                    p->registerAsListener();
                }
            }
        }
        else
        {
            jassertfalse;
        }

        // Annoyingly, because we can't tell the cause of the plugin change, we'll have to simply not
        // refresh parameter values if any modifiers have been assigned or they'll blow away the original
        // modifier values
        if (! wasLatencyChange && ! hasAnyModifiers (plugin))
            plugin.refreshParameterValues();

        plugin.changed();
        plugin.edit.pluginChanged (plugin);
    }

    void handleAsyncUpdate() override
    {
        if (paramChanged)
        {
            paramChanged = false;
            plugin.edit.pluginChanged (plugin);
        }
        if (processorChanged)
        {
            processorChanged = false;
            updateFromPlugin();
        }
    }

    bool paramChanged = false, processorChanged = false;
};

//==============================================================================
class ExternalPlugin::LoadedInstance
{
public:
    static std::unique_ptr<LoadedInstance> create (ExternalPlugin& ep, std::unique_ptr<juce::AudioPluginInstance> pi)
    {
        assert (pi);

        // N.B. We can't use make_unique here as make_unique would need to be a friend of LoadedInstance
        std::unique_ptr<LoadedInstance> loadedInstance (new LoadedInstance());
        loadedInstance->pluginInstance = std::move (pi);
        loadedInstance->processorChangedManager = std::make_unique<ProcessorChangedManager> (ep, *loadedInstance->pluginInstance);

        return loadedInstance;
    }

    juce::AudioPluginInstance& getInstance()
    {
        return *pluginInstance;
    }

    std::unique_ptr<juce::AudioPluginInstance> releaseInstance()
    {
        processorChangedManager.reset();
        return std::move (pluginInstance);
    }

private:
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<ProcessorChangedManager> processorChangedManager;

    LoadedInstance() = default;
};


//==============================================================================
struct AsyncPluginDeleter  : private juce::Timer,
                             private juce::DeletedAtShutdown
{
    AsyncPluginDeleter() = default;

    ~AsyncPluginDeleter() override
    {
        stopTimer();

        while (plugins.size() > 0)
            timerCallback();

        clearSingletonInstance();
    }

    JUCE_DECLARE_SINGLETON (AsyncPluginDeleter, false)

    void deletePlugin (std::unique_ptr<juce::AudioPluginInstance> p)
    {
        if (p)
        {
            plugins.add (p.release());
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

        const juce::ScopedValueSetter<bool> setter (recursive, true, false);

        juce::Component modal;
        modal.enterModalState (false);

        plugins.removeLast();
    }

private:
    juce::OwnedArray<juce::AudioPluginInstance> plugins;
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
            juce::Component modal;
            modal.enterModalState (false);

            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
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

    juce::int64 getTempoAt (juce::int64 samplePos) override
    {
        auto sampleRate = edit.engine.getDeviceManager().getSampleRate();
        return (juce::int64) (10000.0 * edit.tempoSequence.getTempoAt (TimePosition::fromSamples (samplePos, sampleRate)).getBpm());
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
class ExternalPlugin::PluginPlayHead  : public juce::AudioPlayHead
{
public:
    PluginPlayHead (ExternalPlugin& p)
        : plugin (p)
    {
    }

    /** @warning Because some idiotic plugins call getCurrentPosition on the message thread, we can't keep a
                 reference to the context hanging around; it may well have dissapeared by the time these plugins
                 get around to calling it. The best we can do is keep the last info we got.
    */
    void setCurrentContext (const PluginRenderContext* rc)
    {
        if (rc != nullptr)
        {
            time        = rc->editTime.getStart();
            isPlaying   = rc->isPlaying;

            const auto loopTimeRange = plugin.edit.getTransport().getLoopRange();
            loopStart.set (loopTimeRange.getStart());
            loopEnd.set (loopTimeRange.getEnd());
            currentPos.set (time);
        }
        else
        {
            time        = TimePosition();
            isPlaying   = false;
        }
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo result;

        result.setFrameRate (getFrameRate());

        auto& transport = plugin.edit.getTransport();
        auto localTime = time.load();

        result.setIsPlaying (isPlaying);
        result.setIsRecording (transport.isRecording());
        result.setEditOriginTime (transport.getTimeWhenStarted().inSeconds());
        result.setIsLooping (transport.looping);

        if (transport.looping)
            result.setLoopPoints (juce::AudioPlayHead::LoopPoints ({ loopStart.getPPQTime(), loopEnd.getPPQTime() }));

        result.setTimeInSeconds (localTime.inSeconds());
        result.setTimeInSamples (toSamples (localTime, plugin.sampleRate));

        const auto timeSig = currentPos.getTimeSignature();
        result.setBpm (currentPos.getTempo());
        result.setTimeSignature (juce::AudioPlayHead::TimeSignature ({ timeSig.numerator, timeSig.denominator }));

        const auto ppqPositionOfLastBarStart = currentPos.getPPQTimeOfBarStart();
        result.setPpqPositionOfLastBarStart (ppqPositionOfLastBarStart);
        result.setPpqPosition (std::max (ppqPositionOfLastBarStart, currentPos.getPPQTime()));

        return result;
    }

private:
    ExternalPlugin& plugin;
    tempo::Sequence::Position currentPos { createPosition (plugin.edit.tempoSequence) };
    tempo::Sequence::Position loopStart { createPosition (plugin.edit.tempoSequence) };
    tempo::Sequence::Position loopEnd { createPosition (plugin.edit.tempoSequence) };
    std::atomic<TimePosition> time { TimePosition() };
    std::atomic<bool> isPlaying { false };

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
    void readBusLayout (juce::AudioProcessor::BusesLayout& busesLayout,
                        const juce::ValueTree& state,
                        juce::AudioProcessor& plugin, bool isInput)
    {
        jassert (state.hasType (IDs::LAYOUT));
        auto& targetBuses = (isInput ? busesLayout.inputBuses
                                     : busesLayout.outputBuses);
        int maxNumBuses = 0;

        auto buses = state.getChildWithName (isInput ? IDs::INPUTS : IDs::OUTPUTS);

        if (buses.isValid())
        {
            for (auto v : buses)
            {
                if (! v.hasType (IDs::BUS))
                    continue;

                const int busIdx = v[IDs::index];
                maxNumBuses = std::max (maxNumBuses, busIdx + 1);

                // The number of buses on busesLayout may not be in sync with the plugin after adding buses
                // because adding an input bus could also add an output bus
                for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                    if (! plugin.addBus (isInput))
                        break;

                for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                    targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

                const auto layout = v[IDs::layout].toString();

                if (layout.isNotEmpty())
                    targetBuses.getReference (busIdx) = juce::AudioChannelSet::fromAbbreviatedString (layout);
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

    juce::AudioProcessor::BusesLayout readBusesLayout (const juce::var& layout, juce::AudioProcessor& plugin)
    {
        juce::AudioProcessor::BusesLayout busesLayout;

        if (auto* mb = layout.getBinaryData())
        {
            auto v = juce::ValueTree::readFromData (mb->getData(), mb->getSize());
            readBusLayout (busesLayout, v, plugin, true);
            readBusLayout (busesLayout, v, plugin, false);
        }

        return busesLayout;
    }

    juce::ValueTree createBusLayout (const juce::AudioProcessor::BusesLayout& layout, bool isInput)
    {
        auto& buses = (isInput ? layout.inputBuses : layout.outputBuses);

        juce::ValueTree v (isInput ? IDs::INPUTS : IDs::OUTPUTS);

        for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
        {
            auto& set = buses.getReference (busIdx);
            juce::String layoutName = set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString();

            auto bus = createValueTree (IDs::BUS,
                                        IDs::index, busIdx,
                                        IDs::layout, layoutName);

            v.addChild (bus, -1, nullptr);
        }

        return v.getNumChildren() > 0 ? v : juce::ValueTree();
    }

    juce::ValueTree createBusesLayout (const juce::AudioProcessor::BusesLayout& layout)
    {
        auto inputs = createBusLayout (layout, true);
        auto outputs = createBusLayout (layout, false);

        if (inputs.getNumChildren() == 0 && outputs.getNumChildren() == 0)
            return {};

        juce::ValueTree v (IDs::LAYOUT);
        v.addChild (inputs, -1, nullptr);
        v.addChild (outputs, -1, nullptr);

        return v;
    }

    juce::MemoryBlock createBusesLayoutProperty (const juce::AudioProcessor::BusesLayout& layout)
    {
        juce::MemoryBlock mb;

        auto v = createBusesLayout (layout);

        if (v.isValid())
        {
            juce::MemoryOutputStream os (mb, false);
            v.writeToStream (os);
        }

        return mb;
    }

    void restoreChannelLayout (ExternalPlugin& plugin)
    {
        auto setDefaultChannelConfig = [] (juce::AudioProcessor& ap, bool input)
        {
            // If the main bus is mono, set to stereo if supported
            const int n = ap.getBusCount (input);

            if (n >= 1)
            {
                auto bestSet = juce::AudioChannelSet::disabled();

                if (auto bus = ap.getBus (input, 0))
                {
                    if (bus->getCurrentLayout().size() < 2)
                    {
                        for (int i = 0; i < juce::AudioChannelSet::maxChannelsOfNamedLayout; i++)
                        {
                            auto set = bus->supportedLayoutWithChannels (i);

                            if (! set.isDisabled() && set.size() == 2)
                            {
                                bestSet = set;
                                break;
                            }
                        }

                        if (bestSet != juce::AudioChannelSet::disabled())
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
}

//==============================================================================
struct ExternalPlugin::MPEChannelRemapper
{
    static constexpr int lowChannel = 2, highChannel = 16;

    void reset() noexcept
    {
        for (auto& s : sourceAndChannel)
            s = {};
    }

    void clearChannel (int channel) noexcept
    {
        sourceAndChannel[channel] = {};
    }

    void remapMidiChannelIfNeeded (MidiMessageWithSource& m) noexcept
    {
        auto channel = m.getChannel();

        if (channel < lowChannel || channel > highChannel)
            return;

        auto sourceAndChannelID = (((uint32_t) m.mpeSourceID << 5) | (uint32_t) (channel - 1));

        if ((*m.getRawData() & 0xf0) != 0xf0)
        {
            ++counter;

            if (applyRemapIfExisting (channel, sourceAndChannelID, m))
                return;

            for (int chan = lowChannel; chan <= highChannel; ++chan)
                if (applyRemapIfExisting (chan, sourceAndChannelID, m))
                    return;

            if (sourceAndChannel[channel] == 0)
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

    uint32_t sourceAndChannel[17] = {};
    uint32_t lastUsed[17] = {};
    uint32_t counter = 0;

    int getBestChanToReuse() const noexcept
    {
        for (int chan = lowChannel; chan <= highChannel; ++chan)
            if (sourceAndChannel[chan] == 0)
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

    bool applyRemapIfExisting (int channel, uint32_t sourceAndChannelID, MidiMessageWithSource& m) noexcept
    {
        if (sourceAndChannel[channel] == sourceAndChannelID)
        {
            if (m.isNoteOff() || m.isAllNotesOff() || m.isResetAllControllers())
                sourceAndChannel[channel] = 0;
            else
                lastUsed[channel] = counter;

            m.setChannel (channel);
            return true;
        }

        return false;
    }
};

//==============================================================================
ExternalPlugin::ExternalPlugin (PluginCreationInfo info)  : Plugin (info)
{
    CRASH_TRACER

    auto um = getUndoManager();

    dryGain = new PluginWetDryAutomatableParam ("dry level", TRANS("Dry Level"), *this);
    wetGain = new PluginWetDryAutomatableParam ("wet level", TRANS("Wet Level"), *this);

    dryValue.referTo (state, IDs::dry, um);
    wetValue.referTo (state, IDs::wet, um, 1.0f);

    dryGain->attachToCurrentValue (dryValue);
    wetGain->attachToCurrentValue (wetValue);

    desc.uniqueId = (int) state[IDs::uniqueId].toString().getHexValue64();
    desc.deprecatedUid = (int) state[IDs::uid].toString().getHexValue64();
    desc.fileOrIdentifier = state[IDs::filename];
    setEnabled (state.getProperty (IDs::enabled, true));
    desc.name = state[IDs::name];
    desc.manufacturerName = state[IDs::manufacturer];
    identiferString = createIdentifierString (desc);

    initialiseFully();
}

juce::ValueTree ExternalPlugin::create (Engine& e, const juce::PluginDescription& desc)
{
    auto v = createValueTree (IDs::PLUGIN,
                              IDs::type, xmlTypeName,
                              IDs::uniqueId, juce::String::toHexString (desc.uniqueId),
                              IDs::uid, juce::String::toHexString (desc.deprecatedUid),
                              IDs::filename, desc.fileOrIdentifier,
                              IDs::name, desc.name,
                              IDs::manufacturer, desc.manufacturerName);

    if (e.getPluginManager().areGUIsLockedByDefault())
        v.setProperty (IDs::windowLocked, true, nullptr);

    return v;
}

juce::String ExternalPlugin::getLoadError()
{
    if (hasLoadedInstance || isAsyncInitialising)
        return {};

    return loadError.isEmpty() ? TRANS("ERROR! - This plugin couldn't be loaded!")
                               : loadError;
}

bool ExternalPlugin::isInitialisingAsync() const
{
    return isAsyncInitialising;
}

const char* ExternalPlugin::xmlTypeName = "vst";

void ExternalPlugin::initialiseFully()
{
    if (! std::exchange (fullyInitialised, true))
    {
        CRASH_TRACER_PLUGIN (getDebugName());
        doFullInitialisation();
    }
}

void ExternalPlugin::forceFullReinitialise()
{
    TransportControl::ScopedPlaybackRestarter restarter (edit.getTransport());
    engine.getUIBehaviour().recreatePluginWindowContentAsync (*this);
    edit.getTransport().stop (false, true);
    fullyInitialised = false;
    initialiseFully();
    changed();

    if (isInstancePrepared)
        if (auto pi = getAudioPluginInstance(); pi->getSampleRate() > 0 && pi->getBlockSize() > 0)
            pi->prepareToPlay (pi->getSampleRate(), pi->getBlockSize());

    edit.restartPlayback();
    SelectionManager::refreshAllPropertyPanelsShowing (*this);

    if (auto t = getOwnerTrack())
        t->refreshCurrentAutoParam();
}

void ExternalPlugin::updateDebugName()
{
    debugName = desc.name + " (" + desc.pluginFormatName + ")";
}

void ExternalPlugin::buildParameterList()
{
    CRASH_TRACER_PLUGIN (getDebugName());
    autoParamForParamNumbers.clear();
    clearParameterList();
    std::unordered_map<std::string, int> alreadyUsedParamNames;

    addAutomatableParameter (dryGain);
    addAutomatableParameter (wetGain);

    if (auto pi = getAudioPluginInstance())
    {
        auto& parameters = pi->getParameters();
        jassert (parameters.size() < 80000);
        const auto maxAutoParams = std::min (80000, parameters.size());

        for (int i = 0; i < maxAutoParams; ++i)
        {
            auto parameter = parameters.getUnchecked (i);

            if (parameter->isAutomatable() && ! isParameterBlacklisted (*this, *pi, *parameter))
            {
                auto nm = parameter->getName (1024);

                bool emptyName = nm.isEmpty();
                if (emptyName)
                    nm = "Unnamed";

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
                auto parameterID = juce::String (i);

                if (auto paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                    parameterID = paramWithID->paramID;

                auto p = new ExternalAutomatableParameter (parameterID, nm, *this, i, { 0.0f, 1.0f });
                addAutomatableParameter (*p);
                autoParamForParamNumbers.add (p);
                p->valueChangedByPlugin();

                if (count >= 2 && ! emptyName)
                    p->setDisplayName (nm);

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

void ExternalPlugin::refreshParameterValues()
{
    for (auto p : autoParamForParamNumbers)
        if (p != nullptr)
            p->valueChangedByPlugin();
}

static std::unique_ptr<juce::PluginDescription> findMatchingPluginDescription (Engine& engine, const juce::PluginDescription& desc)
{
    auto& pm = engine.getPluginManager();

    if (desc.uniqueId != 0)
        for (auto d : pm.knownPluginList.getTypes())
            if (d.uniqueId == desc.uniqueId && (desc.fileOrIdentifier.isEmpty() || desc.fileOrIdentifier == d.fileOrIdentifier))
                return std::make_unique<juce::PluginDescription> (d);

    if (desc.deprecatedUid != 0)
        for (auto d : pm.knownPluginList.getTypes())
            if (d.deprecatedUid == desc.deprecatedUid)
                return std::make_unique<juce::PluginDescription> (d);

    if (desc.fileOrIdentifier.isNotEmpty())
    {
        for (auto d : pm.knownPluginList.getTypes())
            if (d.fileOrIdentifier == desc.fileOrIdentifier)
                return std::make_unique<juce::PluginDescription> (d);

        return engine.getEngineBehaviour().findDescriptionForFileOrID (desc.fileOrIdentifier);
    }

    return {};
}

static std::unique_ptr<juce::PluginDescription> findDescForName (Engine& engine, const juce::String& name,
                                                                 const juce::String& format)
{
    if (name.isEmpty())
        return {};

    auto& pm = engine.getPluginManager();

    auto findName = [&pm, format] (const juce::String& nameToFind) -> std::unique_ptr<juce::PluginDescription>
    {
        for (auto d : pm.knownPluginList.getTypes())
            if (d.name == nameToFind && d.pluginFormatName == format)
                return std::make_unique<juce::PluginDescription> (d);

        return {};
    };

    if (auto p = findName (name))
        return p;

   #if JUCE_64BIT
    if (auto p = findName (name + " (64 bit)"))
        return p;

    if (auto p = findName (name + " (64-bit)"))
        return p;
   #endif

    return {};
}

std::unique_ptr<juce::PluginDescription> ExternalPlugin::findMatchingPlugin() const
{
    CRASH_TRACER
    auto& pm = engine.getPluginManager();

    if (auto p = pm.knownPluginList.getTypeForIdentifierString (createIdentifierString (desc)))
        return p;

    if (desc.pluginFormatName.isEmpty())
    {
        if (auto p = pm.knownPluginList.getTypeForIdentifierString ("VST" + createIdentifierString (desc)))
            return p;

        if (auto p = pm.knownPluginList.getTypeForIdentifierString ("AudioUnit" + createIdentifierString (desc)))
            return p;
    }

    if (auto p = findMatchingPluginDescription (engine, desc))
        return p;

    auto getPreferredFormat = [] (juce::PluginDescription d)
    {
        auto file = d.fileOrIdentifier.toLowerCase();
        if (file.endsWith (".vst3"))                            return "VST3";
        if (file.endsWith (".vst") || file.endsWith (".dll"))   return "VST";
        if (file.startsWith ("audiounit:"))                     return "AudioUnit";
        return "";
    };

    if (auto p = findDescForName (engine, desc.name, getPreferredFormat (desc)))
        return p;

    for (auto d : pm.knownPluginList.getTypes())
        if (d.name == desc.name)
            return std::make_unique<juce::PluginDescription> (d);

    for (auto d : pm.knownPluginList.getTypes())
        if (juce::File::createFileWithoutCheckingPath (d.fileOrIdentifier).getFileNameWithoutExtension() == desc.name)
            return std::make_unique<juce::PluginDescription> (d);

    return {};
}

//==============================================================================
void ExternalPlugin::processingChanged()
{
    Plugin::processingChanged();

    if (processing)
    {
        if (! getAudioPluginInstance())
            forceFullReinitialise();
    }
    else
    {
        clearParameterList();
        autoParamForParamNumbers.clear();
        getParameterTree().clear();

        deletePluginInstance();
    }
}

void ExternalPlugin::doFullInitialisation()
{
    if (auto foundDesc = findMatchingPlugin())
    {
        desc = *foundDesc;
        identiferString = createIdentifierString (desc);
        updateDebugName();

        if (processing && ! hasLoadedInstance
            && engine.getEngineBehaviour().shouldLoadPlugin (*this))
        {
            if (isDisabled())
                return;

            CRASH_TRACER_PLUGIN (getDebugName());
            loadError = {};

            callBlocking ([this, &foundDesc]
            {
                CRASH_TRACER_PLUGIN (getDebugName());
                startPluginInstanceCreation (*foundDesc);
            });
        }
    }
}

void ExternalPlugin::trackPropertiesChanged()
{
    juce::MessageManager::callAsync ([this, pluginRef = makeSafeRef (*this)]
    {
        if (pluginRef == nullptr)
            return;

        if (auto t = getOwnerTrack(); t != nullptr)
            if (auto pi = getAudioPluginInstance())
                pi->updateTrackProperties ({ t->getName(), t->getColour() });
    });
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

    if (auto pi = getAudioPluginInstance())
    {
        if (pi->getNumPrograms() > 0)
            state.setProperty (IDs::programNum,  pi->getCurrentProgram(), um);

        TRACKTION_ASSERT_MESSAGE_THREAD
        juce::MemoryBlock chunk;

        pi->suspendProcessing (true);
        pi->getStateInformation (chunk);
        saveChangedParametersToState();
        pi->suspendProcessing (false);

        engine.getEngineBehaviour().saveCustomPluginProperties (state, *pi, um);

        if (chunk.getSize() > 0)
            state.setProperty (IDs::state, chunk.toBase64Encoding(), um);
        else
            state.removeProperty (IDs::state, um);

        flushBusesLayoutToValueTree();
    }
}

void ExternalPlugin::flushBusesLayoutToValueTree()
{
    const juce::ScopedValueSetter<bool> svs (isFlushingLayoutToState, true);

    // Save buses layout
    if (auto ap = getAudioPluginInstance())
    {
        auto* um = getUndoManager();

        auto mb = createBusesLayoutProperty (ap->getBusesLayout());

        if (mb.getSize() > 0)
            state.setProperty (IDs::layout, mb, um);
        else
            state.removeProperty (IDs::layout, um);
    }
}

void ExternalPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::String s;

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

    if (auto pi = getAudioPluginInstance();
        pi && s.isNotEmpty())
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        if (getNumPrograms() > 1)
            setCurrentProgram (v.getProperty (IDs::programNum), false);

        juce::MemoryBlock chunk;
        chunk.fromBase64Encoding (s);

        if (chunk.getSize() > 0)
            callBlockingCatching ([&pi, &chunk] { pi->setStateInformation (chunk.getData(), (int) chunk.getSize()); });
    }
}

void ExternalPlugin::getPluginStateFromTree (juce::MemoryBlock& mb)
{
    auto s = state.getProperty (IDs::state).toString();
    mb.reset();

    if (s.isNotEmpty())
        mb.fromBase64Encoding (s);
}

void ExternalPlugin::updateFromMirroredPluginIfNeeded (Plugin& changedPlugin)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (changedPlugin.itemID != masterPluginID)
        return;

    if (auto pi = getAudioPluginInstance())
    {
        if (auto other = dynamic_cast<ExternalPlugin*> (&changedPlugin))
        {
            if (auto otherInstance = other->getAudioPluginInstance();
                otherInstance && other->desc.isDuplicateOf (desc))
            {
                juce::MemoryBlock chunk;
                otherInstance->getStateInformation (chunk);

                if (chunk.getSize() > 0)
                    pi->setStateInformation (chunk.getData(), (int) chunk.getSize());
            }
        }
    }
}

//==============================================================================
void ExternalPlugin::initialise (const PluginInitialisationInfo& info)
{
    CRASH_TRACER_PLUGIN (getDebugName());

    const juce::ScopedLock sl (processMutex);

    if (mpeRemapper == nullptr)
        mpeRemapper = std::make_unique<MPEChannelRemapper>();

    mpeRemapper->reset();
    midiPanicNeeded = false;

    if (auto pi = getAudioPluginInstance())
    {
        // This used to releaseResources() before calling prepareToPlay().
        // However, with VST3, releaseResources() shuts down the MIDI
        // input buses and then there is no way to get them back, which
        // breaks all synths.
        if (! isInstancePrepared)
        {
            pi->prepareToPlay (info.sampleRate, info.blockSizeSamples);
            isInstancePrepared = true;
        }
        else if (info.sampleRate != lastSampleRate || info.blockSizeSamples != lastBlockSizeSamples)
        {
            pi->prepareToPlay (info.sampleRate, info.blockSizeSamples);
        }

        lastSampleRate = info.sampleRate;
        lastBlockSizeSamples = info.blockSizeSamples;

        latencySamples = pi->getLatencySamples();
        latencySeconds = latencySamples / info.sampleRate;

        if (! desc.hasSharedContainer)
        {
            desc = pi->getPluginDescription();
            updateDebugName();
        }

        pi->setPlayHead (nullptr);
        playhead = std::make_unique<PluginPlayHead> (*this);
        pi->setPlayHead (playhead.get());
    }
    else
    {
        playhead.reset();
        latencySamples = 0;
        latencySeconds = 0.0;
        isInstancePrepared = false;
    }
}

void ExternalPlugin::deinitialise()
{
    if (auto pi = getAudioPluginInstance())
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        const juce::ScopedLock sl (processMutex);
        pi->setPlayHead (nullptr); // must be done first!

        if (playhead != nullptr)
            playhead->setCurrentContext (nullptr);
    }
}

void ExternalPlugin::reset()
{
    if (auto pi = getAudioPluginInstance())
    {
        CRASH_TRACER_PLUGIN (getDebugName());
        const juce::ScopedLock sl (processMutex);
        pi->reset();
    }
}

void ExternalPlugin::setEnabled (bool shouldEnable)
{
    bool wasEnabled = isEnabled();
    Plugin::setEnabled (shouldEnable);

    if (wasEnabled != isEnabled())
    {
        reset();
        propertiesChanged();
    }
}

void ExternalPlugin::midiPanic()
{
    midiPanicNeeded = true;
}

//==============================================================================
void ExternalPlugin::prepareIncomingMidiMessages (MidiMessageArray& incoming, int numSamples, bool isPlaying)
{
    if (midiPanicNeeded)
    {
        midiPanicNeeded = false;

        for (auto chan = 1; chan <= 16; ++chan)
        {
            midiBuffer.addEvent (juce::MidiMessage::allNotesOff (chan), 0);
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (chan, 0x40, 0), 0); // sustain pedal off
        }
    }

    if (incoming.isAllNotesOff)
    {
        uint32_t eventsSentOnChannel = 0;

        activeNotes.iterate ([&eventsSentOnChannel, this, isPlaying] (int chan, int noteNumber)
        {
            midiBuffer.addEvent (juce::MidiMessage::noteOff (chan, noteNumber), 0);

            if ((eventsSentOnChannel & (1u << chan)) == 0)
            {
                eventsSentOnChannel |= (1u << chan);

                if (! supportsMPE)
                {
                    midiBuffer.addEvent (juce::MidiMessage::controllerEvent (chan, 66 /* sustain pedal off */, 0), 0);
                    midiBuffer.addEvent (juce::MidiMessage::controllerEvent (chan, 64 /* hold pedal off */, 0), 0);
                }
                else
                {
                    // MPE standard (and JUCE implementation) now requires this instead of allNotesOff.
                    midiBuffer.addEvent (juce::MidiMessage::allControllersOff (1), 0);
                    midiBuffer.addEvent (juce::MidiMessage::allControllersOff (16), 0);
                }

                // NB: Some buggy plugins seem to fail to respond to note-ons if they are preceded
                // by an all-notes-off, so avoid this if just dragging the cursor around while playing.
                if (! isPlaying)
                    midiBuffer.addEvent (juce::MidiMessage::allNotesOff (chan), 0);
            }
        });

        activeNotes.reset();
        mpeRemapper->reset();

        // Reset MPE zone to match MIDI generated by clip
        if (supportsMPE)
        {
            juce::MPEZoneLayout layout;
            layout.setLowerZone (15);

            auto layoutBuffer = juce::MPEMessages::setZoneLayout (layout);
            for (auto itr : layoutBuffer)
            {
                auto result = itr.getMessage();
                int samplePosition = itr.samplePosition;

                midiBuffer.addEvent (result, samplePosition);
            }
        }
    }

    for (auto& m : incoming)
    {
        if (supportsMPE)
            mpeRemapper->remapMidiChannelIfNeeded (m);

        if (m.isNoteOn())
        {
            if (activeNotes.isNoteActive (m.getChannel(), m.getNoteNumber()))
                continue;

            activeNotes.startNote (m.getChannel(), m.getNoteNumber());
        }
        else if (m.isNoteOff())
        {
            activeNotes.clearNote (m.getChannel(), m.getNoteNumber());
        }

        auto sample = juce::jlimit (0, numSamples - 1, juce::roundToInt (m.getTimeStamp() * sampleRate));
        midiBuffer.addEvent (m, sample);
    }

   #if 0
    if (! incoming.isEmpty())
    {
        const uint8_t* midiData;
        int numBytes, midiEventPos;

        DBG ("----------");

        for (juce::MidiBuffer::Iterator iter (midiBuffer); iter.getNextEvent (midiData, numBytes, midiEventPos);)
            DBG (juce::String::toHexString (midiData, numBytes) << "   " << midiEventPos);
    }
   #endif

    incoming.clear();
}

void ExternalPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (! hasLoadedInstance.load (std::memory_order_acquire))
        return;

    if (! isInstancePrepared.load (std::memory_order_acquire))
        return;

    bool pluginEnabled = isEnabled();

    if (! (pluginEnabled || fc.allowBypassedProcessing))
    {
        midiPanicNeeded = false;
        return;
    }

    const bool processedBypass = ! pluginEnabled && fc.allowBypassedProcessing;

    assert (loadedInstance);
    CRASH_TRACER_PLUGIN (getDebugName());

    const juce::ScopedLock sl (processMutex);
    auto pi = getAudioPluginInstance();

    if (playhead != nullptr)
        playhead->setCurrentContext (&fc);

    midiBuffer.clear();

    if (fc.bufferForMidiMessages != nullptr)
        prepareIncomingMidiMessages (*fc.bufferForMidiMessages, fc.bufferNumSamples, fc.isPlaying);

    if (fc.destBuffer != nullptr)
    {
        auto destNumChans = fc.destBuffer->getNumChannels();
        jassert (destNumChans > 0);

        auto numInputChannels = pi->getTotalNumInputChannels();
        auto numOutputChannels = pi->getTotalNumOutputChannels();
        auto numChansToProcess = std::max (std::max (1, numInputChannels), numOutputChannels);

        if (destNumChans == numChansToProcess)
        {
            processPluginBlock (*pi, fc, processedBypass);
        }
        else
        {
            AudioScratchBuffer asb (numChansToProcess, fc.bufferNumSamples);
            auto& buffer = asb.buffer;

            // Copy existing channel or clear data
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

            PluginRenderContext fc2 (fc);
            fc2.destBuffer = &asb.buffer;
            fc2.bufferStartSample = 0;

            processPluginBlock (*pi, fc2, processedBypass);

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
        AudioScratchBuffer asb (std::max (pi->getTotalNumInputChannels(),
                                          pi->getTotalNumOutputChannels()), fc.bufferNumSamples);

        if (processedBypass)
            pi->processBlockBypassed (asb.buffer, midiBuffer);
        else
            pi->processBlock (asb.buffer, midiBuffer);
    }

    if (fc.bufferForMidiMessages != nullptr)
    {
        fc.bufferForMidiMessages->clear();

        if (! midiBuffer.isEmpty())
        {
            for (auto itr : midiBuffer)
            {
                const auto& msg = itr.getMessage();

                auto midiData = msg.getRawData();
                auto numBytes = msg.getRawDataSize();
                auto time = fc.midiBufferOffset + itr.samplePosition / sampleRate;

                fc.bufferForMidiMessages->addMidiMessage (juce::MidiMessage (midiData, numBytes, time), midiSourceID);
            }
        }
    }
}

void ExternalPlugin::processPluginBlock (juce::AudioPluginInstance& pluginInstance, const PluginRenderContext& fc, bool processedBypass)
{
    // N.B. The processMutex is already locked by the calling function

    juce::AudioBuffer<float> asb (fc.destBuffer->getArrayOfWritePointers(), fc.destBuffer->getNumChannels(),
                                  fc.bufferStartSample, fc.bufferNumSamples);

    auto dry = dryGain->getCurrentValue();
    auto wet = wetGain->getCurrentValue();

    if (dry <= 0.00004f)
    {
        if (processedBypass)
            pluginInstance.processBlockBypassed (asb, midiBuffer);
        else
            pluginInstance.processBlock (asb, midiBuffer);

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

        if (processedBypass)
            pluginInstance.processBlockBypassed (asb, midiBuffer);
        else
            pluginInstance.processBlock (asb, midiBuffer);

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
    return std::max (1, getNumOutputs());
}

void ExternalPlugin::getChannelNames (juce::StringArray* ins,
                                      juce::StringArray* outs)
{
    if (auto pi = getAudioPluginInstance())
    {
        CRASH_TRACER_PLUGIN (getDebugName());

        auto getChannelName = [](juce::AudioProcessor::Bus* bus, int index)
        {
            return bus != nullptr ? juce::AudioChannelSet::getChannelTypeName (bus->getCurrentLayout().getTypeOfChannel (index))
                                  : juce::String();
        };

        if (ins != nullptr)
        {
            const int num = pi->getTotalNumInputChannels();

            for (int i = 0; i < num; ++i)
            {
                auto name = getChannelName (pi->getBus (true, 0), i);
                ins->add (name.isNotEmpty() ? name : TRANS("Unnamed"));
            }
        }

        if (outs != nullptr)
        {
            const int num = pi->getTotalNumOutputChannels();

            for (int i = 0; i < num; ++i)
            {
                auto name = getChannelName (pi->getBus (false, 0), i);
                outs->add (name.isNotEmpty() ? name : TRANS("Unnamed"));
            }
        }
    }
}

bool ExternalPlugin::noTail()
{
    CRASH_TRACER_PLUGIN (getDebugName());
    return getTailLength() <= 0.0;
}

double ExternalPlugin::getTailLength() const
{
    CRASH_TRACER_PLUGIN (getDebugName());

    if (auto pi = getAudioPluginInstance())
        return pi->getTailLengthSeconds();

    return 0.0;
}

//==============================================================================
juce::File ExternalPlugin::getFile() const
{
    auto f = juce::File::createFileWithoutCheckingPath (desc.fileOrIdentifier);

    if (f.exists())
        return f;

    return {};
}

juce::String ExternalPlugin::getSelectableDescription()
{
    if (desc.pluginFormatName.isNotEmpty())
        return getName() + " (" + desc.pluginFormatName + " " + TRANS("Plugin") + ")";

    return getName();
}

//==============================================================================
int ExternalPlugin::getNumPrograms() const
{
    if (auto pi = getAudioPluginInstance())
        return pi->getNumPrograms();

    return 0;
}

int ExternalPlugin::getCurrentProgram() const
{
    if (auto pi = getAudioPluginInstance())
        return pi->getCurrentProgram();

    return 0;
}

juce::String ExternalPlugin::getProgramName (int index)
{
    if (index == getCurrentProgram())
        return getCurrentProgramName();

    if (auto pi = getAudioPluginInstance())
        return pi->getProgramName (index);

    return {};
}

bool ExternalPlugin::hasNameForMidiNoteNumber (int note, int midiChannel, juce::String& name)
{
    if (takesMidiInput())
    {
        if (auto pi = getAudioPluginInstance())
        {
            if (auto n = pi->getNameForMidiNoteNumber (note, midiChannel))
            {
                name = *n;
                return true;
            }
        }
    }
    return false;
}

bool ExternalPlugin::hasNameForMidiProgram (int programNum, int bank, juce::String& name)
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

juce::String ExternalPlugin::getNumberedProgramName (int i)
{
    auto s = getProgramName(i);

    if (s.isEmpty())
        s = "(" + TRANS("Unnamed") + ")";

    return juce::String (i + 1) + " - " + s;
}

juce::String ExternalPlugin::getCurrentProgramName()
{
    if (auto pi = getAudioPluginInstance())
        return pi->getProgramName (pi->getCurrentProgram());

    return {};
}

void ExternalPlugin::setCurrentProgramName (const juce::String& name)
{
    CRASH_TRACER_PLUGIN (getDebugName());

    if (auto pi = getAudioPluginInstance())
        pi->changeProgramName (pi->getCurrentProgram(), name);
}

void ExternalPlugin::setCurrentProgram (int index, bool sendChangeMessage)
{
    if (auto pi = getAudioPluginInstance())
    {
        if (getNumPrograms() == 0)
            return;

        CRASH_TRACER_PLUGIN (getDebugName());

        index = juce::jlimit (0, getNumPrograms() - 1, index);

        if (index != getCurrentProgram())
        {
            pi->setCurrentProgram (index);
            state.setProperty (IDs::programNum, index, nullptr);

            if (sendChangeMessage)
            {
                changed();
                refreshParameterValues();
            }
        }
    }
}

bool ExternalPlugin::takesMidiInput()
{
    if (auto pi = getAudioPluginInstance())
        return pi->acceptsMidi();

    return false;
}

bool ExternalPlugin::isMissing()
{
    return ! isDisabled() && ! hasLoadedInstance && ! isAsyncInitialising;
}

bool ExternalPlugin::isDisabled()
{
    return engine.getEngineBehaviour().isPluginDisabled (identiferString);
}

int ExternalPlugin::getNumInputs() const
{
    if (auto pi = getAudioPluginInstance())
        return pi->getTotalNumInputChannels();

    return 0;
}

int ExternalPlugin::getNumOutputs() const
{
    if (auto pi = getAudioPluginInstance())
        return pi->getTotalNumOutputChannels();

    return 0;
}

bool ExternalPlugin::setBusesLayout (juce::AudioProcessor::BusesLayout layout)
{
    if (auto pi = getAudioPluginInstance())
    {
        std::unique_ptr<Edit::ScopedRenderStatus> srs;

        if (! baseClassNeedsInitialising())
            srs = std::make_unique<Edit::ScopedRenderStatus> (edit, true);

        jassert (baseClassNeedsInitialising());

        // We need to release resources before changing the bus layout
        // prepareToPlay will be called when the above ScopedRenderStatus goes out of scope
        pi->releaseResources();
        isInstancePrepared = false;

        if (pi->setBusesLayout (layout))
        {
            if (! edit.isLoading())
            {
                if (auto r = getOwnerRackType())
                    r->checkConnections();

                flushBusesLayoutToValueTree();
            }

            return true;
        }
    }

    return false;
}

bool ExternalPlugin::setBusLayout (juce::AudioChannelSet set, bool isInput, int busIndex)
{
    if (auto pi = getAudioPluginInstance())
    {
        if (auto bus = pi->getBus (isInput, busIndex))
        {
            std::unique_ptr<Edit::ScopedRenderStatus> srs;

            if (! baseClassNeedsInitialising())
                srs = std::make_unique<Edit::ScopedRenderStatus> (edit, true);

            // We need to release resources before changing the bus layout
            // prepareToPlay will be called when the above ScopedRenderStatus goes out of scope
            pi->releaseResources();
            isInstancePrepared = false;

            jassert (baseClassNeedsInitialising());

            if (bus->setCurrentLayout (set))
            {
                if (! edit.isLoading())
                {
                    if (auto r = getOwnerRackType())
                        r->checkConnections();

                    flushBusesLayoutToValueTree();
                }

                return true;
            }
        }
    }

    return false;
}

//==============================================================================
void ExternalPlugin::startPluginInstanceCreation (const juce::PluginDescription& description)
{
   #if JUCE_DEBUG
    jassert (! hasLoadedInstance); // This should have already been deleted!
   #endif

    auto& dm = engine.getDeviceManager();

    if (requiresAsyncInstantiation (engine, description))
    {
        isAsyncInitialising = true;

        // The plugin might be deleted before the instance is created so use a weak-ref
        engine.getPluginManager().pluginFormatManager
            .createPluginInstanceAsync (description, dm.getSampleRate(), dm.getBlockSize(),
                                        [ep = makeSafeRef (*this)]
                                        (auto instance, auto err)
                                        {
                                            if (! ep)
                                                return;

                                            ep->loadError = err;
                                            ep->completePluginInstanceCreation (std::move (instance));

                                            // Trigger any status messages to be updated
                                            ep->changed();

                                            // If we call restartPlayback without clearing the sampleRate and blockSize,
                                            // the plugin won't get fully initialised which we need in order for
                                            // prepareToPlay to be called
                                            ep->sampleRate = {};
                                            ep->blockSizeSamples = {};
                                            ep->edit.restartPlayback();
                                        });
    }
    else
    {
        if (auto newInstance = engine.getPluginManager().createPluginInstance (description,
                                                                               dm.getSampleRate(), dm.getBlockSize(),
                                                                               loadError))
        {
            completePluginInstanceCreation (std::move (newInstance));
        }
    }
}

void ExternalPlugin::completePluginInstanceCreation (std::unique_ptr<juce::AudioPluginInstance> newInstance)
{
    if (! newInstance)
    {
        TRACKTION_LOG_ERROR (loadError);
        return;
    }

    newInstance->enableAllBuses();
    loadedInstance = LoadedInstance::create (*this, std::move (newInstance));
    hasLoadedInstance = true;
    isAsyncInitialising = false;

    auto pi = getAudioPluginInstance();

   #if JUCE_PLUGINHOST_VST
    if (auto xml = juce::VSTPluginFormat::getVSTXML (pi))
        vstXML.reset (VSTXML::createFor (*xml));

    juce::VSTPluginFormat::setExtraFunctions (pi, new ExtraVSTCallbacks (edit));
   #endif

    pi->setPlayHead (playhead.get());
    supportsMPE = pi->supportsMPE();

    engine.getEngineBehaviour().doAdditionalInitialisation (*this);

    restorePluginStateFromValueTree (state);
    buildParameterList();
    restoreChannelLayout (*this);
}

void ExternalPlugin::deletePluginInstance()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    hasLoadedInstance = false;
    isAsyncInitialising = false;
    loadError = {};

    if (loadedInstance)
        if (auto pi = loadedInstance->releaseInstance())
            AsyncPluginDeleter::getInstance()->deletePlugin (std::move (pi));
}

//==============================================================================
void ExternalPlugin::buildParameterTree() const
{
    auto& paramTree = getParameterTree();

    if (paramTree.rootNode->subNodes.size() > 0)
        return;

    CRASH_TRACER_PLUGIN (getDebugName());
    if (auto p1 = getAutomatableParameter (0))
        paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (p1));

    if (auto p2 = getAutomatableParameter (1))
        paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (p2));

    juce::SortedSet<int> paramsInTree;

    if (vstXML)
    {
        for (int i = 0; i < vstXML->paramTree.size(); ++i)
        {
            if (auto param = dynamic_cast<const VSTXML::Param*> (vstXML->paramTree[i]))
            {
                if (auto externalParameter = autoParamForParamNumbers[param->paramID])
                {
                    paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (externalParameter));
                    paramsInTree.add (param->paramID);
                }
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
        if (auto vstParam = dynamic_cast<ExternalAutomatableParameter*> (getAutomatableParameter (i).get()))
            if (! paramsInTree.contains (vstParam->getParameterIndex()))
                paramTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (autoParamForParamNumbers [vstParam->getParameterIndex()]));
}

void ExternalPlugin::buildParameterTree (const VSTXML::Group* group,
                                         AutomatableParameterTree::TreeNode* treeNode,
                                         juce::SortedSet<int>& paramsInTree) const
{
    for (int i = 0; i < group->paramTree.size(); ++i)
    {
        if (auto param = dynamic_cast<const VSTXML::Param*> (group->paramTree[i]))
        {
            if (auto externalParameter = autoParamForParamNumbers[param->paramID])
            {
                treeNode->addSubNode (new AutomatableParameterTree::TreeNode (externalParameter));
                paramsInTree.add (param->paramID);
            }
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
    Plugin::deleteFromParent();
}

juce::AudioPluginInstance* ExternalPlugin::getAudioPluginInstance() const
{
    if (! hasLoadedInstance)
        return {};

    return &loadedInstance->getInstance();
}

void ExternalPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state && id == IDs::layout)
    {
        if (isFlushingLayoutToState)
            return;

        if (auto ap = getAudioPluginInstance())
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

juce::Array<Exportable::ReferencedItem> ExternalPlugin::getReferencedItems()
{
    return engine.getEngineBehaviour().getReferencedItems (*this);
}

void ExternalPlugin::reassignReferencedItem (const ReferencedItem& itm, ProjectItemID newID, double newStartTime)
{
    engine.getEngineBehaviour().reassignReferencedItem (*this, itm, newID, newStartTime);
}


//==============================================================================
struct AudioProcessorEditorContentComp  : public Plugin::EditorComponent
{
    AudioProcessorEditorContentComp (ExternalPlugin& plug) : plugin (plug)
    {
        JUCE_AUTORELEASEPOOL
        {
            if (auto inst = plugin.getAudioPluginInstance())
            {
                editor.reset (inst->createEditorIfNeeded());

                if (editor == nullptr)
                    return;

                addAndMakeVisible (*editor);
            }
        }

        resizeToFitEditor (true);
    }

    bool allowWindowResizing() override
    {
        if (isCmajorPatchPluginFormat (plugin.desc))
            return editor != nullptr && editor->isResizable();

        // Allowing this for VSTs results in some hard-to-prevent size hysteresis..
        return plugin.isVST3()
                && plugin.getVendor().containsIgnoreCase ("Celemony");
    }

    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override
    {
        if (editor != nullptr)
            return editor->getConstrainer();

        return {};
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getLocalBounds());
    }

    void childBoundsChanged (juce::Component* c) override
    {
        if (c == editor.get())
        {
            plugin.edit.pluginChanged (plugin);
            resizeToFitEditor (false);
        }
    }

    void resizeToFitEditor (bool force)
    {
        if (force || ! allowWindowResizing())
        {
            setSize (std::max (8, editor != nullptr ? editor->getWidth() : 0),
                     std::max (8, editor != nullptr ? editor->getHeight() : 0));
        }
    }

    void visibilityChanged() override
    {
        if (editor != nullptr && isShowing())
        {
            if (editor->isShowing())
                editor->grabKeyboardFocus();

            editor->toFront (true);
        }
    }

    ExternalPlugin& plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    AudioProcessorEditorContentComp() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorEditorContentComp)
};

std::unique_ptr<Plugin::EditorComponent> ExternalPlugin::createEditor()
{
    auto ed = std::make_unique<AudioProcessorEditorContentComp> (*this);

    if (ed->editor != nullptr)
        return ed;

    return {};
}

//==============================================================================
bool ExternalPlugin::requiresAsyncInstantiation (Engine& e, const juce::PluginDescription& d)
{
    for (auto format : e.getPluginManager().pluginFormatManager.getFormats())
    {
        if (format->getName() == d.pluginFormatName
            && format->fileMightContainThisPluginType (d.fileOrIdentifier)
            && format->requiresUnblockedMessageThreadDuringCreation (d))
           return true;
    }

    return false;
}

//==============================================================================
PluginWetDryAutomatableParam::PluginWetDryAutomatableParam (const juce::String& xmlTag, const juce::String& name, Plugin& owner)
    : AutomatableParameter (xmlTag, name, owner, { 0.0f, 1.0f })
{
}

PluginWetDryAutomatableParam::~PluginWetDryAutomatableParam()
{
    notifyListenersOfDeletion();
}

juce::String PluginWetDryAutomatableParam::valueToString (float value)
{
    return juce::Decibels::toString (juce::Decibels::gainToDecibels (value), 1);
}

float PluginWetDryAutomatableParam::stringToValue (const juce::String& s)
{
    return juce::Decibels::decibelsToGain (dbStringToDb (s));
}

}} // namespace tracktion { inline namespace engine
