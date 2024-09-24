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

ParameterWithStateValue::ParameterWithStateValue (Plugin& plugin,
                                                  const juce::String& paramID,
                                                  const juce::Identifier& propertyName,
                                                  const juce::String& description,
                                                  float defaultValue,
                                                  juce::NormalisableRange<float> valueRange,
                                                  std::function<juce::String(float)> valueToStringFunction,
                                                  std::function<float(const juce::String&)> stringToValueFunction)
{
    parameter = plugin.addParam (paramID, description, valueRange);
    value = std::make_unique<juce::CachedValue<float>> (plugin.state, propertyName, std::addressof (plugin.edit.getUndoManager()), defaultValue);

    if (valueToStringFunction)
        parameter->valueToStringFunction = std::move (valueToStringFunction);

    if (stringToValueFunction)
        parameter->stringToValueFunction = std::move (stringToValueFunction);

    parameter->attachToCurrentValue (*value);
}

ParameterWithStateValue::~ParameterWithStateValue()
{
    reset();
}

void ParameterWithStateValue::reset()
{
    if (parameter)
    {
        parameter->detachFromCurrentValue();
        parameter.reset();
    }

    value.reset();
}

void ParameterWithStateValue::setParameter (float newValue, juce::NotificationType nt)
{
    parameter->setParameter (newValue, nt);
}

void ParameterWithStateValue::setFromValueTree (const juce::ValueTree& v)
{
    if (value)
    {
        if (auto p = v.getPropertyPointer (value->getPropertyID()))
            *value = static_cast<float> (*p);
        else
            value->resetToDefault();

        if (parameter)
            parameter->updateFromAttachedValue();
    }
}


} // namespace tracktion::inline engine
