/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


// Defined in ExternalPlugin.cpp to clean up plugins waiting to be deleted
extern void cleanUpDanglingPlugins();

static const char* commandLineUID = "PluginScan";

MemoryBlock createScanMessage (const XmlElement& xml)
{
    MemoryOutputStream mo;
    xml.writeToStream (mo, {}, true, false);
    return mo.getMemoryBlock();
}

struct PluginScanMasterProcess  : private ChildProcessMaster
{
    PluginScanMasterProcess (Engine& e) : engine (e) {}

    bool ensureSlaveIsLaunched()
    {
        if (launched)
            return true;

        crashed = false;
        // don't get stdout or strerr from the child process. We don't do anything with it and it fills up the pipe and hangs
        launched = launchSlaveProcess (File::getSpecialLocation (File::currentExecutableFile), commandLineUID, 0, 0);

        if (launched)
        {
            TRACKTION_LOG ("----- Launched Plugin Scan Process");
        }
        else
        {
            TRACKTION_LOG_ERROR ("Failed to launch child process");
            showVirusCheckerWarning();
        }

        return launched;
    }

    bool sendScanRequest (AudioPluginFormat& format, const String& fileOrIdentifier, int requestID)
    {
        XmlElement m ("SCAN");
        m.setAttribute ("id", requestID);
        m.setAttribute ("type", format.getName());
        m.setAttribute ("file", fileOrIdentifier);

        return sendMessageToSlave (createScanMessage (m));
    }

    bool waitForReply (int requestID, const String& fileOrIdentifier,
                       OwnedArray<PluginDescription>& result, KnownPluginList::CustomScanner& scanner)
    {
      #if ! TRACKTION_LOG_ENABLED
        juce::ignoreUnused (fileOrIdentifier);
      #endif

        Time start (Time::getCurrentTime());

        for (;;)
        {
            std::unique_ptr<XmlElement> reply (findReply (requestID));

            RelativeTime elapsed (Time::getCurrentTime() - start);

            if (reply == nullptr || ! reply->hasTagName ("FOUND"))
            {
                if (crashed)
                {
                    TRACKTION_LOG_ERROR ("Plugin crashed:  " + fileOrIdentifier);
                    return false;
                }

                if (scanner.shouldExit() || ! launched)
                {
                    TRACKTION_LOG ("Plugin scan cancelled");
                    return false;
                }

                Thread::sleep (10);
                continue;
            }

            if (reply->getNumChildElements() == 0)
                TRACKTION_LOG ("No plugins found in: " + fileOrIdentifier);

            forEachXmlChildElement (*reply, e)
            {
                PluginDescription desc;

                if (desc.loadFromXml (*e))
                {
                    auto newDesc = new PluginDescription (desc);
                    newDesc->lastInfoUpdateTime = Time::getCurrentTime();
                    result.add (newDesc);

                    TRACKTION_LOG ("Added " + desc.pluginFormatName + ": " + desc.name + "  [" + elapsed.getDescription() + "]");
                }
                else
                {
                    jassertfalse;
                }
            }

            return true;
        }
    }

    void handleMessage (const XmlElement& xml)
    {
        if (xml.hasTagName ("FOUND"))
        {
            const ScopedLock sl (replyLock);
            replies.add (new XmlElement (xml));
        }
    }

    void handleConnectionLost() override
    {
        crashed = true;
    }

    volatile bool launched = false, crashed = false;

private:
    Engine& engine;
    OwnedArray<XmlElement> replies;
    CriticalSection replyLock;
    bool hasShownVirusCheckerWarning = false;

    void showVirusCheckerWarning()
    {
        if (! hasShownVirusCheckerWarning)
        {
            hasShownVirusCheckerWarning = true;

            engine
                .getUIBehaviour().showWarningAlert ("Plugin Scanning...",
                                                    TRANS("There are some problems in launching a child-process to scan for plugins.")
                                                       + newLine + newLine
                                                       + TRANS("If you have a virus-checker or firewall running, you may need to temporarily disable it for the scan to work correctly."));
        }
    }

    XmlElement* findReply (int requestID)
    {
        for (int i = replies.size(); --i >= 0;)
            if (replies.getUnchecked(i)->getIntAttribute ("id") == requestID)
                return replies.removeAndReturn (i);

        return {};
    }

    void handleMessageFromSlave (const MemoryBlock& mb) override
    {
        if (auto xml = std::unique_ptr<XmlElement> (XmlDocument::parse (mb.toString())))
            handleMessage (*xml);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanMasterProcess)
};

//==============================================================================
struct PluginScanSlaveProcess  : public ChildProcessSlave,
                                 private AsyncUpdater
{
    PluginScanSlaveProcess()
    {
        pluginFormatManager.addDefaultFormats();
    }

    void handleConnectionMade() override {}

    void handleConnectionLost() override
    {
        std::exit (0);
    }

    void handleScanMessage (int requestID, const String& formatName, const String& fileOrIdentifier)
    {
        XmlElement result ("FOUND");
        result.setAttribute ("id", requestID);

        for (int i = 0; i < pluginFormatManager.getNumFormats(); ++i)
        {
            auto format = pluginFormatManager.getFormat (i);

            if (format->getName() == formatName)
            {
                OwnedArray<PluginDescription> found;
                format->findAllTypesForFile (found, fileOrIdentifier);

                for (auto pd : found)
                    result.addChildElement (pd->createXml());

                break;
            }
        }

        sendMessageToMaster (createScanMessage (result));
    }

    void handleMessage (const XmlElement& xml)
    {
        if (xml.hasTagName ("SCAN"))
            handleScanMessage (xml.getIntAttribute ("id"),
                               xml.getStringAttribute ("type"),
                               xml.getStringAttribute ("file"));
    }

private:
    AudioPluginFormatManager pluginFormatManager;
    OwnedArray<XmlElement, CriticalSection> pendingMessages;

    void handleMessageFromMaster (const MemoryBlock& mb) override
    {
        if (auto xml = std::unique_ptr<XmlElement> (XmlDocument::parse (mb.toString())))
        {
            pendingMessages.add (xml.release());
            triggerAsyncUpdate();
        }
    }
    void handleMessageSafely (const XmlElement& m)
    {
       #if JUCE_WINDOWS
        __try
        {
       #endif

            handleMessage (m);

       #if JUCE_WINDOWS
        }
        __except (1)
        {
            Process::terminate();
        }
       #endif
    }

    void handleAsyncUpdate() override
    {
        while (pendingMessages.size() > 0)
            if (auto xml = pendingMessages.removeAndReturn (0))
                handleMessageSafely (*xml);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanSlaveProcess)
};


//==============================================================================
#if JUCE_MAC
static void killWithoutMercy (int)
{
    kill (getpid(), SIGKILL);
}

static void setupSignalHandling()
{
    const int signals[] = { SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGABRT };

    for (int i = 0; i < numElementsInArray (signals); ++i)
    {
        ::signal (signals[i], killWithoutMercy);
        ::siginterrupt (signals[i], 1);
    }
}
#endif

bool PluginManager::startChildProcessPluginScan (const String& commandLine)
{
    auto slave = std::make_unique<PluginScanSlaveProcess>();

    if (slave->initialiseFromCommandLine (commandLine, commandLineUID))
    {
       #if JUCE_MAC
        setupSignalHandling();
       #endif

        slave.release(); // allow the slave object to stay alive - it'll handle its own deletion.
        return true;
    }

    return false;
}

//==============================================================================
struct BasicScanner  : public KnownPluginList::CustomScanner
{
    BasicScanner (Engine& e) : engine (e) {}

    bool findPluginTypesFor (AudioPluginFormat& format,
        OwnedArray<PluginDescription>& result,
        const String& fileOrIdentifier) override
    {
        CRASH_TRACER

        format.findAllTypesForFile (result, fileOrIdentifier);
        return true;
    }

    void scanFinished() override
    {
        TRACKTION_LOG ("----- Ended Plugin Scan");

        if (auto callback = engine.getPluginManager().scanCompletedCallback)
            callback();
    }

    Engine& engine;
};

//==============================================================================
struct CustomScanner  : public KnownPluginList::CustomScanner
{
    CustomScanner (Engine& e) : engine (e) {}

    bool findPluginTypesFor (AudioPluginFormat& format,
                             OwnedArray<PluginDescription>& result,
                             const String& fileOrIdentifier) override
    {
        CRASH_TRACER

        if (masterProcess != nullptr && masterProcess->crashed)
            masterProcess = nullptr;

        if (masterProcess == nullptr)
            masterProcess = std::make_unique<PluginScanMasterProcess> (engine);

        int requestID = Random().nextInt();

        if (! masterProcess->ensureSlaveIsLaunched())
        {
            // panic! Can't run the slave for some reason, so just do it here..
            TRACKTION_LOG_ERROR ("Falling back to scanning in main process..");
            format.findAllTypesForFile (result, fileOrIdentifier);
            return true;
        }

        if (! shouldExit()
             && masterProcess->sendScanRequest (format, fileOrIdentifier, requestID)
             && ! shouldExit())
        {
            if (masterProcess->waitForReply (requestID, fileOrIdentifier, result, *this))
                return true;

            // if there's a crash, give it a second chance with a fresh child process,
            // in case the real culprit was whatever plugin preceded this one.
            if (masterProcess->crashed && ! shouldExit())
            {
                masterProcess = nullptr;
                masterProcess = std::make_unique<PluginScanMasterProcess> (engine);

                return masterProcess->ensureSlaveIsLaunched()
                         && ! shouldExit()
                         && masterProcess->sendScanRequest (format, fileOrIdentifier, requestID)
                         && ! shouldExit()
                         && masterProcess->waitForReply (requestID, fileOrIdentifier, result, *this);
            }
        }

        return false;
    }

    void scanFinished() override
    {
        TRACKTION_LOG ("----- Ended Plugin Scan");
        masterProcess = nullptr;

        if (auto callback = engine.getPluginManager().scanCompletedCallback)
            callback();
    }

    Engine& engine;
    std::unique_ptr<PluginScanMasterProcess> masterProcess;
};

//==============================================================================
SettingID getPluginListPropertyName()
{
   #if JUCE_64BIT
    return SettingID::knownPluginList64;
   #else
    return SettingID::knownPluginList;
   #endif
}

//==============================================================================
PluginManager::BuiltInType::BuiltInType (const juce::String& t) : type (t) {}
PluginManager::BuiltInType::~BuiltInType() {}

//==============================================================================
PluginManager::PluginManager (Engine& e)  : engine (e)
{
}

void PluginManager::initialise()
{
    createBuiltInType<VolumeAndPanPlugin>();
    createBuiltInType<VCAPlugin>();
    createBuiltInType<LevelMeterPlugin>();
    createBuiltInType<EqualiserPlugin>();
    createBuiltInType<ReverbPlugin>();
    createBuiltInType<CompressorPlugin>();
    createBuiltInType<ChorusPlugin>();
    createBuiltInType<DelayPlugin>();
    createBuiltInType<PhaserPlugin>();
    createBuiltInType<PitchShiftPlugin>();
    createBuiltInType<LowPassPlugin>();
    createBuiltInType<SamplerPlugin>();
    createBuiltInType<MidiModifierPlugin>();
    createBuiltInType<MidiPatchBayPlugin>();
    createBuiltInType<PatchBayPlugin>();
    createBuiltInType<AuxSendPlugin>();
    createBuiltInType<AuxReturnPlugin>();
    createBuiltInType<TextPlugin>();
    createBuiltInType<FreezePointPlugin>();
    createBuiltInType<InsertPlugin>();

   #if TRACKTION_ENABLE_REWIRE
    createBuiltInType<ReWirePlugin>();
   #endif

    initialised = true;
    pluginFormatManager.addDefaultFormats();

    if (usesSeparateProcessForScanning())
        knownPluginList.setCustomScanner (new CustomScanner (engine));
    else
        knownPluginList.setCustomScanner (new BasicScanner (engine));

    auto xml = engine.getPropertyStorage().getXmlProperty (getPluginListPropertyName());

    if (xml != nullptr)
        knownPluginList.recreateFromXml (*xml);

    knownPluginList.addChangeListener (this);
}

PluginManager::~PluginManager()
{
    knownPluginList.removeChangeListener (this);
    cleanUpDanglingPlugins();
}

void PluginManager::changeListenerCallback (ChangeBroadcaster*)
{
    std::unique_ptr<XmlElement> xml (knownPluginList.createXml());
    engine.getPropertyStorage().setXmlProperty (getPluginListPropertyName(), *xml);
}

Plugin::Ptr PluginManager::createExistingPlugin (Edit& ed, const ValueTree& v)
{
    return createPlugin (ed, v, false);
}

Plugin::Ptr PluginManager::createNewPlugin (Edit& ed, const juce::ValueTree& v)
{
    return createPlugin (ed, v, true);
}

Plugin::Ptr PluginManager::createNewPlugin (Edit& ed, const String& type, const juce::PluginDescription& desc)
{
    jassert (initialised); // must call PluginManager::initialise() before this!
    jassert ((type == ExternalPlugin::xmlTypeName) == type.equalsIgnoreCase (ExternalPlugin::xmlTypeName));

    if (type.equalsIgnoreCase (ExternalPlugin::xmlTypeName))
        return new ExternalPlugin (PluginCreationInfo (ed, ExternalPlugin::create (ed.engine, desc), true));

    if (type.equalsIgnoreCase (RackInstance::xmlTypeName))
    {
        RackType::Ptr rackType;
        auto rackIndex = desc.fileOrIdentifier.getTrailingIntValue();

        if (desc.fileOrIdentifier.startsWith (RackType::getRackPresetPrefix()))
        {
            if (rackIndex >= 0)
                rackType = ed.engine.getEngineBehaviour().createPresetRackType (rackIndex, ed);
            else
                rackType = ed.getRackList().addNewRack();
        }
        else
        {
            rackType = ed.getRackList().getRackType (rackIndex);
        }

        if (rackType != nullptr)
            return new RackInstance (PluginCreationInfo (ed, RackInstance::create (*rackType), true));
    }

    for (auto builtIn : builtInTypes)
    {
        if (builtIn->type == type)
        {
            ValueTree v (IDs::PLUGIN);
            v.setProperty (IDs::type, type, nullptr);

            if (auto p = builtIn->create (PluginCreationInfo (ed, v, true)))
                return p;
        }
    }

    return {};
}

Array<PluginDescription*> PluginManager::getARACompatiblePlugDescriptions()
{
    jassert (initialised); // must call PluginManager::initialise() before this!

    Array<PluginDescription*> descs;

    for (int i = 0; i < knownPluginList.getNumTypes(); ++i)
    {
        if (auto p = knownPluginList.getType (i))
        {
            if (p->name.containsIgnoreCase ("Melodyne"))
            {
                auto version = p->version.trim().removeCharacters ("V").upToFirstOccurrenceOf (".", false, true);

                if (version.getIntValue() >= 2)
                    descs.add (p);
            }
        }
    }

    return descs;
}

bool PluginManager::areGUIsLockedByDefault()
{
    return engine.getPropertyStorage().getProperty (SettingID::filterGui, true);
}

void PluginManager::setGUIsLockedByDefault (bool b)
{
    engine.getPropertyStorage().setProperty (SettingID::filterGui, b);
}

bool PluginManager::doubleClickToOpenWindows()
{
    return engine.getPropertyStorage().getProperty (SettingID::windowsDoubleClick, false);
}

void PluginManager::setDoubleClickToOpenWindows (bool b)
{
    engine.getPropertyStorage().setProperty (SettingID::windowsDoubleClick, b);
}

int PluginManager::getNumberOfThreadsForScanning()
{
    return jlimit (1, SystemStats::getNumCpus(),
                   static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::numThreadsForPluginScanning, 1)));
}

void PluginManager::setNumberOfThreadsForScanning (int numThreads)
{
    engine.getPropertyStorage().setProperty (SettingID::numThreadsForPluginScanning, jlimit (1, SystemStats::getNumCpus(), numThreads));
}

bool PluginManager::usesSeparateProcessForScanning()
{
    return engine.getPropertyStorage().getProperty (SettingID::useSeparateProcessForScanning, true);
}

void PluginManager::setUsesSeparateProcessForScanning (bool b)
{
    engine.getPropertyStorage().setProperty (SettingID::useSeparateProcessForScanning, b);

    if (usesSeparateProcessForScanning())
        engine.getPluginManager().knownPluginList.setCustomScanner (new CustomScanner (engine));
    else
        engine.getPluginManager().knownPluginList.setCustomScanner (new BasicScanner (engine));
}

Plugin::Ptr PluginManager::createPlugin (Edit& ed, const juce::ValueTree& v, bool isNew)
{
    jassert (initialised); // must call PluginManager::initialise() before this!

    auto type = v[IDs::type].toString();
    PluginCreationInfo info (ed, v, isNew);

    if (type == ExternalPlugin::xmlTypeName)
        return new ExternalPlugin (info);

    if (type == RackInstance::xmlTypeName)
        return new RackInstance (info);

    for (auto builtIn : builtInTypes)
        if (builtIn->type == type)
            if (auto af = builtIn->create (info))
                return af;

    if (auto af = engine.getEngineBehaviour().createCustomPlugin (info))
        return af;

    TRACKTION_LOG_ERROR ("Failed to create plugin: " + type);
    return {};
}

//==============================================================================
PluginCache::PluginCache (Edit& ed) : edit (ed)
{
    startTimer (1000);
}

PluginCache::~PluginCache()
{
    activePlugins.clear();
}

Plugin::Ptr PluginCache::getPluginFor (EditItemID pluginID) const
{
    if (! pluginID.isValid())
        return {};

    const ScopedLock sl (lock);

    for (auto f : activePlugins)
        if (EditItemID::fromProperty (f->state, IDs::id) == pluginID)
            return *f;

    return {};
}

Plugin::Ptr PluginCache::getPluginFor (const ValueTree& v) const
{
    const ScopedLock sl (lock);

    for (auto f : activePlugins)
    {
        if (f->state == v)
            return *f;

        jassert (v[IDs::id].toString() != f->itemID.toString());
    }

    return {};
}

Plugin::Ptr PluginCache::getOrCreatePluginFor (const ValueTree& v)
{
    const ScopedLock sl (lock);

    if (auto f = getPluginFor (v))
        return f;

    if (auto f = edit.engine.getPluginManager().createExistingPlugin (edit, v))
    {
        jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager()
                  || dynamic_cast<ExternalPlugin*> (f.get()) == nullptr);

        return addPluginToCache (f);
    }

    return {};
}

Plugin::Ptr PluginCache::createNewPlugin (const juce::ValueTree& v)
{
    const ScopedLock sl (lock);
    auto p = addPluginToCache (edit.engine.getPluginManager().createNewPlugin (edit, v));

    if (p != nullptr && newPluginAddedCallback != nullptr)
        newPluginAddedCallback (*p);

    return p;
}

Plugin::Ptr PluginCache::createNewPlugin (const juce::String& type, const juce::PluginDescription& desc)
{
    jassert (type.isNotEmpty());

    const ScopedLock sl (lock);
    auto p = addPluginToCache (edit.engine.getPluginManager().createNewPlugin (edit, type, desc));

    if (p != nullptr && newPluginAddedCallback != nullptr)
        newPluginAddedCallback (*p);

    return p;
}

Plugin::Array PluginCache::getPlugins() const
{
    const ScopedLock sl (lock);
    return activePlugins;
}

Plugin::Ptr PluginCache::addPluginToCache (Plugin::Ptr p)
{
    if (p == nullptr)
    {
        jassertfalse;
        return {};
    }

    if (auto existing = getPluginFor (p->state))
    {
        jassertfalse;
        return existing;
    }

    jassert (! activePlugins.contains (p));
    activePlugins.add (p);

    return p;
}

void PluginCache::timerCallback()
{
    Plugin::Array toDelete;

    const ScopedLock sl (lock);

    for (int i = activePlugins.size(); --i >= 0;)
    {
        if (auto f = activePlugins.getObjectPointer (i))
        {
            if (f->getReferenceCount() == 1)
            {
                toDelete.add (f);
                activePlugins.remove (i);
            }
        }
    }
}
