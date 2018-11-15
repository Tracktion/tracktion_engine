/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct MelodyneInstance
{
    ~MelodyneInstance()
    {
        if (vst3EntryPoint != nullptr)
            vst3EntryPoint->release();
    }

    ExternalPlugin::Ptr plugin;
    const ARAFactory* factory = nullptr;
    const ARAPlugInExtensionInstance* extensionInstance = nullptr;
    IPlugInEntryPoint* vst3EntryPoint = nullptr;
};

//==============================================================================
struct MelodyneInstanceFactory
{
public:
    static MelodyneInstanceFactory& getInstance()
    {
        auto& p = getInstancePointer();

        if (p == nullptr)
            p = new MelodyneInstanceFactory();

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

        ScopedPointer<MelodyneInstance> w (new MelodyneInstance());
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
    ScopedPointer<AudioPluginInstance> plugin;

    MelodyneInstanceFactory()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        plugin = createMelodynePlugin();

        if (plugin != nullptr)
        {
            getFactoryForPlugin();
            jassert (factory != nullptr);

            if (factory != nullptr)
            {
                if (canBeUsedAsTimeStretchEngine (*factory))
                {
                    setAssertionCallback();
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
        String type (plugin->getPluginDescription().pluginFormatName);

        if (type == "VST3") factory = getFactoryVST3();

      #if 0 // These other formats don't work so well
       #if JUCE_MAC
        else if (type == "AudioUnit") factory = getFactoryAU();
       #endif
        else if (type == "VST") factory = getFactoryVST();
      #endif
    }

    bool setExtensionInstance (MelodyneInstance& w, ARADocumentControllerRef dcRef)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (dcRef == nullptr)
            return false;

        String type (plugin->getPluginDescription().pluginFormatName);

        if (type == "VST3") return setExtensionInstanceVST3 (w, dcRef);

      #if 0 // These other formats don't work so well
       #if JUCE_MAC
        if (type == "AudioUnit") return setExtensionInstanceAU (w, dcRef);
       #endif
        if (type == "VST") return setExtensionInstanceVST (w, dcRef);
      #endif

        return false;
    }

    IPlugInEntryPoint* getVST3EntryPoint (AudioPluginInstance& p)
    {
        IPlugInEntryPoint* ep = nullptr;

        if (Steinberg::Vst::IComponent* component = (Steinberg::Vst::IComponent*) p.getPlatformSpecificData())
            component->queryInterface (IPlugInEntryPoint::iid, (void**) &ep);

        return ep;
    }

    ARAFactory* getFactoryVST3()
    {
        if (auto ep = getVST3EntryPoint (*plugin))
        {
            ARAFactory* f = const_cast<ARAFactory*> (ep->getFactory());
            ep->release();
            return f;
        }

        return {};
    }

    bool setExtensionInstanceVST3 (MelodyneInstance& w, ARADocumentControllerRef dcRef)
    {
        if (auto p = w.plugin->getAudioPluginInstance())
        {
            w.vst3EntryPoint = getVST3EntryPoint (*p);

            if (w.vst3EntryPoint != nullptr)
                w.extensionInstance = w.vst3EntryPoint->bindToDocumentController (dcRef);
        }

        return w.extensionInstance != nullptr;
    }

    static pointer_sized_int dispatchVST (AudioPluginInstance& p, VstIntPtr value, void* ptr)
    {
        return juce::VSTPluginFormat::dispatcher (&p, effVendorSpecific, kARAVendorID, value, ptr, 0.0f);
    }

    ARAFactory* getFactoryVST()
    {
        return (ARAFactory*) dispatchVST (*plugin, kARAEffGetFactory, nullptr);
    }

    bool setExtensionInstanceVST (MelodyneInstance& w, ARADocumentControllerRef dcRef)
    {
        if (auto p = w.plugin->getAudioPluginInstance())
            w.extensionInstance = (const ARAPlugInExtensionInstance*) dispatchVST (*p, kARAEffBindToDocumentController, (void*) dcRef);

        return w.extensionInstance != nullptr;
    }

   #if JUCE_MAC
    ARAFactory* getFactoryAU()
    {
        if (auto audioUnit = (AudioUnit) plugin->getPlatformSpecificData())
        {
            ARAAudioUnitFactory auFactory = { kARAAudioUnitMagic, nullptr };
            UInt32 size = sizeof (auFactory);

            AudioUnitGetProperty (audioUnit, kAudioUnitProperty_ARAFactory,
                                  kAudioUnitScope_Global, 0, &auFactory, &size);

            return const_cast<ARAFactory*> (auFactory.outFactory);
        }

        return {};
    }

    bool setExtensionInstanceAU (MelodyneInstance& w, ARADocumentControllerRef dcRef)
    {
        if (auto p = w.plugin->getAudioPluginInstance())
        {
            if (auto audioUnit = (AudioUnit) p->getPlatformSpecificData())
            {
                ARAAudioUnitPlugInExtensionBinding binding = { kARAAudioUnitMagic, dcRef, nullptr };
                UInt32 size = sizeof (binding);

                AudioUnitGetProperty (audioUnit, kAudioUnitProperty_ARAPlugInExtensionBinding,
                                      kAudioUnitScope_Global, 0, &binding, &size);

                w.extensionInstance = binding.outPlugInExtension;
            }
        }

        return w.extensionInstance != nullptr;
    }
   #endif

    static bool canBeUsedAsTimeStretchEngine (const ARAFactory& factory) noexcept
    {
        return (factory.supportedPlaybackTransformationFlags & kARAPlaybackTransformationTimestretch) != 0
            && (factory.supportedPlaybackTransformationFlags & kARAPlaybackTransformationTimestretchReflectingTempo) != 0;
    }

    void setAssertionCallback()
    {
        static ARAAssertFunction assertFunction = assertCallback;

        const ARAInterfaceConfiguration interfaceConfig =
        {
            kARAInterfaceConfigurationMinSize, kARACurrentAPIGeneration, &assertFunction
        };

        factory->initializeARAWithConfiguration (&interfaceConfig);
    }

    static void ARA_CALL assertCallback (ARAAssertCategory category, const void*, const char* diagnosis)
    {
        String categoryName;

        switch ((int) category)
        {
            case kARAAssertUnspecified:     categoryName = "Unspecified"; break;
            case kARAAssertInvalidArgument: categoryName = "Invalid Argument"; break;
            case kARAAssertInvalidState:    categoryName = "Invalid State"; break;
            case kARAAssertInvalidThread:   categoryName = "Invalid Thread"; break;
            default:                        categoryName = "(Unknown)"; break;
        };

        TRACKTION_LOG_ERROR ("ARA assertion -> \"" + categoryName + "\": " + String::fromUTF8 (diagnosis));
        jassertfalse;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyneInstanceFactory)
};

//==============================================================================
static AudioPluginInstance* createMelodynePlugin (const char* formatToTry,
                                                  const Array<PluginDescription*>& araDescs)
{
    CRASH_TRACER

    String error;

    for (auto pd : araDescs)
    {
        if (pd->pluginFormatName == formatToTry)
            if (auto p = Engine::getInstance().getPluginManager().pluginFormatManager
                            .createPluginInstance (*pd, 44100.0, 512, error))
                return p;
    }

    return {};
}

static AudioPluginInstance* createMelodynePlugin()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto araDescs = Engine::getInstance().getPluginManager().getARACompatiblePlugDescriptions();

    // Search in order of preference, as stated by Celemony:
    if (auto p = createMelodynePlugin ("VST3", araDescs))
        return p;

  #if 0 // These other formats don't work so well
   #if JUCE_MAC
    if (auto p = createMelodynePlugin ("AudioUnit", araDescs))
        return p;
   #endif

    if (auto p = createMelodynePlugin ("VST", araDescs))
        return p;
  #endif

    return {};
}
