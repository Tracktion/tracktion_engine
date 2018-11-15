/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


class EqualiserPlugin::EQAutomatableParameter : public AutomatableParameter
{
public:
    EQAutomatableParameter (EqualiserPlugin& p, const String& xmlTag, const String& name,
                            Plugin& owner, Range<float> valueRange, int paramNumber_,
                            bool isGain_, bool isFreq_, bool isQ_)
        : AutomatableParameter (xmlTag, name, owner, valueRange),
          equaliser (p), paramNumber (paramNumber_),
          isGain (isGain_), isFreq (isFreq_), isQ (isQ_)
    {
    }

    ~EQAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override
    {
        if (isFreq)
            return String (roundToInt (value)) + " Hz";

        if (isGain)
            return Decibels::toString (value);

        return String (value, 3);
    }

    float stringToValue (const String& str) override
    {
        if (isFreq)
            return str.retainCharacters ("0123456789").getFloatValue();

        if (isGain)
            return dbStringToDb (str);

        return str.getFloatValue();
    }

    void parameterChanged (float, bool /*byAutomation*/) override
    {
        equaliser.needToUpdateFilters[paramNumber] = true;
    }

    EqualiserPlugin& equaliser;
    int paramNumber;
    bool isGain, isFreq, isQ;
};

//==============================================================================
const char* EqualiserPlugin::xmlTypeName = "4bandEq";

EqualiserPlugin::EqualiserPlugin (PluginCreationInfo info) : Plugin (info)
{
    for (int i = 4; --i >= 0;)
        needToUpdateFilters[i] = true;

    addAutomatableParameter (loFreq = new EQAutomatableParameter (*this, "Low-pass freq", TRANS("Low-shelf freq"), *this, { minFreq, maxFreq }, 0, false, true, false));
    addAutomatableParameter (loGain = new EQAutomatableParameter (*this, "Low-pass gain", TRANS("Low-shelf gain"), *this, { minGain, maxGain }, 0, true, false, false));
    addAutomatableParameter (loQ    = new EQAutomatableParameter (*this, "Low-pass Q", TRANS("Low-shelf Q"), *this, { minQ, maxQ }, 0, false, false, true));

    addAutomatableParameter (midFreq1 = new EQAutomatableParameter (*this, "Mid freq 1", TRANS("Mid freq 1"), *this, { minFreq, maxFreq }, 1, false, true, false));
    addAutomatableParameter (midGain1 = new EQAutomatableParameter (*this, "Mid gain 1", TRANS("Mid gain 1"), *this, { minGain, maxGain }, 1, true, false, false));
    addAutomatableParameter (midQ1    = new EQAutomatableParameter (*this, "Mid Q 1", TRANS("Mid Q 1"), *this, { minQ, maxQ }, 1, false, false, true));

    addAutomatableParameter (midFreq2 = new EQAutomatableParameter (*this, "Mid freq 2", TRANS("Mid freq 2"), *this, { minFreq, maxFreq }, 2, false, true, false));
    addAutomatableParameter (midGain2 = new EQAutomatableParameter (*this, "Mid gain 2", TRANS("Mid gain 2"), *this, { minGain, maxGain }, 2, true, false, false));
    addAutomatableParameter (midQ2    = new EQAutomatableParameter (*this, "Mid Q 2", TRANS("Mid Q 2"), *this, { minQ, maxQ }, 2, false, false, true));

    addAutomatableParameter (hiFreq = new EQAutomatableParameter (*this, "High-pass freq", TRANS("High-shelf freq"), *this, { minFreq, maxFreq }, 3, false, true, false));
    addAutomatableParameter (hiGain = new EQAutomatableParameter (*this, "High-pass gain", TRANS("High-shelf gain"), *this, { minGain, maxGain }, 3, true, false, false));
    addAutomatableParameter (hiQ    = new EQAutomatableParameter (*this, "High-pass Q", TRANS("High-shelf Q"), *this, { minQ, maxQ }, 3, false, false, true));

    auto um = getUndoManager();

    loFreqValue.referTo (state, IDs::loFreq, um, 80.0f);
    loGainValue.referTo (state, IDs::loGain, um);
    loQValue.referTo (state, IDs::loQ, um, 0.5f);

    hiFreqValue.referTo (state, IDs::hiFreq, um, 17000.0f);
    hiGainValue.referTo (state, IDs::hiGain, um);
    hiQValue.referTo (state, IDs::hiQ, um, 0.5f);

    midFreqValue1.referTo (state, IDs::midFreq1, um, 3000.0f);
    midGainValue1.referTo (state, IDs::midGain1, um);
    midQValue1.referTo (state, IDs::midQ1, um, 0.5f);

    midFreqValue2.referTo (state, IDs::midFreq2, um, 5000.0f);
    midGainValue2.referTo (state, IDs::midGain2, um);
    midQValue2.referTo (state, IDs::midQ2, um, 0.5f);

    phaseInvert.referTo (state, IDs::phaseInvert, um);

    loFreq->attachToCurrentValue (loFreqValue);
    loGain->attachToCurrentValue (loGainValue);
    loQ->attachToCurrentValue (loQValue);

    midFreq1->attachToCurrentValue (midFreqValue1);
    midGain1->attachToCurrentValue (midGainValue1);
    midQ1->attachToCurrentValue (midQValue1);

    midFreq2->attachToCurrentValue (midFreqValue2);
    midGain2->attachToCurrentValue (midGainValue2);
    midQ2->attachToCurrentValue (midQValue2);

    hiFreq->attachToCurrentValue (hiFreqValue);
    hiGain->attachToCurrentValue (hiGainValue);
    hiQ->attachToCurrentValue (hiQValue);
}

EqualiserPlugin::~EqualiserPlugin()
{
    notifyListenersOfDeletion();

    loFreq->detachFromCurrentValue();
    loGain->detachFromCurrentValue();
    loQ->detachFromCurrentValue();

    midFreq1->detachFromCurrentValue();
    midGain1->detachFromCurrentValue();
    midQ1->detachFromCurrentValue();

    midFreq2->detachFromCurrentValue();
    midGain2->detachFromCurrentValue();
    midQ2->detachFromCurrentValue();

    hiFreq->detachFromCurrentValue();
    hiGain->detachFromCurrentValue();
    hiQ->detachFromCurrentValue();
}

String EqualiserPlugin::getTooltip()
{
    return getName() + "$equaliserplugin";
}

void EqualiserPlugin::resetToDefault()
{
    loGain->setParameter (0.0f, dontSendNotification);
    loFreq->setParameter (80.0f, dontSendNotification);
    loQ   ->setParameter (0.5f, dontSendNotification);

    hiGain->setParameter (0.0f, dontSendNotification);
    hiFreq->setParameter (17000.0f, dontSendNotification);
    hiQ   ->setParameter (0.5f, dontSendNotification);

    midGain1->setParameter (0.0f, dontSendNotification);
    midFreq1->setParameter (3000.0f, dontSendNotification);
    midQ1   ->setParameter (0.5f, dontSendNotification);

    midGain2->setParameter (0.0f, dontSendNotification);
    midFreq2->setParameter (5000.0f, dontSendNotification);
    midQ2   ->setParameter (0.5f, dontSendNotification);

    curveNeedsUpdating = true;
}

void EqualiserPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &loFreqValue, &loGainValue, &loQValue,
                                        &hiFreqValue, &hiGainValue, &hiQValue,
                                        &midFreqValue1, &midGainValue1, &midQValue1,
                                        &midFreqValue2, &midGainValue2, &midQValue2, nullptr };
    CachedValue<bool>* cvsBool[]    = { &phaseInvert, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);
}

void EqualiserPlugin::valueTreePropertyChanged (ValueTree& parent, const Identifier& prop)
{
    curveNeedsUpdating = true;
    Plugin::valueTreePropertyChanged (parent, prop);
}

static float convertEQLevelToGain (double db)
{
    return (float) pow (10.0, db / 20.0);
}

void EqualiserPlugin::updateIIRFilters()
{
    const ScopedLock sl (filterLock);

    if (needToUpdateFilters[0])
    {
        needToUpdateFilters[0] = false;

        IIRCoefficients c = IIRCoefficients::makeLowShelf (lastSampleRate, loFreq->getCurrentValue(), loQ->getCurrentValue(),
                                                           convertEQLevelToGain (loGain->getCurrentValue()));

        for (int i = EQ_CHANS; --i >= 0;)
            low[i].setCoefficients (c);
    }

    if (needToUpdateFilters[1])
    {
        needToUpdateFilters[1] = false;

        IIRCoefficients c = IIRCoefficients::makePeakFilter (lastSampleRate, midFreq1->getCurrentValue(), midQ1->getCurrentValue(),
                                                             convertEQLevelToGain (midGain1->getCurrentValue()));

        for (int i = EQ_CHANS; --i >= 0;)
            mid1[i].setCoefficients (c);
    }

    if (needToUpdateFilters[2])
    {
        needToUpdateFilters[2] = false;

        IIRCoefficients c = IIRCoefficients::makePeakFilter (lastSampleRate, midFreq2->getCurrentValue(), midQ2->getCurrentValue(),
                                                             convertEQLevelToGain (midGain2->getCurrentValue()));

        for (int i = EQ_CHANS; --i >= 0;)
            mid2[i].setCoefficients (c);
    }

    if (needToUpdateFilters[3])
    {
        needToUpdateFilters[3] = false;

        IIRCoefficients c = IIRCoefficients::makeHighShelf (lastSampleRate, hiFreq->getCurrentValue(), hiQ->getCurrentValue(),
                                                            convertEQLevelToGain (hiGain->getCurrentValue()));

        for (int i = EQ_CHANS; --i >= 0;)
            high[i].setCoefficients (c);
    }
}

void EqualiserPlugin::initialise (const PlaybackInitialisationInfo&)
{
    for (int i = EQ_CHANS; --i >= 0;)
    {
        low[i].reset();
        mid1[i].reset();
        mid2[i].reset();
        high[i].reset();
    }

    if (lastSampleRate != sampleRate)
        curveNeedsUpdating = true;

    lastSampleRate = (float)sampleRate;

    for (int i = 4; --i >= 0;)
        needToUpdateFilters[i] = true;

    updateIIRFilters();
}

void EqualiserPlugin::deinitialise()
{
}

void EqualiserPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        const ScopedLock sl (filterLock);

        updateIIRFilters();

        jassert (fc.bufferStartSample + fc.bufferNumSamples <= fc.destBuffer->getNumSamples());

        fc.setMaxNumChannels ((int) EQ_CHANS);
        addAntiDenormalisationNoise (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

        for (int i = fc.destBuffer->getNumChannels(); --i >= 0;)
        {
            float* const data = fc.destBuffer->getWritePointer (i, fc.bufferStartSample);

            if (loGain->getCurrentValue() != 0)       low[i] .processSamples (data, fc.bufferNumSamples);
            if (midGain1->getCurrentValue() != 0)     mid1[i].processSamples (data, fc.bufferNumSamples);
            if (midGain2->getCurrentValue() != 0)     mid2[i].processSamples (data, fc.bufferNumSamples);
            if (hiGain->getCurrentValue() != 0)       high[i].processSamples (data, fc.bufferNumSamples);
        }

        if (phaseInvert)
            fc.destBuffer->applyGain (fc.bufferStartSample, fc.bufferNumSamples, -1.0f);
    }
}

float EqualiserPlugin::getDBGainAtFrequency (float f)
{
    if (curveNeedsUpdating)
    {
        updateIIRFilters();

        curve.clear();

        if (loGain->getCurrentValue() == 0 && midGain1->getCurrentValue() == 0
             && midGain2->getCurrentValue() == 0 && hiGain->getCurrentValue() == 0)
            return 0.0f;

        const int sampSize = (1 << fftOrder);

        float samps[sampSize * 2 + 8];
        zeromem (samps, sizeof (samps));
        samps[0] = 1.0f;

        if (loGain->getCurrentValue() != 0)     IIRFilter (low[0]) .processSamples (samps, sampSize);
        if (midGain1->getCurrentValue() != 0)   IIRFilter (mid1[0]).processSamples (samps, sampSize);
        if (midGain2->getCurrentValue() != 0)   IIRFilter (mid2[0]).processSamples (samps, sampSize);
        if (hiGain->getCurrentValue() != 0)     IIRFilter (high[0]).processSamples (samps, sampSize);

        fft.performRealOnlyForwardTransform (samps);

        for (int i = 0; i < sampSize / 2;)
        {
            curve.addPoint (i / (float)(sampSize / 2),
                            gainToDb (jlimit (0.01f, 10.0f, samps[i * 2])));

            if (i < sampSize / 8)
                ++i;
            else if (i < sampSize / 4)
                i += 3;
            else
                i += 6;
        }

        curveNeedsUpdating = false;
        yieldGUIThread();
    }

    return curve.getY (f / (lastSampleRate / 2));
}
