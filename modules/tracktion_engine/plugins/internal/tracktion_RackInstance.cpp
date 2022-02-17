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

struct RackInputAutomatableParameter   : public AutomatableParameter
{
    RackInputAutomatableParameter (const juce::String& xmlTag, const juce::String& name,
                                   Plugin& owner, juce::Range<float> valueRangeToUse)
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse)
    {
    }

    ~RackInputAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    bool isParameterActive() const override
    {
        if (auto rp = dynamic_cast<RackInstance*> (plugin))
            return ! rp->linkInputs;

        return false;
    }
};

struct RackOutputAutomatableParameter   : public AutomatableParameter
{
    RackOutputAutomatableParameter (const juce::String& xmlTag, const juce::String& name,
                                    Plugin& owner, juce::Range<float> valueRangeToUse)
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse)
    {
    }

    ~RackOutputAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    bool isParameterActive() const override
    {
        if (auto rp = dynamic_cast<RackInstance*> (plugin))
            return ! rp->linkOutputs;

        return false;
    }
};

struct RackWetDryAutomatableParam  : public AutomatableParameter
{
    RackWetDryAutomatableParam (const juce::String& xmlTag, const juce::String& name,
                                RackInstance& owner, juce::Range<float> valueRangeToUse)
        : AutomatableParameter (xmlTag, name, owner, valueRangeToUse)
    {
    }

    ~RackWetDryAutomatableParam() override
    {
        notifyListenersOfDeletion();
    }

    juce::String valueToString (float value) override        { return juce::Decibels::toString (juce::Decibels::gainToDecibels (value), 1); }
    float stringToValue (const juce::String& s) override     { return dbStringToDb (s); }
};

//==============================================================================
RackInstance::RackInstance (PluginCreationInfo info)
    : Plugin (info),
      rackTypeID (EditItemID::fromProperty (state, IDs::rackType)),
      type (info.edit.getRackList().getRackTypeForID (rackTypeID))
{
    jassert (type != nullptr);

    addAutomatableParameter (dryGain = new RackWetDryAutomatableParam ("dry level", TRANS("Dry level"), *this, { 0.0f, 1.0f }));
    addAutomatableParameter (wetGain = new RackWetDryAutomatableParam ("wet level", TRANS("Wet level"), *this, { 0.0f, 1.0f }));
    leftInDb = addParam ("left input level", TRANS("Left input level"), { (float) RackInstance::rackMinDb, (float) RackInstance::rackMaxDb });
    addAutomatableParameter (rightInDb = new RackInputAutomatableParameter ("right input level", TRANS("Right input level"), *this, { (float) RackInstance::rackMinDb, (float) RackInstance::rackMaxDb }));
    leftOutDb = addParam ("left output level", TRANS("Left output level"), { (float) RackInstance::rackMinDb, (float) RackInstance::rackMaxDb });
    addAutomatableParameter (rightOutDb = new RackOutputAutomatableParameter ("right output level", TRANS("Right output level"), *this, { (float) RackInstance::rackMinDb, (float) RackInstance::rackMaxDb }));

    auto um = getUndoManager();

    dryValue.referTo (state, IDs::dry, um);
    wetValue.referTo (state, IDs::wet, um, 1.0f);
    leftInValue.referTo (state, IDs::leftInDb, um);
    rightInValue.referTo (state, IDs::rightInDb, um);
    leftOutValue.referTo (state, IDs::leftOutDb, um);
    rightOutValue.referTo (state, IDs::rightOutDb, um);

    leftInputGoesTo.referTo (state, IDs::leftTo, um, 1);
    rightInputGoesTo.referTo (state, IDs::rightTo, um, 2);
    leftOutputComesFrom.referTo (state, IDs::leftFrom, um, 1);
    rightOutputComesFrom.referTo (state, IDs::rightFrom, um, 2);

    dryGain->attachToCurrentValue (dryValue);
    wetGain->attachToCurrentValue (wetValue);
    leftInDb->attachToCurrentValue (leftInValue);
    rightInDb->attachToCurrentValue (rightInValue);
    leftOutDb->attachToCurrentValue (leftOutValue);
    rightOutDb->attachToCurrentValue (rightOutValue);
}

RackInstance::~RackInstance()
{
    notifyListenersOfDeletion();

    dryGain->detachFromCurrentValue();
    wetGain->detachFromCurrentValue();
    leftInDb->detachFromCurrentValue();
    rightInDb->detachFromCurrentValue();
    leftOutDb->detachFromCurrentValue();
    rightOutDb->detachFromCurrentValue();
}

juce::ValueTree RackInstance::create (RackType& type)
{
    return createValueTree (IDs::PLUGIN,
                            IDs::type, RackInstance::xmlTypeName,
                            IDs::rackType, type.rackID);
}

juce::String RackInstance::getTooltip()
{
    if (engine.getPluginManager().doubleClickToOpenWindows())
        return getName() + " (" + TRANS("Double-click to edit the rack") + ")";

    return getName() + " (" + TRANS("Click to edit the rack") + ")";
}

const char* RackInstance::xmlTypeName = "rack";

juce::String RackInstance::getName()
{
    return type != nullptr ? type->rackName
                           : TRANS("Rack type missing!");
}

void RackInstance::replaceRackWithPluginSequence (SelectionManager* sm)
{
    if (RackType::Ptr thisType = type) // (keep a local reference)
    {
        const bool replaceRack = getRackInstancesInEditForType (*type).size() == 1;
        jassert (! replaceRack || getRackInstancesInEditForType (*type).getFirst() == this);

        struct PluginIndexAndPos
        {
            bool operator< (const PluginIndexAndPos& other) const     { return x < other.x; }

            float x = 0.0f;
            Plugin* plugin = nullptr;
        };

        auto rackPlugins = thisType->getPlugins();
        juce::Array<PluginIndexAndPos> pluginLocations;

        for (int i = 0; i < rackPlugins.size(); ++i)
            if (auto rackPlugin = rackPlugins[i])
                pluginLocations.add ({ thisType->getPluginPosition (rackPlugin).x, rackPlugin });

        std::sort (pluginLocations.begin(), pluginLocations.end());

        if (auto list = getOwnerList())
        {
            auto index = list->getPlugins().indexOf (this);

            for (int i = pluginLocations.size(); --i >= 0;)
            {
                auto srcPlugin = pluginLocations.getUnchecked (i).plugin;
                jassert (srcPlugin != nullptr);
                srcPlugin->flushPluginStateToValueTree();
                auto pluginState = srcPlugin->state;

                if (! replaceRack)
                {
                    auto newState = srcPlugin->state.createCopy();
                    EditItemID::remapIDs (newState, nullptr, edit);
                    jassert (EditItemID::fromID (newState) != EditItemID::fromID (srcPlugin->state));
                    pluginState = newState;
                }

                if (auto p = list->insertPlugin (pluginState, index))
                    if (sm != nullptr)
                        sm->selectOnly (*p);
            }

            deleteFromParent();

            if (replaceRack)
                edit.getRackList().removeRackType (thisType);
        }
        else
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Unable to replace rack with plugins"));
        }
    }
}

juce::StringArray RackInstance::getInputChoices (bool includeNumberPrefix)
{
    juce::StringArray inputChoices;

    if (type != nullptr)
    {
        inputChoices.add (getNoPinName());
        auto inputs = type->getInputNames();

        for (int i = 1; i < inputs.size(); ++i)
        {
            if (includeNumberPrefix)
                inputChoices.add (juce::String (i) + ". " + inputs[i]);
            else
                inputChoices.add (inputs[i]);
        }
    }

    return inputChoices;
}

juce::StringArray RackInstance::getOutputChoices (bool includeNumberPrefix)
{
    juce::StringArray outputChoices;

    if (type != nullptr)
    {
        outputChoices.add (getNoPinName());
        auto outputs = type->getOutputNames();

        for (int i = 1; i < outputs.size(); ++i)
        {
            if (includeNumberPrefix)
                outputChoices.add (juce::String (i) + ". " + outputs[i]);
            else
                outputChoices.add (outputs[i]);
        }
    }

    return outputChoices;
}

juce::String RackInstance::getNoPinName()
{
    return TRANS("<none>");
}

void RackInstance::setInputName (Channel c, const juce::String& inputName)
{
    auto index = getInputChoices (false).indexOf (inputName);

    if (index == -1)
        return;

    if (index == 0)
        index = -1;

    switch (c)
    {
        case left:  leftInputGoesTo = index;    break;
        case right: rightInputGoesTo = index;   break;
        default:    break;
    }
}

void RackInstance::setOutputName (Channel c, const juce::String& outputName)
{
    auto index = getOutputChoices (false).indexOf (outputName);

    if (index == -1)
        return;

    if (index == 0)
        index = -1;

    switch (c)
    {
        case left:  leftOutputComesFrom = index;    break;
        case right: rightOutputComesFrom = index;   break;
        default:    break;
    }
}

void RackInstance::initialise (const PluginInitialisationInfo& info)
{
    if (type != nullptr)
        type->registerInstance (this, info);

    initialiseWithoutStopping (info);
}

void RackInstance::initialiseWithoutStopping (const PluginInitialisationInfo&)
{
    const float wet = wetGain->getCurrentValue();
    lastLeftIn   = dbToGain (leftInDb->getCurrentValue());
    lastRightIn  = dbToGain (linkInputs ? leftInDb->getCurrentValue() : rightInDb->getCurrentValue());
    lastLeftOut  = wet * dbToGain (leftOutDb->getCurrentValue());
    lastRightOut = wet * dbToGain (linkOutputs ? leftOutDb->getCurrentValue() : rightOutDb->getCurrentValue());
}

void RackInstance::deinitialise()
{
    if (type != nullptr)
        type->deregisterInstance (this);
}

void RackInstance::updateAutomatableParamPosition (TimePosition time)
{
    Plugin::updateAutomatableParamPosition (time);

    if (type != nullptr)
        type->updateAutomatableParamPositions (time);
}

double RackInstance::getLatencySeconds()
{
    return 0.0;
}

void RackInstance::prepareForNextBlock (TimePosition)
{
}

void RackInstance::applyToBuffer (const PluginRenderContext&)
{
}

juce::String RackInstance::getSelectableDescription()
{
    return TRANS("Plugin Rack");
}

void RackInstance::setInputLevel (Channel c, float v)
{
    const auto& param = c == left ? leftInDb : rightInDb;

    param->setParameter (v, juce::sendNotification);

    if (linkInputs)
    {
        const auto& linkedParam = c == left ? rightInDb : leftInDb;
        linkedParam->setParameter (v, juce::sendNotification);
    }
}

void RackInstance::setOutputLevel (Channel c, float v)
{
    const auto& param = c == left ? leftOutDb : rightOutDb;

    param->setParameter (v, juce::sendNotification);

    if (linkOutputs)
    {
        const auto& linkedParam = c == left ? rightOutDb : leftOutDb;
        linkedParam->setParameter (v, juce::sendNotification);
    }
}

juce::String RackInstance::getInputName (Channel c)
{
    const auto& input = c == left ? leftInputGoesTo : rightInputGoesTo;

    if (type == nullptr || input < 0 || type->getInputNames()[input].isEmpty())
        return getNoPinName();

    return juce::String (input) + ". " + type->getInputNames()[input];
}

juce::String RackInstance::getOutputName (Channel c)
{
    const auto& ouput = c == left ? leftOutputComesFrom : rightOutputComesFrom;

    if (type == nullptr || ouput < 0 || type->getOutputNames()[ouput].isEmpty())
        return getNoPinName();

    return juce::String (ouput) + ". " + type->getOutputNames()[ouput];
}

}} // namespace tracktion { inline namespace engine
