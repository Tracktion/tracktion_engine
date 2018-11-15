/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


DelayPlugin::DelayPlugin (PluginCreationInfo info) : Plugin (info)
{
    feedbackDb    = addParam ("feedback", TRANS("Feedback"), { getMinDelayFeedbackDb(), 0.0f },
                              [] (float value)       { return Decibels::toString (value); },
                              [] (const String& s)   { return dbStringToDb (s); });

    mixProportion = addParam ("mix proportion", TRANS("Mix proportion"), { 0.0f, 1.0f },
                              [] (float value)       { return String (roundToInt (value * 100.0f)) + "% wet"; },
                              [] (const String& s)   { return s.getFloatValue() / 100.0f; });

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

void DelayPlugin::initialise (const PlaybackInitialisationInfo& info)
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

void DelayPlugin::applyToBuffer (const AudioRenderContext& fc)
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

    fc.setMaxNumChannels (2);

    for (int chan = fc.destBuffer->getNumChannels(); --chan >= 0;)
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

void DelayPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &feedbackValue, &mixValue, nullptr };
    CachedValue<int>* cvsInt[]      = { &lengthMs, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
}
