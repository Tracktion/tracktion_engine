/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <ranges>

namespace tracktion::inline engine
{
    //==============================================================================
    //==============================================================================
    class AutomationRecordManagerBase
    {
    public:
        //==============================================================================
        virtual ~AutomationRecordManagerBase() {}

        //==============================================================================
        virtual bool isReadingAutomation() const noexcept = 0;
        virtual void setReadingAutomation (bool) = 0;

        virtual bool isWritingAutomation() const noexcept = 0;
        virtual void setWritingAutomation (bool) = 0;

        //==============================================================================
        virtual bool isRecordingAutomation() const = 0;
        virtual bool isParameterRecording (AutomatableParameter*) const = 0;
        virtual void punchOut (bool toEnd) = 0;

        //==============================================================================
        virtual void postFirstAutomationChange (AutomatableParameter&, float originalValue, AutomationTrigger) = 0;
        virtual void postAutomationChange (AutomatableParameter&, TimePosition time, float value) = 0;
        virtual void parameterBeingDeleted (AutomatableParameter&) = 0;
    };

    namespace v2
    {
        class AutomationRecordManager   : public AutomationRecordManagerBase,
                                          private juce::ChangeListener
        {
        public:
            //==============================================================================
            AutomationRecordManager (Edit& ed)
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
                writingAutomation.referTo (edit.getTransport().state, IDs::automationWrite, nullptr, true);
            }

            ~AutomationRecordManager() override
            {
                if (edit.shouldPlay())
                    edit.getTransport().removeChangeListener (this);
            }

            //==============================================================================
            bool isReadingAutomation() const noexcept override
            {
                return readingAutomation.get();
            }

            void setReadingAutomation (bool b) override
            {
                if (readingAutomation.get() != b)
                {
                    readingAutomation = b;

                    engine.getExternalControllerManager().automationModeChanged (readingAutomation.get(), writingAutomation.get());
                }
            }

            bool isWritingAutomation() const noexcept override
            {
                return writingAutomation.get();
            }

            void setWritingAutomation (bool b) override
            {
                if (writingAutomation != b)
                {
                    writingAutomation = b;

                    engine.getExternalControllerManager().automationModeChanged (readingAutomation.get(), writingAutomation.get());
                }
            }

            //==============================================================================
            bool isRecordingAutomation() const override
            {
                const juce::ScopedLock sl (lock);
                return recordedParams.size() > 0;
            }

            bool isParameterRecording (AutomatableParameter* param) const override
            {
                const juce::ScopedLock sl (lock);

                for (auto p : recordedParams)
                    if (&p->parameter == param)
                        return true;

                return false;
            }

            void punchInWriteParams (TimePosition pos)
            {
                const juce::ScopedLock sl (lock);

                auto punchInWriteParam = [this, pos] (auto& ap)
                {
                    auto value = ap.getCurrentBaseValue();
                    postFirstAutomationChange (ap, value, AutomationTrigger::touch);
                    postAutomationChange (ap, pos, value);
                    ap.startRecordingStatus();
                };

                for (auto p : getAllPlugins (edit, false))
                    if (auto t = p->getOwnerTrack())
                        if (t->automationMode == AutomationMode::write)
                            for (auto ap : p->getAutomatableParameters())
                                if (ap->hasAutomationPoints())
                                    punchInWriteParam (*ap);
            }

            void punchOut (bool toEnd) override
            {
                const juce::ScopedLock sl (lock);

                if (recordedParams.isEmpty())
                    return;

                if (flushTimer.isTimerRunning())
                    flushTimer.timerCallback();

                // Punch out any remaining parameters (which will include those on latch mode)
                // Do this in reverse order as punching out will modify the array
                for (int i = recordedParams.size(); --i >= 0;)
                    punchOut (recordedParams.getUnchecked (i)->parameter, toEnd);

                recordedParams.clear();
                flushTimer.stopTimer();
            }

            //==============================================================================
            Engine& engine;

        private:
            //==============================================================================
            AutomationRecordManager (const AutomationRecordManager&) = delete;

            struct AutomationParamData : public AutomatableParameter::Listener,
                                         public juce::Timer
            {
                AutomationParamData (AutomationRecordManager& o, AutomatableParameter& p, float value, AutomationTrigger t)
                    : owner (o), parameter (p), originalValue (value), trigger (t)
                {
                    if (auto& tc = p.getEdit().getTransport(); tc.looping)
                        valueAtLoopEnd = p.getCurve().getValueAt (tc.getLoopRange().getEnd(), parameter.getCurrentBaseValue());

                    parameter.addListener (this);
                }

                void changed()
                {
                    if (trigger == AutomationTrigger::value)
                        startTimer (2000);
                }

                struct Change
                {
                    inline Change (TimePosition t, float v) noexcept
                        : time (t), value (v) {}

                    TimePosition time;
                    float value;
                };

                AutomationRecordManager& owner;
                AutomatableParameter& parameter;
                const AutomationMode mode = getAutomationMode (parameter);
                juce::Array<Change> changes;
                float originalValue;
                AutomationTrigger trigger;
                std::optional<Change> lastChangeFlushed;
                std::optional<float> valueAtLoopEnd;
                const TimeDuration glideTime = tracktion::AutomationRecordManager::getGlideSeconds (parameter.getEngine());
                juce::SparseSet<TimePosition> timeRangeCovered;

            private:
                const ScopedListener listener { parameter, *this};
                const TransportControl::ReallocationInhibitor reallocationInhibitor { owner.edit.getTransport() };

                void curveHasChanged (AutomatableParameter&) override {}

                void parameterChangeGestureEnd (AutomatableParameter&) override
                {
                    owner.parameterChangeGestureEnd (parameter);
                }

                void timerCallback() override
                {
                    stopTimer();
                    owner.parameterChangeGestureEnd (parameter);
                }
            };

            Edit& edit;
            juce::CriticalSection lock;
            juce::OwnedArray<AutomationParamData> recordedParams;
            juce::CachedValue<AtomicWrapper<bool>> readingAutomation, writingAutomation;
            bool wasPlaying = false;
            LambdaTimer flushTimer { [this] { flushAutomation(); } };

            void postFirstAutomationChange (AutomatableParameter& param, float originalValue, AutomationTrigger trigger) override
            {
                TRACKTION_ASSERT_MESSAGE_THREAD
                auto mode = getAutomationMode (param);

                if (mode == AutomationMode::read)
                {
                    param.resetRecordingStatus();
                    return;
                }
                else if (mode == AutomationMode::touch
                         || mode == AutomationMode::latch
                         || mode == AutomationMode::write)
                {
                    if (param.getCurve().bypass.get())
                        param.getCurve().bypass = false;
                }

                flushTimer.startTimerHz (10);

                auto entry = std::make_unique<AutomationParamData> (*this, param, originalValue, trigger);
                entry->changed();

                const juce::ScopedLock sl (lock);
                recordedParams.add (entry.release());
            }

            void postAutomationChange (AutomatableParameter& param, TimePosition time, float value) override
            {
                TRACKTION_ASSERT_MESSAGE_THREAD
                const juce::ScopedLock sl (lock);

                for (auto p : recordedParams)
                {
                    if (&p->parameter == &param)
                    {
                        p->changes.add (AutomationParamData::Change (time, value));
                        p->changed();
                        break;
                    }
                }
            }

            void punchOut (AutomatableParameter& param, bool toEnd)
            {
                auto recordedParam = std::ranges::find_if (recordedParams,
                                                           [&param] (auto& p) { return &p->parameter == &param; });

                if (recordedParam == recordedParams.end())
                    return;

                if (flushTimer.isTimerRunning())
                    flushTimer.timerCallback();

                auto endTime = edit.getTransport().getPosition();

                // If the punch out was triggered by a position change, we want to make
                // sure the playhead position is used as the end time
                if (auto epc = edit.getTransport().getCurrentPlaybackContext())
                {
                    endTime = epc->isLooping() ? std::max (epc->getUnloopedPosition(),
                                                           epc->getLoopTimes().getEnd())
                                               : epc->getPosition();
                }

                if (toEnd)
                    endTime = std::max (endTime, toPosition (param.getCurve().getLength()) + 1_td);

                auto& autoParamData = *(*recordedParam);
                auto& curve = param.getCurve();
                auto um = &edit.getUndoManager();
                assert (autoParamData.lastChangeFlushed);

                // Glide last param
                if (autoParamData.glideTime != 0_td)
                {
                    const TimeRange glideRange (autoParamData.lastChangeFlushed->time, autoParamData.glideTime);
                    const auto valueAtGlideEnd = curve.getValueAt (glideRange.getEnd(), param.getCurrentBaseValue());
                    curve.removePointsInRegion (glideRange.withStart (glideRange.getStart() + 1us), um);
                    curve.addPoint (glideRange.getEnd(), valueAtGlideEnd, param.isDiscrete() ? 1.0f : 0.0f, um);
                }

                // Then simplify the sections
                if (engine.getPropertyStorage().getProperty (SettingID::simplifyAfterRecording, true))
                    for (auto subRange : autoParamData.timeRangeCovered.getRanges())
                        curve.simplify (TimeRange (subRange.getStart(), subRange.getEnd()).expanded (0.001_td),
                                        0.01, 0.002f, um);

                param.resetRecordingStatus();

                if (autoParamData.mode == AutomationMode::write)
                    if (auto t = param.getTrack())
                        t->automationMode = AutomationMode::latch;

                recordedParams.removeObject (*recordedParam);

                if (recordedParams.isEmpty())
                    flushTimer.stopTimer();
            }

            void parameterBeingDeleted (AutomatableParameter& param) override
            {
                const juce::ScopedLock sl (lock);

                for (int i = recordedParams.size(); --i >= 0;)
                    if (&recordedParams.getUnchecked (i)->parameter == &param)
                        recordedParams.remove (i);
            }

            void parameterChangeGestureEnd (AutomatableParameter& param)
            {
                // Touch: Punch out
                // Latch: Do nothing until play stops (continue writing existing value)
                // Write: Punch out - change to latch mode
                const juce::ScopedLock sl (lock);

                for (int i = recordedParams.size(); --i >= 0;)
                {
                    if (auto recParam = recordedParams.getUnchecked (i))
                    {
                        if (&recParam->parameter == &param)
                        {
                            using enum AutomationMode;

                            switch (recParam->mode)
                            {
                                case read:
                                    break;
                                case touch:
                                    punchOut (param, false);
                                    break;
                                case latch:
                                    break;
                                case write:
                                {
                                    punchOut (param, false);

                                    if (auto t = param.getTrack())
                                        t->automationMode = latch;

                                    break;
                                }
                            };
                        }
                    }
                }
            }

            void flushAutomation()
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
                                                                                                          : curve.getValueAt (timeRangeLoopEnd.getEnd() + 1us, paramData->parameter.getCurrentBaseValue()));
                        // Extend end so that points at the loop end are cleared
                        flushSection (paramData->parameter, curve, withEndExtended (timeRangeLoopEnd, TimeDuration (1us)), changesLoopEnd);
                        paramData->timeRangeCovered.addRange ({ timeRangeLoopEnd.getStart(), timeRangeLoopEnd.getEnd() });

                        // Then the wrapped start
                        const auto endChangeLoopStart = changesLoopStart.empty() ? endChangeLoopEnd
                                                                                 : changesLoopStart.back();
                        changesLoopStart.emplace (changesLoopStart.begin(),
                                                  timeRangeLoopStart.getStart(), endChangeLoopEnd.value);
                        changesLoopStart.emplace_back (timeRangeLoopStart.getEnd(), endChangeLoopStart.value);
                        flushSection (paramData->parameter, curve, timeRangeLoopStart, changesLoopStart);
                        paramData->timeRangeCovered.addRange ({ timeRangeLoopStart.getStart(), timeRangeLoopStart.getEnd() });
                        paramData->lastChangeFlushed = endChangeLoopStart;
                    }
                    else
                    {
                        const TimeRange timeRange { paramData->lastChangeFlushed->time,
                                                    transportPos + paramData->glideTime };
                        const auto endChange = paramData->changes.isEmpty() ? *paramData->lastChangeFlushed
                                                                            : paramData->changes.getReference (paramData->changes.size() - 1);
                        paramData->changes.add ({ timeRange.getEnd(), endChange.value });

                        flushSection (paramData->parameter, curve, timeRange, paramData->changes);
                        paramData->timeRangeCovered.addRange ({ timeRange.getStart(), timeRange.getEnd() });
                        paramData->lastChangeFlushed = endChange;
                    }

                    paramData->changes.clearQuick();
                }
            }

            static void flushSection (AutomatableParameter& parameter, AutomationCurve& curve, TimeRange time, std::span<AutomationParamData::Change> changes)
            {
                if (time.isEmpty())
                    return;

                auto um = &parameter.getEdit().getUndoManager();

                // Remove all events in this range
                curve.removePointsInRegion (time.withStart (time.getStart() + 1us), um);

                // Iterate all the changes and add them to the curve
                for (auto change : changes)
                {
                    assert (time.containsInclusive (change.time));
                    const float newVal = parameter.snapToState (change.value);
                    curve.addPoint (change.time, newVal, parameter.isDiscrete() ? 1.0f : 0.0f, um);
                }
            }

            void changeListenerCallback (juce::ChangeBroadcaster* source) override
            {
                // called back when transport state changes
                auto& tc = edit.getTransport();

                if (source == &tc)
                {
                    const bool isPlaying = tc.isPlaying()
                                           || tc.isRecording();

                    if (wasPlaying != isPlaying)
                    {
                        wasPlaying = isPlaying;

                        if (isPlaying)
                        {
                            if (isWritingAutomation())
                                punchInWriteParams (tc.getPosition());
                        }
                        else
                        {
                            punchOut (false);

                            const juce::ScopedLock sl (lock);
                            jassert (recordedParams.isEmpty());
                            recordedParams.clear();
                        }
                    }
                }
            }
        };
    }

    //==============================================================================
    //==============================================================================
    AutomationRecordManager::AutomationRecordManager (Edit& ed)
        : engine (ed.engine)
    {
        pimpl = std::make_unique<v2::AutomationRecordManager> (ed);
    }

    AutomationRecordManager::~AutomationRecordManager()
    {
    }

    bool AutomationRecordManager::isReadingAutomation() const noexcept
    {
        return pimpl->isReadingAutomation();
    }

    void AutomationRecordManager::setReadingAutomation (bool b)
    {
        auto old = isReadingAutomation();
        pimpl->setReadingAutomation (b);

        if (old != b)
            listeners.call (&Listener::automationModeChanged);
    }

    bool AutomationRecordManager::isWritingAutomation() const noexcept
    {
        return pimpl->isWritingAutomation();
    }

    void AutomationRecordManager::setWritingAutomation (bool b)
    {
        auto old = isWritingAutomation();
        pimpl->setWritingAutomation (b);

        if (old != b)
            listeners.call (&Listener::automationModeChanged);
    }

    void AutomationRecordManager::toggleWriteAutomationMode()
    {
        punchOut (false);

        setWritingAutomation (! isWritingAutomation());
    }

    //==============================================================================
    bool AutomationRecordManager::isRecordingAutomation() const
    {
        return pimpl->isRecordingAutomation();
    }

    bool AutomationRecordManager::isParameterRecording (AutomatableParameter* param) const
    {
        return pimpl->isParameterRecording (param);
    }

    void AutomationRecordManager::punchOut (bool toEnd)
    {
        pimpl->punchOut (toEnd);
    }

    //==============================================================================
    void AutomationRecordManager::addListener (Listener* l)
    {
        listeners.add (l);
    }

    void AutomationRecordManager::removeListener (Listener* l)
    {
        listeners.remove (l);
    }

    //==============================================================================
    TimeDuration AutomationRecordManager::getGlideSeconds (Engine& e)
    {
        return TimeDuration::fromSeconds (static_cast<double> (e.getPropertyStorage().getProperty (SettingID::glideLength)));
    }

    void AutomationRecordManager::setGlideSeconds (Engine& e, TimeDuration secs)
    {
        e.getPropertyStorage().setProperty (SettingID::glideLength, secs.inSeconds());
    }

    //==============================================================================
    void AutomationRecordManager::postFirstAutomationChange (AutomatableParameter& param, float originalValue, AutomationTrigger trigger)
    {
        pimpl->postFirstAutomationChange (param, originalValue, trigger);
    }

    void AutomationRecordManager::postAutomationChange (AutomatableParameter& param, TimePosition time, float value)
    {
        pimpl->postAutomationChange (param, time, value);
    }

    void AutomationRecordManager::parameterBeingDeleted (AutomatableParameter& param)
    {
        pimpl->parameterBeingDeleted (param);
    }
} // namespace tracktion::inline engine
