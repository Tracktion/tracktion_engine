/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class LFOModifier   : public Modifier,
                      private ValueTreeAllEventListener
{
public:
    LFOModifier (Edit&, const juce::ValueTree&);
    ~LFOModifier();

    using Ptr = juce::ReferenceCountedObjectPtr<LFOModifier>;
    using Array = juce::ReferenceCountedArray<LFOModifier>;

    void initialise() override;
    juce::String getName() override                     { return TRANS("LFO Modifier"); }

    //==============================================================================
    /** Returns the current value of the LFO.
        N.B. this may be greater than +-1 if a suitable offset has been applied.
    */
    float getCurrentValue() override;

    /** Returns the current phase between 0 & 1. */
    float getCurrentPhase() const;

    /** */
    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    /** */
    AudioNode* createPreFXAudioNode (AudioNode*) override;

    //==============================================================================
    struct Assignment : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const LFOModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
		juce::ReferenceCountedObjectPtr<LFOModifier> getLFOModifier() const;

        const EditItemID lfoModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override    { return getName(); }

    //==============================================================================
    enum Wave
    {
        waveSine        = 0,
        waveTriangle    = 1,
        waveSawUp       = 2,
        waveSawDown     = 3,
        waveSquare      = 4,
        fourStepsUp     = 5,
        fourStepsDown   = 6,
        eightStepsUp    = 7,
        eightStepsDown  = 8,
        random          = 9,
        noise           = 10
    };

    static juce::StringArray getWaveNames();

    juce::CachedValue<float> wave, syncType, rate, rateType, depth, bipolar, phase, offset;

    AutomatableParameter::Ptr waveParam, syncTypeParam, rateParam, rateTypeParam, depthParam, bipolarParam, phaseParam, offsetParam;

private:
    struct LFOModifierTimer;
    std::unique_ptr<LFOModifierTimer> modifierTimer;
    struct ModifierAudioNode;

    LambdaTimer changedTimer;
    std::atomic<float> currentPhase { 0.0f }, currentValue { 0.0f };

    void valueTreeChanged() override;
};

} // namespace tracktion_engine
