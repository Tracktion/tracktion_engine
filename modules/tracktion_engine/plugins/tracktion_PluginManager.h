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

class PluginManager  : private juce::ChangeListener
{
public:
    PluginManager (Engine&);
    ~PluginManager() override;

    void initialise();

   #if TRACKTION_AIR_WINDOWS
    void initialiseAirWindows();
   #endif

    //==============================================================================
    static bool startChildProcessPluginScan (const juce::String& commandLine);

    bool areGUIsLockedByDefault();
    void setGUIsLockedByDefault (bool);

    bool doubleClickToOpenWindows();
    void setDoubleClickToOpenWindows (bool);

    int getNumberOfThreadsForScanning();
    void setNumberOfThreadsForScanning (int);

    bool usesSeparateProcessForScanning();
    void setUsesSeparateProcessForScanning (bool);

    //==============================================================================
    Plugin::Ptr createExistingPlugin (Edit&, const juce::ValueTree&);
    Plugin::Ptr createNewPlugin (Edit&, const juce::ValueTree&);
    Plugin::Ptr createNewPlugin (Edit&, const juce::String& type, const juce::PluginDescription&);

    juce::Array<juce::PluginDescription> getARACompatiblePlugDescriptions();

    juce::AudioPluginFormatManager pluginFormatManager;
    juce::KnownPluginList knownPluginList;

    //==============================================================================
    struct BuiltInType
    {
        BuiltInType (const juce::String& type);
        virtual ~BuiltInType();

        const juce::String type;

        virtual Plugin::Ptr create (PluginCreationInfo) = 0;
    };

    void registerBuiltInType (std::unique_ptr<BuiltInType>);

    //==============================================================================
    template <typename Type>
    struct BuiltInTypeBase  : public PluginManager::BuiltInType
    {
        BuiltInTypeBase() : BuiltInType (Type::xmlTypeName) {}
        Plugin::Ptr create (PluginCreationInfo info) override   { return new Type (info); }
    };

    template <typename Type>
    void createBuiltInType()  { registerBuiltInType (std::make_unique<BuiltInTypeBase<Type>>()); }

    //==============================================================================
    /** Callback that is used to create plugin instances from a PluginDescription.
        By default this simply uses the PluginManager's pluginFormatManager but it
        can be set to provide custom behaviour.
    */
    std::function<std::unique_ptr<juce::AudioPluginInstance> (const juce::PluginDescription&,
                                                              double rate, int blockSize,
                                                              juce::String& errorMessage)> createPluginInstance;

    /** Callback that is used to determine if a plugin should use fine-grain automation or not. */
    std::function<bool (Plugin&)> canUseFineGrainAutomation;

    // this can be set to provide a function that gets called when a scan finishes
    std::function<void()> scanCompletedCallback;

private:
    Engine& engine;

    juce::CriticalSection existingListLock;
    juce::OwnedArray<BuiltInType> builtInTypes;
    bool initialised = false;

    Plugin::Ptr createPlugin (Edit&, const juce::ValueTree&, bool isNew);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
};

//==============================================================================
class PluginCache  : private juce::Timer
{
public:
    PluginCache (Edit&);
    ~PluginCache() override;

    //==============================================================================
    Plugin::Ptr getPluginFor (EditItemID pluginID) const;
    Plugin::Ptr getPluginFor (const juce::ValueTree&) const;
    Plugin::Ptr getPluginFor (juce::AudioProcessor&) const;
    Plugin::Ptr getOrCreatePluginFor (const juce::ValueTree&);
    Plugin::Ptr createNewPlugin (const juce::ValueTree&);
    Plugin::Ptr createNewPlugin (const juce::String& type, const juce::PluginDescription&);

    Plugin::Array getPlugins() const;

    //==============================================================================
    /** Callback which can be set to be notified of when a new plugin is added. */
    std::function<void(const Plugin&)> newPluginAddedCallback;

private:
    Edit& edit;
    Plugin::Array activePlugins;
    juce::CriticalSection lock;

    Plugin::Ptr addPluginToCache (Plugin::Ptr);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginCache)
};

}} // namespace tracktion { inline namespace engine
