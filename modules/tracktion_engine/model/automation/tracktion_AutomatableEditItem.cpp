/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AutomatableEditItem::AutomatableEditItem (Edit& edit, const ValueTree& v)
    : EditItem (EditItemID::readOrCreateNewID (edit, v), edit),
      elementState (v)
{
    remapOnTempoChange.referTo (elementState, IDs::remapOnTempoChange, &edit.getUndoManager(), false);
}

AutomatableEditItem::~AutomatableEditItem()
{
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

AutomatableParameter::Ptr AutomatableEditItem::getAutomatableParameterByID (const String& paramID) const
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

    const ScopedLock sl (activeParameterLock);
    activeParameters.clear();
}

int AutomatableEditItem::indexOfAutomatableParameter (const AutomatableParameter::Ptr& param) const
{
    return automatableParams.indexOf (param);
}

AutomatableParameterTree& AutomatableEditItem::getParameterTree() const
{
    if (setIfDifferent (parameterTreeBuilt, true))
        buildParameterTree();

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

    return params;
}

//==============================================================================
void AutomatableEditItem::setAutomatableParamPosition (double time)
{
    if (setIfDifferent (lastTime, time))
        if (edit.getAutomationRecordManager().isReadingAutomation())
            updateAutomatableParamPosition (time);
}

bool AutomatableEditItem::isBeingActivelyPlayed() const
{
    return Time::getApproximateMillisecondCounter() < (unsigned int) (systemTimeOfLastPlayedBlock + 150);
}

void AutomatableEditItem::updateAutomatableParamPosition (double time)
{
    for (auto p : automatableParams)
        if (p->isAutomationActive())
            p->updateToFollowCurve (time);
}

void AutomatableEditItem::updateParameterStreams (double time)
{
    const ScopedLock sl (activeParameterLock);

    for (auto p : activeParameters)
        p->updateFromAutomationSources (time);
}

void AutomatableEditItem::resetRecordingStatus()
{
    for (auto p : automatableParams)
        p->resetRecordingStatus();
}

//==============================================================================
void AutomatableEditItem::buildParameterTree() const
{
    for (auto p : automatableParams)
        parameterTree.rootNode->addSubNode (new AutomatableParameterTree::TreeNode (p));
}

void AutomatableEditItem::updateLastPlaybackTime()
{
    systemTimeOfLastPlayedBlock = Time::getApproximateMillisecondCounter();
}

void AutomatableEditItem::addAutomatableParameter (const AutomatableParameter::Ptr& param)
{
    jassert (param != nullptr);
    automatableParams.add (param);
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

void AutomatableEditItem::updateActiveParameters()
{
    CRASH_TRACER
    ReferenceCountedArray<AutomatableParameter> nowActiveParams;

    for (auto ap : automatableParams)
        if (ap->isAutomationActive())
            nowActiveParams.add (ap);

    const ScopedLock sl (activeParameterLock);
    activeParameters.swapWith (nowActiveParams);
    automationActive.store (! activeParameters.isEmpty(), std::memory_order_relaxed);

    lastTime = -1.0;
}

void AutomatableEditItem::saveChangedParametersToState()
{
    MemoryOutputStream stream;

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
        MemoryInputStream stream (*mb, false);

        while (! stream.isExhausted())
        {
            // N.B. we must read both the string and the float here as they always both written, otherwise the stream will get out of sync
            auto paramID = stream.readString();
            auto value = stream.readFloat();

            if (auto ap = getAutomatableParameterByID (paramID))
                ap->setParameter (value, dontSendNotification);
        }
    }
}
