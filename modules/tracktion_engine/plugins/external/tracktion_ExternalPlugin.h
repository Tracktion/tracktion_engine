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

// JUCE Changed the format of PluginDescription::createIdentifierString
// breaking all our saved files. This reverts to the old format
juce::String createIdentifierString (const juce::PluginDescription&);


class ExternalPlugin  : public Plugin
{
public:
    ExternalPlugin (PluginCreationInfo);
    ~ExternalPlugin() override;

    juce::String getIdentifierString() override { return createIdentifierString (desc); }

    using Ptr = juce::ReferenceCountedObjectPtr<ExternalPlugin>;

    void selectableAboutToBeDeleted() override;

    static juce::ValueTree create (Engine&, const juce::PluginDescription&);

    void processingChanged() override;

    //==============================================================================
    void initialiseFully() override;
    void forceFullReinitialise();

    juce::String getLoadError();

    static const char* xmlTypeName;

    void flushPluginStateToValueTree() override;
    void flushBusesLayoutToValueTree();
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;
    void getPluginStateFromTree (juce::MemoryBlock&);

    void updateFromMirroredPluginIfNeeded (Plugin&) override;

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void reset() override;
    void setEnabled (bool enabled) override;

    juce::Array<Exportable::ReferencedItem> getReferencedItems() override;
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

    void applyToBuffer (const PluginRenderContext&) override;

    bool producesAudioWhenNoAudioInput() override   { return isAutomationNeeded() || isSynth() || ! noTail(); }
    int getNumOutputChannelsGivenInputs (int numInputs) override;
    void getChannelNames (juce::StringArray* ins, juce::StringArray* outs) override;

    bool isVST() const noexcept             { return desc.pluginFormatName == "VST"; }
    bool isVST3() const noexcept            { return desc.pluginFormatName == "VST3"; }
    bool isAU() const noexcept              { return desc.pluginFormatName == "AudioUnit"; }
    juce::String getName() override         { return desc.name; }
    juce::String getVendor() override       { return desc.manufacturerName; }
    juce::String getTooltip() override      { return getName() + "$vstfilter"; }
    juce::String getPluginType() override   { return xmlTypeName; }
    bool isSynth() override                 { return desc.isInstrument; }
    bool takesMidiInput() override;
    bool takesAudioInput() override         { return (! isSynth()) || (dryGain->getCurrentValue() > 0.0f); }
    bool isMissing() override;
    bool isDisabled() override;
    double getLatencySeconds() override     { return latencySeconds; }
    bool noTail() override;
    double getTailLength() const override;
    bool needsConstantBufferSize() override { return false; }
    void trackPropertiesChanged() override;

    juce::AudioProcessor* getWrappedAudioProcessor() const override     { return pluginInstance.get(); }
    void deleteFromParent() override;

    //==============================================================================
    // selectable stuff
    juce::String getSelectableDescription() override;

    //==============================================================================
    juce::File getFile() const;
    juce::String getPluginUID() const           { return juce::String::toHexString (desc.deprecatedUid); }

    const char* getDebugName() const noexcept   { return debugName.toUTF8(); }

    //==============================================================================
    int getNumInputs() const;
    int getNumOutputs() const;

    /** Attempts to change the layout of the plugin. */
    bool setBusesLayout (juce::AudioProcessor::BusesLayout);

    /** Attempts to change the layout of the plugin. */
    bool setBusLayout (juce::AudioChannelSet, bool isInput, int busIndex);

    //==============================================================================
    int getNumPrograms() const;
    int getCurrentProgram() const;
    juce::String getProgramName (int index);
    juce::String getNumberedProgramName (int i);
    juce::String getCurrentProgramName();
    void setCurrentProgram (int index, bool sendChangeMessage);
    void setCurrentProgramName (const juce::String& name);
    bool hasNameForMidiProgram (int programNum, int bank, juce::String& name) override;
    bool hasNameForMidiNoteNumber (int note, int midiChannel, juce::String& name) override;

    //==============================================================================
    const VSTXML* getVSTXML() const noexcept        { return vstXML.get(); }

    juce::AudioPluginInstance* getAudioPluginInstance() const;

    juce::PluginDescription desc;

    juce::CachedValue<float> dryValue, wetValue;
    AutomatableParameter::Ptr dryGain, wetGain;

    ActiveNoteList getActiveNotes() const           { return activeNotes; }

private:
    //==============================================================================
    juce::CriticalSection lock;
    juce::String debugName, identiferString, loadError;

    struct ProcessorChangedManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<ProcessorChangedManager> processorChangedManager;
    std::unique_ptr<VSTXML> vstXML;
    int latencySamples = 0;
    double latencySeconds = 0;
    bool isInstancePrepared = false;

    double lastSampleRate = 0.0;
    int lastBlockSizeSamples = 0;

    juce::MidiBuffer midiBuffer;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    ActiveNoteList activeNotes;

    class PluginPlayHead;
    std::unique_ptr<PluginPlayHead> playhead;

    bool fullyInitialised = false, supportsMPE = false, isFlushingLayoutToState = false;

    struct MPEChannelRemapper;
    std::unique_ptr<MPEChannelRemapper> mpeRemapper;

    void prepareIncomingMidiMessages (MidiMessageArray& incoming, int numSamples, bool isPlaying);

    juce::Array<ExternalAutomatableParameter*> autoParamForParamNumbers;

    //==============================================================================
    juce::String createPluginInstance (const juce::PluginDescription&);
    void deletePluginInstance();

    //==============================================================================
    void buildParameterTree() const override;
    void buildParameterTree (const VSTXML::Group*, AutomatableParameterTree::TreeNode*, juce::SortedSet<int>&) const;

    //==============================================================================
    void doFullInitialisation();
    void buildParameterList();
    void refreshParameterValues();
    void updateDebugName();
    void processPluginBlock (const PluginRenderContext&, bool processedBypass);

    std::unique_ptr<juce::PluginDescription> findMatchingPlugin() const;
    std::unique_ptr<juce::PluginDescription> findDescForUID (int uid, int deprecatedUid) const;
    std::unique_ptr<juce::PluginDescription> findDescForFileOrID (const juce::String&) const;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalPlugin)
};

//==============================================================================
/** specialised AutomatableParameter for wet/dry.
    Having a subclass just lets it label itself more nicely.
 */
struct PluginWetDryAutomatableParam  : public AutomatableParameter
{
    PluginWetDryAutomatableParam (const juce::String& xmlTag, const juce::String& name, Plugin&);
    ~PluginWetDryAutomatableParam() override;

    juce::String valueToString (float value) override;
    float stringToValue (const juce::String& s) override;

    PluginWetDryAutomatableParam() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWetDryAutomatableParam)
};

}} // namespace tracktion { inline namespace engine
