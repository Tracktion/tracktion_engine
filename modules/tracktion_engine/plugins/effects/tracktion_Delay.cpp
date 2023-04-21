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

DelayPlugin::DelayPlugin (PluginCreationInfo info) : Plugin (info)
{
    feedbackDb    = addParam ("feedback", TRANS("Feedback"), { getMinDelayFeedbackDb(), 0.0f },
                              [] (float value)              { return juce::Decibels::toString (value); },
                              [] (const juce::String& s)    { return dbStringToDb (s); });

    mixProportion = addParam ("mix proportion", TRANS("Mix proportion"), { 0.0f, 1.0f },
                              [] (float value)              { return juce::String (juce::roundToInt (value * 100.0f)) + "% wet"; },
                              [] (const juce::String& s)    { return s.getFloatValue() / 100.0f; });

    auto um = getUndoManager();

    feedbackValue.referTo (state, IDs::feedback, um, -6.0f);
    mixValue.referTo (state, IDs::mix, um, 0.3f);
    lengthMs.referTo (state, IDs::length, um, 150);

    feedbackDb->attachToCurrentValue (feedbackValue);
    mixProportion->attachToCurrentValue (mixValue);
}

DelayPlugin::~DelayPlugin()
{
    notifyListenersOfDeletion();

    feedbackDb->detachFromCurrentValue();
    mixProportion->detachFromCurrentValue();
}

const char* DelayPlugin::xmlTypeName = "delay";

void DelayPlugin::initialise (const PluginInitialisationInfo& info)
{
    const int lengthInSamples = (int) (lengthMs * info.sampleRate / 1000.0);
    delayBuffer.ensureMaxBufferSize (lengthInSamples);
    delayBuffer.clearBuffer();
}

void DelayPlugin::deinitialise()
{
    delayBuffer.releaseBuffer();
}

void DelayPlugin::reset()
{
    delayBuffer.clearBuffer();
}

void DelayPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    const float feedbackGain = feedbackDb->getCurrentValue() > getMinDelayFeedbackDb()
                                  ? dbToGain (feedbackDb->getCurrentValue()) : 0.0f;

    const AudioFadeCurve::CrossfadeLevels wetDry (mixProportion->getCurrentValue());

    const int lengthInSamples = (int) (lengthMs * sampleRate / 1000.0);
    delayBuffer.ensureMaxBufferSize (lengthInSamples);

    const int offset = delayBuffer.bufferPos;

    clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

    for (int chan = std::min (2, fc.destBuffer->getNumChannels()); --chan >= 0;)
    {
        float* const d = fc.destBuffer->getWritePointer (chan, fc.bufferStartSample);
        float* const buf = (float*) delayBuffer.buffers[chan].getData();

        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            float* const b = buf + ((i + offset) % lengthInSamples);

            float in = d[i];
            d[i] = wetDry.gain2 * in + wetDry.gain1 * *b;
            in += *b * feedbackGain;
            JUCE_UNDENORMALISE (in);
            *b = in;
        }
    }

    delayBuffer.bufferPos = (delayBuffer.bufferPos + fc.bufferNumSamples) % lengthInSamples;

    zeroDenormalisedValuesIfNeeded (*fc.destBuffer);
}

void DelayPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[]  = { &feedbackValue, &mixValue, nullptr };
    juce::CachedValue<int>* cvsInt[]      = { &lengthMs, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class DelayPluginTests : public juce::UnitTest
{
public:
    DelayPluginTests() : juce::UnitTest ("DelayPlugin", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        runRestoreStateTests();
    }

private:
    void runRestoreStateTests()
    {
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);

        beginTest ("Delay plugin instantiation");
        {
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (DelayPlugin::xmlTypeName, {});
            auto delay = dynamic_cast<DelayPlugin*> (pluginPtr.get());
            expect (delay != nullptr);
        }

        beginTest ("Restore feedback parameter from ValueTree");
        {
            Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (DelayPlugin::xmlTypeName, {});
            auto delay = dynamic_cast<DelayPlugin*> (pluginPtr.get());
            expect (delay != nullptr);

            float desiredValue = -30.0f;

            auto preset = createValueTree (IDs::PLUGIN,
                                           IDs::type, delay->getPluginName(),
                                           IDs::feedback, desiredValue);

            pluginPtr->restorePluginStateFromValueTree (preset);
            pluginPtr->flushPluginStateToValueTree();

            auto feedbackParam = delay->feedbackDb;

            expectWithinAbsoluteError (feedbackParam->getCurrentExplicitValue(), desiredValue, 0.001f);
            expectWithinAbsoluteError (feedbackParam->getCurrentValue(), desiredValue, 0.001f);
            expectWithinAbsoluteError (feedbackParam->getCurrentValue(), feedbackParam->getCurrentExplicitValue(), 0.001f);
            expect (! pluginPtr->state.hasProperty (IDs::parameters), "State has erroneous parameters property");
        }
    }
};

static DelayPluginTests delayPluginTests;

#endif // TRACKTION_UNIT_TESTS

}} // namespace tracktion { inline namespace engine
