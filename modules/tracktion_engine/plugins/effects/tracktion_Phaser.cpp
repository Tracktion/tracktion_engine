/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


PhaserPlugin::PhaserPlugin (PluginCreationInfo info)  : Plugin (info)
{
    auto um = getUndoManager();

    depth.referTo (state, IDs::depth, um, 5.0f);
    rate.referTo (state, IDs::rate, um, 0.4f);
    feedbackGain.referTo (state, IDs::feedback, um, 0.7f);
}

PhaserPlugin::~PhaserPlugin()
{
    notifyListenersOfDeletion();
}

const char* PhaserPlugin::xmlTypeName = "phaser";

void PhaserPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    zeromem (filterVals, sizeof (filterVals));

    const float delayMs = 100.0f;
    sweep = minSweep = (double_Pi * delayMs) / sampleRate;
    sweepFactor = 1.001f;
}

void PhaserPlugin::deinitialise()
{
}

void PhaserPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    fc.setMaxNumChannels (2);

    const double range = pow (2.0, (double) depth);
    const double sweepUp = pow (range, rate / (sampleRate / 2));
    const double sweepDown = 1.0 / sweepUp;

    const float delayMs = 100.0f;
    const float maxSweep = (float) ((double_Pi * delayMs * range) / sampleRate);

    double swpFactor = sweepFactor > 1.0 ? sweepUp : sweepDown;
    double swp = sweep;

    for (int chan = fc.destBuffer->getNumChannels(); --chan >= 0;)
    {
        float* b = fc.destBuffer->getWritePointer (chan, fc.bufferStartSample);
        swp = sweep;
        swpFactor = sweepFactor;

        for (int i = fc.bufferNumSamples; --i >= 0;)
        {
            float inval = *b;
            const double coef = (1.0 - swp) / (1.0 + swp);
            double* const fv = filterVals[chan];

            double t = inval + feedbackGain * fv[7];

            JUCE_UNDENORMALISE (t);

            fv[1] = coef * (fv[1] + t) - fv[0];
            JUCE_UNDENORMALISE (fv[1]);
            fv[0] = t;

            fv[3] = coef * (fv[3] + fv[1]) - fv[2];
            JUCE_UNDENORMALISE (fv[3]);
            fv[2] = fv[1];

            fv[5] = coef * (fv[5] + fv[3]) - fv[4];
            JUCE_UNDENORMALISE (fv[5]);
            fv[4] = fv[3];

            fv[7] = coef * (fv[7] + fv[5]) - fv[6];
            JUCE_UNDENORMALISE (fv[7]);
            fv[6] = fv[5];

            inval += (float)fv[7];
            JUCE_UNDENORMALISE (inval);

            *b++ = inval;

            swp *= swpFactor;

            if (swp > maxSweep)       swpFactor = sweepDown;
            else if (swp < minSweep)  swpFactor = sweepUp;
        }
    }

    zeroDenormalisedValuesIfNeeded (*fc.destBuffer);

    sweep = swp;
    sweepFactor = swpFactor;
}

String PhaserPlugin::getSelectableDescription()
{
    return TRANS("Phaser Plugin");
}

void PhaserPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &depth, &rate, &feedbackGain, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
}
