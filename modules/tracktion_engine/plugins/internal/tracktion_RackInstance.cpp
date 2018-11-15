/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct RackInputAutomatableParameter   : public AutomatableParameter
{
    RackInputAutomatableParameter (const String& xmlTag, const String& name, Plugin& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~RackInputAutomatableParameter()
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
    RackOutputAutomatableParameter (const String& xmlTag, const String& name, Plugin& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~RackOutputAutomatableParameter()
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
    RackWetDryAutomatableParam (const String& xmlTag, const String& name, RackInstance& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~RackWetDryAutomatableParam()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override         { return Decibels::toString (Decibels::gainToDecibels (value), 1); }
    float stringToValue (const String& s) override      { return dbStringToDb (s); }
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

ValueTree RackInstance::create (RackType& type)
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, RackInstance::xmlTypeName, nullptr);
    type.rackID.setProperty (v, IDs::rackType, nullptr);
    return v;
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
        struct PluginIndexAndPos
        {
            bool operator< (const PluginIndexAndPos& other) const     { return x < other.x; }

            int index;
            float x;
        };

        juce::Array<PluginIndexAndPos> pluginLocations;

        for (int i = 0; i < thisType->getPlugins().size(); ++i)
            pluginLocations.add ({ i, thisType->getPluginPosition (i).x });

        std::sort (pluginLocations.begin(), pluginLocations.end());

        if (auto list = getOwnerList())
        {
            auto index = list->getPlugins().indexOf (this);

            for (int i = pluginLocations.size(); --i >= 0;)
            {
                auto srcPlugin = thisType->getPlugins()[pluginLocations.getUnchecked (i).index];

                srcPlugin->flushPluginStateToValueTree();
                auto newState = srcPlugin->state.createCopy();
                EditItemID::remapIDs (newState, nullptr, edit);
                jassert (EditItemID::fromID (newState) != EditItemID::fromID (srcPlugin->state));

                if (auto p = list->insertPlugin (newState, index))
                    if (sm != nullptr)
                        sm->selectOnly (*p);
            }

            removeFromParent();
        }
        else
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Unable to replace rack with plugins"));
        }
    }
}

StringArray RackInstance::getInputChoices (bool includeNumberPrefix)
{
    StringArray inputChoices;

    if (type != nullptr)
    {
        inputChoices.add (getNoPinName());
        auto inputs = type->getInputNames();

        for (int i = 1; i < inputs.size(); ++i)
        {
            if (includeNumberPrefix)
                inputChoices.add (String (i) + ". " + inputs[i]);
            else
                inputChoices.add (inputs[i]);
        }
    }

    return inputChoices;
}

StringArray RackInstance::getOutputChoices (bool includeNumberPrefix)
{
    StringArray outputChoices;

    if (type != nullptr)
    {
        outputChoices.add (getNoPinName());
        auto outputs = type->getOutputNames();

        for (int i = 1; i < outputs.size(); ++i)
        {
            if (includeNumberPrefix)
                outputChoices.add (String (i) + ". " + outputs[i]);
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

void RackInstance::setInputName (Channel c, const String& inputName)
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

void RackInstance::setOutputName (Channel c, const String& outputName)
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

void RackInstance::initialise (const PlaybackInitialisationInfo& info)
{
    if (type != nullptr)
        type->registerInstance (this, info);

    initialiseWithoutStopping (info);
}

void RackInstance::initialiseWithoutStopping (const PlaybackInitialisationInfo& info)
{
    if (getLatencySeconds() > 0.0)
    {
        delaySize = getLatencySamples();
        delayBuffer.setSize (2, delaySize);
        delayBuffer.clear();
    }
    else
    {
        delaySize = 0;
        delayBuffer.setSize (1, 64);
    }

    delayPos = 0;

    const float wet = wetGain->getCurrentValue();
    lastLeftIn   = dbToGain (leftInDb->getCurrentValue());
    lastRightIn  = dbToGain (linkInputs ? leftInDb->getCurrentValue() : rightInDb->getCurrentValue());
    lastLeftOut  = wet * dbToGain (leftOutDb->getCurrentValue());
    lastRightOut = wet * dbToGain (linkOutputs ? leftOutDb->getCurrentValue() : rightOutDb->getCurrentValue());

    if (type != nullptr)
        type->initialisePluginsIfNeeded (info);
}

void RackInstance::deinitialise()
{
    if (type != nullptr)
        type->deregisterInstance (this);
}

void RackInstance::updateAutomatableParamPosition (double time)
{
    Plugin::updateAutomatableParamPosition (time);

    if (type != nullptr)
        type->updateAutomatableParamPositions (time);
}

double RackInstance::getLatencySeconds()
{
    if (type != nullptr)
        return type->getLatencySeconds (sampleRate, blockSizeSamples);

    return 0.0;
}

int RackInstance::getLatencySamples()
{
    return roundToInt (getLatencySeconds() * sampleRate);
}

void RackInstance::prepareForNextBlock (const AudioRenderContext& rc)
{
    // N.B. This will be called by the EditPlaybackContext during normal playback
    if (type != nullptr && rc.isRendering)
        type->newBlockStarted();
}

void RackInstance::applyToBuffer (const AudioRenderContext& fc)
{
    const float wet = wetGain->getCurrentValue();

    if (type != nullptr)
    {
        SCOPED_REALTIME_CHECK

        float leftIn   = dbToGain (leftInDb->getCurrentValue());
        float rightIn  = dbToGain (linkInputs ? leftInDb->getCurrentValue() : rightInDb->getCurrentValue());
        float leftOut  = wet * dbToGain (leftOutDb->getCurrentValue());
        float rightOut = wet * dbToGain (linkOutputs ? leftOutDb->getCurrentValue() : rightOutDb->getCurrentValue());

        type->process (fc,
                       leftInputGoesTo,      lastLeftIn,   leftIn,
                       rightInputGoesTo,     lastRightIn,  rightIn,
                       leftOutputComesFrom,  lastLeftOut,  leftOut,
                       rightOutputComesFrom, lastRightOut, rightOut,
                       dryGain->getCurrentValue(),
                       (delaySize > 0) ? &delayBuffer : nullptr,
                       delayPos);

        lastLeftIn  = leftIn;
        lastRightIn  = rightIn;
        lastLeftOut  = leftOut;
        lastRightOut = rightOut;
    }
}

juce::String RackInstance::getSelectableDescription()
{
    return TRANS("Plugin Rack");
}

void RackInstance::setInputLevel (Channel c, float v)
{
    const auto& param = c == left ? leftInDb : rightInDb;

    param->setParameter (v, sendNotification);

    if (linkInputs)
        param->setParameter (v, sendNotification);
}

void RackInstance::setOutputLevel (Channel c, float v)
{
    const auto& param = c == left ? leftOutDb : rightOutDb;

    param->setParameter (v, sendNotification);

    if (linkOutputs)
        param->setParameter (v, sendNotification);
}

juce::String RackInstance::getInputName (Channel c)
{
    const auto& input = c == left ? leftInputGoesTo : rightInputGoesTo;

    if (type == nullptr || input < 0 || type->getInputNames()[input].isEmpty())
        return getNoPinName();

    return String (input) + ". " + type->getInputNames()[input];
}

juce::String RackInstance::getOutputName (Channel c)
{
    const auto& ouput = c == left ? leftOutputComesFrom : rightOutputComesFrom;

    if (type == nullptr || ouput < 0 || type->getOutputNames()[ouput].isEmpty())
        return getNoPinName();

    return String (ouput) + ". " + type->getOutputNames()[ouput];
}
