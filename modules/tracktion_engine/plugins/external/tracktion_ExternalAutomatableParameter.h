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

class ExternalAutomatableParameter   : public AutomatableParameter,
                                       private juce::AudioProcessorParameter::Listener,
                                       private juce::AsyncUpdater
{
public:
    ExternalAutomatableParameter (const juce::String& parameterID,
                                  const juce::String& name,
                                  ExternalPlugin& owner, int parameterIndex_,
                                  juce::Range<float> valueRangeToUse)
        : AutomatableParameter (parameterID, name, owner, valueRangeToUse),
          parameterIndex (parameterIndex_)
    {
        if (auto vstXML = owner.getVSTXML())
        {
            param = vstXML->getParamForID (parameterIndex, nullptr);

            if (param != nullptr)
            {
                displayName = param->name;

                for (auto group = param->parent; group != nullptr; group = group->parent)
                    displayName = group->name + " >> " + displayName;

                valueType = vstXML->getValueType (param->type);
            }
        }

        registerAsListener();
    }

    ~ExternalAutomatableParameter() override
    {
        CRASH_TRACER
        cancelPendingUpdate();
        unregisterAsListener();
        notifyListenersOfDeletion();
    }
    
    void registerAsListener()
    {
        if (auto p = getParam())
            p->addListener (this);
        else
            jassertfalse;
    }

    void unregisterAsListener()
    {
        if (auto p = getParam())
            p->removeListener (this);
    }

    std::optional<float> getDefaultValue() const override
    {
        if (auto p = getParam())
            return p->getDefaultValue();

        return {};
    }

    void parameterChanged (float newValue, bool byAutomation) override
    {
        if (auto p = getParam())
        {
            if (p->getValue() != newValue)
            {
                if (! byAutomation)
                    markAsChanged();

                p->setValue (newValue);
            }
        }
    }

    void beginParameterChangeGesture() override
    {
        if (auto p = getParam())
            p->beginChangeGesture();
    }

    void endParameterChangeGesture() override
    {
        if (auto p = getParam())
            p->endChangeGesture();
    }

    juce::String valueToString (float value) override
    {
        if (auto p = getParam())
            return p->getText (value, 16);

        return juce::String (value, 2);
    }

    float stringToValue (const juce::String& str) override
    {
        if (auto p = getParam())
            return p->getValueForText (str);

        return dbStringToDb (str);
    }

    juce::String getCurrentValueAsString() override
    {
        if (auto p = getParam())
            return p->getCurrentValueAsText();

        return {};
    }

    juce::String getLabel() override
    {
        if (auto p = getParam())
            return p->getLabel();

        return {};
    }

    void valueChangedByPlugin()
    {
        if (auto p = getParam())
            setParameter (p->getValue(), juce::sendNotification);
    }

    juce::String getParameterName() const override
    {
        if (displayName.isNotEmpty())
            return displayName;

        if (auto p = getParam())
        {
            auto name = p->getName (1024);
            if (name.isNotEmpty())
                return name;
        }

        return TRANS("Unnamed") + " " + juce::String (parameterIndex + 1);
    }

    int getParameterIndex() const noexcept
    {
        return parameterIndex;
    }

    juce::String getParameterShortName (int suggestedLength) const override
    {
        if (auto p = getParam())
            return p->getName (suggestedLength);

        return {};
    }

    void setDisplayName (const juce::String& str)
    {
        displayName = str;
    }

    bool isDiscrete() const override
    {
        return param != nullptr
                && (param->numberOfStates >= 2 || param->type == "switch");
    }

    int getNumberOfStates() const override
    {
        if (isDiscrete())
            return (param->numberOfStates >= 2) ? param->numberOfStates : 2;

        return 0;
    }

    float getValueForState (int state) const override
    {
        jassert (state >= 0 && state < getNumberOfStates());

        if (valueType != nullptr && valueType->entries.size() > 0)
            if (const VSTXML::Entry* e = valueType->entries[state])
                return (e->range.high + e->range.low) / 2.0f;

        return juce::jlimit (0.0f, 1.0f, 1.0f / float (getNumberOfStates() - 1) * state);
    }

    juce::String getLabelForValue (float val) const override
    {
        if (param != nullptr && param->type == "switch")
            return (val >= 0.0f && val < 0.5f) ? TRANS("Off")
                                               : TRANS("On");

        if (valueType != nullptr)
        {
            for (const auto& v : valueType->entries)
                if (v->range.contains (val))
                    return v->name;
        }

        return {};
    }

    juce::StringArray getAllLabels() const override
    {
        juce::StringArray labels;

        if (param->type == "switch")
        {
            labels.add (TRANS("Off"));
            labels.add (TRANS("On"));
        }
        else if (valueType && valueType->entries.size() > 0)
        {
            for (int i = 0; i < valueType->entries.size(); ++i)
                labels.add (valueType->entries[i]->name);

            labels.removeEmptyStrings();
        }

        return labels;
    }

    float snapToState (float val) const override
    {
        if (isDiscrete())
        {
            if (valueType != nullptr)
            {
                for (const auto& v : valueType->entries)
                    if (v->range.contains (val))
                        return (v->range.high + v->range.low) / 2;
            }

            int state = juce::jlimit (0, getNumberOfStates() - 1,
                                      (int) (val * getNumberOfStates()));
            return getValueForState (state);
        }

        return val;
    }

    bool hasLabels() const override
    {
        return (param != nullptr && param->type == "switch")
                 || (valueType != nullptr && valueType->entries.size() > 0
                      && valueType->entries[0]->name.isNotEmpty());
    }

    int getStateForValue (float val) const override
    {
        if (isDiscrete())
        {
            if (valueType != nullptr)
            {
                for (int i = 0; i < valueType->entries.size(); ++i)
                    if (valueType->entries.getUnchecked (i)->range.contains (val))
                        return i;
            }

            return juce::roundToInt (std::floor (val * getNumberOfStates()));
        }

        return 0;
    }

private:
    const int parameterIndex;
    juce::String displayName;

    const VSTXML::Param* param = nullptr;
    const VSTXML::ValueType* valueType = nullptr;

    juce::AudioPluginInstance* getPlugin() const noexcept
    {
        jassert (plugin != nullptr);
        return static_cast<ExternalPlugin&> (*plugin).getAudioPluginInstance();
    }

    juce::AudioProcessorParameter* getParam() const noexcept
    {
        if (auto pi = getPlugin())
            return pi->getParameters()[parameterIndex];

        return {};
    }

    void markAsChanged() const noexcept
    {
        getEdit().pluginChanged (*plugin);
    }

    void parameterValueChanged (int index, float /*newValue*/) override
    {
        if (parameterIndex == index)
            triggerAsyncUpdate();
        else
            jassertfalse;
    }

    void parameterGestureChanged (int index, bool gestureIsStarting) override
    {
        if (! juce::MessageManager::existsAndIsCurrentThread())
            return;
        
        if (parameterIndex == index)
        {
            if (gestureIsStarting)
            {
                parameterChangeGestureBegin();
            }
            else
            {
                parameterChangeGestureEnd();
                markAsChanged();
            }
        }
        else
        {
            jassertfalse;
        }
    }

    void handleAsyncUpdate() override
    {
        // Some plugins send spurious parameter value changes (e.g. when their UI is opened)
        // which can reset modifier base values so disable these from coming through if there
        // are active modifiers etc.
        // It would be nice to be able to rely on the begin/end gestures to figure out if the user
        // is actively changing the parameter but as this isn't mandated by the plguin APIs, we can't
        // do this and we just have to disable control from the plugin UI when there are active Modifiers
        if (hasActiveModifierAssignments())
            return;

        valueChangedByPlugin();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalAutomatableParameter)
};

}} // namespace tracktion { inline namespace engine
