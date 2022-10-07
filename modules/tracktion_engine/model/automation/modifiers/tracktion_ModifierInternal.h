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

namespace PredefinedWavetable
{
    static inline float getSinSample (float phase)
    {
        return (std::sin (phase * juce::MathConstants<float>::pi * 2.0f) + 1.0f) / 2.0f;
    }

    static inline float getTriangleSample (float phase)
    {
        return (phase < 0.5f) ? (2.0f * phase) : (-2.0f * phase + 2.0f);
    }

    static inline float getSawUpSample (float phase)
    {
        return phase;
    }

    static inline float getSawDownSample (float phase)
    {
        return 1.0f - phase;
    }

    static inline float getSquareSample (float phase)
    {
        return phase < 0.5f ? 1.0f : 0.0f;
    }

    static inline float getStepsUpSample (float phase, int totalNumSteps)
    {
        jassert (totalNumSteps > 1);
        const float stageAmmount = 1.0f / (totalNumSteps - 1);

        const int stage = juce::jlimit (0, totalNumSteps - 1, (int) std::floor (phase * totalNumSteps));

        return stageAmmount * stage;
    }

    static inline float getStepsDownSample (float phase, int totalNumStages)
    {
        return getStepsUpSample (1.0f - phase, totalNumStages);
    }
}

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
        jassert (juce::isPositiveAndBelow (newPosition, rampDuration));
        rampPos = juce::jlimit (0.0f, rampDuration, newPosition) / rampDuration;
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
namespace modifier
{
    inline juce::StringArray getEnabledNames()
    {
        return { NEEDS_TRANS("Disabled"),
                 NEEDS_TRANS("Enabled") };
    }
}

//==============================================================================
struct DiscreteLabelledParameter  : public AutomatableParameter
{
    DiscreteLabelledParameter (const juce::String& xmlTag,
                               const juce::String& name,
                               AutomatableEditItem& owner,
                               juce::Range<float> valueRangeToUse,
                               int numStatesToUse = 0,
                               juce::StringArray labelsToUse = {})
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse),
          numStates (numStatesToUse), labels (labelsToUse)
    {
        jassert (labels.isEmpty() || labels.size() == numStates);
    }

    ~DiscreteLabelledParameter() override
    {
        notifyListenersOfDeletion();
    }

    bool isDiscrete() const override             { return numStates != 0; }
    int getNumberOfStates() const override       { return numStates; }

    float getValueForState (int i) const override
    {
        if (numStates == 0)
            return 0.0;

        return juce::jmap ((float) i, 0.0f, float (numStates - 1), valueRange.start, valueRange.end);
    }

    int getStateForValue (float value) const override
    {
        if (numStates == 0)
            return 0;

        return juce::roundToInt (juce::jmap (value, valueRange.start, valueRange.end, 0.0f, float (numStates - 1)));
    }

    bool hasLabels()  const override                   { return labels.size() > 0; }
    juce::StringArray getAllLabels() const override    { return labels; }

    juce::String getLabelForValue (float val) const override
    {
        const int s = getStateForValue (val);

        return juce::isPositiveAndBelow (s, getNumberOfStates()) ? labels[s] : juce::String();
    }

    float snapToState (float val) const override
    {
        return getValueForState (juce::jlimit (0, getNumberOfStates() - 1, juce::roundToInt (val)));
    }

private:
    const int numStates = 0;
    const juce::StringArray labels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscreteLabelledParameter)
};

//==============================================================================
struct SuffixedParameter    : public AutomatableParameter
{
    SuffixedParameter (const juce::String& xmlTag, const juce::String& name,
                       AutomatableEditItem& owner, juce::NormalisableRange<float> valueRangeToUse,
                       juce::String suffixToUse = {})
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse),
          suffix (suffixToUse)
    {
    }

    ~SuffixedParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String valueToString (float value) override
    {
        if (valueRange.interval == 1.0f)
            return juce::String (juce::roundToInt (value));

        return AutomatableParameter::valueToString (value);
    }

    juce::String getLabel() override           { return suffix; }

    const juce::String suffix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuffixedParameter)
};

//==============================================================================
//==============================================================================
static inline AutomatableParameter* createDiscreteParameter (AutomatableEditItem& item,
                                                             const juce::String& paramID, const juce::String& name,
                                                             juce::Range<float> valueRange,
                                                             juce::CachedValue<float>& val,
                                                             const juce::StringArray& labels)
{
    auto p = new DiscreteLabelledParameter (paramID, name, item, valueRange, labels.size(), labels);
    p->attachToCurrentValue (val);

    return p;
}

static inline AutomatableParameter* createSuffixedParameter (AutomatableEditItem& item,
                                                             const juce::String& paramID, const juce::String& name,
                                                             juce::NormalisableRange<float> valueRange, float centreVal,
                                                             juce::CachedValue<float>& val,
                                                             const juce::String& suffix)
{
    valueRange.setSkewForCentre (centreVal);
    auto p = new SuffixedParameter (paramID, name, item, valueRange, suffix);
    p->attachToCurrentValue (val);

    return p;
}

}} // namespace tracktion { inline namespace engine
