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

CompressorPlugin::CompressorPlugin (PluginCreationInfo info)  : Plugin (info)
{
    thresholdGain   = { *this, "threshold", IDs::threshold, TRANS("Threshold"),
                        dbToGain (-6.0f), { 0.01f, 1.0f },
                        [] (float value)             { return gainToDbString (value); },
                        [] (const juce::String& s)   { return dbStringToGain (s); } };

    ratio           = { *this, "ratio", IDs::ratio, TRANS("Ratio"),
                        0.5f, { 0.0f, 0.95f },
                        [] (float value)             { return (value > 0.001f ? juce::String (1.0f / value, 2)
                                                                              : juce::String ("INF")) + " : 1"; },
                        [] (const juce::String& s)   { return s.getFloatValue(); } };

    attackMs        = { *this, "attack", IDs::attack, TRANS("Attack"),
                        100.0f, { 0.3f, 200.0f },
                        [] (float value)             { return juce::String (value, 1) + " ms"; },
                        [] (const juce::String& s)   { return s.getFloatValue(); } };

    releaseMs       = { *this, "release", IDs::release, TRANS("Release"),
                        100.0f, { 10.0f, 300.0f },
                        [] (float value)             { return juce::String (value, 1) + " ms"; },
                        [] (const juce::String& s)   { return s.getFloatValue(); } };

    outputDb        = { *this, "output gain", IDs::outputDb, TRANS("Output gain"),
                        0.0f, { -10.0f, 24.0f },
                        [] (float value)             { return juce::Decibels::toString (value); },
                        [] (const juce::String& s)   { return dbStringToDb (s); } };

    sidechainDb     = { *this, "input gain", IDs::inputDb, TRANS("Sidechain gain"),
                        0.0f, { -24.0f, 24.0f },
                        [] (float value)             { return juce::Decibels::toString (value); },
                        [] (const juce::String& s)   { return dbStringToDb (s); } };

    useSidechainTrigger.referTo (state, IDs::sidechainTrigger, getUndoManager());
}

CompressorPlugin::~CompressorPlugin()
{
    notifyListenersOfDeletion();

    thresholdGain.reset();
    ratio.reset();
    attackMs.reset();
    releaseMs.reset();
    outputDb.reset();
    sidechainDb.reset();
}

const char* CompressorPlugin::xmlTypeName = "compressor";

void CompressorPlugin::getChannelNames (juce::StringArray* ins,
                                        juce::StringArray* outs)
{
    Plugin::getChannelNames (ins, outs);

    if (ins != nullptr)
        ins->add (TRANS("Sidechain Trigger"));
}

void CompressorPlugin::initialise (const PluginInitialisationInfo&)
{
    currentLevel = 0.0;
    lastSamp = 0.0f;
}

void CompressorPlugin::deinitialise()
{
}

static const float preFilterAmount = 0.9f; // more = smoother level detection

void CompressorPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    const double logThreshold = std::log10 (0.01);
    const double attackFactor = std::pow (10.0, logThreshold / (attackMs.getCurrentValue() * sampleRate / 1000.0));
    const double releaseFactor = std::pow (10.0, logThreshold / (releaseMs.getCurrentValue() * sampleRate / 1000.0));
    const float outputGain = dbToGain (outputDb.getCurrentValue());
    const float thresh = thresholdGain.getCurrentValue();
    const float rat = ratio.getCurrentValue();
    const bool useSidechain = useSidechainTrigger.get();
    const float sidechainGain = dbToGain (sidechainDb.getCurrentValue());

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

    clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);
}

float CompressorPlugin::getThreshold() const
{
    return thresholdGain.getCurrentValue();
}

void CompressorPlugin::setThreshold (float t)
{
    thresholdGain.setParameter (juce::jlimit (getMinThreshold(),
                                              getMaxThreshold(), t),
                                juce::sendNotification);
}

float CompressorPlugin::getRatio() const
{
    return ratio.getCurrentValue();
}

void CompressorPlugin::setRatio (float r)
{
    ratio.setParameter (juce::jlimit (0.05f, 1.0f, r),
                        juce::sendNotification);
}

void CompressorPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    thresholdGain.setFromValueTree (v);
    ratio.setFromValueTree (v);
    attackMs.setFromValueTree (v);
    releaseMs.setFromValueTree (v);
    outputDb.setFromValueTree (v);
    sidechainDb.setFromValueTree (v);

    copyPropertiesToCachedValues (v, useSidechainTrigger);
}

void CompressorPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id)
{
    if (v == state && id == IDs::sidechainTrigger)
        propertiesChanged();

    Plugin::valueTreePropertyChanged (v, id);
}

}} // namespace tracktion { inline namespace engine
