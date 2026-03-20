/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

class RackInstance  : public Plugin
{
public:
    RackInstance (PluginCreationInfo);
    ~RackInstance() override;

    using Ptr = juce::ReferenceCountedObjectPtr<RackInstance>;

    static juce::ValueTree create (RackType&);

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() const override;
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getTooltip() override;

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;

    bool takesAudioInput() override                     { return true; }
    bool takesMidiInput() override                      { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool isSynth() override                             { return true; }
    bool canBeAddedToRack() override                    { return false; }
    int getNumOutputChannelsGivenInputs (int numInputs) override;
    ChannelConfiguration getMainBusInputChannelConfiguration() const override;
    ChannelConfiguration getMainBusOutputChannelConfiguration() const override;
    double getLatencySeconds() override;
    bool needsConstantBufferSize() override             { return true; }

    void prepareForNextBlock (TimePosition) override;
    void applyToBuffer (const PluginRenderContext&) override;
    void updateAutomatableParamPosition (TimePosition) override;

    juce::String getSelectableDescription() override;

    void replaceRackWithPluginSequence (SelectionManager*);

    //==============================================================================
    /** Returns the available rack input pin names (for UI dropdowns). */
    juce::StringArray getInputChoices (bool includeNumberPrefix);
    /** Returns the available rack output pin names (for UI dropdowns). */
    juce::StringArray getOutputChoices (bool includeNumberPrefix);
    juce::String getNoPinName();

    //==============================================================================
    const EditItemID rackTypeID;
    const RackType::Ptr type;

    /** Number of input/output channels (independent of each other). */
    juce::CachedValue<int> numInputChannels, numOutputChannels;

    int getNumInputChannels() const;
    int getNumOutputChannels() const;
    void setNumInputChannels (int);
    void setNumOutputChannels (int);

    /** Number of channel mappings (dynamically sized). */
    int getNumChannelMappings() const;

    /** Returns the 1-based rack input pin index for the given channel (-1 = none). */
    int getInputMapping (int channelIndex) const;
    /** Returns the 1-based rack output pin index for the given channel (-1 = none). */
    int getOutputMapping (int channelIndex) const;

    /** Sets the input mapping for a channel. rackPinIndex is 1-based (-1 = none). */
    void setInputMapping (int channelIndex, int rackPinIndex);
    /** Sets the output mapping for a channel. rackPinIndex is 1-based (-1 = none). */
    void setOutputMapping (int channelIndex, int rackPinIndex);

    /** Sets the input mapping by name (from getInputChoices). */
    void setInputMappingByName (int channelIndex, const juce::String& inputName);
    /** Sets the output mapping by name (from getOutputChoices). */
    void setOutputMappingByName (int channelIndex, const juce::String& outputName);

    /** Returns the input/output gain parameter for a channel. */
    AutomatableParameter::Ptr getInputGainParam (int channelIndex) const;
    AutomatableParameter::Ptr getOutputGainParam (int channelIndex) const;

    /** Returns the display name for an input/output mapping. */
    juce::String getInputMappingName (int channelIndex);
    juce::String getOutputMappingName (int channelIndex);

    void setInputLevel (int channelIndex, float value);
    void setOutputLevel (int channelIndex, float value);

    /** Adds a new channel mapping (appended at end). */
    void addChannelMapping();
    /** Removes the last channel mapping. */
    void removeLastChannelMapping();

    bool linkInputs = true, linkOutputs = true;

    AutomatableParameter::Ptr dryGain, wetGain;
    juce::CachedValue<float> dryValue, wetValue;

    static constexpr double rackMinDb = -100.0;
    static constexpr double rackMaxDb = 12.0;

private:
    //==============================================================================
    struct ChannelMapping
    {
        juce::CachedValue<int> inputGoesTo;
        juce::CachedValue<int> outputComesFrom;
        juce::CachedValue<float> inputGainValue;
        juce::CachedValue<float> outputGainValue;
        AutomatableParameter::Ptr inputGainDb;
        AutomatableParameter::Ptr outputGainDb;
    };

    juce::OwnedArray<ChannelMapping> channelMappings;

    void createChannelMapping (int channelIndex, int defaultInput, int defaultOutput);
    void trimChannelMappingsToSize (int needed);
    void migrateFromLegacyFormat();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackInstance)
};

} // namespace tracktion::inline engine
