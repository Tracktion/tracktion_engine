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

class RackInstance  : public Plugin
{
public:
    RackInstance (PluginCreationInfo);
    ~RackInstance() override;

    static juce::ValueTree create (RackType&);

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() override;
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getTooltip() override;

    void initialise (const PluginInitialisationInfo&) override;
    void initialiseWithoutStopping (const PluginInitialisationInfo&) override;
    void deinitialise() override;

    bool takesAudioInput() override                     { return true; }
    bool takesMidiInput() override                      { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool isSynth() override                             { return true; }
    bool canBeAddedToRack() override                    { return false; }
    int getNumOutputChannelsGivenInputs (int) override  { return 2; }
    double getLatencySeconds() override;
    bool needsConstantBufferSize() override             { return true; }

    void prepareForNextBlock (TimePosition) override;
    void applyToBuffer (const PluginRenderContext&) override;
    void updateAutomatableParamPosition (TimePosition) override;

    juce::String getSelectableDescription() override;

    void replaceRackWithPluginSequence (SelectionManager*);

    //==============================================================================
    enum Channel { left, right };

    juce::StringArray getInputChoices (bool includeNumberPrefix);
    juce::StringArray getOutputChoices (bool includeNumberPrefix);
    juce::String getNoPinName();

    void setInputName (Channel, const juce::String& inputName);
    void setOutputName (Channel, const juce::String& outputName);

    //==============================================================================
    const EditItemID rackTypeID;
    const RackType::Ptr type;

    bool linkInputs = true, linkOutputs = true;
    juce::CachedValue<int> leftInputGoesTo, rightInputGoesTo, leftOutputComesFrom, rightOutputComesFrom;
    juce::CachedValue<float> dryValue, wetValue, leftInValue, rightInValue, leftOutValue, rightOutValue;

    AutomatableParameter::Ptr dryGain, wetGain;
    AutomatableParameter::Ptr leftInDb, rightInDb, leftOutDb, rightOutDb;

    void setInputLevel (Channel, float);
    void setOutputLevel (Channel, float);

    juce::String getInputName (Channel);
    juce::String getOutputName (Channel);

    static constexpr double rackMinDb = -100.0;
    static constexpr double rackMaxDb = 12.0;

private:
    float lastLeftIn = 0.0f, lastRightIn = 0.0f, lastLeftOut = 0.0f, lastRightOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackInstance)
};

}} // namespace tracktion { inline namespace engine
