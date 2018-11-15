/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


CompressorPlugin::CompressorPlugin (PluginCreationInfo info)  : Plugin (info)
{
    thresholdGain   = addParam ("threshold", TRANS("Threshold"), { 0.01f, 1.0f },
                                [] (float value)       { return gainToDbString (value); },
                                [] (const String& s)   { return dbStringToGain (s); });

    ratio           = addParam ("ratio", TRANS("Ratio"), { 0.0f, 0.95f },
                                [] (float value)        { return (value > 0.001f ? String (1.0f / value, 2)
                                                                                 : String ("INF")) + " : 1"; },
                                [] (const String& s)    { return s.getFloatValue(); });

    attackMs        = addParam ("attack", TRANS("Attack"), { 0.3f, 200.0f },
                                [] (float value)       { return String (value, 1) + " ms"; },
                                [] (const String& s)   { return s.getFloatValue(); });

    releaseMs       = addParam ("release", TRANS("Release"), { 10.0f, 300.0f },
                                [] (float value)       { return String (value, 1) + " ms"; },
                                [] (const String& s)   { return s.getFloatValue(); });

    outputDb        = addParam ("output gain", TRANS("Output gain"), { -10.0f, 24.0f },
                                [] (float value)       { return Decibels::toString (value); },
                                [] (const String& s)   { return dbStringToDb (s); });

    sidechainDb     = addParam ("input gain", TRANS("Sidechain gain"), { -24.0f, 24.0f },
                                [] (float value)       { return Decibels::toString (value); },
                                [] (const String& s)   { return dbStringToDb (s); });

    auto um = getUndoManager();

    thresholdValue.referTo (state, IDs::threshold, um, dbToGain (-6.0f));
    thresholdGain->attachToCurrentValue (thresholdValue);

    ratioValue.referTo (state, IDs::ratio, um, 0.5f);
    ratio->attachToCurrentValue (ratioValue);

    attackValue.referTo (state, IDs::attack, um, 100.0f);
    attackMs->attachToCurrentValue (attackValue);

    releaseValue.referTo (state, IDs::release, um, 100.0f);
    releaseMs->attachToCurrentValue (releaseValue);

    outputValue.referTo (state, IDs::outputDb, um);
    outputDb->attachToCurrentValue (outputValue);

    useSidechainTrigger.referTo (state, IDs::sidechainTrigger, um);

    sidechainValue.referTo (state, IDs::inputDb, um);
    sidechainDb->attachToCurrentValue (sidechainValue);
}

CompressorPlugin::~CompressorPlugin()
{
    notifyListenersOfDeletion();

    thresholdGain->detachFromCurrentValue();
    ratio->detachFromCurrentValue();
    attackMs->detachFromCurrentValue();
    releaseMs->detachFromCurrentValue();
    outputDb->detachFromCurrentValue();
    sidechainDb->detachFromCurrentValue();
}

const char* CompressorPlugin::xmlTypeName = "compressor";

void CompressorPlugin::getChannelNames (StringArray* ins, StringArray* outs)
{
    Plugin::getChannelNames (ins, outs);

    if (ins != nullptr)
        ins->add (TRANS("Sidechain Trigger"));
}

void CompressorPlugin::initialise (const PlaybackInitialisationInfo&)
{
    currentLevel = 0.0;
    lastSamp = 0.0f;
}

void CompressorPlugin::deinitialise()
{
}

static const float preFilterAmount = 0.9f; // more = smoother level detection

void CompressorPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    fc.setMaxNumChannels (3);

    const double logThreshold = std::log10 (0.01);
    const double attackFactor = std::pow (10.0, logThreshold / (attackMs->getCurrentValue() * sampleRate / 1000.0));
    const double releaseFactor = std::pow (10.0, logThreshold / (releaseMs->getCurrentValue() * sampleRate / 1000.0));
    const float outputGain = dbToGain (outputDb->getCurrentValue());
    const float thresh = thresholdGain->getCurrentValue();
    const float rat = ratio->getCurrentValue();
    const bool useSidechain = useSidechainTrigger.get();
    const float sidechainGain = dbToGain (sidechainDb->getCurrentValue());

    float* b1 = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);

    if (fc.destBuffer->getNumChannels() >= 2)
    {
        float* b2 = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
        float* b3 = fc.destBuffer->getNumChannels() > 2 ? fc.destBuffer->getWritePointer (2, fc.bufferStartSample) : nullptr;

        for (int i = fc.bufferNumSamples; --i >= 0;)
        {
            float samp1 = *b1 + 1.0f;
            samp1 -= 1.0f;
            float samp2 = *b2 + 1.0f;
            samp2 -= 1.0f;

            float sampAvg = 0.0f;

            if (useSidechain && b3 != nullptr)
            {
                sampAvg = lastSamp * preFilterAmount
                            + std::abs (*b3++ * sidechainGain) * ((1.0f - preFilterAmount));
            }
            else
            {
                sampAvg = lastSamp * preFilterAmount
                            + std::abs (samp1 + samp2) * ((1.0f - preFilterAmount) * 0.5f);
            }

            JUCE_UNDENORMALISE (sampAvg);

            lastSamp = sampAvg;

            if (sampAvg > thresh)
                currentLevel = (currentLevel - sampAvg) * attackFactor + sampAvg;
            else
                currentLevel = (currentLevel - sampAvg) * releaseFactor + sampAvg;

            float r = outputGain;

            if (currentLevel > thresh)
            {
                r *= (float)((thresh + (currentLevel - thresh) * rat)
                              / currentLevel);
            }

            *b1++ = samp1 * r;
            *b2++ = samp2 * r;
        }
    }
    else
    {
        for (int i = fc.bufferNumSamples; --i >= 0;)
        {
            const float samp = *b1;
            const float sampAvg = lastSamp * preFilterAmount
                                    + std::abs (samp) * (1.0f - preFilterAmount);
            lastSamp = sampAvg;

            JUCE_UNDENORMALISE (lastSamp);

            if (sampAvg > thresh)
                currentLevel = (currentLevel - sampAvg) * attackFactor + sampAvg;
            else
                currentLevel = (currentLevel - sampAvg) * releaseFactor + sampAvg;

            float r = outputGain;

            if (currentLevel > thresh)
                r *= (float)((thresh + (currentLevel - thresh) * rat) / currentLevel);

            *b1++ = samp * r;
        }
    }
}

float CompressorPlugin::getThreshold() const
{
    return thresholdGain->getCurrentValue();
}

void CompressorPlugin::setThreshold (float t)
{
    thresholdGain->setParameter (jlimit (getMinThreshold(), getMaxThreshold(), t), sendNotification);
}

float CompressorPlugin::getRatio() const
{
    return ratio->getCurrentValue();
}

void CompressorPlugin::setRatio (float r)
{
    ratio->setParameter (jlimit (0.05f, 1.0f, r), sendNotification);
}

void CompressorPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &thresholdValue, &ratioValue, &attackValue, &releaseValue, &outputValue, &sidechainValue, nullptr };
    CachedValue<bool>* cvsBool[]    = { &useSidechainTrigger, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);
}

void CompressorPlugin::valueTreePropertyChanged (ValueTree& v, const Identifier& id)
{
    if (v == state && id == IDs::sidechainTrigger)
        propertiesChanged();

    Plugin::valueTreePropertyChanged (v, id);
}
