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

ChorusPlugin::ChorusPlugin (PluginCreationInfo info)  : Plugin (info)
{
    depthParam = addParam ("depth", TRANS("Depth"), { 0.0f, 10.0f },
                          [] (float value) { return juce::String (value, 1) + " ms"; },
                          [] (const juce::String& s) { return s.getFloatValue(); });

    speedParam = addParam ("speed", TRANS("Speed"), { 0.0f, 10.0f },
                          [] (float value) { return juce::String (value, 1) + " Hz"; },
                          [] (const juce::String& s) { return s.getFloatValue(); });

    widthParam = addParam ("width", TRANS("Width"), { 0.0f, 1.0f },
                          [] (float value) { return juce::String ((int)(100.0f * value)) + "%"; },
                          [] (const juce::String& s) { return s.getFloatValue(); });

    mixParam = addParam ("mix", TRANS("Mix"), { 0.0f, 1.0f },
                        [] (float value) { return juce::String ((int)(100.0f * value)) + "%"; },
                        [] (const juce::String& s) { return s.getFloatValue(); });

    auto um = getUndoManager();

    depthMs.referTo (state, IDs::depthMs, um, 3.0f);
    speedHz.referTo (state, IDs::speedHz, um, 1.0f);
    width.referTo (state, IDs::width, um, 0.5f);
    mixProportion.referTo (state, IDs::mixProportion, um, 0.5f);

    // Attach parameters to their values
    depthParam->attachToCurrentValue (depthMs);
    speedParam->attachToCurrentValue (speedHz);
    widthParam->attachToCurrentValue (width);
    mixParam->attachToCurrentValue (mixProportion);
}

ChorusPlugin::~ChorusPlugin()
{
    notifyListenersOfDeletion();

    // Detach parameters from their values
    depthParam->detachFromCurrentValue();
    speedParam->detachFromCurrentValue();
    widthParam->detachFromCurrentValue();
    mixParam->detachFromCurrentValue();
}

// Add getter/setter implementations
void ChorusPlugin::setDepth (float value)    { depthParam->setParameter (juce::jlimit (0.0f, 10.0f, value), juce::sendNotification); }
float ChorusPlugin::getDepth()               { return depthParam->getCurrentValue(); }

void ChorusPlugin::setSpeed (float value)    { speedParam->setParameter (juce::jlimit (0.0f, 10.0f, value), juce::sendNotification); }
float ChorusPlugin::getSpeed()               { return speedParam->getCurrentValue(); }

void ChorusPlugin::setWidth (float value)    { widthParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ChorusPlugin::getWidth()               { return widthParam->getCurrentValue(); }

void ChorusPlugin::setMix (float value)      { mixParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ChorusPlugin::getMix()                 { return mixParam->getCurrentValue(); }

const char* ChorusPlugin::xmlTypeName = "chorus";

void ChorusPlugin::initialise (const PluginInitialisationInfo& info)
{
    const float delayMs = 20.0f;
    auto maxLengthMs = 1 + juce::roundToInt (delayMs + depthMs);
    auto bufferSizeSamples = juce::roundToInt ((maxLengthMs * info.sampleRate) / 1000.0);
    delayBuffer.ensureMaxBufferSize (bufferSizeSamples);
    delayBuffer.clearBuffer();
    phase = 0.0f;
}

void ChorusPlugin::deinitialise()
{
    delayBuffer.releaseBuffer();
}

void ChorusPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    float ph = 0.0f;
    int bufPos = 0;

    const float delayMs = 20.0f;
    const float minSweepSamples = (float) ((delayMs * sampleRate) / 1000.0);
    const float maxSweepSamples = (float) (((delayMs + depthMs) * sampleRate) / 1000.0);
    const float speed = (float)((juce::MathConstants<double>::pi * 2.0) / (sampleRate / speedHz));
    const int maxLengthMs = 1 + juce::roundToInt (delayMs + depthMs);
    const int lengthInSamples = juce::roundToInt ((maxLengthMs * sampleRate) / 1000.0);

    delayBuffer.ensureMaxBufferSize (lengthInSamples);

    const float feedbackGain = 0.0f; // xxx not sure why this value was here..
    const float lfoFactor = 0.5f * (maxSweepSamples - minSweepSamples);
    const float lfoOffset = minSweepSamples + lfoFactor;

    AudioFadeCurve::CrossfadeLevels wetDry (mixProportion);

    clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

    for (int chan = std::min (2, fc.destBuffer->getNumChannels()); --chan >= 0;)
    {
        float* const d = fc.destBuffer->getWritePointer (chan, fc.bufferStartSample);
        float* const buf = (float*) delayBuffer.buffers[chan].getData();

        ph = phase;
        if (chan > 0)
            ph += juce::MathConstants<float>::pi * width;

        bufPos = delayBuffer.bufferPos;

        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            const float in = d[i];

            const float sweep = lfoOffset + lfoFactor * sinf (ph);
            ph += speed;

            int intSweepPos = juce::roundToInt (sweep);
            const float interp = sweep - intSweepPos;
            intSweepPos = bufPos + lengthInSamples - intSweepPos;

            const float out = buf[(intSweepPos - 1) % lengthInSamples] * interp
                              + buf[intSweepPos % lengthInSamples] * (1.0f - interp);

            float n = in + out * feedbackGain;

            JUCE_UNDENORMALISE (n);

            buf[bufPos] = n;

            d[i] = out * wetDry.gain1 + in * wetDry.gain2;

            bufPos = (bufPos + 1) % lengthInSamples;
        }
    }

    jassert (! hasFloatingPointDenormaliseOccurred());
    zeroDenormalisedValuesIfNeeded (*fc.destBuffer);

    phase = ph;
    if (phase >= juce::MathConstants<float>::pi * 2)
        phase -= juce::MathConstants<float>::pi * 2;

    delayBuffer.bufferPos = bufPos;
}

void ChorusPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    copyPropertiesToCachedValues (v, depthMs, width, mixProportion, speedHz);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}} // namespace tracktion { inline namespace engine
