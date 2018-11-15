/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AutomationRecordManager::AutomationRecordManager (Edit& ed)
    : engine (ed.engine), edit (ed)
{
    if (edit.shouldPlay())
    {
        const MessageManagerLock mml (Thread::getCurrentThread());

        if (mml.lockWasGained())
            edit.getTransport().addChangeListener (this);
        else
            jassertfalse;
    }

    glideLength = engine.getPropertyStorage().getProperty (SettingID::glideLength);

    readingAutomation.referTo (edit.getTransport().state, IDs::automationRead, nullptr, true);
}

AutomationRecordManager::~AutomationRecordManager()
{
    if (edit.shouldPlay())
        edit.getTransport().removeChangeListener (this);
}

void AutomationRecordManager::setReadingAutomation (bool b)
{
    if (readingAutomation != b)
    {
        readingAutomation = b;
        sendChangeMessage();

        engine.getExternalControllerManager().automationModeChanged (readingAutomation, writingAutomation);
    }
}

void AutomationRecordManager::setWritingAutomation (bool b)
{
    if (writingAutomation != b)
    {
        writingAutomation = b;
        sendChangeMessage();

        engine.getExternalControllerManager().automationModeChanged (readingAutomation, writingAutomation);
    }
}

double AutomationRecordManager::getGlideSeconds() const
{
    return glideLength;
}

void AutomationRecordManager::setGlideSeconds (double secs)
{
    if (glideLength != secs)
    {
        glideLength = secs;
        engine.getPropertyStorage().setProperty (SettingID::glideLength, secs);
    }
}

void AutomationRecordManager::changeListenerCallback (ChangeBroadcaster* source)
{
    // called back when transport state changes
    if (source == &edit.getTransport())
    {
        const bool isPlaying = edit.getTransport().isPlaying()
                                || edit.getTransport().isRecording();

        if (wasPlaying != isPlaying)
        {
            wasPlaying = isPlaying;

            if (! isPlaying)
                punchOut (false);

            jassert (recordedParams.isEmpty());
            const ScopedLock sl (lock);
            recordedParams.clear();
        }
    }
}

bool AutomationRecordManager::isRecordingAutomation() const
{
    return recordedParams.size() > 0;
}

bool AutomationRecordManager::isParameterRecording (AutomatableParameter* param) const
{
    const ScopedLock sl (lock);

    for (auto p : recordedParams)
        if (p->parameter == param)
            return true;

    return false;
}

void AutomationRecordManager::punchOut (bool toEnd)
{
    const ScopedLock sl (lock);

    if (recordedParams.size() > 0)
    {
        auto endTime = edit.getTransport().getCurrentPosition();

        for (auto param : recordedParams)
        {
            if (toEnd)
                endTime = jmax (endTime, param->parameter->getCurve().getLength() + 1.0);

            applyChangesToParameter (param, endTime, toEnd);
            param->parameter->resetRecordingStatus();
        }

        recordedParams.clear();
        sendChangeMessage();
    }
}

void AutomationRecordManager::applyChangesToParameter (AutomationParamData* parameter, double end, bool toEnd)
{
    CRASH_TRACER
    OwnedArray<AutomationCurve> newCurves;

    {
        std::unique_ptr<AutomationCurve> curve (new AutomationCurve());

        curve->setOwnerParameter (parameter->parameter);

        for (int i = 0; i < parameter->changes.size(); ++i)
        {
            auto& change = parameter->changes.getReference(i);

            if (i > 0 && change.time < parameter->changes.getReference (i - 1).time - 0.1)
            {
                // gone back to the start - must be looping, so add this to the list and carry on..
                if (curve->getNumPoints() > 0)
                {
                    newCurves.add (curve.release());
                    curve.reset (new AutomationCurve());
                    curve->setOwnerParameter (parameter->parameter);
                }
            }

            const float oldVal = (i == 0) ? parameter->parameter->getCurve().getValueAt (change.time)
                                          : curve->getValueAt (change.time);

            const float newVal = parameter->parameter->snapToState (change.value);

            if (parameter->parameter->isDiscrete())
            {
                curve->addPoint (change.time, oldVal, 0.0f);
                curve->addPoint (change.time, newVal, 0.0f);
            }
            else
            {
                if (std::abs (oldVal - newVal) > (parameter->parameter->getValueRange().getLength() * 0.21f))
                {
                    if (i == 0)
                        curve->addPoint (change.time - 0.000001, oldVal, 0.0f);
                    else
                        curve->addPoint (change.time, oldVal, 0.0f);
                }

                curve->addPoint (change.time, newVal, 0.0f);
            }
        }

        if (curve->getNumPoints() > 0)
            newCurves.add (curve.release());
    }

    for (int i = 0; i < newCurves.size(); ++i)
    {
        auto& curve = *newCurves.getUnchecked (i);

        if (curve.getNumPoints() > 0)
        {
            auto startTime = curve.getPointTime (0);
            auto endTime = end;

            if (i < newCurves.size() - 1)
                endTime = curve.getPointTime (curve.getNumPoints() - 1);

            // if the curve is empty, set the parameter so that the bits outside the new curve
            // are set to the levels they were at when we started recording..
            if (parameter->parameter->getCurve().getNumPoints() == 0)
                parameter->parameter->setParameter (parameter->originalValue, sendNotification);

            auto& c = parameter->parameter->getCurve();

            c.mergeOtherCurve (curve, { startTime, endTime + glideLength },
                               startTime, glideLength, false, toEnd);

            if (engine.getPropertyStorage().getProperty (SettingID::simplifyAfterRecording, true))
                c.simplify (EditTimeRange (startTime, endTime + glideLength).expanded (0.001), 0.01, 0.002f);
        }
    }
}

void AutomationRecordManager::toggleWriteAutomationMode()
{
    punchOut (false);

    setWritingAutomation (! isWritingAutomation());
}

void AutomationRecordManager::postFirstAutomationChange (AutomatableParameter* param, float originalValue)
{
    jassert (param != nullptr);

    auto entry = new AutomationParamData();
    entry->parameter = param;
    entry->originalValue = originalValue;

    // recording status has changed, so inform our listeners
    sendChangeMessage();

    const ScopedLock sl (lock);
    recordedParams.add (entry);
}

void AutomationRecordManager::postAutomationChange (AutomatableParameter* param, double time, float value)
{
    const ScopedLock sl (lock);

    for (auto p : recordedParams)
    {
        if (p->parameter == param)
        {
            p->changes.add (AutomationParamData::Change (time, value));
            break;
        }
    }
}

void AutomationRecordManager::parameterBeingDeleted (AutomatableParameter* param)
{
    const ScopedLock sl (lock);

    for (int i = recordedParams.size(); --i >= 0;)
        if (recordedParams.getUnchecked (i)->parameter == param)
            recordedParams.remove (i);
}
