/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

AutomatableEditItem::AutomatableEditItem (Edit& ed, const juce::ValueTree& v)
    : EditItem (ed, v),
      elementState (v)
{
    remapOnTempoChange.referTo (elementState, IDs::remapOnTempoChange, &edit.getUndoManager(), false);
    edit.automatableEditItemCache.addItem (*this);
}

AutomatableEditItem::~AutomatableEditItem()
{
    edit.automatableEditItemCache.removeItem (*this);
}

//==============================================================================
void AutomatableEditItem::flushPluginStateToValueTree()
{
    saveChangedParametersToState();
}

//==============================================================================
juce::Array<AutomatableParameter*> AutomatableEditItem::getAutomatableParameters() const
{
    juce::Array<AutomatableParameter*> params;
    params.addArray (automatableParams);
    return params;
}

int AutomatableEditItem::getNumAutomatableParameters() const
{
    return automatableParams.size();
}

void AutomatableEditItem::visitAllAutomatableParams (const std::function<void(AutomatableParameter&)>& visit) const
{
    for (auto p : automatableParams)
        visit (*p);
}

AutomatableParameter::Ptr AutomatableEditItem::getAutomatableParameterByID (const juce::String& paramID) const
{
    for (auto p : automatableParams)
        if (p->paramID == paramID)
            return p;

    return {};
}

void AutomatableEditItem::deleteParameter (AutomatableParameter* p)
{
    automatableParams.removeObject (p);
    rebuildParameterTree();
}

void AutomatableEditItem::deleteAutomatableParameters()
{
    CRASH_TRACER
    auto& ech = edit.getParameterChangeHandler();
    auto& pcm = edit.getParameterControlMappings();

    for (auto& ap : automatableParams)
    {
        if (ech.getPendingParam (false).get() == ap)
            ech.getPendingParam (true);

        if (pcm.isParameterMapped (*ap))
            pcm.removeParameterMapping (*ap);
    }

    automatableParams.clear();
    parameterTree.clear();

    {
        // N.B. swap under the lock here to minimise the time held
        juce::ReferenceCountedArray<AutomatableParameter> nowActiveParams;

        {
            const std::scoped_lock sl (activeParameterLock);
            std::swap (activeParameters, nowActiveParams);
        }
    }

    sendListChangeMessage();
}

int AutomatableEditItem::indexOfAutomatableParameter (const AutomatableParameter::Ptr& param) const
{
    return automatableParams.indexOf (param);
}

AutomatableParameterTree& AutomatableEditItem::getParameterTree() const
{
    if (setIfDifferent (parameterTreeBuilt, true))
    {
        buildParameterTree();
        sendListChangeMessage();
    }

    return parameterTree;
}

juce::ReferenceCountedArray<AutomatableParameter> AutomatableEditItem::getFlattenedParameterTree() const
{
    juce::ReferenceCountedArray<AutomatableParameter> params;

    for (auto node : getParameterTree().rootNode->subNodes)
    {
        if (node->type == AutomatableParameterTree::Parameter)
            params.add (node->parameter);
        else if (node->type == AutomatableParameterTree::Group)
            params.addArray (getFlattenedParameterTree (*node));
    }

    jassert (! params.contains (nullptr));

    return params;
}

//==============================================================================
bool AutomatableEditItem::isAutomationNeeded() const noexcept
{
    return numActiveParameters.load (std::memory_order_acquire) > 0;
}

void AutomatableEditItem::setAutomatableParamPosition (TimePosition time)
{
    if (setIfDifferent (lastTime, time))
        if (edit.getAutomationRecordManager().isReadingAutomation())
            updateAutomatableParamPosition (time);
}

bool AutomatableEditItem::isBeingActivelyPlayed() const
{
    return juce::Time::getApproximateMillisecondCounter() < (unsigned int) (systemTimeOfLastPlayedBlock + 150);
}

void AutomatableEditItem::updateAutomatableParamPosition (TimePosition time)
{
    if (! isAutomationNeeded())
        return;

    for (auto p : automatableParams)
        if (p->isAutomationActive())
            p->updateToFollowCurve (time);
}

void AutomatableEditItem::updateParameterStreams (TimePosition time)
{
    const std::scoped_lock sl (activeParameterLock);

    for (auto p : activeParameters)
        p->updateFromAutomationSources (time);
}

void AutomatableEditItem::resetRecordingStatus()
{
    for (auto p : automatableParams)
        p->resetRecordingStatus();
}

void AutomatableEditItem::updateStreamIterators()
{
    for (auto p : automatableParams)
        p->updateStream();
}

void AutomatableEditItem::addActiveParameter (const AutomatableParameter& p)
{
    if (auto param = automatableParams[automatableParams.indexOf (&p)])
    {
        // This confusing order is to avoid allocating whilst the lock is held
        juce::ReferenceCountedArray<AutomatableParameter> newParams;
        int numParams = 0;

         {
            const std::scoped_lock sl (activeParameterLock);

            if (activeParameters.contains (param))
                return;

            numParams = activeParameters.size() + 1;
        }

        newParams.ensureStorageAllocated (numParams);

        {
            const std::scoped_lock sl (activeParameterLock);
            newParams = activeParameters;
        }

        newParams.add (param);

        const std::scoped_lock sl (activeParameterLock);
        std::swap (activeParameters, newParams);
    }

    lastTime = -1.0s;
}

void AutomatableEditItem::removeActiveParameter (const AutomatableParameter& p)
{
    if (auto param = automatableParams[automatableParams.indexOf (&p)])
    {
        // This confusing order is to avoid removing whilst the lock is held,
        // juce::ReferenceCountedArray can free when that happens
        juce::ReferenceCountedArray<AutomatableParameter> nowActiveParams;
        int numParams = 0;

        {
            const std::scoped_lock sl (activeParameterLock);

            if (! activeParameters.contains (param))
                return;

            numParams = activeParameters.size() - 1;
        }

        nowActiveParams.ensureStorageAllocated (numParams);

        {
            const std::scoped_lock sl (activeParameterLock);
            nowActiveParams = activeParameters;
        }

        nowActiveParams.removeObject (param);

        const std::scoped_lock sl (activeParameterLock);
        std::swap (activeParameters, nowActiveParams);
    }

    lastTime = -1.0s;
}

//==============================================================================
void AutomatableEditItem::buildParameterTree() const
{
    for (auto p : automatableParams)
        parameterTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (p));
}

void AutomatableEditItem::updateLastPlaybackTime()
{
    systemTimeOfLastPlayedBlock = juce::Time::getApproximateMillisecondCounter();
}

void AutomatableEditItem::clearParameterList()
{
    automatableParams.clear();
    rebuildParameterTree();
}

void AutomatableEditItem::addAutomatableParameter (const AutomatableParameter::Ptr& param)
{
    jassert (param != nullptr);
    automatableParams.add (param);

    if (param->isAutomationActive())
        addActiveParameter (*param);

    rebuildParameterTree();
}

void AutomatableEditItem::rebuildParameterTree()
{
    parameterTree.clear();
    parameterTreeBuilt = false;
}

juce::ReferenceCountedArray<AutomatableParameter> AutomatableEditItem::getFlattenedParameterTree (AutomatableParameterTree::TreeNode& node) const
{
    juce::ReferenceCountedArray<AutomatableParameter> params;

    for (auto subNode : node.subNodes)
    {
        if (subNode->type == AutomatableParameterTree::Parameter)
            params.add (subNode->parameter);
        else if (subNode->type == AutomatableParameterTree::Group)
            params.addArray (getFlattenedParameterTree (*subNode));
    }

    return params;
}

bool AutomatableEditItem::isActiveParameter (AutomatableParameter& p)
{
    const std::scoped_lock sl (activeParameterLock);
    return activeParameters.contains (p);
}

void AutomatableEditItem::saveChangedParametersToState()
{
    juce::MemoryOutputStream stream;

    for (auto ap : automatableParams)
    {
        if (ap->getCurrentValue() != ap->getCurrentExplicitValue())
        {
            stream.writeString (ap->paramID);
            stream.writeFloat (ap->getCurrentExplicitValue());
        }
    }

    stream.flush();
    auto um = &edit.getUndoManager();

    if (stream.getDataSize() > 0)
        elementState.setProperty (IDs::parameters, stream.getMemoryBlock(), um);
    else
        elementState.removeProperty (IDs::parameters, um);
}

void AutomatableEditItem::restoreChangedParametersFromState()
{
    if (auto mb = elementState[IDs::parameters].getBinaryData())
    {
        juce::MemoryInputStream stream (*mb, false);

        while (! stream.isExhausted())
        {
            // N.B. we must read both the string and the float here as they always both written, otherwise the stream will get out of sync
            auto paramID = stream.readString();
            auto value = stream.readFloat();

            if (auto ap = getAutomatableParameterByID (paramID))
                ap->setParameter (value, juce::dontSendNotification);
        }
    }
}

AutomatableEditItem::ParameterChangeListeners::ParameterChangeListeners (AutomatableEditItem& e) : owner (e) {}

void AutomatableEditItem::ParameterChangeListeners::handleAsyncUpdate()
{
    listeners.call ([this] (ParameterListChangeListener& l) { l.parameterListChanged (owner); });
}

void AutomatableEditItem::sendListChangeMessage() const
{
    if (parameterChangeListeners != nullptr)
        parameterChangeListeners->triggerAsyncUpdate();
}

void AutomatableEditItem::addParameterListChangeListener (ParameterListChangeListener* l)
{
    if (parameterChangeListeners == nullptr)
        parameterChangeListeners = std::make_unique<ParameterChangeListeners> (*this);

    parameterChangeListeners->listeners.add (l);
}

void AutomatableEditItem::removeParameterListChangeListener (ParameterListChangeListener* l)
{
    if (parameterChangeListeners != nullptr)
        parameterChangeListeners->listeners.remove (l);
}

}} // namespace tracktion { inline namespace engine
