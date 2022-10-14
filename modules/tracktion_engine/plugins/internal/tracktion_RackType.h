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

struct RackConnection
{
    RackConnection (const juce::ValueTree&, juce::UndoManager*);

    juce::ValueTree state;

    juce::CachedValue<EditItemID> sourceID, destID;
    juce::CachedValue<int> sourcePin, destPin;
};


//==============================================================================
class RackType  : public Selectable,
                  public juce::ReferenceCountedObject,
                  public MacroParameterElement,
                  private juce::ValueTree::Listener
{
public:
    RackType (const juce::ValueTree&, Edit&);
    ~RackType() override;

    using Ptr = juce::ReferenceCountedObjectPtr<RackType>;

    juce::ValueTree createStateCopy (bool includeAutomation);
    juce::Result restoreStateFromValueTree (const juce::ValueTree&);

    //==============================================================================
    static Ptr createTypeToWrapPlugins (const Plugin::Array&, Edit&);

    //==============================================================================
    // tells the rack type about each instance that will be using it..
    // These are called by the initialise methods of the instances.
    void registerInstance (RackInstance*, const PluginInitialisationInfo&);
    void initialisePluginsIfNeeded (const PluginInitialisationInfo&) const;
    void deregisterInstance (RackInstance*);

    void updateAutomatableParamPositions (TimePosition);

    double getLatencySeconds (double sampleRate, int blockSize);

    //==============================================================================
    juce::Array<Plugin*> getPlugins() const;

    // check whether this plugin's allowed to go in a rack, so e.g. racks
    // themselves aren't allowed.
    static bool isPluginAllowed (const Plugin::Ptr&);

    /** Adds a plugin to the Rack optionally connecting it to the input and outputs.
        @returns    true if the plugin was added, false otherwise
    */
    bool addPlugin (const Plugin::Ptr&, juce::Point<float> pos, bool canAutoConnect);

    Plugin* getPluginForID (EditItemID);

    juce::Point<float> getPluginPosition (const Plugin::Ptr&) const;
    juce::Point<float> getPluginPosition (int index) const;
    void setPluginPosition (int index, juce::Point<float> pos);

    //==============================================================================
    juce::Array<const RackConnection*> getConnections() const noexcept;

    bool addConnection (EditItemID source, int sourcePin,
                        EditItemID dest, int destPin);

    bool removeConnection (EditItemID source, int sourcePin,
                           EditItemID dest, int destPin);

    // check that this doesn't create loops, etc
    bool isConnectionLegal (EditItemID source, int sourcePin,
                            EditItemID dest, int destPin) const;

    juce::Array<EditItemID> getPluginsWhichTakeInputFrom (EditItemID) const;

    void checkConnections();
    static void removeBrokenConnections (juce::ValueTree&, juce::UndoManager*);

    void createInstanceForSideChain (Track&, const juce::BigInteger& channelMask,
                                     EditItemID pluginID, int pinIndex);

    //==============================================================================
    juce::StringArray getInputNames() const;
    juce::StringArray getOutputNames() const;

    int addInput (int index, const juce::String& name);
    int addOutput (int index, const juce::String& name);
    void removeInput (int index);
    void removeOutput (int index);

    void renameInput (int index, const juce::String& name);
    void renameOutput (int index, const juce::String& name);

    bool pasteClipboard();

    //==============================================================================
    ModifierList& getModifierList() const noexcept;

    //==============================================================================
    juce::UndoManager* getUndoManager() const;

    //==============================================================================
    juce::String getSelectableDescription() override;

    //==============================================================================
    juce::StringArray getAudioInputNamesWithChannels() const;
    juce::StringArray getAudioOutputNamesWithChannels() const;

    static RackType::Ptr findRackTypeContaining (const Plugin&);

    static const char* getRackPresetPrefix() noexcept   { return "rackpreset::"; }

    //==============================================================================
    struct WindowState  : public PluginWindowState
    {
        WindowState (RackType&, juce::ValueTree windowStateTree);

        RackType& rack;
        juce::ValueTree state;
        JUCE_DECLARE_NON_COPYABLE (WindowState)
    };

    juce::Array<WindowState*> getWindowStates() const;

    void loadWindowPosition();
    void saveWindowPosition();
    void hideWindowForShutdown();

    Edit& edit;

    juce::ValueTree state;    // do not change the order of
    const EditItemID rackID;  // these two members!

    juce::CachedValue<juce::String> rackName;

private:
    struct PluginInfo
    {
        Plugin::Ptr plugin;
        juce::ValueTree state;
    };

    struct RackPluginList;
    std::unique_ptr<RackPluginList> pluginList;

    struct ConnectionList;
    std::unique_ptr<ConnectionList> connectionList;

    struct WindowStateList;
    std::unique_ptr<WindowStateList> windowStateList;

    mutable std::unique_ptr<ModifierList> modifierList;

    juce::Array<RackInstance*> activeRackInstances;
    int numberOfInstancesInEdit = 0;

    std::atomic<int> numActiveInstances { 0 };

    //==============================================================================
    bool arePluginsConnectedIndirectly (EditItemID src, EditItemID dest, int depth = 0) const;
    void countInstancesInEdit();

    void removeAllInputsAndOutputs();
    void addDefaultInputs();
    void addDefaultOutputs();

    void triggerUpdate();
    void updateRenderContext();

    //==============================================================================
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeParentChanged (juce::ValueTree&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
};


//==============================================================================
class RackTypeList
{
public:
    RackTypeList (Edit&);
    ~RackTypeList();

    void initialise (const juce::ValueTree&);

    int size() const;
    RackType::Ptr getRackType (int index) const;
    RackType::Ptr getRackTypeForID (EditItemID) const;
    RackType::Ptr findRackContaining (Plugin&) const;
    RackType::Ptr addRackTypeFrom (const juce::ValueTree&);
    RackType::Ptr addNewRack();
    void removeRackType (const RackType::Ptr& type);
    void importRackFiles (const juce::Array<juce::File>&);

    const juce::Array<RackType*>& getTypes() const noexcept;

private:
    Edit& edit;
    juce::ValueTree state;

    struct ValueTreeList;
    std::unique_ptr<ValueTreeList> list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackTypeList)
};

}} // namespace tracktion { inline namespace engine
