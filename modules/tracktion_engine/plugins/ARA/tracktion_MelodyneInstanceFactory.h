/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

struct MelodyneInstance
{
    ExternalPlugin::Ptr plugin;
    const ARAFactory* factory = nullptr;
    const ARAPlugInExtensionInstance* extensionInstance = nullptr;
};

//==============================================================================
struct MelodyneInstanceFactory
{
public:
    static MelodyneInstanceFactory& getInstance (Engine& engine)
    {
        auto& p = getInstancePointer();

        if (p == nullptr)
            p = new MelodyneInstanceFactory (engine);

        return *p;
    }

    static void shutdown()
    {
        CRASH_TRACER
        delete getInstancePointer();
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

    MelodyneInstance* createInstance (ExternalPlugin& p, ARADocumentControllerRef dcRef)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (plugin != nullptr);

        std::unique_ptr<MelodyneInstance> w (new MelodyneInstance());
        w->plugin = &p;
        w->factory = factory;
        w->extensionInstance = nullptr;

        if (! setExtensionInstance (*w, dcRef))
            w = nullptr;

        return w.release();
    }

    const ARAFactory* factory = nullptr;

private:
    // Because ARA has some state which is global to the DLL, this dummy instance
    // of the plugin is kept hanging around until shutdown, forcing the DLL to
    // remain in memory until we're sure all other instances have gone away. Not
    // pretty, but not sure how else we could handle this.
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    MelodyneInstanceFactory (Engine& engine)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        plugin = createMelodynePlugin (engine);

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

                    const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig =
                    {
                        std::min<ARAAPIGeneration> (factory->highestSupportedApiGeneration, kARAAPIGeneration_2_0_Draft),
                        assertFuncPtr
                    };

                    factory->initializeARAWithConfiguration (&interfaceConfig);
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

    ~MelodyneInstanceFactory()
    {
        if (factory != nullptr)
            factory->uninitializeARA();

        plugin = nullptr;
    }

    static MelodyneInstanceFactory*& getInstancePointer()
    {
        static MelodyneInstanceFactory* instance;
        return instance;
    }

    void getFactoryForPlugin()
    {
        auto type = plugin->getPluginDescription().pluginFormatName;

        if (type == "VST3")
            factory = getFactoryVST3();

        if (factory != nullptr && factory->lowestSupportedApiGeneration > kARAAPIGeneration_2_0_Draft)
            factory = nullptr;
    }

    bool setExtensionInstance (MelodyneInstance& w, ARADocumentControllerRef dcRef)
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
        
        auto getIComponent = [] (juce::AudioPluginInstance& p) -> Steinberg::Vst::IComponent*
        {
            struct VST3Visitor : public juce::ExtensionsVisitor
            {
                void visitVST3Client (const VST3Client& client) override
                {
                    icomponent = static_cast<Steinberg::Vst::IComponent*> (client.getIComponentPtr());
                }
                
                Steinberg::Vst::IComponent* icomponent = nullptr;
            };

            VST3Visitor vst3Visitor;
            p.getExtensions (vst3Visitor);
            
            return vst3Visitor.icomponent;
        };
        
        if (auto component = getIComponent (p))
            component->queryInterface (entrypoint_t::iid, (void**) &ep);

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

    bool setExtensionInstanceVST3 (MelodyneInstance& w, ARADocumentControllerRef dcRef)
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyneInstanceFactory)
};

//==============================================================================
static std::unique_ptr<juce::AudioPluginInstance> createMelodynePlugin (Engine& engine,
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

static std::unique_ptr<juce::AudioPluginInstance> createMelodynePlugin (Engine& engine)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto araDescs = engine.getPluginManager().getARACompatiblePlugDescriptions();

    if (auto p = createMelodynePlugin (Engine::getInstance(), "VST3", araDescs))
        return p;

    return {};
}
