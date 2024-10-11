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
    AutomationRecordManager::AutomationParamData::AutomationParamData (AutomatableParameter& p, float value)
        : parameter (p), originalValue (value)
    {
        if (auto& tc = p.getEdit().getTransport(); tc.looping)
            valueAtLoopEnd = p.getCurve().getValueAt (tc.getLoopRange().getEnd());
    }


    //==============================================================================
    //==============================================================================
    AutomationRecordManager::AutomationRecordManager (Edit& ed)
        : engine (ed.engine), edit (ed)
    {
        if (edit.shouldPlay())
        {
            auto getMessageManagerLock = []() -> std::unique_ptr<juce::MessageManagerLock>
            {
                if (auto job = juce::ThreadPoolJob::getCurrentThreadPoolJob())
                    return std::make_unique<juce::MessageManagerLock> (job);

                if (auto t = juce::Thread::getCurrentThread())
                    return std::make_unique<juce::MessageManagerLock> (t);

                return {};
            };

            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            {
                edit.getTransport().addChangeListener (this);
            }
            else if (auto mml = getMessageManagerLock(); mml && mml->lockWasGained())
            {
                edit.getTransport().addChangeListener (this);
            }
            else
            {
                callBlocking ([this]
                              { edit.getTransport().addChangeListener (this); });
            }
        }

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

            engine.getExternalControllerManager().automationModeChanged (readingAutomation, writingAutomation);
        }
    }

    void AutomationRecordManager::setWritingAutomation (bool b)
    {
        if (writingAutomation != b)
        {
            writingAutomation = b;

            engine.getExternalControllerManager().automationModeChanged (readingAutomation, writingAutomation);
        }
    }

    TimeDuration AutomationRecordManager::getGlideSeconds (Engine& e)
    {
        return TimeDuration::fromSeconds (static_cast<double> (e.getPropertyStorage().getProperty (SettingID::glideLength)));
    }

    void AutomationRecordManager::setGlideSeconds (Engine& e, TimeDuration secs)
    {
        e.getPropertyStorage().setProperty (SettingID::glideLength, secs.inSeconds());
    }

    void AutomationRecordManager::changeListenerCallback (juce::ChangeBroadcaster* source)
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

                const juce::ScopedLock sl (lock);
                jassert (recordedParams.isEmpty());
                recordedParams.clear();
            }
        }
    }

    bool AutomationRecordManager::isRecordingAutomation() const
    {
        const juce::ScopedLock sl (lock);
        return recordedParams.size() > 0;
    }

    bool AutomationRecordManager::isParameterRecording (AutomatableParameter* param) const
    {
        const juce::ScopedLock sl (lock);

        for (auto p : recordedParams)
            if (&p->parameter == param)
                return true;

        return false;
    }

    void AutomationRecordManager::punchOut (bool toEnd)
    {
        const juce::ScopedLock sl (lock);

        if (recordedParams.isEmpty())
            return;

        auto endTime = edit.getTransport().getPosition();

        // If the punch out was triggered by a position change, we want to make
        // sure the playhead position is used as the end time
        if (auto epc = edit.getTransport().getCurrentPlaybackContext())
        {
            endTime = epc->isLooping() ? std::max (epc->getUnloopedPosition(),
                                                   epc->getLoopTimes().getEnd())
                                       : epc->getPosition();
        }

        for (auto param : recordedParams)
        {
            if (toEnd)
                endTime = std::max (endTime, toPosition (param->parameter.getCurve().getLength()) + TimeDuration::fromSeconds (1.0));

            applyChangesToParameter (*param, endTime, toEnd ? ToEnd::yes : ToEnd::no, CanGlide::yes, IsFlushing::no);
            param->parameter.resetRecordingStatus();
        }

        recordedParams.clear();

        flushTimer.stopTimer();
    }

    void AutomationRecordManager::flushAutomation()
    {
        const juce::ScopedLock sl (lock);

        if (recordedParams.isEmpty())
            return;

        auto& tc = edit.getTransport();
        const auto transportPos = tc.isPlayContextActive() ? tc.getCurrentPlaybackContext()->getPosition()
                                                           : tc.getPosition();
        const auto loopRange = tc.getLoopRange();

        for (auto paramData : recordedParams)
        {
            auto& curve = paramData->parameter.getCurve();

            // If this is the first change, add the original value just
            // before the first event to anchor the curve
            if (! paramData->lastChangeFlushed)
            {
                assert (! paramData->changes.isEmpty());
                const auto firstEventTime = paramData->changes.begin()->time;
                paramData->changes.insert (0, { firstEventTime, paramData->originalValue });
                paramData->lastChangeFlushed.emplace (firstEventTime, paramData->originalValue);
            }

            // - Just iterate through the changes
            // - If we loop around, flush the section just written with the appropriate time range to the end of the loop, adding a point at the end of the loop
            // - For the next section, make sure we flush the final end value to the end + glide time
            // - If no changes were added
            if (transportPos < paramData->lastChangeFlushed->time)
            {
                const auto timeRangeLoopEnd = loopRange.withStart (paramData->lastChangeFlushed->time);
                const auto timeRangeLoopStart = loopRange.withEnd (transportPos + paramData->glideTime);
                std::vector<AutomationParamData::Change> changesLoopEnd, changesLoopStart;

                for (auto change : paramData->changes)
                {
                    if (timeRangeLoopEnd.containsInclusive (change.time))
                        changesLoopEnd.push_back (change);
                    else if (timeRangeLoopStart.containsInclusive (change.time))
                        changesLoopStart.push_back (change);
                }

                // Flush end of loop
                const auto endChangeLoopEnd = changesLoopEnd.empty() ? *paramData->lastChangeFlushed
                                                                     : changesLoopEnd.back();
                changesLoopEnd.emplace_back (timeRangeLoopEnd.getEnd(), endChangeLoopEnd.value);
                changesLoopEnd.emplace_back (timeRangeLoopEnd.getEnd(), paramData->valueAtLoopEnd ? *paramData->valueAtLoopEnd
                                                                                                  : curve.getValueAt (timeRangeLoopEnd.getEnd() + 1us));
                // Extend end so that points at the loop end are cleared
                flushSection (curve, withEndExtended (timeRangeLoopEnd, TimeDuration (1us)), changesLoopEnd);

                // Then the wrapped start
                const auto endChangeLoopStart = changesLoopStart.empty() ? endChangeLoopEnd
                                                                         : changesLoopStart.back();
                changesLoopStart.emplace (changesLoopStart.begin(),
                                          timeRangeLoopStart.getStart(), endChangeLoopEnd.value);
                changesLoopStart.emplace_back (timeRangeLoopStart.getEnd(), endChangeLoopStart.value);
                flushSection (curve, timeRangeLoopStart, changesLoopStart);
                paramData->lastChangeFlushed = endChangeLoopStart;
            }
            else
            {
                const TimeRange timeRange { paramData->lastChangeFlushed->time,
                                            transportPos + paramData->glideTime };
                const auto endChange = paramData->changes.isEmpty() ? *paramData->lastChangeFlushed
                                                                    : paramData->changes.getReference (paramData->changes.size() - 1);
                paramData->changes.add ({ timeRange.getEnd(), endChange.value });

                flushSection (curve, timeRange, paramData->changes);
                paramData->lastChangeFlushed = endChange;
            }

            paramData->changes.clearQuick();
        }
    }

    void AutomationRecordManager::flushSection (AutomationCurve& curve, TimeRange time, std::span<AutomationParamData::Change> changes)
    {
        if (time.isEmpty())
            return;

        auto& parameter = *curve.getOwnerParameter();

        // Remove all events in this range
        // N.B. If the parameter is discrete, don't remove the extra previous point added to nudge the time forward
        curve.removePointsInRegion (parameter.isDiscrete() ? time.withStart (time.getStart() + 1us)
                                                           : time);

        // Iterate all the changes and add them to the curve
        for (auto change : changes)
        {
            assert (time.containsInclusive (change.time));
            const float newVal = parameter.snapToState (change.value);
            curve.addPoint (change.time, newVal, parameter.isDiscrete() ? 1.0f : 0.0f);
        }
    }

    void AutomationRecordManager::applyChangesToParameter (AutomationParamData& paramData, TimePosition end, ToEnd toEnd, CanGlide canGlide, IsFlushing isFlushing)
    {
        CRASH_TRACER
        auto& parameter = paramData.parameter;
        auto& curve = parameter.getCurve();
        juce::OwnedArray<AutomationCurve> newCurves;

        {
            std::unique_ptr<AutomationCurve> newCurve (new AutomationCurve());
            newCurve->setOwnerParameter (&parameter);

            for (int i = 0; i < paramData.changes.size(); ++i)
            {
                auto& change = paramData.changes.getReference (i);

                if (i > 0 && change.time < paramData.changes.getReference (i - 1).time - 0.1s)
                {
                    // gone back to the start - must be looping, so add this to the list and carry on..
                    if (newCurve->getNumPoints() > 0)
                    {
                        newCurves.add (newCurve.release());
                        newCurve = std::make_unique<AutomationCurve>();
                        newCurve->setOwnerParameter (&parameter);
                    }
                }

                const float oldVal = (i == 0) ? (curve.getNumPoints() > 0 ? curve.getValueAt (change.time)
                                                                          : paramData.originalValue)
                                              : newCurve->getValueAt (change.time);

                const float newVal = parameter.snapToState (change.value);

                if (parameter.isDiscrete())
                {
                    newCurve->addPoint (change.time, oldVal, 0.0f);
                    newCurve->addPoint (change.time, newVal, 0.0f);
                }
                else
                {
                    if (std::abs (oldVal - newVal) > (parameter.getValueRange().getLength() * 0.21f))
                    {
                        if (i == 0)
                            newCurve->addPoint (change.time - 0.000001s, oldVal, 0.0f);
                        else
                            newCurve->addPoint (change.time, oldVal, 0.0f);
                    }

                    newCurve->addPoint (change.time, newVal, 0.0f);
                }
            }

            if (newCurve->getNumPoints() > 0)
                newCurves.add (newCurve.release());
        }

        auto glideLength = canGlide == CanGlide::yes ? getGlideSeconds (engine) : 0_td;

        for (int i = 0; i < newCurves.size(); ++i)
        {
            auto& newCurve = *newCurves.getUnchecked (i);

            if (newCurve.getNumPoints() == 0)
                continue;

            auto startTime = newCurve.getPointTime (0);
            auto endTime = end;

            if (i < newCurves.size() - 1)
                endTime = newCurve.getPointTime (newCurve.getNumPoints() - 1);

            // If the original curve is empty, set the parameter so that the bits outside the new curve
            // are set to the levels they were at when we started recording..
            if (curve.getNumPoints() == 0)
                parameter.setParameter (paramData.originalValue, juce::sendNotification);

            TimeRange curveRange (startTime, endTime + glideLength);
            curve.mergeOtherCurve (newCurve, curveRange, startTime, glideLength, isFlushing == IsFlushing::yes, toEnd == ToEnd::yes);

            if (engine.getPropertyStorage().getProperty (SettingID::simplifyAfterRecording, true))
                curve.simplify (curveRange.expanded (0.001s), 0.01, 0.002f);
        }
    }

    void AutomationRecordManager::toggleWriteAutomationMode()
    {
        punchOut (false);

        setWritingAutomation (! isWritingAutomation());
    }

    void AutomationRecordManager::postFirstAutomationChange (AutomatableParameter& param, float originalValue)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        auto entry = std::make_unique<AutomationParamData> (param, originalValue);

        flushTimer.startTimerHz (10);

        const juce::ScopedLock sl (lock);
        recordedParams.add (entry.release());
    }

    void AutomationRecordManager::postAutomationChange (AutomatableParameter& param, TimePosition time, float value)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        const juce::ScopedLock sl (lock);

        for (auto p : recordedParams)
        {
            if (&p->parameter == &param)
            {
                p->changes.add (AutomationParamData::Change (time, value));
                break;
            }
        }
    }

    void AutomationRecordManager::parameterBeingDeleted (AutomatableParameter& param)
    {
        const juce::ScopedLock sl (lock);

        for (int i = recordedParams.size(); --i >= 0;)
            if (&recordedParams.getUnchecked (i)->parameter == &param)
                recordedParams.remove (i);
    }

} // namespace tracktion::inline engine
