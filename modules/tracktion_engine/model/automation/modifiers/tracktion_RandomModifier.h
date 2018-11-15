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
class RandomModifier    : public Modifier,
                          private ValueTreeAllEventListener
{
public:
    //==============================================================================
    RandomModifier (Edit&, const juce::ValueTree&);
    ~RandomModifier();

    using Ptr = juce::ReferenceCountedObjectPtr<RandomModifier>;
    using Array = juce::ReferenceCountedArray<RandomModifier>;

    void initialise() override;
    juce::String getName() override                     { return TRANS("Random Modifier"); }

    //==============================================================================
    /** Returns the current value of the LFO.
        N.B. this may be greater than +-1 if a suitable offset has been applied.
    */
    float getCurrentValue() override;

    /** Returns the current phase between 0 & 1. */
    float getCurrentPhase() const noexcept;

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    AudioNode* createPreFXAudioNode (AudioNode*) override;

    //==============================================================================
    struct Assignment : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const RandomModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
		juce::ReferenceCountedObjectPtr<RandomModifier> getRandomModifier() const;

        const EditItemID randomModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override    { return getName(); }

    //==============================================================================
    enum Type
    {
        random  = 0,
        noise   = 1
    };

    static juce::StringArray getTypeNames();

    juce::CachedValue<float> type, shape, syncType, rate, rateType, depth, stepDepth, smooth, bipolar;

    AutomatableParameter::Ptr typeParam, shapeParam, syncTypeParam, rateParam, rateTypeParam, depthParam, stepDepthParam, smoothParam, bipolarParam;

private:
    struct RandomModifierTimer;
    std::unique_ptr<RandomModifierTimer> modifierTimer;
    struct ModifierAudioNode;

    juce::Random rand;
    LambdaTimer changedTimer;
    std::atomic<float> currentPhase { 1.0f }, currentValue { 0.0f };
    float previousRandom = 0.0f, currentRandom = 0.0f, randomDifference = 0.0f;

    void setPhase (float newPhase);
    void valueTreeChanged() override;
};

} // namespace tracktion_engine
