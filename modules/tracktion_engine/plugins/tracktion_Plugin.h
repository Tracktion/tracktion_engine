/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** Holds the information needed to construct a Plugin: the owning Edit,
    the ValueTree that stores the plugin's persistent state, and whether
    this is a brand-new insertion vs. a reload from saved state.
*/
struct PluginCreationInfo
{
    PluginCreationInfo (const PluginCreationInfo&) = default;
    PluginCreationInfo (PluginCreationInfo&&) = default;
    PluginCreationInfo& operator= (PluginCreationInfo&&) = delete;

    PluginCreationInfo (Edit& ed, const juce::ValueTree& s, bool isNew) noexcept
        : edit (ed), state (s), isNewPlugin (isNew) {}

    PluginCreationInfo() = delete;
    PluginCreationInfo& operator= (const PluginCreationInfo&) = delete;

    Edit& edit;
    juce::ValueTree state;
    bool isNewPlugin = false;
};

//==============================================================================
/** Passed into Plugins when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PluginInitialisationInfo
{
    TimePosition startTime;
    double sampleRate;
    int blockSizeSamples;
};

//==============================================================================
//==============================================================================
/**
    The context passed to plugin render methods to provide it with buffers to fill.
*/
struct PluginRenderContext
{
    PluginRenderContext (juce::AudioBuffer<float>* buffer,
                         int bufferStart, int bufferSize,
                         MidiMessageArray* midiBuffer, double midiOffset,
                         TimeRange editTime, bool playing, bool scrubbing, bool rendering,
                         bool allowBypassedProcessing) noexcept;

    /** @deprecated Use the constructor without the bufferChannels parameter.
        The destBufferChannels field was never used by the engine.
    */
    PluginRenderContext (juce::AudioBuffer<float>* buffer,
                         const juce::AudioChannelSet& bufferChannels,
                         int bufferStart, int bufferSize,
                         MidiMessageArray* midiBuffer, double midiOffset,
                         TimeRange editTime, bool playing, bool scrubbing, bool rendering,
                         bool allowBypassedProcessing) noexcept;

    /** Creates a copy of another PluginRenderContext. */
    PluginRenderContext (const PluginRenderContext&) = default;
    PluginRenderContext (PluginRenderContext&&) = default;

    /** Deleted assignment operators. */
    PluginRenderContext& operator= (const PluginRenderContext&) = delete;
    PluginRenderContext& operator= (PluginRenderContext&&) = delete;

    /** The target audio buffer which needs to be filled.
        This may be nullptr if no audio is being processed.
    */
    juce::AudioBuffer<float>* destBuffer = nullptr;

    /** @deprecated This field is not used by the engine and will be removed in a future version. */
    juce::AudioChannelSet destBufferChannels;

    /** The index of the start point in the audio buffer from which data must be written. */
    int bufferStartSample = 0;

    /** The number of samples to write into the audio buffer. */
    int bufferNumSamples = 0;

    /** A buffer of MIDI events to process.
        This may be nullptr if no MIDI is being sent
    */
    MidiMessageArray* bufferForMidiMessages = nullptr;

    /** A time offset to add to the timestamp of any events in the MIDI buffer. */
    double midiBufferOffset = 0.0;

    /** The edit time range this context represents. */
    TimeRange editTime;

    /** True if the the playhead is currently playing. */
    bool isPlaying = false;

    /** True if the the audio is currently being scrubbed. */
    bool isScrubbing = false;

    /** True if the rendering is happening as part of an offline render rather than live playback. */
    bool isRendering = false;

    /** If this is true and the plugin supports it, this will call the bypassed processing method of the plugin.
        If this is false, the plugin simply won't be processed. This can be used to ensure bypassed plugins
        still introduce their reported latency.
    */
    bool allowBypassedProcessing = false;
};


//==============================================================================
//==============================================================================
/** Base class for all audio plugins in the Tracktion Engine.

    A Plugin lives inside a PluginList (owned by a Track, Clip, or RackType)
    and processes audio/MIDI via applyToBuffer(). Its persistent state is stored
    in a juce::ValueTree, and it supports automation, sidechain routing, and
    mirroring.

    Subclasses include ExternalPlugin (hosted VST/AU/etc.), and the built-in
    internal plugins (volume/pan, EQ, delay, etc.).

    @par Lifecycle

    Plugins are created via PluginCreationInfo, then prepared for playback with
    initialise() (called indirectly through baseClassInitialise()) and torn down
    with deinitialise() (via baseClassDeinitialise()). Between those calls the
    audio graph may call applyToBuffer() on the audio thread.

    @par Threading

    applyToBuffer() runs on the audio thread -- implementations must not
    allocate, lock, or perform I/O. Most other methods are message-thread only
    unless noted otherwise.
*/
class Plugin  : public Selectable,
                public juce::ReferenceCountedObject,
                public Exportable,
                public AutomatableEditItem,
                public MacroParameterElement,
                protected ValueTreeAllEventListener
{
public:
    Plugin (PluginCreationInfo);
    ~Plugin() override;

    void selectableAboutToBeDeleted() override;

    //==============================================================================
    using Ptr      = juce::ReferenceCountedObjectPtr<Plugin>;
    using Array    = juce::ReferenceCountedArray<Plugin>;

    /** called by the system to let the plugin manage its automation stuff */
    void playStartedOrStopped();

    /** Gives the plugin a chance to do extra initialisation when it's been added
        to an edit
    */
    virtual void initialiseFully();

    virtual void flushPluginStateToValueTree() override;

    //==============================================================================
    // Naming & identity

    /** The name of the type, e.g. "Compressor" */
    virtual juce::String getName() const override = 0;

    /** Returns a string identifying the plugin type (used as the XML tag name). */
    virtual juce::String getPluginType() = 0;

    /** Returns the plugin vendor/manufacturer name. */
    virtual juce::String getVendor()                              { return "Tracktion"; }

    /** Returns an abbreviated name, ideally fitting within suggestedLength characters. */
    virtual juce::String getShortName (int /*suggestedLength*/)   { return getName(); }

    /** Returns the custom name if set, otherwise the built-in getName(). */
    juce::String getDisplayName() const;

    /** Returns the raw custom name (may be empty). */
    juce::String getCustomName() const;

    /** Sets a custom display name. Trimmed and limited to 64 chars. Empty clears. */
    void setCustomName (const juce::String&);

    /** A unique string to identify a plugin independent of install location. */
    virtual juce::String getIdentifierString()                    { return getPluginType(); }

    /** Returns a tooltip string for UI display. */
    virtual juce::String getTooltip();

    //==============================================================================
    // Enable / freeze / processing state

    /** Enable/disable the plugin. */
    virtual void setEnabled (bool);
    bool isEnabled() const noexcept                         { return enabled.get(); }

    /** Freezing is stronger than disabling -- a frozen plugin cannot be interacted with at all. */
    void setFrozen (bool shouldBeFrozen);
    bool isFrozen() const noexcept                          { return frozen; }

    /** Enable/disable processing. When processing is disabled, the plugin should minimise
        memory usage and release any resources possible. */
    void setProcessingEnabled (bool p)                      { processing = p; }
    bool isProcessingEnabled() const noexcept               { return processing; }

    //==============================================================================
    /** Gives the plugin a chance to set itself up before being played.
        This won't be called concurrently with the process thread.

        The sample rate and the average block size - although the blocks
        won't always be the same, and may be bigger.

        Don't call this directly or the initialise count will become out of sync.
        @see baseClassInitialise
        [[ message_thread ]]
    */
    virtual void initialise (const PluginInitialisationInfo&) = 0;

    /** Tells the plugin that the audio graph has changed but the plugin isn't being
        re-initialised - i.e. it's being re-used, maybe by being moved to a different
        track, etc.
        This can be called concurrently whilst the plugin is being processed so
        implementations of it must be thread safe.
        [[ message_thread ]]
    */
    virtual void initialiseWithoutStopping (const PluginInitialisationInfo&)  {}

    /** Called after play stops to release resources.
        Don't call this directly or the initialise count will become out of sync.
        @see baseClassDeinitialise
    */
    virtual void deinitialise() = 0;

    /** Should reset synth voices, tails, clear delay buffers, etc. */
    virtual void reset();

    /** Track name or colour has changed. */
    virtual void trackPropertiesChanged();

    /** Tells the plugin to turn off any playing notes, if applicable */
    virtual void midiPanic();

    //==============================================================================
    /** Process the next block of data.

        The incoming buffer will have an unknown number of channels, and the plugin has to deal
        with them however it wants to.

        The buffer should be resized to the number of output channels that the plugin wants to
        return (which should be the same or less than the number of output channel names it returns
        from getChannelNames() - never more than this).
    */
    virtual void applyToBuffer (const PluginRenderContext&) = 0;

    /** Called between successive rendering blocks. */
    virtual void prepareForNextBlock (TimePosition /*editTime*/) {}

    // wrapper on applyTobuffer, called by the node
    void applyToBufferWithAutomation (const PluginRenderContext&);

    //==============================================================================
    /** Plugins can return false if they want to avoid the overhead of measuring the CPU usage.
        It's a small overhead but with many tracks, the level meters and vol/pan plugins can make a difference.
    */
    virtual bool shouldMeasureCpuUsage() const noexcept  { return true; }

    /** Returns the proportion of the current buffer size spent processing this plugin. */
    double getCpuUsage() const noexcept     { return juce::jlimit (0.0, 1.0, timeToCpuScale * cpuUsageMs.load()); }

    //==============================================================================
    // Channel configuration & audio characteristics

    /** Returns the number of output channels this plugin will produce for track routing.

        E.g. some might be able to do mono, so will return 1 if the input is 1, 2 if it is 2, etc.

        The default impl returns the number of items that getChannelNames() returns.
        Subclasses with sidechain buses must override this to return only the main bus
        output count, since sidechain channels are routed separately.
    */
    virtual int getNumOutputChannelsGivenInputs (int numInputChannels);

    /** Returns true if the plugin can produce audio when its input is silent
        (e.g. synths, plugins with tails, or plugins driven by automation). */
    virtual bool producesAudioWhenNoAudioInput()        { return isAutomationNeeded(); }

    /** Returns true if the plugin has no tail (i.e. output is silent once input stops). */
    virtual bool noTail()                               { return true; }

    /** Fills the provided StringArrays with the names of this plugin's input and/or
        output channels. Either pointer may be nullptr if not needed. */
    virtual void getChannelNames (juce::StringArray* ins, juce::StringArray* outs);

    /** Returns the main bus input channel configuration for this plugin.
        Default implementation returns stereo for backward compatibility.
    */
    virtual ChannelConfiguration getMainBusInputChannelConfiguration() const;

    /** Returns the main bus output channel configuration for this plugin.
        Default implementation returns stereo for backward compatibility.
    */
    virtual ChannelConfiguration getMainBusOutputChannelConfiguration() const;

    virtual bool takesAudioInput()                      { return ! isSynth(); }
    virtual bool takesMidiInput()                       { return false; }
    virtual bool isSynth()                              { return false; }

    /** Returns the plugin's processing latency in seconds. */
    virtual double getLatencySeconds()                  { return 0.0; }

    /** Returns the length of the plugin's tail in seconds (e.g. reverb decay). */
    virtual double getTailLength() const                { return 0.0; }

    /** Returns true if this plugin supports sidechain input.
        Default checks whether getChannelNames() reports more inputs than the main bus. */
    virtual bool canSidechain();

    //==============================================================================
    // Parameters

    /** Registers a new automatable parameter owned by this plugin. */
    AutomatableParameter* addParam (const juce::String& paramID, const juce::String& name, juce::NormalisableRange<float> valueRange);

    /** Registers a new automatable parameter with custom string conversion functions. */
    AutomatableParameter* addParam (const juce::String& paramID, const juce::String& name, juce::NormalisableRange<float> valueRange,
                                    std::function<juce::String(float)> valueToStringFunction,
                                    std::function<float(const juce::String&)> stringToValueFunction);

    //==============================================================================
    // Sidechain routing

    /** Returns the names of this plugin's input channels (convenience wrapper around getChannelNames). */
    juce::StringArray getInputChannelNames();

    /** Returns the names of available sidechain sources for this plugin.
        If allowNone is true, includes a "none" entry. */
    juce::StringArray getSidechainSourceNames (bool allowNone);

    /** Sets the sidechain source by its display name. */
    void setSidechainSourceByName (const juce::String& name);

    /** Returns the display name of the current sidechain source. */
    juce::String getSidechainSourceName();

    /** Attempts to automatically determine a sensible sidechain routing. */
    void guessSidechainRouting();

    //==============================================================================
    // Sidechain wiring
    //
    // Wires describe channel-level connections used for sidechain routing.

    /** Represents a single channel-to-channel connection for sidechain routing. */
    struct Wire
    {
        Wire (const juce::ValueTree&, juce::UndoManager*);

        juce::ValueTree state;
        juce::CachedValue<int> sourceChannelIndex, destChannelIndex;
    };

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
    // Placement constraints & status

    virtual bool canBeAddedToClip()                                     { return true; }
    virtual bool canBeAddedToRack()                                     { return true; }
    virtual bool canBeAddedToFolderTrack()                              { return false; }
    virtual bool canBeAddedToMaster()                                   { return true; }
    virtual bool canBeDisabled()                                        { return true; }
    virtual bool canBeMoved()                                           { return true; }

    /** Returns true if this plugin requires a constant audio buffer size (no variable-size blocks). */
    virtual bool needsConstantBufferSize()                              { return false; }

    /** Returns true if the plugin's backing file/DLL is missing (e.g. uninstalled VST). */
    virtual bool isMissing()                                            { return false; }

    /** Returns true if the plugin has been forcibly disabled (e.g. after a crash). */
    virtual bool isDisabled()                                           { return false; }

    /** Returns true if this plugin lives inside a RackType. */
    bool isInRack() const;
    juce::ReferenceCountedObjectPtr<RackType> getOwnerRackType() const;

    /** Returns true if this plugin is being used as a clip effect. */
    bool isClipEffectPlugin() const;

    /** Returns the underlying juce::AudioProcessor, if this plugin wraps one.
        Only ExternalPlugin returns non-null by default. */
    virtual juce::AudioProcessor* getWrappedAudioProcessor() const      { return {}; }

    //==============================================================================
    // Quick control
    //
    // A single parameter nominated for one-knob control surfaces.

    AutomatableParameter::Ptr getQuickControlParameter() const;
    void setQuickControlParameter (AutomatableParameter*);

    //==============================================================================
    /** Attempts to delete this plugin, whether it's a master plugin, track plugin, etc.
        This will call removeFromParent but also hide any automation parameters etc. being
        shown on tracks and hide plugin windows etc.
        Use this method if the plugin is being fully deleted from the Edit.
    */
    virtual void deleteFromParent();

    /** Detaches the plugin from any parent it might be in. This is a little more complicated
        than just removing its ValueTree from its parent one.
        Use this method if the plugin is to be inserted somewhere else in the Edit.
    */
    void removeFromParent();

    //==============================================================================
    // Ownership & graph position

    /** Returns the track if it's a track or clip plugin. */
    Track* getOwnerTrack() const;

    /** Returns the clip if that's what it's in. */
    Clip* getOwnerClip() const;

    /** Returns the PluginList that contains this plugin. */
    PluginList* getOwnerList() const;

    /** Returns the previous plugin in the same PluginList, or nullptr. */
    Ptr findPluginThatFeedsIntoThis() const;

    /** Returns the next plugin in the same PluginList, or nullptr. */
    Ptr findPluginThatThisFeedsInto() const;

    /** Marks the plugin as changed and notifies the Edit. */
    void changed() override;

    //==============================================================================
    juce::Array<ReferencedItem> getReferencedItems() override;
    void reassignReferencedItem (const ReferencedItem&, ProjectItemRef newRef, double newStartTime) override;

    /** Called when ProjectItem sources are re-assigned so you can reload from the new source. */
    virtual void sourceMediaChanged()  {}

    //==============================================================================
    static bool areSelectedPluginsRackable (SelectionManager&);
    static RackInstance* wrapSelectedPluginsInRack (SelectionManager&);
    static void sortPlugins (Plugin::Array&);
    static void sortPlugins (std::vector<Plugin*>&);

    //==============================================================================
    // Plugin mirroring
    //
    // Allows one plugin to mirror the state of another (e.g. linked EQs).

    /** Sets a master plugin whose settings this one will mirror. Returns false if the types don't match. */
    bool setPluginToMirror (const Plugin::Ptr&);

    /** Called to pull state from the mirrored master. Override to apply format-specific state. */
    virtual void updateFromMirroredPluginIfNeeded (Plugin&) {}

    /** Returns the plugin this one is mirroring, or nullptr. */
    Plugin::Ptr getMirroredPlugin() const;

    //==============================================================================
    /** Filter categories used when enumerating plugins. */
    enum class Type
    {
        allPlugins,
        folderTrackPlugins,
        effectPlugins,
    };

    //==============================================================================
    // Base class lifecycle helpers
    //
    // Call baseClassInitialise / baseClassDeinitialise instead of initialise / deinitialise
    // directly, so the internal reference count stays in sync.

    /** Returns true if baseClassInitialise() has not yet been called (or has been fully deinitialised). */
    bool baseClassNeedsInitialising() const noexcept        { return initialiseCount == 0; }

    /** Prepares the plugin for playback. Calls initialise() and increments the internal init count. */
    void baseClassInitialise (const PluginInitialisationInfo&);

    /** Tears down the plugin after playback. Calls deinitialise() when the init count reaches zero. */
    void baseClassDeinitialise();

    //==============================================================================
    void setSidechainSourceID (EditItemID newID)            { sidechainSourceID = newID; }
    EditItemID getSidechainSourceID() const                 { return sidechainSourceID; }

    //==============================================================================
    // Editor & window

    /** Base class for plugin editor components (custom UIs shown in plugin windows). */
    struct EditorComponent  : public juce::Component
    {
        virtual bool allowWindowResizing() = 0;
        virtual juce::ComponentBoundsConstrainer* getBoundsConstrainer() = 0;
    };

    /** Creates a custom editor component for this plugin, or nullptr if none. */
    virtual std::unique_ptr<EditorComponent> createEditor()     { return {}; }

    /** Persistent state for the plugin's editor window (position, visibility, etc.). */
    struct WindowState  : public PluginWindowState
    {
        WindowState (Plugin&);

        Plugin& plugin;

    private:
        WindowState() = delete;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowState)
    };

    std::unique_ptr<WindowState> windowState;

    /** Shows the plugin's editor window, creating it if necessary. */
    void showWindowExplicitly();

    /** Hides the plugin window during shutdown (avoids dangling references). */
    void hideWindowForShutdown();

    //==============================================================================
    Engine& engine;

    /** The ValueTree that stores this plugin's persistent state within the Edit. */
    juce::ValueTree state;

    juce::UndoManager* getUndoManager() const noexcept;

    //==============================================================================
    /** @internal */
    bool isInitialising() const             { return isInitialisingFlag; }

protected:
    //==============================================================================
    // Persistent state (backed by the ValueTree)

    juce::CachedValue<AtomicWrapper<bool>> enabled;
    juce::CachedValue<bool> frozen, processing;
    juce::CachedValue<juce::String> quickParamName;
    juce::CachedValue<juce::String> customName;
    juce::CachedValue<EditItemID> masterPluginID, sidechainSourceID;

    /** Current sample rate and block size, set during baseClassInitialise(). */
    double sampleRate = 44100.0;
    int blockSizeSamples = 512;

    //==============================================================================
    // ValueTree callbacks

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChanged() override;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;

    /** Called when the processing enabled state changes. */
    virtual void processingChanged();

    //==============================================================================
    /** Helper to populate stereo left/right channel name arrays. */
    static void getLeftRightChannelNames (juce::StringArray* ins, juce::StringArray* outs);
    static void getLeftRightChannelNames (juce::StringArray* chans);

private:
    mutable AutomatableParameter::Ptr quickControlParameter;

    std::atomic<int> initialiseCount { 0 };
    double timeToCpuScale = 0;
    std::atomic<double> cpuUsageMs { 0 };
    std::atomic<bool> isClipEffect { false };

    juce::ValueTree getConnectionsTree();
    struct WireList;
    std::unique_ptr<WireList> sidechainWireList;
    std::atomic<bool> isInitialisingFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Plugin)
};

} // namespace tracktion::inline engine
