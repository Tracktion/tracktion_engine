/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

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
    float stringToValue (const juce::String& s) override     { return juce::Decibels::decibelsToGain (dbStringToDb (s)); }
};

//==============================================================================
void RackInstance::createChannelMapping (int channelIndex, int defaultInput, int defaultOutput)
{
    auto um = getUndoManager();
    auto mapping = std::make_unique<ChannelMapping>();

    // Find or create the CHANNELMAP child ValueTree for this index
    juce::ValueTree mapTree;

    for (auto child : state)
    {
        if (child.hasType (IDs::CHANNELMAP) && (int) child[IDs::index] == channelIndex)
        {
            mapTree = child;
            break;
        }
    }

    if (! mapTree.isValid())
    {
        mapTree = juce::ValueTree (IDs::CHANNELMAP);
        mapTree.setProperty (IDs::index, channelIndex, nullptr);
        state.addChild (mapTree, -1, um);
    }

    mapping->inputGoesTo.referTo (mapTree, IDs::inputTo, um, defaultInput);
    mapping->outputComesFrom.referTo (mapTree, IDs::outputFrom, um, defaultOutput);
    mapping->inputGainValue.referTo (mapTree, IDs::inputGain, um, 0.0f);
    mapping->outputGainValue.referTo (mapTree, IDs::outputGain, um, 0.0f);

    auto inputParamID = "ch" + juce::String (channelIndex + 1) + " input level";
    auto inputParamName = TRANS("Ch") + " " + juce::String (channelIndex + 1) + " " + TRANS("input level");
    auto outputParamID = "ch" + juce::String (channelIndex + 1) + " output level";
    auto outputParamName = TRANS("Ch") + " " + juce::String (channelIndex + 1) + " " + TRANS("output level");

    const juce::Range<float> gainRange { (float) rackMinDb, (float) rackMaxDb };

    if (channelIndex == 0)
    {
        // Channel 0 input is always "active" (not linked)
        mapping->inputGainDb = addParam (inputParamID, inputParamName, gainRange);
        mapping->outputGainDb = addParam (outputParamID, outputParamName, gainRange);
    }
    else
    {
        addAutomatableParameter (mapping->inputGainDb = new RackInputAutomatableParameter (inputParamID, inputParamName, *this, gainRange));
        addAutomatableParameter (mapping->outputGainDb = new RackOutputAutomatableParameter (outputParamID, outputParamName, *this, gainRange));
    }

    mapping->inputGainDb->attachToCurrentValue (mapping->inputGainValue);
    mapping->outputGainDb->attachToCurrentValue (mapping->outputGainValue);

    channelMappings.add (std::move (mapping));
}

void RackInstance::migrateFromLegacyFormat()
{
    // Check for old-style stereo properties
    if (! state.hasProperty (IDs::leftTo) && ! state.hasProperty (IDs::rightTo))
        return;

    // Already has CHANNELMAP children — don't migrate twice
    for (auto child : state)
        if (child.hasType (IDs::CHANNELMAP))
            return;

    auto um = getUndoManager();

    // Create CHANNELMAP children from legacy properties
    auto createMapTree = [&] (int index, int inputDefault, int outputDefault,
                              juce::Identifier inputProp, juce::Identifier outputProp,
                              juce::Identifier inputGainProp, juce::Identifier outputGainProp)
    {
        juce::ValueTree mapTree (IDs::CHANNELMAP);
        mapTree.setProperty (IDs::index, index, nullptr);
        mapTree.setProperty (IDs::inputTo, (int) state.getProperty (inputProp, inputDefault), nullptr);
        mapTree.setProperty (IDs::outputFrom, (int) state.getProperty (outputProp, outputDefault), nullptr);
        mapTree.setProperty (IDs::inputGain, (float) state.getProperty (inputGainProp, 0.0f), nullptr);
        mapTree.setProperty (IDs::outputGain, (float) state.getProperty (outputGainProp, 0.0f), nullptr);
        state.addChild (mapTree, -1, um);
    };

    createMapTree (0, 1, 1, IDs::leftTo, IDs::leftFrom, IDs::leftInDb, IDs::leftOutDb);
    createMapTree (1, 2, 2, IDs::rightTo, IDs::rightFrom, IDs::rightInDb, IDs::rightOutDb);

    // Remove legacy properties
    state.removeProperty (IDs::leftTo, um);
    state.removeProperty (IDs::rightTo, um);
    state.removeProperty (IDs::leftFrom, um);
    state.removeProperty (IDs::rightFrom, um);
    state.removeProperty (IDs::leftInDb, um);
    state.removeProperty (IDs::rightInDb, um);
    state.removeProperty (IDs::leftOutDb, um);
    state.removeProperty (IDs::rightOutDb, um);
}

//==============================================================================
RackInstance::RackInstance (PluginCreationInfo info)
    : Plugin (info),
      rackTypeID (EditItemID::fromProperty (state, IDs::rackType)),
      type (info.edit.getRackList().getRackTypeForID (rackTypeID))
{
    jassert (type != nullptr);

    addAutomatableParameter (dryGain = new RackWetDryAutomatableParam ("dry level", TRANS("Dry level"), *this, { 0.0f, 1.0f }));
    addAutomatableParameter (wetGain = new RackWetDryAutomatableParam ("wet level", TRANS("Wet level"), *this, { 0.0f, 1.0f }));

    auto um = getUndoManager();
    dryValue.referTo (state, IDs::dry, um);
    wetValue.referTo (state, IDs::wet, um, 1.0f);
    dryGain->attachToCurrentValue (dryValue);
    wetGain->attachToCurrentValue (wetValue);

    // Migrate old stereo format to CHANNELMAP children
    migrateFromLegacyFormat();

    // Count existing CHANNELMAP children
    int numExisting = 0;

    for (auto child : state)
        if (child.hasType (IDs::CHANNELMAP))
            ++numExisting;

    if (numExisting > 0)
    {
        // Load existing mappings
        for (int i = 0; i < numExisting; ++i)
            createChannelMapping (i, i + 1, i + 1);
    }
    else
    {
        // Default: create stereo mapping
        createChannelMapping (0, 1, 1);
        createChannelMapping (1, 2, 2);
    }

    // Default numInputChannels/numOutputChannels to the CHANNELMAP count for backward compat
    numInputChannels.referTo (state, IDs::numInputChannels, um, channelMappings.size());
    numOutputChannels.referTo (state, IDs::numOutputChannels, um, channelMappings.size());

    // Ensure enough CHANNELMAPs exist for max(in, out)
    int needed = std::max (numInputChannels.get(), numOutputChannels.get());

    while (channelMappings.size() < needed)
        createChannelMapping (channelMappings.size(), -1, -1);
}

RackInstance::~RackInstance()
{
    notifyListenersOfDeletion();

    dryGain->detachFromCurrentValue();
    wetGain->detachFromCurrentValue();

    for (auto mapping : channelMappings)
    {
        mapping->inputGainDb->detachFromCurrentValue();
        mapping->outputGainDb->detachFromCurrentValue();
    }
}

juce::ValueTree RackInstance::create (RackType& type)
{
    return createValueTree (IDs::PLUGIN,
                            IDs::type, RackInstance::xmlTypeName,
                            IDs::rackType, type.itemID);
}

juce::String RackInstance::getTooltip()
{
    if (engine.getPluginManager().doubleClickToOpenWindows())
        return getName() + " (" + TRANS("Double-click to edit the rack") + ")";

    return getName() + " (" + TRANS("Click to edit the rack") + ")";
}

const char* RackInstance::xmlTypeName = "rack";

juce::String RackInstance::getName() const
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

//==============================================================================
int RackInstance::getNumChannelMappings() const
{
    return channelMappings.size();
}

int RackInstance::getNumInputChannels() const    { return numInputChannels; }
int RackInstance::getNumOutputChannels() const   { return numOutputChannels; }

int RackInstance::getNumOutputChannelsGivenInputs (int numInputs)
{
    return std::max (numInputs, numOutputChannels.get());
}

Plugin::BusLayout RackInstance::getBusses() const
{
    return BusLayout::singleInOut (ChannelConfiguration::discreteChannels (numInputChannels),
                                   ChannelConfiguration::discreteChannels (numOutputChannels));
}

void RackInstance::trimChannelMappingsToSize (int needed)
{
    while (channelMappings.size() > needed)
        removeLastChannelMapping();
}

void RackInstance::setNumInputChannels (int num)
{
    num = std::max (1, num);
    numInputChannels = num;

    int needed = std::max (num, numOutputChannels.get());

    while (channelMappings.size() < needed)
        createChannelMapping (channelMappings.size(), -1, -1);

    trimChannelMappingsToSize (needed);
    edit.restartPlayback();
}

void RackInstance::setNumOutputChannels (int num)
{
    num = std::max (1, num);
    numOutputChannels = num;

    int needed = std::max (numInputChannels.get(), num);

    while (channelMappings.size() < needed)
        createChannelMapping (channelMappings.size(), -1, -1);

    trimChannelMappingsToSize (needed);
    edit.restartPlayback();
}

int RackInstance::getInputMapping (int channelIndex) const
{
    if (auto m = channelMappings[channelIndex])
        return m->inputGoesTo;

    return -1;
}

int RackInstance::getOutputMapping (int channelIndex) const
{
    if (auto m = channelMappings[channelIndex])
        return m->outputComesFrom;

    return -1;
}

void RackInstance::setInputMapping (int channelIndex, int rackPinIndex)
{
    if (auto m = channelMappings[channelIndex])
        m->inputGoesTo = rackPinIndex;
}

void RackInstance::setOutputMapping (int channelIndex, int rackPinIndex)
{
    if (auto m = channelMappings[channelIndex])
        m->outputComesFrom = rackPinIndex;
}

void RackInstance::setInputMappingByName (int channelIndex, const juce::String& inputName)
{
    auto index = getInputChoices (false).indexOf (inputName);

    if (index == -1)
        return;

    if (index == 0)
        index = -1;

    setInputMapping (channelIndex, index);
}

void RackInstance::setOutputMappingByName (int channelIndex, const juce::String& outputName)
{
    auto index = getOutputChoices (false).indexOf (outputName);

    if (index == -1)
        return;

    if (index == 0)
        index = -1;

    setOutputMapping (channelIndex, index);
}

AutomatableParameter::Ptr RackInstance::getInputGainParam (int channelIndex) const
{
    if (auto m = channelMappings[channelIndex])
        return m->inputGainDb;

    return {};
}

AutomatableParameter::Ptr RackInstance::getOutputGainParam (int channelIndex) const
{
    if (auto m = channelMappings[channelIndex])
        return m->outputGainDb;

    return {};
}

juce::String RackInstance::getInputMappingName (int channelIndex)
{
    if (auto m = channelMappings[channelIndex])
    {
        int pinIndex = m->inputGoesTo;

        if (type == nullptr || pinIndex < 0 || type->getInputNames()[pinIndex].isEmpty())
            return getNoPinName();

        return juce::String (pinIndex) + ". " + type->getInputNames()[pinIndex];
    }

    return getNoPinName();
}

juce::String RackInstance::getOutputMappingName (int channelIndex)
{
    if (auto m = channelMappings[channelIndex])
    {
        int pinIndex = m->outputComesFrom;

        if (type == nullptr || pinIndex < 0 || type->getOutputNames()[pinIndex].isEmpty())
            return getNoPinName();

        return juce::String (pinIndex) + ". " + type->getOutputNames()[pinIndex];
    }

    return getNoPinName();
}

void RackInstance::setInputLevel (int channelIndex, float v)
{
    if (auto m = channelMappings[channelIndex])
    {
        m->inputGainDb->setParameter (v, juce::sendNotification);

        if (linkInputs)
        {
            for (auto other : channelMappings)
                if (other != m)
                    other->inputGainDb->setParameter (v, juce::sendNotification);
        }
    }
}

void RackInstance::setOutputLevel (int channelIndex, float v)
{
    if (auto m = channelMappings[channelIndex])
    {
        m->outputGainDb->setParameter (v, juce::sendNotification);

        if (linkOutputs)
        {
            for (auto other : channelMappings)
                if (other != m)
                    other->outputGainDb->setParameter (v, juce::sendNotification);
        }
    }
}

void RackInstance::addChannelMapping()
{
    int newIndex = channelMappings.size();
    createChannelMapping (newIndex, -1, -1);
}

void RackInstance::removeLastChannelMapping()
{
    if (channelMappings.size() <= 1)
        return;

    int lastIndex = channelMappings.size() - 1;

    if (auto m = channelMappings[lastIndex])
    {
        m->inputGainDb->detachFromCurrentValue();
        m->outputGainDb->detachFromCurrentValue();
    }

    // Remove the CHANNELMAP ValueTree child
    for (int i = state.getNumChildren(); --i >= 0;)
    {
        auto child = state.getChild (i);

        if (child.hasType (IDs::CHANNELMAP) && (int) child[IDs::index] == lastIndex)
        {
            state.removeChild (i, getUndoManager());
            break;
        }
    }

    channelMappings.remove (lastIndex);
}

//==============================================================================
void RackInstance::initialise (const PluginInitialisationInfo& info)
{
    if (type != nullptr)
        type->registerInstance (this, info);
}

void RackInstance::deinitialise()
{
    if (type != nullptr)
        type->deregisterInstance (this);
}

void RackInstance::updateAutomatableParamPosition (TimePosition time)
{
    Plugin::updateAutomatableParamPosition (time);
}

double RackInstance::getLatencySeconds()
{
    return 0.0;
}

void RackInstance::prepareForNextBlock (TimePosition time)
{
    // Racks aren't processed as normal plugins so just update the automation here
    if (isAutomationNeeded()
        && (edit.getAutomationRecordManager().isReadingAutomation()))
       updateParameterStreams (time);
}

void RackInstance::applyToBuffer (const PluginRenderContext&)
{
}

juce::String RackInstance::getSelectableDescription()
{
    return TRANS("Plugin Rack");
}

} // namespace tracktion::inline engine
