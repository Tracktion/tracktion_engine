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

    /** Returns the factory for a plugin type if one has already been created,
        without creating one. */
    static ARAPluginFactory* getExistingInstance (const juce::PluginDescription& desc)
    {
        auto& registry = getRegistry();
        auto it = registry.find (desc.createIdentifierString());

        return it != registry.end() ? it->second.get() : nullptr;
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
        getLookupCache().clear();
    }

    /** Factories loaded for archive-ID lookups, kept for the session so their modules
        aren't torn down (bundleExit) while plugin background threads may be running.
        Keyed by plugin identifier string. */
    static std::map<juce::String, juce::ARAFactoryResult>& getLookupCache()
    {
        static std::map<juce::String, juce::ARAFactoryResult> cache;
        return cache;
    }

    /** The full role set used for clip player instances, which render playback audio
        as well as hosting the editor. */
    static constexpr ARAPlugInInstanceRoleFlags allInstanceRoles = kARAPlaybackRendererRole
                                                                    | kARAEditorRendererRole
                                                                    | kARAEditorViewRole;

    ExternalPlugin::Ptr createPlugin (Edit& ed)
    {
        if (factory != nullptr)
            return createPlugin (ed, pluginDescription);

        return {};
    }

    ExternalPlugin::Ptr createPlugin (Edit& ed, const juce::PluginDescription& desc)
    {
        auto newState = ExternalPlugin::create (ed.engine, desc);
        ExternalPlugin::Ptr p = new ExternalPlugin (PluginCreationInfo (ed, newState, true));

        if (p->getAudioPluginInstance() != nullptr)
        {
            // ARA-bound instances must be destroyed before their document
            // controller, so they can't go through the shared async deleter
            p->setDeletesPluginInstanceSynchronously (true);
            return p;
        }

        return {};
    }

    ARAInstance* createInstance (ExternalPlugin& p, ARADocumentControllerRef dcRef,
                                 ARAPlugInInstanceRoleFlags roles = allInstanceRoles)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (factory != nullptr);

        std::unique_ptr<ARAInstance> w (new ARAInstance());
        w->plugin = &p;
        w->factory = factory;
        w->extensionInstance = nullptr;

        if (! setExtensionInstance (*w, dcRef, roles))
            w = nullptr;

        return w.release();
    }

    const ARAFactory* factory = nullptr;

    ~ARAPluginFactory()
    {
        // The dummy instance must go before uninitializeARA - the spec requires all
        // of a module's plugin instances to be destroyed before ARA is uninitialised
        plugin = nullptr;
        initGuard.reset();
    }

private:
    const juce::PluginDescription pluginDescription;

    // Fallback route only (module missing kARAMainFactoryClass): because ARA has
    // some state which is global to the DLL, this dummy instance of the plugin is
    // kept hanging around until shutdown, forcing the DLL to remain in memory
    // until we're sure all other instances have gone away
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    // Shared reference to the module-level factory, also held by getLookupCache()
    // for the session. JUCE pairs initializeARA/uninitializeARA inside the wrapper
    // and keeps the module loaded while any reference exists. Declared before
    // initGuard so the module reference outlives the guard's uninitializeARA call
    juce::ARAFactoryWrapper adoptedARAFactory;

    // Fallback route only: pairs initializeARA/uninitializeARA for a factory
    // obtained from the dummy instance's entry point
    std::unique_ptr<ARAFactoryInitGuard> initGuard;

    ARAPluginFactory (Engine& engine, const juce::PluginDescription& desc)
        : pluginDescription (desc)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        // Read the ARAFactory from the module-level IMainFactory (kARAMainFactoryClass)
        // instead of instantiating a full dummy plugin just to query its entry point.
        // The wrapper is shared with the archive-ID lookup cache so each module is
        // initialised exactly once per session, whichever call site reaches it first -
        // ARA forbids initialising a module twice. JUCE pairs initializeARA/
        // uninitializeARA inside the wrapper and keeps the module loaded while any
        // reference to it exists, so neither the dummy instance nor initGuard is
        // needed on this path.
        auto& cache = getLookupCache();
        auto key = pluginDescription.createIdentifierString();
        auto cached = cache.find (key);

        if (cached == cache.end())
        {
            juce::ARAFactoryResult result;
            engine.getPluginManager().pluginFormatManager
                .createARAFactoryAsync (pluginDescription, [&result] (juce::ARAFactoryResult r) { result = std::move (r); });

            // The callback is synchronous for VST3; if a format ever completes
            // asynchronously the factory will be null and the fallback below is used
            if (result.araFactory.get() != nullptr)
                cached = cache.emplace (key, std::move (result)).first;
        }

        if (cached != cache.end() && cached->second.araFactory.get() != nullptr)
        {
            adoptedARAFactory = cached->second.araFactory;
            factory = adoptedARAFactory.get();

            if (! supportsRequiredApiGeneration (*factory))
            {
                factory = nullptr;
                adoptedARAFactory = {};
            }
        }
        else
        {
            // Defensive fallback for modules missing kARAMainFactoryClass (the ARA2
            // spec requires it): read the factory from a dummy instance's entry point.
            // The instance is kept until shutdown to pin the DLL in memory while
            // other instances may exist
            plugin = createARAPluginFromDescription (engine, pluginDescription);

            if (plugin != nullptr)
            {
                getFactoryForPlugin();

                if (factory != nullptr)
                {
                    // The spec requires assertFunctionAddress to always be a valid pointer
                    // (which may point to a null function pointer in release builds)
                    static ARAAssertFunction assertFunction =
                       #if JUCE_LOG_ASSERTIONS || JUCE_DEBUG
                        assertCallback;
                       #else
                        nullptr;
                       #endif

                    const SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig =
                    {
                        std::min<ARAAPIGeneration> (factory->highestSupportedApiGeneration, kARAAPIGeneration_2_0_Final),
                        &assertFunction
                    };

                    initGuard = std::make_unique<ARAFactoryInitGuard> (factory, &interfaceConfig);
                }
                else
                {
                    jassertfalse;
                    plugin = nullptr;
                }
            }
        }

        // Plugins that can't time-stretch (analysis/alignment tools such as MTrackAlign or
        // ReChoir, or the ARA SDK example plugin) are still hosted: the playback region is
        // configured with the plugin's own supportedPlaybackTransformationFlags (see
        // ARAPlaybackRegion in tracktion_ARAWrapperInterfaces.h), so a non-time-stretch plugin
        // simply renders pass-through. Previously these were rejected outright, which made
        // editor-only ARA plugins unusable.
        if (factory != nullptr && ! canBeUsedAsTimeStretchEngine (*factory))
            TRACKTION_LOG ("ARA plugin does not support time-stretching - hosting in pass-through mode");
    }

    static std::map<juce::String, std::unique_ptr<ARAPluginFactory>>& getRegistry()
    {
        static std::map<juce::String, std::unique_ptr<ARAPluginFactory>> registry;
        return registry;
    }

    void getFactoryForPlugin()
    {
        auto type = pluginDescription.pluginFormatName;

        if (type == "VST3")
            factory = getFactoryVST3();

        if (factory != nullptr && ! supportsRequiredApiGeneration (*factory))
            factory = nullptr;
    }

    static bool supportsRequiredApiGeneration (const ARAFactory& f) noexcept
    {
        // The plugin must support our API generation: reject both plugins that are
        // too new (lowest > ours) and ARA1-only plugins (highest < ours), since the
        // ARA2 bind via IPlugInEntryPoint2 would fail later anyway
        return f.lowestSupportedApiGeneration <= kARAAPIGeneration_2_0_Final
            && f.highestSupportedApiGeneration >= kARAAPIGeneration_2_0_Final;
    }

    bool setExtensionInstance (ARAInstance& w, ARADocumentControllerRef dcRef, ARAPlugInInstanceRoleFlags roles)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (dcRef == nullptr)
            return false;

        auto type = pluginDescription.pluginFormatName;

        if (type == "VST3")
            return setExtensionInstanceVST3 (w, dcRef, roles);

        return false;
    }

    template<typename entrypoint_t>
    Steinberg::IPtr<entrypoint_t> getVST3EntryPoint (juce::AudioPluginInstance& p)
    {
        entrypoint_t* ep = nullptr;

        if (auto vst3Client = p.getVST3Client())
            if (auto* component = vst3Client->getIComponentPtr())
                component->queryInterface (entrypoint_t::iid, (void**) &ep);

        // queryInterface returns an owned reference, so adopt it - IPtr's
        // constructor defaults to addRef, which took a second reference and
        // leaked one plugin ref per query. For single-component plugins (where
        // the entry point shares the IComponent's refcount) that left every
        // instance one ref short of destruction at shutdown (QA 16453)
        return Steinberg::owned (ep);
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

    bool setExtensionInstanceVST3 (ARAInstance& w, ARADocumentControllerRef dcRef, ARAPlugInInstanceRoleFlags roles)
    {
        if (auto p = w.plugin->getAudioPluginInstance())
        {
            auto vst3EntryPoint2 = getVST3EntryPoint<IPlugInEntryPoint2> (*p);

            // knownRoles must always declare every role this host implements - a role
            // missing from knownRoles makes the plugin fall back to handling it
            // internally. Only assignedRoles is narrowed (e.g. editor-only for the
            // browser/panel instance, so it doesn't act as a playback renderer).
            if (vst3EntryPoint2 != nullptr)
                w.extensionInstance = vst3EntryPoint2->bindToDocumentControllerWithRoles (dcRef, allInstanceRoles, roles);
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
