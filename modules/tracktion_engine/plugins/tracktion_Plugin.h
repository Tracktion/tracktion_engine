/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct PluginCreationInfo
{
    PluginCreationInfo (const PluginCreationInfo&) = default;
    PluginCreationInfo (PluginCreationInfo&&) = default;
    PluginCreationInfo& operator= (PluginCreationInfo&&) = default;

    PluginCreationInfo (Edit& ed, const juce::ValueTree& s, bool isNew) noexcept
        : edit (ed), state (s), isNewPlugin (isNew) {}

    PluginCreationInfo() = delete;
    PluginCreationInfo& operator= (const PluginCreationInfo&) = delete;

    Edit& edit;
    juce::ValueTree state;
    bool isNewPlugin = false;
};


//==============================================================================
class Plugin  : public Selectable,
                public juce::ReferenceCountedObject,
                public Exportable,
                public AutomatableEditItem,
                public MacroParameterElement,
                protected ValueTreeAllEventListener
{
public:
    Plugin (PluginCreationInfo);
    ~Plugin();

    void selectableAboutToBeDeleted() override;

    //==============================================================================
    using Ptr      = juce::ReferenceCountedObjectPtr<Plugin>;
    using WeakRef  = juce::WeakReference<Plugin>;
    using Array    = juce::ReferenceCountedArray<Plugin>;

    /** called by the system to let the plugin manage its automation stuff */
    void playStartedOrStopped();

    /** Gives the plugin a chance to do extra initialisation when it's been added
        to an edit
    */
    virtual void initialiseFully();

    virtual void flushPluginStateToValueTree() override;

    //==============================================================================
    /** The name of the type, e.g. "Compressor" */
    virtual juce::String getName() override = 0;
    virtual juce::String getPluginType() = 0;

    virtual juce::String getVendor()                              { return "Tracktion"; }
    virtual juce::String getShortName (int /*suggestedLength*/)   { return getName(); }

    /** A unique string to idenitify plugin independant of install location */
    virtual juce::String getIdentifierString()                    { return getPluginType(); }

    /** default returns the name, others can return special stuff if needed */
    virtual juce::String getTooltip();

    //==============================================================================
    /** Enable/disable the plugin.  */
    virtual void setEnabled (bool);
    bool isEnabled() const noexcept                         { return enabled; }

    /** This is a bit different to being enabled as when frozen a plugin can't be interacted with. */
    void setFrozen (bool shouldBeFrozen)                    { frozen = shouldBeFrozen; }
    bool isFrozen() const noexcept                          { return frozen; }

    /** Enable/Disable processing. If processing is disabled, plugin should minimize memory usage
        and release any resources possilbe */
    void setProcessingEnabled (bool p)                      { processing = p; }
    bool isProcessingEnabled() const noexcept               { return processing; }

    //==============================================================================
    /** Gives the plugin a chance to set itself up before being played

        the sample rate and the average block size - although the blocks
        won't always be the same, and may be bigger
    */
    virtual void initialise (const PlaybackInitialisationInfo&) = 0;

    /** tells the plugin that the audio graph has changed but the plugin isn't being
        re-initialised - i.e. it's being re-used, maybe by being moved to a different
        track, etc.
    */
    virtual void initialiseWithoutStopping (const PlaybackInitialisationInfo&)  {}

    /** called after play stops */
    virtual void deinitialise() = 0;

    /** should reset synth voices, tails, clear delay buffers, etc. */
    virtual void reset();

    //==============================================================================
    /** Process the next block of data.

        The incoming buffer will have an unknown number of channels, and the plugin has to deal
        with them however it wants to.

        The buffer should be resized to the number of output channels that the plugin wants to
        return (which should be the same or less than the number of output channel names it returns
        from getChannelNames() - never more than this).

        Other parameters and principles are similar to AudioNode::readBlock()
    */
    virtual void applyToBuffer (const AudioRenderContext&) = 0;

    /** Called between successive rendering blocks. */
    virtual void prepareForNextBlock (const AudioRenderContext&) {}

    // wrapper on applyTobuffer, called by the node
    void applyToBufferWithAutomation (const AudioRenderContext&);

    /** Creates a new audio node that will render this plugin. */
    AudioNode* createAudioNode (AudioNode* input, bool applyAntiDenormalisationNoise);

    double getCpuUsage() const noexcept     { return juce::jlimit (0.0, 1.0, timeToCpuScale * cpuUsageMs.load()); }

    //==============================================================================
    /** This must return the number of output channels that the plugin will produce, given
        a number of input channels.

        E.g. some might be able to do mono, so will return 1 if the input is 1, 2 if it is 2, etc.

        The default impl just returns the number of items that getChannelNames() returns.
    */
    virtual int getNumOutputChannelsGivenInputs (int numInputChannels);

    virtual bool producesAudioWhenNoAudioInput()        { return isAutomationNeeded(); }
    virtual bool noTail()                               { return true; }

    virtual void getChannelNames (juce::StringArray* ins, juce::StringArray* outs);
    virtual bool takesAudioInput()                      { return ! isSynth(); }
    virtual bool takesMidiInput()                       { return false; }
    virtual bool isSynth()                              { return false; }
    virtual int getLatencySamples()                     { return 0; }
    virtual double getLatencySeconds()                  { return 0.0; }
    virtual double getTailLength() const                { return 0.0; }
    virtual bool mustBePlayedLiveWhenOnAClip() const    { return false; }
    virtual bool canSidechain();

    juce::StringArray getInputChannelNames();
    juce::StringArray getSidechainSourceNames (bool allowNone);
    void setSidechainSourceByName (const juce::String& name);
    juce::String getSidechainSourceName();
    void guessSidechainRouting();

    //==============================================================================
    struct Wire
    {
        Wire (const juce::ValueTree&, juce::UndoManager*);

        juce::ValueTree state;
        juce::CachedValue<int> sourceChannelIndex, destChannelIndex;
    };

    //==============================================================================
    int getNumWires() const;
    Wire* getWire (int index) const;

    void makeConnection (int srcChannel, int dstChannel, juce::UndoManager*);
    void breakConnection (int srcChannel, int dstChannel);

    //==============================================================================
    /** If it's a synth that names its notes, this can return the name it uses for this note 0-127.
        Midi channel is 1-16
    */
    virtual bool hasNameForMidiNoteNumber (int note, int midiChannel, juce::String& name);

    /** Returns the name for a midi program, if there is one.
        programNum = 0 to 127.
    */
    virtual bool hasNameForMidiProgram (int programNum, int bank, juce::String& name);
    virtual bool hasNameForMidiBank (int bank, juce::String& name);

    //==============================================================================
    virtual bool canBeAddedToClip()                                     { return true; }
    virtual bool canBeAddedToRack()                                     { return true; }
    virtual bool canBeAddedToFolderTrack()                              { return false; }
    virtual bool canBeAddedToMaster()                                   { return true; }
    virtual bool canBeDisabled()                                        { return true; }
    virtual bool canBeMoved()                                           { return true; }
    virtual bool needsConstantBufferSize() = 0;

    /** for things like VSTs where the DLL is missing.    */
    virtual bool isMissing()                                            { return false; }

    /** Plugins can be disabled to avoid them crashing Edits. */
    virtual bool isDisabled()                                           { return false; }

    bool isInRack() const;
    juce::ReferenceCountedObjectPtr<RackType> getOwnerRackType() const;

    bool isClipEffectPlugin() const;

    //==============================================================================
    AutomatableParameter::Ptr getQuickControlParameter() const;
    void setQuickControlParameter (AutomatableParameter*);

    //==============================================================================
    /** Attempts to delete this plugin, whether it's a master plugin, track plugin, etc. */
    virtual void deleteFromParent();

    // Detaches the plugin from any parent it might be in. This is a little more complicated
    // than just removing its ValueTree from its parent one.
    void removeFromParent();

    //==============================================================================
    /** Returns the track if it's a track or clip plugin. */
    Track* getOwnerTrack() const;

    /** Returns the clip if that's what it's in. */
    Clip* getOwnerClip() const;

    PluginList* getOwnerList() const;

    Ptr findPluginThatFeedsIntoThis() const;
    Ptr findPluginThatThisFeedsInto() const;

    /** method from Selectable, that's been overridden here to also tell the edit that it's changed. */
    void changed() override;

    //==============================================================================
    juce::Array<ReferencedItem> getReferencedItems() override;
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

    /** Called when ProjectItem sources are re-assigned so you can reload from the new source. */
    virtual void sourceMediaChanged()  {}

    //==============================================================================
    static bool areSelectedPluginsRackable (SelectionManager&);
    static RackInstance* wrapSelectedPluginsInRack (SelectionManager&);
    static void sortPlugins (Plugin::Array&);

    //==============================================================================
    // setting a master plugin whose settings will be mirrored
    bool setPluginToMirror (const Plugin::Ptr&); // fails if type is wrong
    virtual void updateFromMirroredPluginIfNeeded (Plugin&) {}
    Plugin::Ptr getMirroredPlugin() const;

    //==============================================================================
    enum class Type
    {
        allPlugins,
        folderTrackPlugins,
        effectPlugins,
    };

    //==============================================================================
    bool baseClassNeedsInitialising() const noexcept        { return initialiseCount == 0; }
    void baseClassInitialise (const PlaybackInitialisationInfo&);
    void baseClassDeinitialise();

    //==============================================================================
    void setSidechainSourceID (EditItemID newID)            { sidechainSourceID = newID; }
    EditItemID getSidechainSourceID() const                 { return sidechainSourceID; }

    //==============================================================================
    struct WindowState  : public PluginWindowState
    {
        WindowState (Plugin&);

        Plugin& plugin;

    private:
        WindowState() = delete;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowState)
    };

    std::unique_ptr<WindowState> windowState;

    void showWindowExplicitly();
    void hideWindowForShutdown();

    //==============================================================================
    Engine& engine;
    juce::ValueTree state;

    juce::UndoManager* getUndoManager() const noexcept;

protected:
    //==============================================================================
    juce::CachedValue<bool> enabled, frozen, processing;
    juce::CachedValue<juce::String> quickParamName;
    juce::CachedValue<EditItemID> masterPluginID, sidechainSourceID;

    double sampleRate = 44100.0;
    int blockSizeSamples = 512;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChanged() override;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    virtual void processingChanged();

    //==============================================================================
    AutomatableParameter* addParam (const juce::String& paramID, const juce::String& name, juce::Range<float> valueRange);
    AutomatableParameter* addParam (const juce::String& paramID, const juce::String& name, juce::Range<float> valueRange,
                                    std::function<juce::String(float)> valueToStringFunction,
                                    std::function<float(const juce::String&)> stringToValueFunction);

    //==============================================================================
    static void getLeftRightChannelNames (juce::StringArray* ins, juce::StringArray* outs);
    static void getLeftRightChannelNames (juce::StringArray* chans);

private:
    friend class PluginAudioNode;
    friend class FineGrainPluginAudioNode;

    mutable AutomatableParameter::Ptr quickControlParameter;

    int initialiseCount = 0;
    double timeToCpuScale = 0;
    std::atomic<double> cpuUsageMs { 0 };

    juce::ValueTree getConnectionsTree();
    struct WireList;
    juce::ScopedPointer<WireList> sidechainWireList;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Plugin)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Plugin)
};

} // namespace tracktion_engine
