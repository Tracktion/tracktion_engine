/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace PredefinedWavetable
{
    static float getSinSample (float phase)
    {
        return (std::sin (phase * float_Pi * 2.0f) + 1.0f) / 2.0f;
    }

    static float getTriangleSample (float phase)
    {
        return (phase < 0.5f) ? (2.0f * phase) : (-2.0f * phase + 2.0f);
    }

    static float getSawUpSample (float phase)
    {
        return phase;
    }

    static float getSawDownSample (float phase)
    {
        return 1.0f - phase;
    }

    static float getSquareSample (float phase)
    {
        return phase < 0.5f ? 1.0f : 0.0f;
    }

    static float getStepsUpSample (float phase, int totalNumSteps)
    {
        jassert (totalNumSteps > 1);
        const float stageAmmount = 1.0f / (totalNumSteps - 1);

        const int stage = jlimit (0, totalNumSteps - 1, (int) std::floor (phase * totalNumSteps));

        return stageAmmount * stage;
    }

    static float getStepsDownSample (float phase, int totalNumStages)
    {
        return getStepsUpSample (1.0f - phase, totalNumStages);
    }
};

/** A ramp which goes between 0 and 1 over a set duration. */
struct Ramp
{
    Ramp() = default;

    void setDuration (float newDuration) noexcept
    {
        jassert (newDuration > 0.0f);
        rampDuration = newDuration;
        process (0.0f); // Re-sync ramp pos
    }

    void setPosition (float newPosition) noexcept
    {
        jassert (isPositiveAndBelow (newPosition, rampDuration));
        rampPos = jlimit (0.0f, rampDuration, newPosition) / rampDuration;
    }

    void process (float duration) noexcept
    {
        rampPos += (duration / rampDuration);

        while (rampPos > 1.0f)
            rampPos -= 1.0f;
    }

    float getPosition() const noexcept
    {
        return rampPos * rampDuration;
    }

    float getProportion() const noexcept
    {
        return rampPos;
    }

private:
    float rampPos = 0.0f, rampDuration = 1.0f;
};

//==============================================================================
struct DiscreteLabelledParameter  : public AutomatableParameter
{
    DiscreteLabelledParameter (const String& xmlTag, const String& name,
                               AutomatableEditItem& owner, Range<float> valueRange,
                               int numStatesToUse = 0,
                               StringArray labelsToUse = {})
        : AutomatableParameter (xmlTag, name, owner, valueRange),
          numStates (numStatesToUse), labels (labelsToUse)
    {
        jassert (labels.isEmpty() || labels.size() == numStates);
    }

    ~DiscreteLabelledParameter()
    {
        notifyListenersOfDeletion();
    }

    bool isDiscrete() const override             { return numStates != 0; }
    int getNumberOfStates() const override       { return numStates; }

    float getValueForState (int i) const override
    {
        if (numStates == 0)
            return 0.0;

        return jmap ((float) i, 0.0f, float (numStates - 1), valueRange.start, valueRange.end);
    }

    int getStateForValue (float value) const override
    {
        if (numStates == 0)
            return 0;

        return roundToInt (jmap (value, valueRange.start, valueRange.end, 0.0f, float (numStates - 1)));
    }

    bool hasLabels()  const override             { return labels.size() > 0; }
    StringArray getAllLabels() const override    { return labels; }

    String getLabelForValue (float val) const override
    {
        const int s = getStateForValue (val);

        return isPositiveAndBelow (s, getNumberOfStates()) ? labels[s] : String();
    }

    float snapToState (float val) const override
    {
        return getValueForState (jlimit (0, getNumberOfStates() - 1, roundToInt (val)));
    }

private:
    const int numStates = 0;
    const StringArray labels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscreteLabelledParameter)
};

//==============================================================================
struct SuffixedParameter    : public AutomatableParameter
{
    SuffixedParameter (const String& xmlTag, const String& name,
                       AutomatableEditItem& owner, NormalisableRange<float> valueRange,
                       String suffixToUse = {})
        : AutomatableParameter (xmlTag, name, owner, valueRange),
          suffix (suffixToUse)
    {
    }

    ~SuffixedParameter()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override
    {
        if (valueRange.interval == 1.0f)
            return String (roundToInt (value));

        return AutomatableParameter::valueToString (value);
    }

    String getLabel() override                              { return suffix; }

    const String suffix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuffixedParameter)
};
