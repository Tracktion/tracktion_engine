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
class StepModifier  : public Modifier,
                      private ValueTreeAllEventListener
{
public:
    //==============================================================================
    StepModifier (Edit&, const juce::ValueTree&);
    ~StepModifier();

    using Ptr = juce::ReferenceCountedObjectPtr<StepModifier>;
    using Array = juce::ReferenceCountedArray<StepModifier>;

    void initialise() override;
    float getCurrentValue() override;

    juce::String getName() override                 { return TRANS("Step Modifier"); }

    //==============================================================================
    enum { maxNumSteps = 64 };
    float getStep (int step) const;
    void setStep (int step, float value);

    int getCurrentStep() const noexcept;

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    AudioNode* createPreFXAudioNode (AudioNode*) override;

    //==============================================================================
    struct Assignment : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const StepModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
		juce::ReferenceCountedObjectPtr<StepModifier> getStepModifier() const;

        const EditItemID stepModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override;

    //==============================================================================
    juce::CachedValue<float> syncType, numSteps, rate, rateType, depth;

    AutomatableParameter::Ptr syncTypeParam, numStepsParam, rateParam, rateTypeParam, depthParam;

private:
    struct StepModifierTimer;
    std::unique_ptr<StepModifierTimer> stepModifierTimer;
    struct StepModifierAudioNode;

    LambdaTimer changedTimer;
    float steps[maxNumSteps];
    std::atomic<int> currentStep { 0 };

    void flushStepsToProperty();
    void restoreStepsFromProperty();

    void valueTreeChanged() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};

} // namespace tracktion_engine
