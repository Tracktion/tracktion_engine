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

/** */
class StepModifier  : public Modifier,
                      private ValueTreeAllEventListener
{
public:
    //==============================================================================
    StepModifier (Edit&, const juce::ValueTree&);
    ~StepModifier() override;

    using Ptr = juce::ReferenceCountedObjectPtr<StepModifier>;
    using Array = juce::ReferenceCountedArray<StepModifier>;

    using Modifier::initialise;
    void initialise() override;
    float getCurrentValue() override;

    juce::String getName() override                 { return TRANS("Step Modifier"); }

    //==============================================================================
    enum { maxNumSteps = 64 };
    float getStep (int step) const;
    void setStep (int step, float value);

    int getCurrentStep() const noexcept;

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    ProcessingPosition getProcessingPosition() override { return ProcessingPosition::preFX; }
    void applyToBuffer (const PluginRenderContext&) override;

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

    LambdaTimer changedTimer;
    float steps[maxNumSteps];
    std::atomic<int> currentStep { 0 };

    void flushStepsToProperty();
    void restoreStepsFromProperty();

    void valueTreeChanged() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};

}} // namespace tracktion { inline namespace engine
