/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class BreakpointOscillatorModifier  : public Modifier,
                                      private ValueTreeAllEventListener
{
public:
    //==============================================================================
    BreakpointOscillatorModifier (Edit&, const juce::ValueTree&);
    ~BreakpointOscillatorModifier();

    using Ptr = juce::ReferenceCountedObjectPtr<BreakpointOscillatorModifier>;
    using Array = juce::ReferenceCountedArray<BreakpointOscillatorModifier>;

    void initialise() override;
    juce::String getName() override                     { return TRANS("Breakpoint Modifier"); }

    //==============================================================================
    /** Returns the current value of the modifier.  */
    float getCurrentValue() override;

    /** Returns the envelope value before the bipolar and depth have been applied. */
    float getCurrentEnvelopeValue() const noexcept;

    /** Returns the current phase between 0 & 1. */
    float getCurrentPhase() const noexcept;

    /** Returns the total time for this envelope. */
    float getTotalTime() const;

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    AudioNode* createPreFXAudioNode (AudioNode*) override;

    //==============================================================================
    struct Assignment   : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const BreakpointOscillatorModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
		juce::ReferenceCountedObjectPtr<BreakpointOscillatorModifier> getModifier() const;

        const EditItemID breakpointOscillatorModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override    { return getName(); }

    //==============================================================================
    juce::CachedValue<float> numActivePoints, syncType, rate, rateType, depth, bipolar,
                             stageZeroValue,
                             stageOneValue, stageOneTime, stageOneCurve,
                             stageTwoValue, stageTwoTime, stageTwoCurve,
                             stageThreeValue, stageThreeTime, stageThreeCurve,
                             stageFourValue, stageFourTime, stageFourCurve;

    AutomatableParameter::Ptr numActivePointsParam, syncTypeParam, rateParam, rateTypeParam, depthParam, bipolarParam,
                              stageZeroValueParam,
                              stageOneValueParam, stageOneTimeParam, stageOneCurveParam,
                              stageTwoValueParam, stageTwoTimeParam, stageTwoCurveParam,
                              stageThreeValueParam, stageThreeTimeParam, stageThreeCurveParam,
                              stageFourValueParam, stageFourTimeParam, stageFourCurveParam;

    struct Stage
    {
        AutomatableParameter *valueParam = nullptr, *timeParam = nullptr, *curveParam = nullptr;
    };

    Stage getStage (int index) const;

private:
    struct BreakpointOscillatorModifierTimer;
    std::unique_ptr<BreakpointOscillatorModifierTimer> modifierTimer;
    struct ModifierAudioNode;

    LambdaTimer changedTimer;
    std::atomic<float> currentPhase { 1.0f }, currentValue { 0.0f }, currentEnvelopeValue { 0.0f };

    struct Section { float value = 0.0f, time = 0.0f, curve = 0.0f; };
    std::array<Section, 5> getAllSections() const;
    std::pair<Section, Section> getSectionsSurrounding (float time) const;

    void setPhase (float newPhase);

    void valueTreeChanged() override;
};

} // namespace tracktion_engine
