/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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
    ~RackType();

    using Ptr = juce::ReferenceCountedObjectPtr<RackType>;

    juce::ValueTree createStateCopy (bool includeAutomation);
    juce::Result restoreStateFromValueTree (const juce::ValueTree&);

    //==============================================================================
    static Ptr createTypeToWrapPlugins (const Plugin::Array&, Edit&);

    //==============================================================================
    // tells the rack type about each instance that will be using it..
    // These are called by the initialise methods of the instances.
    void registerInstance (RackInstance*, const PlaybackInitialisationInfo&);
    void initialisePluginsIfNeeded (const PlaybackInitialisationInfo&) const;
    void deregisterInstance (RackInstance*);

    void updateAutomatableParamPositions (double time);

    double getLatencySeconds (double sampleRate, int blockSize);

    void newBlockStarted();

    /** Returns true if the RackType has a valid render context to use or if it is still loading. */
    bool isReadyToRender() const;
    void process (const AudioRenderContext&,
                  int leftInputGoesTo, float leftInputGain1, float leftInputGain2,
                  int rightInputGoesTo, float rightInputGain1, float rightInputGain2,
                  int leftOutComesFrom, float leftOutGain1, float leftOutGain2,
                  int rightOutComesFrom, float rightOutGain1, float rightOutGain2,
                  float dryGain,
                  juce::AudioBuffer<float>* delayBuffer,
                  int& delayPos);

    //==============================================================================
    juce::Array<Plugin*> getPlugins() const;

    // check whether this plugin's allowed to go in a rack, so e.g. racks
    // themselves aren't allowed.
    static bool isPluginAllowed (const Plugin::Ptr&);

    void addPlugin (const Plugin::Ptr&, juce::Point<float> pos, bool canAutoConnect);

    Plugin* getPluginForID (EditItemID);

    juce::Point<float> getPluginPosition (const Plugin::Ptr&) const;
    juce::Point<float> getPluginPosition (int index) const;
    void setPluginPosition (int index, juce::Point<float> pos);

    //==============================================================================
    juce::Array<const RackConnection*> getConnections() const noexcept;

    void addConnection (EditItemID source, int sourcePin,
                        EditItemID dest, int destPin);

    void removeConnection (EditItemID source, int sourcePin,
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

    juce::CriticalSection renderLock;   // make sure this will be deconstructed after
                                        // any subclass that may use it

    struct RackPluginList;
    std::unique_ptr<RackPluginList> pluginList;

    struct ConnectionList;
    std::unique_ptr<ConnectionList> connectionList;

    struct WindowStateList;
    std::unique_ptr<WindowStateList> windowStateList;

    mutable std::unique_ptr<ModifierList> modifierList;

    struct LatencyCalculation;
    mutable std::unique_ptr<LatencyCalculation> latencyCalculation;

    juce::Array<RackInstance*> activeRackInstances;
    int numberOfInstancesInEdit = 0;
    double sampleRate = 0;
    int blockSize = 0;
    bool isFirstCallbackOfBlock;
    juce::AudioBuffer<float> tempBufferOut, tempBufferIn;
    MidiMessageArray tempMidiBufferOut, tempMidiBufferIn;

    struct PluginRenderingInfo;
    struct RenderContext;
    std::shared_ptr<RenderContext> renderContext;
    AsyncCaller renderContextBuilder;
    std::atomic<int> numActiveInstances { 0 };

    //==============================================================================
    bool arePluginsConnectedIndirectly (EditItemID src, EditItemID dest, int depth = 0) const;
    void countInstancesInEdit();
    void updateTempBuferSizes();

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
    juce::ScopedPointer<ValueTreeList> list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackTypeList)
};

} // namespace tracktion_engine
