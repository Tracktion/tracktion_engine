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

// Defined in ExternalPlugin.cpp to clean up plugins waiting to be deleted
extern void cleanUpDanglingPlugins();

static const char* commandLineUID = "PluginScan";

inline juce::MemoryBlock createScanMessage (const juce::XmlElement& xml)
{
    juce::MemoryOutputStream mo;
    xml.writeTo (mo, juce::XmlElement::TextFormat().withoutHeader().singleLine());
    return mo.getMemoryBlock();
}

struct PluginScanMasterProcess  : private juce::ChildProcessCoordinator
{
    PluginScanMasterProcess (Engine& e) : engine (e) {}

    bool ensureSlaveIsLaunched()
    {
        if (launched)
            return true;

        crashed = false;
        // don't get stdout or strerr from the child process. We don't do anything with it and it fills up the pipe and hangs
        launched = launchWorkerProcess (juce::File::getSpecialLocation (juce::File::currentExecutableFile),
                                        commandLineUID, 0, 0);

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

    bool sendScanRequest (juce::AudioPluginFormat& format,
                          const juce::String& fileOrIdentifier, int requestID)
    {
        juce::XmlElement m ("SCAN");
        m.setAttribute ("id", requestID);
        m.setAttribute ("type", format.getName());
        m.setAttribute ("file", fileOrIdentifier);

        return sendMessageToWorker (createScanMessage (m));
    }

    bool waitForReply (int requestID, const juce::String& fileOrIdentifier,
                       juce::OwnedArray<juce::PluginDescription>& result,
                       juce::KnownPluginList::CustomScanner& scanner)
    {
      #if ! TRACKTION_LOG_ENABLED
        juce::ignoreUnused (fileOrIdentifier);
      #endif

        auto start = juce::Time::getCurrentTime();

        for (;;)
        {
            auto reply = findReply (requestID);

            auto elapsed = juce::Time::getCurrentTime() - start;

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

                juce::Thread::sleep (10);
                continue;
            }

            if (reply->getNumChildElements() == 0)
                TRACKTION_LOG ("No plugins found in: " + fileOrIdentifier);

            for (auto e : reply->getChildIterator())
            {
                juce::PluginDescription desc;

                if (desc.loadFromXml (*e))
                {
                    auto newDesc = new juce::PluginDescription (desc);
                    newDesc->lastInfoUpdateTime = juce::Time::getCurrentTime();
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

    void handleMessage (const juce::XmlElement& xml)
    {
        if (xml.hasTagName ("FOUND"))
        {
            const juce::ScopedLock sl (replyLock);
            replies.add (new juce::XmlElement (xml));
        }
    }

    void handleConnectionLost() override
    {
        crashed = true;
    }

    volatile bool launched = false, crashed = false;

private:
    Engine& engine;
    juce::OwnedArray<juce::XmlElement> replies;
    juce::CriticalSection replyLock;
    bool hasShownVirusCheckerWarning = false;

    void showVirusCheckerWarning()
    {
        if (! hasShownVirusCheckerWarning)
        {
            hasShownVirusCheckerWarning = true;

            engine
                .getUIBehaviour().showWarningAlert ("Plugin Scanning...",
                                                    TRANS("There are some problems in launching a child-process to scan for plugins.")
                                                       + juce::newLine + juce::newLine
                                                       + TRANS("If you have a virus-checker or firewall running, you may need to temporarily disable it for the scan to work correctly."));
        }
    }

    std::unique_ptr<juce::XmlElement> findReply (int requestID)
    {
        for (int i = replies.size(); --i >= 0;)
            if (replies.getUnchecked(i)->getIntAttribute ("id") == requestID)
                return std::unique_ptr<juce::XmlElement> (replies.removeAndReturn (i));

        return {};
    }

    void handleMessageFromWorker (const juce::MemoryBlock& mb) override
    {
        if (auto xml = juce::parseXML (mb.toString()))
            handleMessage (*xml);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScanMasterProcess)
};

//==============================================================================
struct PluginScanSlaveProcess  : public juce::ChildProcessWorker,
                                 private juce::AsyncUpdater
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

    void handleScanMessage (int requestID, const juce::String& formatName, const juce::String& fileOrIdentifier)
    {
        juce::XmlElement result ("FOUND");
        result.setAttribute ("id", requestID);

        for (int i = 0; i < pluginFormatManager.getNumFormats(); ++i)
        {
            auto format = pluginFormatManager.getFormat (i);

            if (format->getName() == formatName)
            {
                juce::OwnedArray<juce::PluginDescription> found;
                format->findAllTypesForFile (found, fileOrIdentifier);

                for (auto pd : found)
                    result.addChildElement (pd->createXml().release());

                break;
            }
        }

        sendMessageToCoordinator (createScanMessage (result));
    }

    void handleMessage (const juce::XmlElement& xml)
    {
        if (xml.hasTagName ("SCAN"))
            handleScanMessage (xml.getIntAttribute ("id"),
                               xml.getStringAttribute ("type"),
                               xml.getStringAttribute ("file"));
    }

private:
    juce::AudioPluginFormatManager pluginFormatManager;
    juce::OwnedArray<juce::XmlElement, juce::CriticalSection> pendingMessages;

    void handleMessageFromCoordinator (const juce::MemoryBlock& mb) override
    {
        if (auto xml = juce::parseXML (mb.toString()))
        {
            pendingMessages.add (xml.release());
            triggerAsyncUpdate();
        }
    }

    void handleMessageSafely (const juce::XmlElement& m)
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
            juce::Process::terminate();
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

    for (int sig : signals)
    {
        ::signal (sig, killWithoutMercy);
        ::siginterrupt (sig, 1);
    }
}
#endif

bool PluginManager::startChildProcessPluginScan (const juce::String& commandLine)
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
struct CustomScanner  : public juce::KnownPluginList::CustomScanner
{
    CustomScanner (Engine& e) : engine (e) {}

    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override
    {
        CRASH_TRACER

        if (engine.getPluginManager().usesSeparateProcessForScanning()
             && shouldUseSeparateProcessToScan (format))
        {
            if (masterProcess != nullptr && masterProcess->crashed)
                masterProcess = nullptr;

            if (masterProcess == nullptr)
                masterProcess = std::make_unique<PluginScanMasterProcess> (engine);

            if (masterProcess->ensureSlaveIsLaunched())
            {
                auto requestID = juce::Random().nextInt();

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

            // panic! Can't run the slave for some reason, so just do it here..
            TRACKTION_LOG_ERROR ("Falling back to scanning in main process..");
            masterProcess.reset();
        }

        format.findAllTypesForFile (result, fileOrIdentifier);
        return true;
    }

    static bool shouldUseSeparateProcessToScan (juce::AudioPluginFormat& format)
    {
        auto name = format.getName();

        return name.containsIgnoreCase ("VST")
                || name.containsIgnoreCase ("AudioUnit")
                || name.containsIgnoreCase ("LADSPA");
    }

    void scanFinished() override
    {
        TRACKTION_LOG ("----- Ended Plugin Scan");
        masterProcess.reset();

        if (auto callback = engine.getPluginManager().scanCompletedCallback)
            callback();
    }

    Engine& engine;
    std::unique_ptr<PluginScanMasterProcess> masterProcess;
};

//==============================================================================
inline SettingID getPluginListPropertyName()
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
PluginManager::PluginManager (Engine& e)
    : engine (e)
{
    createPluginInstance = [this] (const juce::PluginDescription& description, double rate, int blockSize, juce::String& errorMessage)
    {
        return std::unique_ptr<juce::AudioPluginInstance> (pluginFormatManager.createPluginInstance (description, rate,
                                                                                                     blockSize, errorMessage));
    };
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
    createBuiltInType<FourOscPlugin>();
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
    knownPluginList.setCustomScanner (std::make_unique<CustomScanner> (engine));

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

#if TRACKTION_AIR_WINDOWS
void PluginManager::initialiseAirWindows()
{
    createBuiltInType<AirWindowsADClip7>();
    createBuiltInType<AirWindowsADT>();
    createBuiltInType<AirWindowsAQuickVoiceClip>();
    createBuiltInType<AirWindowsAcceleration>();
    createBuiltInType<AirWindowsAir>();
    createBuiltInType<AirWindowsAtmosphereBuss>();
    createBuiltInType<AirWindowsAtmosphereChannel>();
    createBuiltInType<AirWindowsAura>();
    createBuiltInType<AirWindowsAverage>();
    createBuiltInType<AirWindowsBassDrive>();
    createBuiltInType<AirWindowsBassKit>();
    createBuiltInType<AirWindowsBiquad>();
    createBuiltInType<AirWindowsBiquad2>();
    createBuiltInType<AirWindowsBitGlitter>();
    createBuiltInType<AirWindowsBitShiftGain>();
    createBuiltInType<AirWindowsBite>();
    createBuiltInType<AirWindowsBlockParty>();
    createBuiltInType<AirWindowsBrassRider>();
    createBuiltInType<AirWindowsBuildATPDF>();
    createBuiltInType<AirWindowsBussColors4>();
    createBuiltInType<AirWindowsButterComp>();
    createBuiltInType<AirWindowsButterComp2>();
    createBuiltInType<AirWindowsC5RawBuss>();
    createBuiltInType<AirWindowsC5RawChannel>();
    createBuiltInType<AirWindowsCStrip>();
    createBuiltInType<AirWindowsCapacitor>();
    createBuiltInType<AirWindowsChannel4>();
    createBuiltInType<AirWindowsChannel5>();
    createBuiltInType<AirWindowsChannel6>();
    createBuiltInType<AirWindowsChannel7>();
    createBuiltInType<AirWindowsChorus>();
    createBuiltInType<AirWindowsChorusEnsemble>();
    createBuiltInType<AirWindowsClipOnly>();
    createBuiltInType<AirWindowsCoils>();
    createBuiltInType<AirWindowsCojones>();
    createBuiltInType<AirWindowsCompresaturator>();
    createBuiltInType<AirWindowsConsole4Buss>();
    createBuiltInType<AirWindowsConsole4Channel>();
    createBuiltInType<AirWindowsConsole5Buss>();
    createBuiltInType<AirWindowsConsole5Channel>();
    createBuiltInType<AirWindowsConsole5DarkCh>();
    createBuiltInType<AirWindowsConsole6Buss>();
    createBuiltInType<AirWindowsConsole6Channel>();
    createBuiltInType<AirWindowsCrunchyGrooveWear>();
    createBuiltInType<AirWindowsCrystal>();
    createBuiltInType<AirWindowsDCVoltage>();
    createBuiltInType<AirWindowsDeBess>();
    createBuiltInType<AirWindowsDeEss>();
    createBuiltInType<AirWindowsDeHiss>();
    createBuiltInType<AirWindowsDeRez>();
    createBuiltInType<AirWindowsDeRez2>();
    createBuiltInType<AirWindowsDeckwrecka>();
    createBuiltInType<AirWindowsDensity>();
    createBuiltInType<AirWindowsDesk>();
    createBuiltInType<AirWindowsDesk4>();
    createBuiltInType<AirWindowsDistance>();
    createBuiltInType<AirWindowsDistance2>();
    createBuiltInType<AirWindowsDitherFloat>();
    createBuiltInType<AirWindowsDitherMeDiskers>();
    createBuiltInType<AirWindowsDitherMeTimbers>();
    createBuiltInType<AirWindowsDitherbox>();
    createBuiltInType<AirWindowsDoublePaul>();
    createBuiltInType<AirWindowsDrive>();
    createBuiltInType<AirWindowsDrumSlam>();
    createBuiltInType<AirWindowsDubCenter>();
    createBuiltInType<AirWindowsDubSub>();
    createBuiltInType<AirWindowsDustBunny>();
    createBuiltInType<AirWindowsDyno>();
    createBuiltInType<AirWindowsEQ>();
    createBuiltInType<AirWindowsEdIsDim>();
    createBuiltInType<AirWindowsElectroHat>();
    createBuiltInType<AirWindowsEnergy>();
    createBuiltInType<AirWindowsEnsemble>();
    createBuiltInType<AirWindowsEveryTrim>();
    createBuiltInType<AirWindowsFacet>();
    createBuiltInType<AirWindowsFathomFive>();
    createBuiltInType<AirWindowsFloor>();
    createBuiltInType<AirWindowsFocus>();
    createBuiltInType<AirWindowsFracture>();
    createBuiltInType<AirWindowsFromTape>();
    createBuiltInType<AirWindowsGatelope>();
    createBuiltInType<AirWindowsGolem>();
    createBuiltInType<AirWindowsGringer>();
    createBuiltInType<AirWindowsGrooveWear>();
    createBuiltInType<AirWindowsGuitarConditioner>();
    createBuiltInType<AirWindowsHardVacuum>();
    createBuiltInType<AirWindowsHermeTrim>();
    createBuiltInType<AirWindowsHermepass>();
    createBuiltInType<AirWindowsHighGlossDither>();
    createBuiltInType<AirWindowsHighImpact>();
    createBuiltInType<AirWindowsHighpass>();
    createBuiltInType<AirWindowsHighpass2>();
    createBuiltInType<AirWindowsHolt>();
    createBuiltInType<AirWindowsHombre>();
    createBuiltInType<AirWindowsInterstage>();
    createBuiltInType<AirWindowsIronOxide5>();
    createBuiltInType<AirWindowsIronOxideClassic>();
    createBuiltInType<AirWindowsLeftoMono>();
    createBuiltInType<AirWindowsLogical4>();
    createBuiltInType<AirWindowsLoud>();
    createBuiltInType<AirWindowsLowpass>();
    createBuiltInType<AirWindowsLowpass2>();
    createBuiltInType<AirWindowsMV>();
    createBuiltInType<AirWindowsMelt>();
    createBuiltInType<AirWindowsMidSide>();
    createBuiltInType<AirWindowsMoNoam>();
    createBuiltInType<AirWindowsMojo>();
    createBuiltInType<AirWindowsMonitoring>();
    createBuiltInType<AirWindowsNCSeventeen>();
    createBuiltInType<AirWindowsNaturalizeDither>();
    createBuiltInType<AirWindowsNodeDither>();
    createBuiltInType<AirWindowsNoise>();
    createBuiltInType<AirWindowsNonlinearSpace>();
    createBuiltInType<AirWindowsNotJustAnotherCD>();
    createBuiltInType<AirWindowsNotJustAnotherDither>();
    createBuiltInType<AirWindowsOneCornerClip>();
    createBuiltInType<AirWindowsPDBuss>();
    createBuiltInType<AirWindowsPDChannel>();
    createBuiltInType<AirWindowsPafnuty>();
    createBuiltInType<AirWindowsPaulDither>();
    createBuiltInType<AirWindowsPeaksOnly>();
    createBuiltInType<AirWindowsPhaseNudge>();
    createBuiltInType<AirWindowsPocketVerbs>();
    createBuiltInType<AirWindowsPodcast>();
    createBuiltInType<AirWindowsPodcastDeluxe>();
    createBuiltInType<AirWindowsPoint>();
    createBuiltInType<AirWindowsPop>();
    createBuiltInType<AirWindowsPowerSag>();
    createBuiltInType<AirWindowsPowerSag2>();
    createBuiltInType<AirWindowsPressure4>();
    createBuiltInType<AirWindowsPurestAir>();
    createBuiltInType<AirWindowsPurestConsoleBuss>();
    createBuiltInType<AirWindowsPurestConsoleChannel>();
    createBuiltInType<AirWindowsPurestDrive>();
    createBuiltInType<AirWindowsPurestEcho>();
    createBuiltInType<AirWindowsPurestGain>();
    createBuiltInType<AirWindowsPurestSquish>();
    createBuiltInType<AirWindowsPurestWarm>();
    createBuiltInType<AirWindowsPyewacket>();
    createBuiltInType<AirWindowsRawGlitters>();
    createBuiltInType<AirWindowsRawTimbers>();
    createBuiltInType<AirWindowsRecurve>();
    createBuiltInType<AirWindowsRemap>();
    createBuiltInType<AirWindowsResEQ>();
    createBuiltInType<AirWindowsRighteous4>();
    createBuiltInType<AirWindowsRightoMono>();
    createBuiltInType<AirWindowsSideDull>();
    createBuiltInType<AirWindowsSidepass>();
    createBuiltInType<AirWindowsSingleEndedTriode>();
    createBuiltInType<AirWindowsSlew>();
    createBuiltInType<AirWindowsSlew2>();
    createBuiltInType<AirWindowsSlewOnly>();
    createBuiltInType<AirWindowsSmooth>();
    createBuiltInType<AirWindowsSoftGate>();
    createBuiltInType<AirWindowsSpatializeDither>();
    createBuiltInType<AirWindowsSpiral>();
    createBuiltInType<AirWindowsSpiral2>();
    createBuiltInType<AirWindowsStarChild>();
    createBuiltInType<AirWindowsStereoFX>();
    createBuiltInType<AirWindowsStudioTan>();
    createBuiltInType<AirWindowsSubsOnly>();
    createBuiltInType<AirWindowsSurge>();
    createBuiltInType<AirWindowsSurgeTide>();
    createBuiltInType<AirWindowsSwell>();
    createBuiltInType<AirWindowsTPDFDither>();
    createBuiltInType<AirWindowsTapeDelay>();
    createBuiltInType<AirWindowsTapeDither>();
    createBuiltInType<AirWindowsTapeDust>();
    createBuiltInType<AirWindowsTapeFat>();
    createBuiltInType<AirWindowsThunder>();
    createBuiltInType<AirWindowsToTape5>();
    createBuiltInType<AirWindowsToVinyl4>();
    createBuiltInType<AirWindowsToneSlant>();
    createBuiltInType<AirWindowsTransDesk>();
    createBuiltInType<AirWindowsTremolo>();
    createBuiltInType<AirWindowsTubeDesk>();
    createBuiltInType<AirWindowsUnBox>();
    createBuiltInType<AirWindowsVariMu>();
    createBuiltInType<AirWindowsVibrato>();
    createBuiltInType<AirWindowsVinylDither>();
    createBuiltInType<AirWindowsVoiceOfTheStarship>();
    createBuiltInType<AirWindowsVoiceTrick>();
    createBuiltInType<AirWindowsWider>();
    createBuiltInType<AirWindowscurve>();
    createBuiltInType<AirWindowsuLawDecode>();
    createBuiltInType<AirWindowsuLawEncode>();
}
#endif

void PluginManager::changeListenerCallback (juce::ChangeBroadcaster*)
{
    std::unique_ptr<juce::XmlElement> xml (knownPluginList.createXml());
    engine.getPropertyStorage().setXmlProperty (getPluginListPropertyName(), *xml);
}

Plugin::Ptr PluginManager::createExistingPlugin (Edit& ed, const juce::ValueTree& v)
{
    if (auto p = createPlugin (ed, v, false))
    {
        p->initialiseFully();
        return p;
    }

    return {};
}

Plugin::Ptr PluginManager::createNewPlugin (Edit& ed, const juce::ValueTree& v)
{
    if (auto p = createPlugin (ed, v, true))
    {
        p->initialiseFully();
        return p;
    }

    return {};
}

Plugin::Ptr PluginManager::createNewPlugin (Edit& ed, const juce::String& type, const juce::PluginDescription& desc)
{
    jassert (initialised); // must call PluginManager::initialise() before this!
    jassert ((type == ExternalPlugin::xmlTypeName) == type.equalsIgnoreCase (ExternalPlugin::xmlTypeName));

    if (type.equalsIgnoreCase (ExternalPlugin::xmlTypeName))
        return new ExternalPlugin (PluginCreationInfo (ed, ExternalPlugin::create (ed.engine, desc), true));

    if (type.equalsIgnoreCase (RackInstance::xmlTypeName))
    {
        // If you're creating a RackInstance, you need to specify the Rack index!
        jassert (desc.fileOrIdentifier.isNotEmpty());
        
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
            auto v = createValueTree (IDs::PLUGIN,
                                      IDs::type, type);

            if (ed.engine.getPluginManager().areGUIsLockedByDefault())
                v.setProperty (IDs::windowLocked, true, nullptr);

            if (auto p = builtIn->create (PluginCreationInfo (ed, v, true)))
                return p;
        }
    }

    return {};
}

juce::Array<juce::PluginDescription> PluginManager::getARACompatiblePlugDescriptions()
{
    jassert (initialised); // must call PluginManager::initialise() before this!

    juce::Array<juce::PluginDescription> descs;

    for (const auto& p : knownPluginList.getTypes())
    {
        if (p.name.containsIgnoreCase ("Melodyne"))
        {
            auto version = p.version.trim().removeCharacters ("V").upToFirstOccurrenceOf (".", false, true);

            if (version.getIntValue() >= 4)
                descs.add (p);
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
    return juce::jlimit (1, juce::SystemStats::getNumCpus(),
                         static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::numThreadsForPluginScanning, 1)));
}

void PluginManager::setNumberOfThreadsForScanning (int numThreads)
{
    engine.getPropertyStorage().setProperty (SettingID::numThreadsForPluginScanning,
                                             juce::jlimit (1, juce::SystemStats::getNumCpus(), numThreads));
}

bool PluginManager::usesSeparateProcessForScanning()
{
    if (engine.getEngineBehaviour().canScanPluginsOutOfProcess())
        return engine.getPropertyStorage().getProperty (SettingID::useSeparateProcessForScanning, true);
    return false;
}

void PluginManager::setUsesSeparateProcessForScanning (bool b)
{
    engine.getPropertyStorage().setProperty (SettingID::useSeparateProcessForScanning, b);
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
void PluginManager::registerBuiltInType (std::unique_ptr<BuiltInType> t)
{
    for (auto builtIn : builtInTypes)
        if (builtIn->type == t->type)
            return;

    builtInTypes.add (t.release());
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

    const juce::ScopedLock sl (lock);

    for (auto p : activePlugins)
        if (EditItemID::fromProperty (p->state, IDs::id) == pluginID)
            return *p;

    return {};
}

Plugin::Ptr PluginCache::getPluginFor (const juce::ValueTree& v) const
{
    const juce::ScopedLock sl (lock);

    for (auto p : activePlugins)
        if (p->state == v)
            return *p;

    return {};
}

Plugin::Ptr PluginCache::getPluginFor (juce::AudioProcessor& ap) const
{
    const juce::ScopedLock sl (lock);

    for (auto p : activePlugins)
        if (p->getWrappedAudioProcessor() == &ap)
            return *p;

    return {};
}

Plugin::Ptr PluginCache::getOrCreatePluginFor (const juce::ValueTree& v)
{
    const juce::ScopedLock sl (lock);

    if (auto f = getPluginFor (v))
        return f;

    if (auto f = edit.engine.getPluginManager().createExistingPlugin (edit, v))
    {
        jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager()
                  || dynamic_cast<ExternalPlugin*> (f.get()) == nullptr);

        return addPluginToCache (f);
    }

    return {};
}

Plugin::Ptr PluginCache::createNewPlugin (const juce::ValueTree& v)
{
    const juce::ScopedLock sl (lock);
    auto p = addPluginToCache (edit.engine.getPluginManager().createNewPlugin (edit, v));

    if (p != nullptr && newPluginAddedCallback != nullptr)
        newPluginAddedCallback (*p);

    return p;
}

Plugin::Ptr PluginCache::createNewPlugin (const juce::String& type, const juce::PluginDescription& desc)
{
    jassert (type.isNotEmpty());

    const juce::ScopedLock sl (lock);
    auto p = addPluginToCache (edit.engine.getPluginManager().createNewPlugin (edit, type, desc));

    if (p != nullptr && newPluginAddedCallback != nullptr)
        newPluginAddedCallback (*p);

    return p;
}

Plugin::Array PluginCache::getPlugins() const
{
    const juce::ScopedLock sl (lock);
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

    {
        const juce::ScopedLock sl (lock);

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
}

}} // namespace tracktion { inline namespace engine
