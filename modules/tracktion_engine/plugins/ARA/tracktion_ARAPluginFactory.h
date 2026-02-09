/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

//==============================================================================
/** Holds an instance of an ARA plugin along with its factory and extension info. */
struct ARAInstance
{
    ExternalPlugin::Ptr plugin;
    const ARAFactory* factory = nullptr;
    const ARAPlugInExtensionInstance* extensionInstance = nullptr;
};

/** @deprecated Use ARAInstance instead */
using MelodyneInstance = ARAInstance;

//==============================================================================
/** RAII guard that ref-counts initializeARA/uninitializeARA calls per unique ARAFactory pointer.
    Multiple ARAPluginFactory entries may share the same underlying ARAFactory* (same DLL);
    this ensures initializeARA is called once on first use and uninitializeARA once on last destruction.
*/
struct ARAFactoryInitGuard
{
    ARAFactoryInitGuard (const ARAFactory* f, const ARAInterfaceConfiguration* config)
        : factoryPtr (f)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        auto& refCounts = getRefCounts();

        if (refCounts[factoryPtr]++ == 0)
            factoryPtr->initializeARAWithConfiguration (config);
    }

    ~ARAFactoryInitGuard()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        auto& refCounts = getRefCounts();

        if (--refCounts[factoryPtr] == 0)
        {
            factoryPtr->uninitializeARA();
            refCounts.erase (factoryPtr);
        }
    }

    const ARAFactory* factoryPtr;

private:
    static std::map<const ARAFactory*, int>& getRefCounts()
    {
        static std::map<const ARAFactory*, int> refs;
        return refs;
    }

    JUCE_DECLARE_NON_COPYABLE (ARAFactoryInitGuard)
};

//==============================================================================
/** Factory for creating ARA plugin instances.
    Maintains a registry of per-plugin-type factories, keyed by PluginDescription identifier string.
*/
struct ARAPluginFactory
{
public:
    /** Returns (or creates) the factory for a specific ARA plugin type. */
    static ARAPluginFactory& getInstance (Engine& engine, const juce::PluginDescription& desc)
    {
        auto key = desc.createIdentifierString();
        auto& registry = getRegistry();
        auto it = registry.find (key);

        if (it == registry.end())
        {
            auto* f = new ARAPluginFactory (engine, desc);
            registry[key] = std::unique_ptr<ARAPluginFactory> (f);
            return *f;
        }

        return *it->second;
    }

    /** Picks the preferred default ARA plugin description for legacy clips.
        Prefers Melodyne since legacy clips were always Melodyne. */
    static juce::PluginDescription findPreferredDefault (const juce::Array<juce::PluginDescription>& descs)
    {
        for (auto& d : descs)
            if (d.name.containsIgnoreCase ("Melodyne"))
                return d;

        // If there is no default, Melodyne might not be installed so
        // just return an empty desc to avoid overwriting the data
        return {};
    }

    /** Returns the factory for the first/default ARA plugin (backward compat). */
    static ARAPluginFactory* getDefaultInstance (Engine& engine)
    {
        auto& registry = getRegistry();

        if (! registry.empty())
            return registry.begin()->second.get();

        // Create one from the first available ARA plugin
        auto araDescs = engine.getPluginManager().getARACompatiblePlugDescriptions();

        if (araDescs.isEmpty())
            return nullptr;

        return &getInstance (engine, findPreferredDefault (araDescs));
    }

    static void shutdown()
    {
        CRASH_TRACER
        getRegistry().clear();
    }

    ExternalPlugin::Ptr createPlugin (Edit& ed)
    {
        if (plugin != nullptr)
        {
            auto newState = ExternalPlugin::create (ed.engine, plugin->getPluginDescription());
            ExternalPlugin::Ptr p = new ExternalPlugin (PluginCreationInfo (ed, newState, true));

            if (p->getAudioPluginInstance() != nullptr)
                return p;
        }

        return {};
    }

    ExternalPlugin::Ptr createPlugin (Edit& ed, const juce::PluginDescription& desc)
    {
        auto newState = ExternalPlugin::create (ed.engine, desc);
        ExternalPlugin::Ptr p = new ExternalPlugin (PluginCreationInfo (ed, newState, true));

        if (p->getAudioPluginInstance() != nullptr)
            return p;

        return {};
    }

    ARAInstance* createInstance (ExternalPlugin& p, ARADocumentControllerRef dcRef)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (plugin != nullptr);

        std::unique_ptr<ARAInstance> w (new ARAInstance());
        w->plugin = &p;
        w->factory = factory;
        w->extensionInstance = nullptr;

        if (! setExtensionInstance (*w, dcRef))
            w = nullptr;

        return w.release();
    }

    const ARAFactory* factory = nullptr;

    ~ARAPluginFactory()
    {
        initGuard.reset();
        plugin = nullptr;
    }

private:
    // Because ARA has some state which is global to the DLL, this dummy instance
    // of the plugin is kept hanging around until shutdown, forcing the DLL to
    // remain in memory until we're sure all other instances have gone away. Not
    // pretty, but not sure how else we could handle this.
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<ARAFactoryInitGuard> initGuard;

    ARAPluginFactory (Engine& engine, const juce::PluginDescription& desc)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        plugin = createARAPluginFromDescription (engine, desc);

        if (plugin != nullptr)
        {
            getFactoryForPlugin();

            if (factory != nullptr)
            {
                if (canBeUsedAsTimeStretchEngine (*factory))
                {
                    ARAAssertFunction* assertFuncPtr = nullptr;
                   #if JUCE_LOG_ASSERTIONS || JUCE_DEBUG
                    static ARAAssertFunction assertFunction = assertCallback;
                    assertFuncPtr = &assertFunction;
                   #endif

                    const SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig =
                    {
                        std::min<ARAAPIGeneration> (factory->highestSupportedApiGeneration, kARAAPIGeneration_2_0_Final),
                        assertFuncPtr
                    };

                    initGuard = std::make_unique<ARAFactoryInitGuard> (factory, &interfaceConfig);
                }
                else
                {
                    TRACKTION_LOG_ERROR ("ARA-compatible plugin could not be used for time-stretching!");
                    jassertfalse;
                    factory = nullptr;
                    plugin = nullptr;
                }
            }
            else
            {
                jassertfalse;
                plugin = nullptr;
            }
        }
    }

    static std::map<juce::String, std::unique_ptr<ARAPluginFactory>>& getRegistry()
    {
        static std::map<juce::String, std::unique_ptr<ARAPluginFactory>> registry;
        return registry;
    }

    void getFactoryForPlugin()
    {
        auto type = plugin->getPluginDescription().pluginFormatName;

        if (type == "VST3")
            factory = getFactoryVST3();

        if (factory != nullptr && factory->lowestSupportedApiGeneration > kARAAPIGeneration_2_0_Final)
            factory = nullptr;
    }

    bool setExtensionInstance (ARAInstance& w, ARADocumentControllerRef dcRef)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (dcRef == nullptr)
            return false;

        auto type = plugin->getPluginDescription().pluginFormatName;

        if (type == "VST3")
            return setExtensionInstanceVST3 (w, dcRef);

        return false;
    }

    template<typename entrypoint_t>
    Steinberg::IPtr<entrypoint_t> getVST3EntryPoint (juce::AudioPluginInstance& p)
    {
        entrypoint_t* ep = nullptr;

        // Use getPlatformSpecificData() instead of getExtensions() to avoid
        // triggering JUCE's internal ARA init/uninit cycle in getARAFactory()
        JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
        if (auto* component = static_cast<Steinberg::Vst::IComponent*> (p.getPlatformSpecificData()))
            component->queryInterface (entrypoint_t::iid, (void**) &ep);
        JUCE_END_IGNORE_DEPRECATION_WARNINGS

        return { ep };
    }

    ARAFactory* getFactoryVST3()
    {
        if (auto ep = getVST3EntryPoint<IPlugInEntryPoint> (*plugin))
        {
            ARAFactory* f = const_cast<ARAFactory*> (ep->getFactory());
            return f;
        }

        return {};
    }

    bool setExtensionInstanceVST3 (ARAInstance& w, ARADocumentControllerRef dcRef)
    {
        if (auto p = w.plugin->getAudioPluginInstance())
        {
            auto vst3EntryPoint2 = getVST3EntryPoint<IPlugInEntryPoint2> (*p);

            if (vst3EntryPoint2 != nullptr)
            {
                ARAPlugInInstanceRoleFlags roles = kARAPlaybackRendererRole | kARAEditorRendererRole | kARAEditorViewRole;
                w.extensionInstance = vst3EntryPoint2->bindToDocumentControllerWithRoles (dcRef, roles, roles);
            }
        }

        return w.extensionInstance != nullptr;
    }

    static bool canBeUsedAsTimeStretchEngine (const ARAFactory& factory) noexcept
    {
        return (factory.supportedPlaybackTransformationFlags & kARAPlaybackTransformationTimestretch) != 0
            && (factory.supportedPlaybackTransformationFlags & kARAPlaybackTransformationTimestretchReflectingTempo) != 0;
    }

    static void ARA_CALL assertCallback (ARAAssertCategory category, const void* problematicArgument, const char* diagnosis)
    {
        juce::String categoryName;

        switch ((int) category)
        {
            case kARAAssertUnspecified:     categoryName = "Unspecified"; break;
            case kARAAssertInvalidArgument: categoryName = "Invalid Argument"; break;
            case kARAAssertInvalidState:    categoryName = "Invalid State"; break;
            case kARAAssertInvalidThread:   categoryName = "Invalid Thread"; break;
            default:                        categoryName = "(Unknown)"; break;
        };

        TRACKTION_LOG_ERROR ("ARA assertion -> \"" + categoryName + "\": " + juce::String::fromUTF8 (diagnosis)
                              + ": " + juce::String (juce::pointer_sized_int (problematicArgument)));
        jassertfalse;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAPluginFactory)
};

/** @deprecated Use ARAPluginFactory instead */
using MelodyneInstanceFactory = ARAPluginFactory;

//==============================================================================
static std::unique_ptr<juce::AudioPluginInstance> createARAPluginFromDescription (Engine& engine,
                                                                                   const juce::PluginDescription& desc)
{
    CRASH_TRACER

    juce::String error;
    auto& pfm = engine.getPluginManager().pluginFormatManager;

    if (auto p = pfm.createPluginInstance (desc, 44100.0, 512, error))
        return p;

    return {};
}

static std::unique_ptr<juce::AudioPluginInstance> createARAPlugin (Engine& engine,
                                                                    const char* formatToTry,
                                                                    const juce::Array<juce::PluginDescription>& araDescs)
{
    CRASH_TRACER

    juce::String error;
    auto& pfm = engine.getPluginManager().pluginFormatManager;

    for (auto pd : araDescs)
        if (pd.pluginFormatName == formatToTry)
            if (auto p = pfm.createPluginInstance (pd, 44100.0, 512, error))
                return p;

    return {};
}

static std::unique_ptr<juce::AudioPluginInstance> createARAPlugin (Engine& engine)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto araDescs = engine.getPluginManager().getARACompatiblePlugDescriptions();

    if (auto p = createARAPlugin (engine, "VST3", araDescs))
        return p;

    return {};
}

/** @deprecated Use createARAPlugin instead */
[[deprecated("Use createARAPlugin instead")]]
inline std::unique_ptr<juce::AudioPluginInstance> createMelodynePlugin (Engine& engine)
{
    return createARAPlugin (engine);
}
