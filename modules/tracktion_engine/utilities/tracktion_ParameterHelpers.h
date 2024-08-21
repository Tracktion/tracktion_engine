/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

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
/// A helper class that creates an AutomatableParameter and links it to a
/// CachedValue, to make it easier to create and manage parameters whose values
/// are stored as a property in a plugin's ValueTree state.
struct ParameterWithStateValue
{
    ParameterWithStateValue() = default;

    ParameterWithStateValue (Plugin&,
                             const juce::String& paramID,
                             const juce::Identifier& propertyName,
                             const juce::String& description,
                             float defaultValue,
                             juce::NormalisableRange<float> valueRange,
                             std::function<juce::String(float)> valueToStringFunction = {},
                             std::function<float(const juce::String&)> stringToValueFunction = {});

    ~ParameterWithStateValue();
    ParameterWithStateValue (ParameterWithStateValue&&) = default;
    ParameterWithStateValue& operator= (ParameterWithStateValue&&) = default;

    AutomatableParameter& getParameter()            { return *parameter; }
    juce::CachedValue<float>& getCachedValue()      { return *value; }

    /// Returns the current value from the AutomatableParameter
    float getCurrentValue() const                   { return parameter->getCurrentValue(); }
    /// Sets a new value via the AutomatableParameter
    void setParameter (float newValue, juce::NotificationType);

    /// Releases the parameter and CachedValue
    void reset();

    /// Copies the matching property from the given ValueTree to this parameter
    void setFromValueTree (const juce::ValueTree&);

    AutomatableParameter::Ptr parameter;
    std::unique_ptr<juce::CachedValue<float>> value;
};


} // namespace tracktion::inline engine
