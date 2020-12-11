/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
namespace Distortion
{
    inline float saturate (float input, float drive, float lowclip, float highclip)
    {
        input = jlimit (lowclip, highclip, input);
        return input - drive * input * std::fabs (input);
    }

    inline void distortion (float* data, int count, float drive, float lowclip, float highclip)
    {
        if (drive <= 0.f)
            return;

        float gain = drive < 0.5f ? 2.0f * drive + 1.0f : drive * 4.0f;

        while (--count >= 0)
        {
            *data = saturate (*data, drive, lowclip, highclip) * gain;
            data++;
        }
    }
}

inline void clip (float* data, int numSamples)
{
    while (--numSamples >= 0)
    {
        *data = jlimit (-1.0f, 1.0f, *data);
        data++;
    }
}

//==============================================================================
class FODelayLine
{
public:
    FODelayLine (float maximumDelay = 0.001f, float sr = 44100.0f)
    {
        resize (maximumDelay, sr);
    }

    void resize (float maximumDelay, float sr)
    {
        sampleRate = sr;
        numSamples = (int) std::ceil (maximumDelay * sampleRate);
        sampleBuffer.resize ((size_t) numSamples);
        memset (sampleBuffer.data(), 0, sizeof(float) * (size_t) numSamples);
        currentPos = 0;
    }

    void reset()
    {
        memset (sampleBuffer.data(), 0, sizeof(float) * (size_t) numSamples);
    }

    inline float samplesToSeconds (float numSamplesIn, float sampleRateIn)
    {
        return numSamplesIn / sampleRateIn;
    }

    inline float read (float atTime)
    {
        jassert (atTime >= 0.0f && atTime < samplesToSeconds (float (numSamples), sampleRate));

        float pos = jmax (1.0f, atTime * (sampleRate - 1));

        int intPos = (int) std::floor (pos);
        float f = pos - intPos;

        int n1 = currentPos - intPos;
        while (n1 < 0)
            n1 += numSamples;
        while (n1 >= numSamples)
            n1 -= numSamples;

        int n2 = n1 - 1;
        if (n2 < 0)
            n2 += numSamples;

        jassert (n1 >= 0 && n1 < numSamples);
        jassert (n2 >= 0 && n2 < numSamples);

        return (1.0f - f) * sampleBuffer[(size_t) n1] + f * sampleBuffer[(size_t) n2];
    }

    inline void write (const float input)
    {
        sampleBuffer[(size_t) currentPos] = input;
        currentPos++;

        if (currentPos >= numSamples)
            currentPos = 0;
    }

protected:
    int numSamples {0};
    float sampleRate {44100};
    int currentPos {0};
    std::vector<float> sampleBuffer;
};

//==============================================================================
class FODelay
{
public:
    void process (AudioSampleBuffer& buffer, int numSamples)
    {
        float* lOut = buffer.getWritePointer (0);
        float* rOut = buffer.getWritePointer (1);

        AudioScratchBuffer scratchBuffer (buffer);

        float* lWork = scratchBuffer.buffer.getWritePointer (0);
        float* rWork = scratchBuffer.buffer.getWritePointer (1);

        FloatVectorOperations::copy (lWork, lOut, numSamples);
        FloatVectorOperations::copy (rWork, rOut, numSamples);

        for (int i = 0; i < numSamples; i++)
        {
            const float lVal = leftDelay.read  (delay);
            const float rVal = rightDelay.read (delay);

            leftDelay.write  (lWork[i] + (feedback * lVal) + (crossfeed * rVal));
            rightDelay.write (rWork[i] + (feedback * rVal) + (crossfeed * lVal));

            lWork[i] = lVal;
            rWork[i] = rVal;
        }

        // Wet/Dry Mix
        for (int i = 0; i < numSamples; i++)
        {
            AudioFadeCurve::CrossfadeLevels wetDry (mix);

            lOut[i] = (wetDry.gain2 * lOut[i]) + (wetDry.gain1 * lWork[i]);
            rOut[i] = (wetDry.gain2 * rOut[i]) + (wetDry.gain1 * rWork[i]);
        }
    }

    void setSampleRate (double sr)
    {
        leftDelay.resize (5.1f, float (sr));
        rightDelay.resize (5.1f, float (sr));
    }

    void setParams (float delayIn, float feedbackIn, float crossfeedIn, float mixIn)
    {
        delay = delayIn;
        feedback = jmin (0.99f, feedbackIn);
        crossfeed = jmin (0.99f, crossfeedIn);
        mix = mixIn;
    }

    void reset()
    {
        leftDelay.reset();
        rightDelay.reset();
    }

private:
    FODelayLine leftDelay, rightDelay;

    float mix = 0, feedback = 0, delay = 0, crossfeed = 0;
};

//==============================================================================
class FOChorus
{
public:
    void process (AudioSampleBuffer& buffer, int numSamples)
    {
        float ph = 0.0f;
        int bufPos = 0;

        const float delayMs = 20.0f;
        const float minSweepSamples = (float) ((delayMs * sampleRate) / 1000.0);
        const float maxSweepSamples = (float) (((delayMs + depthMs) * sampleRate) / 1000.0);
        const float speed = (float)((double_Pi * 2.0) / (sampleRate / speedHz));
        const int maxLengthMs = 1 + roundToInt (delayMs + depthMs);
        const int lengthInSamples = roundToInt ((maxLengthMs * sampleRate) / 1000.0);

        delayBuffer.ensureMaxBufferSize (lengthInSamples);

        const float lfoFactor = 0.5f * (maxSweepSamples - minSweepSamples);
        const float lfoOffset = minSweepSamples + lfoFactor;

        AudioFadeCurve::CrossfadeLevels wetDry (mix);

        for (int chan = buffer.getNumChannels(); --chan >= 0;)
        {
            float* const d = buffer.getWritePointer (chan, 0);
            float* const buf = (float*) delayBuffer.buffers[chan].getData();

            ph = phase;
            if (chan > 0)
                ph += float_Pi * width;

            bufPos = delayBuffer.bufferPos;

            for (int i = 0; i < numSamples; ++i)
            {
                const float in = d[i];

                const float sweep = lfoOffset + lfoFactor * sinf (ph);
                ph += speed;

                int intSweepPos = roundToInt (sweep);
                const float interp = sweep - intSweepPos;
                intSweepPos = bufPos + lengthInSamples - intSweepPos;

                const float out = buf[(intSweepPos - 1) % lengthInSamples] * interp + buf[intSweepPos % lengthInSamples] * (1.0f - interp);

                float n = in;

                JUCE_UNDENORMALISE (n);

                buf[bufPos] = n;
                d[i] = out * wetDry.gain1 + in * wetDry.gain2;
                bufPos = (bufPos + 1) % lengthInSamples;
            }
        }

        jassert (! hasFloatingPointDenormaliseOccurred());
        zeroDenormalisedValuesIfNeeded (buffer);

        phase = ph;
        if (phase >= MathConstants<float>::pi * 2)
            phase -= MathConstants<float>::pi * 2;

        delayBuffer.bufferPos = bufPos;
    }

    void setSampleRate (double sr)
    {
        sampleRate = sr;

        const float delayMs = 20.0f;
        const int maxLengthMs = 1 + roundToInt (delayMs + depthMs);
        int bufferSizeSamples = roundToInt ((maxLengthMs * sr) / 1000.0);
        delayBuffer.ensureMaxBufferSize (bufferSizeSamples);
        delayBuffer.clearBuffer();
        phase = 0.0f;
    }

    void setParams (float speedIn, float depthIn, float widthIn, float mixIn)
    {
        speedHz = speedIn;
        depthMs = depthIn;
        width = widthIn;
        mix = mixIn;
    }

    void reset()
    {
        delayBuffer.clearBuffer();
    }

private:
    DelayBufferBase delayBuffer;
    double sampleRate = 0;

    float phase = 0, speedHz = 1.0f, depthMs = 3.0f, width = 0.5f, mix = 0;
};

//==============================================================================
class FourOscVoice : public MPESynthesiserVoice
{
public:
    FourOscVoice (FourOscPlugin& s) : synth (s)
    {
        for (auto p : synth.getAutomatableParameters())
            smoothers[p] = {};
    }

    void noteStarted() override
    {
        if (isPlaying)
        {
            if (synth.isLegato())
            {
                activeNote.setTargetValue (currentlyPlayingNote.initialNote);

                ampAdsr.noteOn();
                filterAdsr.noteOn();
                modAdsr1.noteOn();
                modAdsr2.noteOn();
            }
            else
            {
                noteStopped (true);
                retrigger = true;
                isQuickStop = true;
            }
        }
        else
        {
            activeNote.setCurrentAndTargetValue (currentlyPlayingNote.initialNote);

            isPlaying = true;
            isQuickStop = false;
            retrigger = false;

            ampAdsr.reset();
            filterAdsr.reset();
            modAdsr1.reset();
            modAdsr2.reset();
            lfo1.reset();
            lfo2.reset();

            ScopedValueSetter<bool> svs (snapAllValues, true);
            updateParams (0);

            ampAdsr.noteOn();
            filterAdsr.noteOn();
            modAdsr1.noteOn();
            modAdsr2.noteOn();
            lfo1.reset();
            lfo2.reset();

            filterL1.reset();
            filterR1.reset();
            filterL2.reset();
            filterR2.reset();

            for (auto& o : oscillators)
                o.start();

            filterFrequencySmoother.snapToValue();

            firstBlock = true;
        }
    }

    void noteStopped (bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampAdsr.noteOff();
            filterAdsr.noteOff();
            modAdsr1.noteOff();
            modAdsr2.noteOff();
        }
        else
        {
            ampAdsr.reset();
            filterAdsr.reset();
            modAdsr1.reset();
            modAdsr2.reset();
            clearCurrentNote();
            isPlaying = false;
            isQuickStop = false;
        }
    }

    void setCurrentSampleRate (double newRate) override
    {
        if (newRate > 0)
        {
            MPESynthesiserVoice::setCurrentSampleRate (newRate);

            for (auto& o : oscillators)
                o.setSampleRate (newRate);

            ampAdsr.setSampleRate (newRate);
            filterAdsr.setSampleRate (newRate);
            modAdsr1.setSampleRate (newRate);
            modAdsr2.setSampleRate (newRate);
            lfo1.setSampleRate (newRate);
            lfo2.setSampleRate (newRate);

            lastLegato = paramValue (synth.legato);
            activeNote.reset (newRate, paramValue (synth.legato) / 1000.0f);
            filterFrequencySmoother.reset (newRate, 0.05f);

            for (auto& itr : smoothers)
                itr.second.reset (newRate, 0.01f);
        }
    }

    float velocityToGain (float velocity, float velocitySensitivity = 1.0f)
    {
        float v = velocity * velocitySensitivity + 1.0f - velocitySensitivity;
        return v * std::pow (25.0f, v) * 0.04f;
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        ScopedValueSetter<bool> svs (snapAllValues, firstBlock ? true : snapAllValues);

        updateParams (numSamples);

        if (firstBlock)
        {
            filterFrequencySmoother.snapToValue();
            firstBlock = false;
        }

        if (numSamples > renderBuffer.getNumSamples())
            renderBuffer.setSize (2, numSamples, false, false, true);

        renderBuffer.clear();

        // Run oscillators
        for (auto& o : oscillators)
            o.process (renderBuffer, 0, numSamples);

        // Apply velocity
        float velocityGain = velocityToGain (currentlyPlayingNote.noteOnVelocity.asUnsignedFloat(), paramValue (synth.ampVelocity) / 100.0f);
        velocityGain = jlimit (0.0f, 1.0f, velocityGain);
        renderBuffer.applyGain (velocityGain);

        // Apply filter
        if (synth.filterTypeValue != 0)
        {
            filterL1.processSamples (renderBuffer.getWritePointer (0), numSamples);
            filterR1.processSamples (renderBuffer.getWritePointer (1), numSamples);

            if (synth.filterSlopeValue == 24)
            {
                clip (renderBuffer.getWritePointer (0), numSamples);
                clip (renderBuffer.getWritePointer (1), numSamples);

                filterL2.processSamples (renderBuffer.getWritePointer (0), numSamples);
                filterR2.processSamples (renderBuffer.getWritePointer (1), numSamples);
            }
        }

        // Apply ADSR
        ampAdsr.applyEnvelopeToBuffer (renderBuffer, 0, numSamples);

        // Add to output
        if (outputBuffer.getNumChannels() == 1)
        {
            outputBuffer.addFrom (0, startSample, renderBuffer, 0, 0, numSamples, 0.5f);
            outputBuffer.addFrom (0, startSample, renderBuffer, 1, 0, numSamples, 0.5f);
        }
        else
        {
            outputBuffer.addFrom (0, startSample, renderBuffer, 0, 0, numSamples);
            outputBuffer.addFrom (1, startSample, renderBuffer, 1, 0, numSamples);
        }

        if (! ampAdsr.isActive())
        {
            isPlaying = false;
            if (retrigger)
            {
                noteStarted();
                retrigger = false;
                isQuickStop = false;
            }
            else
            {
                clearCurrentNote();
            }
        }

        for (auto& itr : smoothers)
            itr.second.process (numSamples);
    }

    void applyEnvelopeToBuffer (ADSR& adsr, AudioSampleBuffer& buffer, int startSample, int numSamples)
    {
        float* l = buffer.getWritePointer (0, startSample);
        float* r = buffer.getWritePointer (1, startSample);

        while (--numSamples >= 0)
        {
            float db = adsr.getNextSample() * 100.0f - 100.0f;
            float gain = Decibels::decibelsToGain (db);

            *l++ *= gain;
            *r++ *= gain;
        }
    }

    void getLiveModulationPositions (AutomatableParameter::Ptr param, Array<float>& positions)
    {
        if (isActive())
            positions.add (param->valueRange.convertTo0to1 (paramValue (param)));
    }

    void getLiveFilterFrequency (Array<float>& positions)
    {
        if (isActive())
             positions.add ((12.0f * std::log2 (lastFilterFreq / 440.0f) + 69.0f) / 135.076232f);
    }

    void updateParams (int numSamples)
    {
        // Update mod values
        currentModValue[(int)FourOscPlugin::lfo1] = lfo1.getCurrentValue();
        currentModValue[(int)FourOscPlugin::lfo2] = lfo2.getCurrentValue();
        currentModValue[(int)FourOscPlugin::env1] = modAdsr1.getEnvelopeValue();
        currentModValue[(int)FourOscPlugin::env2] = modAdsr2.getEnvelopeValue();

        currentModValue[FourOscPlugin::mpePressure]   = currentlyPlayingNote.pressure.asUnsignedFloat();
        currentModValue[FourOscPlugin::mpeTimbre]     = currentlyPlayingNote.timbre.asUnsignedFloat();
        currentModValue[FourOscPlugin::midiNoteNum]   = currentlyPlayingNote.initialNote / 127.0f;
        currentModValue[FourOscPlugin::midiVelocity]  = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();

        for (int i = 0; i <= 127; i++)
            currentModValue[FourOscPlugin::ccBankSelect + i] = synth.controllerValues[i];

        // Flush the LFOs and envelopes
        lfo1.process (numSamples);
        lfo2.process (numSamples);
        for (int i = 0; i < numSamples; i++)
        {
            modAdsr1.getNextSample();
            modAdsr2.getNextSample();
        }

        // Mod
        modAdsr1.setParameters ({
            paramValue (synth.modEnvParams[0]->modAttack),
            paramValue (synth.modEnvParams[0]->modDecay),
            paramValue (synth.modEnvParams[0]->modSustain) / 100.0f,
            paramValue (synth.modEnvParams[0]->modRelease),
        });

        modAdsr2.setParameters ({
            paramValue (synth.modEnvParams[1]->modAttack),
            paramValue (synth.modEnvParams[1]->modDecay),
            paramValue (synth.modEnvParams[1]->modSustain) / 100.0f,
            paramValue (synth.modEnvParams[1]->modRelease),
        });

        float lfoFreq1;
        if (synth.lfoParams[0]->syncValue)
            lfoFreq1 = 1.0f / ((synth.lfoParams[0]->beatValue.get()) / (synth.getCurrentTempo() / 60.0f));
        else
            lfoFreq1 = paramValue (synth.lfoParams[0]->rate);

        lfo1.setParameters ({
            lfoFreq1,
            0,
            0,
            paramValue (synth.lfoParams[0]->depth),
            (SimpleLFO::WaveShape) synth.lfoParams[0]->waveShapeValue.get(),
            0.5f
        });

        float lfoFreq2;
        if (synth.lfoParams[1]->syncValue)
            lfoFreq2 = 1.0f / ((synth.lfoParams[1]->beatValue.get()) / (synth.getCurrentTempo() / 60.0f));
        else
            lfoFreq2 = paramValue (synth.lfoParams[1]->rate);

        lfo2.setParameters ({
            lfoFreq2,
            0,
            0,
            paramValue (synth.lfoParams[1]->depth) / 2,
            (SimpleLFO::WaveShape) synth.lfoParams[1]->waveShapeValue.get(),
            0.5f
        });

        // Amp
        ampAdsr.setAnalog (synth.ampAnalogValue);

        ampAdsr.setParameters ({
            paramValue (synth.ampAttack),
            paramValue (synth.ampDecay),
            paramValue (synth.ampSustain) / 100.0f,
            isQuickStop ? jmin (0.01f, paramValue (synth.ampRelease)) : paramValue (synth.ampRelease)
        });

        // Filter
        filterAdsr.setParameters ({
            paramValue (synth.filterAttack),
            paramValue (synth.filterDecay),
            paramValue (synth.filterSustain) / 100.0f,
            paramValue (synth.filterRelease)
        });

        int type = synth.filterTypeValue;
        float filterEnv = filterAdsr.getEnvelopeValue();
        float filterSens = paramValue (synth.filterVelocity) / 100.0f;
        filterSens = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat() * filterSens + 1.0f - filterSens;
        filterEnv *= filterSens;

        for (int i = 0; i < numSamples; i++)
            filterAdsr.getNextSample();

        auto getMidiNoteInHertz = [](float noteNumber)
        {
            return 440.0f * std::pow (2.0f, (noteNumber - 69) / 12.0f);
        };

        float freqNote = paramValue (synth.filterFreq);
        freqNote += (currentlyPlayingNote.initialNote - 60) * paramValue (synth.filterKey) / 100.0f;
        freqNote += filterEnv * (paramValue (synth.filterAmount) * 137);

        filterFrequencySmoother.setValue (freqNote / 135.076232f);
        if (snapAllValues)
            filterFrequencySmoother.snapToValue();

        freqNote = filterFrequencySmoother.getCurrentValue() * 135.076232f;
        filterFrequencySmoother.process (numSamples);

        lastFilterFreq = jlimit (8.0f, jmin (20000.0f, float (currentSampleRate) / 2.0f), getMidiNoteInHertz (freqNote));
        float q = 0.70710678118655f / (1.0f - (paramValue (synth.filterResonance) / 100.0f) * 0.99f);

        if (type != 0)
        {
            IIRCoefficients coefs1, coefs2;

            if (type == 1)
            {
                coefs1 = IIRCoefficients::makeLowPass (currentSampleRate, lastFilterFreq, q);
                coefs2 = IIRCoefficients::makeLowPass (currentSampleRate, lastFilterFreq, 0.70710678118655f);
            }
            else if (type == 2)
            {
                coefs1 = IIRCoefficients::makeHighPass (currentSampleRate, lastFilterFreq, q);
                coefs2 = IIRCoefficients::makeHighPass (currentSampleRate, lastFilterFreq, 0.70710678118655f);
            }
            else if (type == 3)
            {
                coefs1 = IIRCoefficients::makeBandPass (currentSampleRate, lastFilterFreq, q);
                coefs2 = IIRCoefficients::makeBandPass (currentSampleRate, lastFilterFreq, 0.70710678118655f);
            }
            else if (type == 4)
            {
                coefs1 = IIRCoefficients::makeNotchFilter (currentSampleRate, lastFilterFreq, q);
                coefs2 = IIRCoefficients::makeNotchFilter (currentSampleRate, lastFilterFreq, 0.70710678118655f);
            }

            filterL1.setCoefficients (coefs1);
            filterR1.setCoefficients (coefs1);

            filterL2.setCoefficients (coefs2);
            filterR2.setCoefficients (coefs2);
        }

        // Oscillators
        double activeNoteSmoothed = activeNote.getNextValue();
        activeNote.skip (numSamples);

        int idx = 0;
        for (auto& o : oscillators)
        {
            double note = activeNoteSmoothed + currentlyPlayingNote.totalPitchbendInSemitones;
            note += roundToInt (paramValue (synth.oscParams[idx]->tune)) + paramValue (synth.oscParams[idx]->fineTune) / 100.0;

            o.setNote (float (note));
            o.setGain (Decibels::decibelsToGain (paramValue (synth.oscParams[idx]->level)));
            o.setWave ((Oscillator::Waves)(int (synth.oscParams[idx]->waveShapeValue.get())));
            o.setPulseWidth (paramValue (synth.oscParams[idx]->pulseWidth));
            o.setNumVoices (synth.oscParams[idx]->voicesValue);
            o.setDetune (paramValue (synth.oscParams[idx]->detune));
            o.setSpread (paramValue (synth.oscParams[idx]->spread) / 100.0f);
            o.setPan (paramValue (synth.oscParams[idx]->pan));

            idx++;
        }

        if (lastLegato != paramValue (synth.legato) && ! activeNote.isSmoothing())
        {
            lastLegato = paramValue (synth.legato);
            activeNote.reset (currentSampleRate, lastLegato / 1000.0f);
        }
    }

    void notePressureChanged() override     {}
    void notePitchbendChanged() override    {}
    void noteTimbreChanged() override       {}
    void noteKeyStateChanged() override     {}

private:
    float paramValue (AutomatableParameter::Ptr param)
    {
        jassert (param != nullptr);
        if (param == nullptr)
            return 0.0f;

        auto smoothItr = smoothers.find (param.get());
        if (smoothItr == smoothers.end())
            return param->getCurrentValue();

        auto modItr = synth.modMatrix.find (param.get());
        if (modItr == synth.modMatrix.end() || ! modItr->second.isModulated())
        {
            smoothItr->second.setValue (param->getCurrentNormalisedValue());

            if (snapAllValues)
                smoothItr->second.snapToValue();

            return param->valueRange.convertFrom0to1 (smoothItr->second.getCurrentValue());
        }
        else
        {
            float val = param->getCurrentNormalisedValue();

            auto& mod = modItr->second;

            for (int i = mod.firstModIndex; i < numElementsInArray (mod.depths) && i <= mod.lastModIndex; i++)
            {
                float d = mod.depths[i];
                if (d > -1000.0f)
                    val += currentModValue[i] * d;
            }

            val = jlimit (0.0f, 1.0f, val);

            smoothItr->second.setValue (val);

            if (snapAllValues)
                smoothItr->second.snapToValue();

            return param->valueRange.convertFrom0to1 (smoothItr->second.getCurrentValue());
        }
    }

    FourOscPlugin& synth;

    juce::AudioSampleBuffer renderBuffer {2, 512};
    MultiVoiceOscillator oscillators[4];
    ExpEnvelope ampAdsr;
    LinEnvelope filterAdsr, modAdsr1, modAdsr2;
    SimpleLFO lfo1, lfo2;
    juce::IIRFilter filterL1, filterR1, filterL2, filterR2;

    ValueSmoother<float> filterFrequencySmoother;

    bool retrigger = false, isPlaying = false, isQuickStop = false, snapAllValues = false, firstBlock = false;
    LinearSmoothedValue<float> activeNote;
    float lastLegato = -1.0f, lastFilterFreq = 0;

    float currentModValue[FourOscPlugin::numModSources] = {0};

    std::map<AutomatableParameter*, ValueSmoother<float>> smoothers;
};

//==============================================================================
FourOscPlugin::OscParams::OscParams (FourOscPlugin& plugin, int oscNum)
{
    auto um = plugin.getUndoManager();

    auto oscID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num));
    };

    waveShapeValue.referTo (plugin.state, oscID (IDs::waveShape, oscNum), um, oscNum == 1 ? 1 : 0);
    tuneValue.referTo (plugin.state, oscID (IDs::tune, oscNum), um, 0);
    fineTuneValue.referTo (plugin.state, oscID (IDs::fineTune, oscNum), um, 0);
    levelValue.referTo (plugin.state, oscID (IDs::level, oscNum), um, 0);
    pulseWidthValue.referTo (plugin.state, oscID (IDs::pulseWidth, oscNum), um, 0.5);
    voicesValue.referTo (plugin.state, oscID (IDs::voices, oscNum), um, 1);
    detuneValue.referTo (plugin.state, oscID (IDs::detune, oscNum), um, 0);
    spreadValue.referTo (plugin.state, oscID (IDs::spread, oscNum), um, 0);
    panValue.referTo (plugin.state, oscID (IDs::pan, oscNum), um, 0);

    auto paramID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num)).toString();
    };

    tune        = plugin.addParam (paramID (IDs::tune, oscNum), TRANS("Tune") + " " + String (oscNum), {-36.0f, 36.0f, 1.0f}, "st");
    fineTune    = plugin.addParam (paramID (IDs::fineTune, oscNum), TRANS("Fine Tune") + " " + String (oscNum), {-100.0f, 100.0f});
    level       = plugin.addParam (paramID (IDs::level, oscNum), TRANS("Level") + " " + String (oscNum), {-100.0f, 0.0f, 0.0f, 4.0f}, "dB");
    pulseWidth  = plugin.addParam (paramID (IDs::pulseWidth, oscNum), TRANS("Pulse Width") + " " + String (oscNum), {0.01f, 0.99f});
    detune      = plugin.addParam (paramID (IDs::detune, oscNum), TRANS("Detune") + " " + String (oscNum), {0.0f, 0.5f});
    spread      = plugin.addParam (paramID (IDs::spread, oscNum), TRANS("Spread") + " " + String (oscNum), {-100.0f, 100.0f}, "%");
    pan         = plugin.addParam (paramID (IDs::pan, oscNum), TRANS("Pan") + " " + String (oscNum), {-1.0f, 1.0f});
}

void FourOscPlugin::OscParams::attach()
{
    tune->attachToCurrentValue (tuneValue);
    fineTune->attachToCurrentValue (fineTuneValue);
    level->attachToCurrentValue (levelValue);
    pulseWidth->attachToCurrentValue (pulseWidthValue);
    detune->attachToCurrentValue (detuneValue);
    spread->attachToCurrentValue (spreadValue);
    pan->attachToCurrentValue (panValue);
}

void FourOscPlugin::OscParams::detach()
{
    tune->detachFromCurrentValue();
    fineTune->detachFromCurrentValue();
    level->detachFromCurrentValue();
    pulseWidth->detachFromCurrentValue();
    detune->detachFromCurrentValue();
    spread->detachFromCurrentValue();
    pan->detachFromCurrentValue();
}

//==============================================================================
FourOscPlugin::LFOParams::LFOParams (FourOscPlugin& plugin, int lfoNum)
{
    auto um = plugin.getUndoManager();

    auto lfoID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num));
    };

    waveShapeValue.referTo (plugin.state, lfoID (IDs::lfoWaveShape, lfoNum), um, lfoNum == 1 ? 1 : 0);
    syncValue.referTo (plugin.state, lfoID (IDs::lfoSync, lfoNum), um, false);
    rateValue.referTo (plugin.state, lfoID (IDs::lfoRate, lfoNum), um, 1);
    depthValue.referTo (plugin.state, lfoID (IDs::lfoDepth, lfoNum), um, 1.0f);
    beatValue.referTo (plugin.state, lfoID (IDs::lfoBeat, lfoNum), um, 1);

    auto paramID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num)).toString();
    };

    rate        = plugin.addParam (paramID (IDs::lfoRate, lfoNum),  TRANS("Rate") + " " + String (lfoNum), {0.0f, 500.0f, 0.0f, 0.3f}, "Hz");
    depth       = plugin.addParam (paramID (IDs::lfoDepth, lfoNum), TRANS("Depth") + " " + String (lfoNum), {0.0f, 1.0f});
}

void FourOscPlugin::LFOParams::attach()
{
    depth->attachToCurrentValue (depthValue);
    rate->attachToCurrentValue (rateValue);
}

void FourOscPlugin::LFOParams::detach()
{
    depth->detachFromCurrentValue();
    rate->detachFromCurrentValue();
}

//==============================================================================
FourOscPlugin::MODEnvParams::MODEnvParams (FourOscPlugin& plugin, int modNum)
{
    auto um = plugin.getUndoManager();

    auto modID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num));
    };

    modAttackValue.referTo (plugin.state, modID (IDs::modAttack, modNum), um, 0.1f);
    modDecayValue.referTo (plugin.state, modID (IDs::modDecay, modNum), um, 0.1f);
    modSustainValue.referTo (plugin.state, modID (IDs::modSustain, modNum), um, 80.0f);
    modReleaseValue.referTo (plugin.state, modID (IDs::modRelease, modNum), um, 0.1f);

    auto paramID = [] (Identifier i, int num)
    {
        return Identifier (i.toString() + String (num)).toString();
    };

    modAttack   = plugin.addParam (paramID (IDs::modAttack, modNum),  TRANS("Mod Attack") + " " + String (modNum),  {0.0f, 60.0f, 0.0f, 0.2f});
    modDecay    = plugin.addParam (paramID (IDs::modDecay, modNum),   TRANS("Mod Decay") + " " + String (modNum),   {0.0f, 60.0f, 0.0f, 0.2f});
    modSustain  = plugin.addParam (paramID (IDs::modSustain, modNum), TRANS("Mod Sustain") + " " + String (modNum), {0.0f,   100.0f}, "%");
    modRelease  = plugin.addParam (paramID (IDs::modRelease, modNum), TRANS("Mod Release") + " " + String (modNum), {0.001f, 60.0f, 0.0f, 0.2f});
}

void FourOscPlugin::MODEnvParams::attach()
{
    modAttack->attachToCurrentValue (modAttackValue);
    modDecay->attachToCurrentValue (modDecayValue);
    modSustain->attachToCurrentValue (modSustainValue);
    modRelease->attachToCurrentValue (modReleaseValue);
}

void FourOscPlugin::MODEnvParams::detach()
{
    modAttack->detachFromCurrentValue();
    modDecay->detachFromCurrentValue();
    modSustain->detachFromCurrentValue();
    modRelease->detachFromCurrentValue();
}

//==============================================================================
FourOscPlugin::FourOscPlugin (PluginCreationInfo info)  : Plugin (info)
{
    auto um = getUndoManager();

    levelMeasurer.addClient (*this);

    instrument->enableLegacyMode();
    setPitchbendTrackingMode (MPEInstrument::allNotesOnChannel);

    setVoiceStealingEnabled (true);

    delay = std::make_unique<FODelay>();
    chorus = std::make_unique<FOChorus>();

    for (int i = 0; i < 4; i++) oscParams.add (new OscParams (*this, i + 1));
    for (int i = 0; i < 2; i++) lfoParams.add (new LFOParams (*this, i + 1));
    for (int i = 0; i < 2; i++) modEnvParams.add (new MODEnvParams (*this, i + 1));

    // Amp
    ampAttackValue.referTo (state, IDs::ampAttack, um, 0.1f);
    ampDecayValue.referTo (state, IDs::ampDecay, um, 0.1f);
    ampSustainValue.referTo (state, IDs::ampSustain, um, 80.0f);
    ampReleaseValue.referTo (state, IDs::ampRelease, um, 0.1f);
    ampVelocityValue.referTo (state, IDs::ampVelocity, um, 100.0f);
    ampAnalogValue.referTo (state, IDs::ampAnalog, um, true);

    ampAttack   = addParam ("ampAttack",   TRANS("Amp Attack"),   {0.001f, 60.0f, 0.0f, 0.2f});
    ampDecay    = addParam ("ampDecay",    TRANS("Amp Decay"),    {0.001f, 60.0f, 0.0f, 0.2f});
    ampSustain  = addParam ("ampSustain",  TRANS("Amp Sustain"),  {0.0f,   100.0f}, "%");
    ampRelease  = addParam ("ampRelease",  TRANS("Amp Release"),  {0.001f, 60.0f, 0.0f, 0.2f});
    ampVelocity = addParam ("ampVelocity", TRANS("Amp Velocity"), {0.0f, 100.0f}, "%");

    ampAttack->attachToCurrentValue (ampAttackValue);
    ampDecay->attachToCurrentValue (ampDecayValue);
    ampSustain->attachToCurrentValue (ampSustainValue);
    ampRelease->attachToCurrentValue (ampReleaseValue);
    ampVelocity->attachToCurrentValue (ampVelocityValue);

    // Filter
    filterAttackValue.referTo (state, IDs::filterAttack, um, 0.1f);
    filterDecayValue.referTo (state, IDs::filterDecay, um, 0.1f);
    filterSustainValue.referTo (state, IDs::filterSustain, um, 80.0f);
    filterReleaseValue.referTo (state, IDs::filterRelease, um, 0.1f);
    filterFreqValue.referTo (state, IDs::filterFreq, um, 69.0f);
    filterResonanceValue.referTo (state, IDs::filterResonance, um, 0.5f);
    filterAmountValue.referTo (state, IDs::filterAmount, um, 0.0f);
    filterKeyValue.referTo (state, IDs::filterKey, um, 0.0f);
    filterVelocityValue.referTo (state, IDs::filterVelocity, um, 0.0f);
    filterTypeValue.referTo (state, IDs::filterType, um, 0);
    filterSlopeValue.referTo (state, IDs::filterSlope, um, 12);

    filterAttack    = addParam ("filterAttack",     TRANS("Filter Attack"),     {0.0f, 60.0f, 0.0f, 0.2f});
    filterDecay     = addParam ("filterDecay",      TRANS("Filter Decay"),      {0.0f, 60.0f, 0.0f, 0.2f});
    filterSustain   = addParam ("filterSustain",    TRANS("Filter Sustain"),    {0.0f, 100.0f}, "%");
    filterRelease   = addParam ("filterRelease",    TRANS("Filter Release"),    {0.0f, 60.0f, 0.0f, 0.2f});
    filterFreq      = addParam ("filterFreq",       TRANS("Filter Freq"),       {0.0f, 135.076232f});
    filterResonance = addParam ("filterResonance",  TRANS("Filter Resonance"),  {0.0f, 100.0f}, "%");
    filterAmount    = addParam ("filterAmount",     TRANS("Filter Amount"),     {-1.0f, 1.0f});
    filterKey       = addParam ("filterKey",        TRANS("Filter Key"),        {0.0f, 100.0f}, "%");
    filterVelocity  = addParam ("filterVelocity",   TRANS("Filter Velocity"),   {0.0f, 100.0f}, "%");

    filterAttack->attachToCurrentValue (filterAttackValue);
    filterDecay->attachToCurrentValue (filterDecayValue);
    filterSustain->attachToCurrentValue (filterSustainValue);
    filterRelease->attachToCurrentValue (filterReleaseValue);
    filterFreq->attachToCurrentValue (filterFreqValue);
    filterResonance->attachToCurrentValue (filterResonanceValue);
    filterAmount->attachToCurrentValue (filterAmountValue);
    filterKey->attachToCurrentValue (filterKeyValue);
    filterVelocity->attachToCurrentValue (filterVelocityValue);

    // Build the mod matrix before we add any global params
    for (auto p : getAutomatableParameters())
        modMatrix[p] = ModAssign();

    // Effects: Distortion
    distortionOnValue.referTo (state, IDs::distortionOn, um);
    distortionValue.referTo (state, IDs::distortion, um, 0.0f);

    distortion = addParam ("distortion", TRANS("Distortion"), {0.0f, 1.0f});

    distortion->attachToCurrentValue (distortionValue);

    // Effects: Reverb
    reverbOnValue.referTo (state, IDs::reverbOn, um);
    reverbSizeValue.referTo (state, IDs::reverbSize, um, 0.0f);
    reverbDampingValue.referTo (state, IDs::reverbDamping, um, 0.0f);
    reverbWidthValue.referTo (state, IDs::reverbWidth, um, 0.0f);
    reverbMixValue.referTo (state, IDs::reverbMix, um, 0.0);

    reverbSize      = addParam ("reverbSize",     TRANS("Size"),    {0.0f, 1.0f});
    reverbDamping   = addParam ("reverbDamping",  TRANS("Damping"), {0.0f, 1.0f});
    reverbWidth     = addParam ("reverbWidth",    TRANS("Width"),   {0.0f, 1.0f});
    reverbMix       = addParam ("reverbMix",      TRANS("Mix"),     {0.0f, 1.0f});

    reverbSize->attachToCurrentValue (reverbSizeValue);
    reverbDamping->attachToCurrentValue (reverbDampingValue);
    reverbWidth->attachToCurrentValue (reverbWidthValue);
    reverbMix->attachToCurrentValue (reverbMixValue);

    // Effects: Delay
    delayOnValue.referTo (state, IDs::delayOn, um);
    delayFeedbackValue.referTo (state, IDs::delayFeedback, um, -10.0f);
    delayCrossfeedValue.referTo (state, IDs::delayCrossfeed, um, -100.0f);
    delayMixValue.referTo (state, IDs::delayMix, um, 0.0f);
    delayValue.referTo (state, IDs::delay, um, 1.0f);

    delayFeedback   = addParam ("delayFeedback",    TRANS("Feedback"),  {-100.0f, 0.0f, 0.0f, 4.0f}, "dB");
    delayCrossfeed  = addParam ("delayCrossfeed",   TRANS("Crossfeed"), {-100.0f, 0.0f, 0.0f, 4.0f}, "dB");
    delayMix        = addParam ("delayMix",         TRANS("Mix"),       {0.0f, 1.0f});

    delayFeedback->attachToCurrentValue (delayFeedbackValue);
    delayCrossfeed->attachToCurrentValue (delayCrossfeedValue);
    delayMix->attachToCurrentValue (delayMixValue);

    // Effects: Chorus
    chorusOnValue.referTo (state, IDs::chorusOn, um);
    chorusSpeedValue.referTo (state, IDs::chosusSpeed, um, 1.0f);
    chorusDepthValue.referTo (state, IDs::chorusDepth, um, 3.0f);
    chorusWidthValue.referTo (state, IDs::chrousWidth, um, 0.5f);
    chorusMixValue.referTo (state, IDs::chorusMix, um, 0.0f);

    chorusSpeed    = addParam ("chorusSpeed",      TRANS("Speed"),       {0.1f, 10.0f}, "Hz");
    chorusDepth    = addParam ("chorusDepth",      TRANS("Depth"),       {0.1f, 20.0f}, "ms");
    chorusWidth    = addParam ("chorusWidth",      TRANS("Width"),       {0.0f, 1.0f});
    chorusMix      = addParam ("chorusMix",        TRANS("Mix"),         {0.0f, 1.0f});

    chorusSpeed->attachToCurrentValue (chorusSpeedValue);
    chorusDepth->attachToCurrentValue (chorusDepthValue);
    chorusWidth->attachToCurrentValue (chorusWidthValue);
    chorusMix->attachToCurrentValue (chorusMixValue);

    // Master
    voiceModeValue.referTo (state, IDs::voiceMode, um, 2);
    voicesValue.referTo (state, IDs::voices, um, 32);
    legatoValue.referTo (state, IDs::legato, um, 0);
    masterLevelValue.referTo (state, IDs::masterLevel, um, 0);

    legato          = addParam ("legato",           TRANS("Legato"),    {0.0f, 500.0f}, "ms");
    masterLevel     = addParam ("masterLevel",      TRANS("Level"),     {-100.0f, 0.0f, 0.0f, 4.0f});

    legato->attachToCurrentValue (legatoValue);
    masterLevel->attachToCurrentValue (masterLevelValue);

    // Oscillators
    for (auto o : oscParams)
        o->attach();

    // Mod
    for (auto l : lfoParams)
        l->attach();

    for (auto e : modEnvParams)
        e->attach();

    for (auto p : getAutomatableParameters())
        smoothers[p] = {};

    // Setup text functions
    setupTextFunctions();

    valueTreePropertyChanged (state, IDs::voiceMode);
    valueTreePropertyChanged (state, IDs::mpe);

    loadModMatrix();
}

FourOscPlugin::~FourOscPlugin()
{
    notifyListenersOfDeletion();

    // Oscillators
    for (auto o : oscParams)
        o->detach();

    // Mod
    for (auto l : lfoParams)
        l->detach();

    for (auto e : modEnvParams)
        e->detach();

    // Amp
    ampAttack->detachFromCurrentValue();
    ampDecay->detachFromCurrentValue();
    ampSustain->detachFromCurrentValue();
    ampRelease->detachFromCurrentValue();
    ampVelocity->detachFromCurrentValue();

    // Filter
    filterAttack->detachFromCurrentValue();
    filterDecay->detachFromCurrentValue();
    filterSustain->detachFromCurrentValue();
    filterRelease->detachFromCurrentValue();
    filterFreq->detachFromCurrentValue();
    filterResonance->detachFromCurrentValue();
    filterAmount->detachFromCurrentValue();
    filterKey->detachFromCurrentValue();
    filterVelocity->detachFromCurrentValue();

    // Effects: Distortion
    distortion->detachFromCurrentValue();

    // Effects: Reverb
    reverbSize->detachFromCurrentValue();
    reverbDamping->detachFromCurrentValue();
    reverbWidth->detachFromCurrentValue();
    reverbMix->detachFromCurrentValue();

    // Effects: Delay
    delayFeedback->detachFromCurrentValue();
    delayCrossfeed->detachFromCurrentValue();
    delayMix->detachFromCurrentValue();

    // Effects: Chorus
    chorusSpeed->detachFromCurrentValue();
    chorusDepth->detachFromCurrentValue();
    chorusWidth->detachFromCurrentValue();
    chorusMix->detachFromCurrentValue();

    // Voices
    legato->detachFromCurrentValue();
    masterLevel->detachFromCurrentValue();
}

const char* FourOscPlugin::xmlTypeName = "4osc";

AutomatableParameter* FourOscPlugin::addParam (const juce::String& paramID, const juce::String& name, juce::NormalisableRange<float> valueRange, juce::String label)
{
    auto p = Plugin::addParam (paramID, name, valueRange);

    if (label.isNotEmpty())
        labels[paramID] = label;

    return p;
}

void FourOscPlugin::setupTextFunctions()
{
    // Add a default function that does number of decimal places nicely and adds a labels
    for (auto p : getAutomatableParameters())
    {
        juce::String label;
        auto itr = labels.find (p->paramID);
        if (itr != labels.end())
            label = itr->second;

        auto basicValueToTextFunction = [label] (float value)
        {
            juce::String text;
            float v = std::abs (value);
            if (v > 100)
                text = juce::String (roundToInt (value));
            else if (v > 10)
                text = juce::String (value, 1);
            else if (v > 1)
                text = juce::String (value, 2);
            else
                text = juce::String (value, 3);

            if (label.isNotEmpty())
                text += label;

            return text;
        };

        p->valueToStringFunction = basicValueToTextFunction;
    }

    auto timeValueToTextFunction = [] (float value)
    {
        if (value < 1.0f)
            return juce::String (roundToInt (value * 1000)) + "ms";
        return juce::String (value, 2) + "s";
    };

    auto panValueToTextFunction = [] (float value)
    {
        if (value < 0.0f)
            return juce::String (roundToInt (-value * 100)) + "L";
        return juce::String (roundToInt (value * 100)) + "R";
    };

    auto percentValueToTextFunction = [] (float value)
    {
        return juce::String (roundToInt (value * 100)) + "%";
    };

    auto tuneValueToTextFunction = [] (float value)
    {
        return juce::String (roundToInt (value)) + "st";
    };

    auto freqValueToTextFunction = [] (float value)
    {
        float freq = 440.0f * std::pow (2.0f, (value - 69) / 12.0f);
        return String (roundToInt (freq)) + "Hz";
    };

    auto textToFreqValueFunction = [] (juce::String text)
    {
        float freq = text.getFloatValue();
        return 12.0f * std::log2 (freq / 440.0f) + 69.0f;
    };

    auto textToTimeValueFunction = [] (juce::String text)
    {
        float time = text.getFloatValue();
        return (text.contains ("ms") || time > 10.0f) ? (time / 1000.0f) : time;
    };

    for (auto p : oscParams)
    {
        p->pulseWidth->valueToStringFunction = percentValueToTextFunction;
        p->tune->valueToStringFunction = tuneValueToTextFunction;
        p->detune->valueToStringFunction = percentValueToTextFunction;
        p->pan->valueToStringFunction = panValueToTextFunction;
    }

    for (auto p : lfoParams)
    {
        p->depth->valueToStringFunction = percentValueToTextFunction;
    }

    for (auto p : modEnvParams)
    {
        p->modAttack->valueToStringFunction = timeValueToTextFunction;
        p->modAttack->stringToValueFunction = textToTimeValueFunction;
        p->modDecay->valueToStringFunction = timeValueToTextFunction;
        p->modDecay->stringToValueFunction = textToTimeValueFunction;
        p->modRelease->valueToStringFunction = timeValueToTextFunction;
        p->modRelease->stringToValueFunction = textToTimeValueFunction;
    }

    ampAttack->valueToStringFunction = timeValueToTextFunction;
    ampAttack->stringToValueFunction = textToTimeValueFunction;
    ampDecay->valueToStringFunction = timeValueToTextFunction;
    ampDecay->stringToValueFunction = textToTimeValueFunction;
    ampRelease->valueToStringFunction = timeValueToTextFunction;
    ampRelease->stringToValueFunction = textToTimeValueFunction;

    filterAttack->valueToStringFunction = timeValueToTextFunction;
    filterAttack->stringToValueFunction = textToTimeValueFunction;
    filterDecay->valueToStringFunction = timeValueToTextFunction;
    filterDecay->stringToValueFunction = textToTimeValueFunction;
    filterRelease->valueToStringFunction = timeValueToTextFunction;
    filterRelease->stringToValueFunction = textToTimeValueFunction;
    filterFreq->valueToStringFunction = freqValueToTextFunction;
    filterFreq->stringToValueFunction = textToFreqValueFunction;
    filterAmount->valueToStringFunction = percentValueToTextFunction;

    distortion->valueToStringFunction = percentValueToTextFunction;

    delayMix->valueToStringFunction = percentValueToTextFunction;

    chorusWidth->valueToStringFunction = percentValueToTextFunction;
    chorusMix->valueToStringFunction = percentValueToTextFunction;

    reverbSize->valueToStringFunction = percentValueToTextFunction;
    reverbWidth->valueToStringFunction = percentValueToTextFunction;
    reverbDamping->valueToStringFunction = percentValueToTextFunction;
    reverbMix->valueToStringFunction = percentValueToTextFunction;
}

float FourOscPlugin::getLevel (int channel)
{
    auto& peak = levels[channel];

    const int elapsedMilliseconds = jmax (0, int (Time::getApproximateMillisecondCounter() - peak.time) - 50);
    float currentLevel = peak.dB - (48.0f * elapsedMilliseconds / 1000.0f);

    auto latest = getAndClearAudioLevel (channel);
    if (latest.dB > currentLevel)
    {
        peak = latest;
        return jlimit (-100.0f, 0.0f, peak.dB);
    }
    return jlimit (-100.0f, 0.0f, currentLevel);
}

void FourOscPlugin::valueTreeChanged()
{
    Plugin::valueTreeChanged();
}

void FourOscPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    Plugin::valueTreePropertyChanged (v, i);

    if (v.hasType (IDs::PLUGIN))
    {
        if (i == IDs::voiceMode
            || i == IDs::voices)
        {
            juce::ScopedLock sl (voicesLock);
            if (voiceModeValue == 2)
            {
                reduceNumVoices (voicesValue.get());
                while (getNumVoices() < voicesValue.get())
                    addVoice (new FourOscVoice (*this));
            }
            else
            {
                while (getNumVoices() < 1)
                    addVoice (new FourOscVoice (*this));

                reduceNumVoices (1);
            }
        }
        else if (i == IDs::mpe)
        {
            if ((bool) state[IDs::mpe])
            {
                MPEZoneLayout zones;
                zones.setLowerZone (15);
                instrument->setZoneLayout (zones);
                setPitchbendTrackingMode (MPEInstrument::lastNotePlayedOnChannel);
            }
            else
            {
                instrument->enableLegacyMode();
                setPitchbendTrackingMode (MPEInstrument::allNotesOnChannel);
            }
        }
    }
    else if (v.hasType (IDs::MODMATRIX) || v.hasType (IDs::MODMATRIXITEM))
    {
        if (! flushingState)
            triggerAsyncUpdate();
    }
}

void FourOscPlugin::valueTreeChildAdded (juce::ValueTree& v, juce::ValueTree& c)
{
    Plugin::valueTreeChildAdded (v, c);

    if (c.hasType (IDs::MODMATRIX) || c.hasType (IDs::MODMATRIXITEM))
        if (! flushingState)
            triggerAsyncUpdate();
}

void FourOscPlugin::valueTreeChildRemoved (juce::ValueTree& v, juce::ValueTree& c, int i)
{
    Plugin::valueTreeChildRemoved (v, c, i);

    if (c.hasType (IDs::MODMATRIX) || c.hasType (IDs::MODMATRIXITEM))
        if (! flushingState)
            triggerAsyncUpdate();
}

void FourOscPlugin::handleController (int, int controllerNumber, int controllerValue)
{
    controllerValues[controllerNumber] = controllerValue / 127.0f;
}

void FourOscPlugin::handleAsyncUpdate()
{
    loadModMatrix();
}

void FourOscPlugin::loadModMatrix()
{
    // Disable all modulation
    for (auto& itr : modMatrix)
        for (int s = lfo1; s < numModSources; s++)
            itr.second.depths[s] = -1000.0f;

    // Read modulation from state ValueTree
    auto mm = state.getChildWithName (IDs::MODMATRIX);
    if (! mm.isValid())
        return;

    for (auto mmi : mm)
    {
        auto paramId = mmi.getProperty (IDs::modParam).toString();
        auto src     = idToModulationSource (mmi.getProperty (IDs::modItem).toString());
        float depth  = (float) mmi.getProperty (IDs::modDepth);

        if (src != none)
        {
            if (auto p = getAutomatableParameterByID (paramId))
            {
                auto itr = modMatrix.find (p.get());
                if (itr != modMatrix.end())
                    itr->second.depths[src] = depth;
                else
                    jassertfalse;
            }
        }
    }

    // Update cached lookup info
    for (auto& itr : modMatrix)
        itr.second.updateCachedInfo();
}

void FourOscPlugin::flushPluginStateToValueTree()
{
    ScopedValueSetter<bool> svs (flushingState, true);

    auto um = getUndoManager();

    auto vt = state.getChildWithName (IDs::MODMATRIX);
    if (vt.isValid())
        state.removeChild (vt, um);

    auto mm = ValueTree (IDs::MODMATRIX);

    for (const auto& itr : modMatrix)
    {
        for (int s = lfo1; s < numModSources; s++)
        {
            if (itr.second.depths[s] >= -1.0f)
            {
                auto mmi = ValueTree (IDs::MODMATRIXITEM);
                mmi.setProperty (IDs::modParam, itr.first->paramID, um);
                mmi.setProperty (IDs::modItem, modulationSourceToID ((ModSource)s), um);
                mmi.setProperty (IDs::modDepth, itr.second.depths[s], um);

                mm.addChild (mmi, -1, um);
            }
        }
    }

    state.addChild (mm, -1, um);
    
    Plugin::flushPluginStateToValueTree(); // Add any parameter values that are being modified
}

void FourOscPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    setCurrentPlaybackSampleRate (info.sampleRate);

    reverb.setSampleRate (info.sampleRate);
    delay->setSampleRate (info.sampleRate);
    chorus->setSampleRate (info.sampleRate);

    reverb.reset();
    delay->reset();
    chorus->reset();

    for (auto& itr : smoothers)
        itr.second.reset (info.sampleRate, 0.01f);
}

void FourOscPlugin::deinitialise()
{
}

//==============================================================================
void FourOscPlugin::reset()
{
    turnOffAllVoices (false);
}

void FourOscPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    juce::ScopedLock sl (voicesLock);

    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        // find the tempo
        double now = fc.editTime;
        currentPos.setTime (now);
        currentTempo = float (currentPos.getCurrentTempo().bpm);

        // Handle all notes off first
        if (fc.bufferForMidiMessages != nullptr)
            if (fc.bufferForMidiMessages->isAllNotesOff)
                turnOffAllVoices (true);

        // Chop the buffer in 32 sample blocks so modulation is smooth
        int todo = fc.bufferNumSamples;
        int pos  = fc.bufferStartSample;

        while (todo > 0)
        {
            int thisBlock = jmin (32, todo);

            AudioScratchBuffer workBuffer (2, thisBlock);
            workBuffer.buffer.clear();

            juce::MidiBuffer midi;
            if (fc.bufferForMidiMessages != nullptr)
            {
                for (auto m : *fc.bufferForMidiMessages)
                {
                    int midiPos = int (m.getTimeStamp());
                    if (midiPos >= pos && midiPos < pos + thisBlock)
                        midi.addEvent (m, midiPos - pos);
                }
            }

            applyToBuffer (workBuffer.buffer, midi);

            if (fc.destBuffer->getNumChannels() == 1)
            {
                fc.destBuffer->copyFrom (0, pos, workBuffer.buffer, 0, 0, thisBlock);
            }
            else
            {
                fc.destBuffer->copyFrom (0, pos, workBuffer.buffer, 0, 0, thisBlock);
                fc.destBuffer->copyFrom (1, pos, workBuffer.buffer, 1, 0, thisBlock);
            }

            levelMeasurer.processBuffer (workBuffer.buffer, 0, thisBlock);

            todo -= thisBlock;
            pos += thisBlock;
        }

        for (int ch = 2; ch < fc.destBuffer->getNumChannels(); ch++)
            fc.destBuffer->clear (ch, fc.bufferStartSample, fc.bufferNumSamples);
    }
}

void FourOscPlugin::applyToBuffer (AudioSampleBuffer& buffer, MidiBuffer& midi)
{
    updateParams (buffer);
    renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    applyEffects (buffer);

    for (auto& itr : smoothers)
        itr.second.process (buffer.getNumSamples());
}

void FourOscPlugin::applyEffects (AudioSampleBuffer& buffer)
{
    int numSamples = buffer.getNumSamples();

    // Apply Distortion
    if (distortionOnValue)
    {
        float drive = paramValue (distortion);
        float clip = 1.0f / (2.0f * drive);
        Distortion::distortion (buffer.getWritePointer (0), numSamples, drive, -clip, clip);
        Distortion::distortion (buffer.getWritePointer (1), numSamples, drive, -clip, clip);
    }

    // Apply Chorus
    if (chorusOnValue)
        chorus->process (buffer, numSamples);

    // Apply Delay
    if (delayOnValue)
        delay->process (buffer, numSamples);

    // Apply Reverb
    if (reverbOnValue)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    // Apply master level
    buffer.applyGain (Decibels::decibelsToGain (paramValue (masterLevel)));
}

void FourOscPlugin::updateParams (AudioSampleBuffer& buffer)
{
    ignoreUnused (buffer);

    // Reverb
    AudioFadeCurve::CrossfadeLevels wetDry (paramValue (reverbMix));

    Reverb::Parameters params;
    params.roomSize = paramValue (reverbSize);
    params.damping = paramValue (reverbDamping);
    params.width = paramValue (reverbWidth);
    params.wetLevel = wetDry.gain1;
    params.dryLevel = wetDry.gain2;
    params.freezeMode = 0;

    reverb.setParameters (params);

    // Delay
    float delayTime = (delayValue.get()) / (currentTempo / 60.0f);
    delay->setParams (delayTime,
                      Decibels::decibelsToGain (paramValue (delayFeedback)),
                      Decibels::decibelsToGain (paramValue (delayCrossfeed)),
                      paramValue (delayMix));

    // Chorus
    chorus->setParams (paramValue (chorusSpeed),
                       paramValue (chorusDepth),
                       paramValue (chorusWidth),
                       paramValue (chorusMix));
}

//==============================================================================
void FourOscPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[]  = { &ampAttackValue, &ampDecayValue, &ampSustainValue, &ampReleaseValue, &ampVelocityValue,
        &filterAttackValue, &filterDecayValue, &filterSustainValue, &filterReleaseValue, &filterFreqValue, &filterResonanceValue,
        &filterAmountValue, &filterKeyValue, &filterVelocityValue, &distortionValue, &reverbSizeValue, &reverbDampingValue, &reverbWidthValue,
        &reverbMixValue, &delayValue, &delayFeedbackValue, &delayCrossfeedValue, &delayMixValue,
        &chorusSpeedValue, &chorusDepthValue, &chorusWidthValue, &chorusMixValue, &legatoValue,
        &masterLevelValue, nullptr };

    juce::CachedValue<int>* cvsInt[] { &voiceModeValue, &voicesValue, &filterTypeValue, &filterSlopeValue, nullptr };
    juce::CachedValue<bool>* cvsBool[] { &ampAnalogValue, &distortionOnValue, &reverbOnValue, &delayOnValue, &chorusOnValue, nullptr };

    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);

    auto um = getUndoManager();

    for (auto p : oscParams)
        p->restorePluginStateFromValueTree (v);

    for (auto p : lfoParams)
        p->restorePluginStateFromValueTree (v);

    for (auto p : modEnvParams)
        p->restorePluginStateFromValueTree (v);

    auto mm = state.getChildWithName (IDs::MODMATRIX);
    if (mm.isValid())
        state.removeChild (mm, um);

    mm = v.getChildWithName (IDs::MODMATRIX);
    if (mm.isValid())
        state.addChild (mm.createCopy(), -1, um);

    valueTreePropertyChanged (state, IDs::voiceMode);
}

juce::String FourOscPlugin::modulationSourceToName (ModSource src)
{
    switch (src)
    {
        case lfo1:          return TRANS("LFO 1");
        case lfo2:          return TRANS("LFO 2");
        case env1:          return TRANS("Envelope 1");
        case env2:          return TRANS("Envelope 2");
        case mpePressure:   return TRANS("MPE Pressure");
        case mpeTimbre:     return TRANS("MPE Timbre");
        case midiNoteNum:   return TRANS("MIDI Note Number");
        case midiVelocity:  return TRANS("MIDI Velocity");
        case none:
        case ccBankSelect:
        case ccPolyMode:
        case numModSources:
        default:
        {
            if (src >= ccBankSelect && src <= ccPolyMode)
            {
                auto prefix = String ("CC#") + String ((int)(src - ccBankSelect));
                auto name = String (juce::MidiMessage::getControllerName (src - ccBankSelect));
                if (name.isEmpty())
                    return prefix;
                return prefix + " " + name;
            }

            jassertfalse;
            return {};
        }
    }
}

juce::String FourOscPlugin::modulationSourceToID (FourOscPlugin::ModSource src)
{
    switch (src)
    {
        case lfo1:          return "lfo1";
        case lfo2:          return "lfo2";
        case env1:          return "env1";
        case env2:          return "env2";
        case mpePressure:   return "mpePressure";
        case mpeTimbre:     return "mpeTimbre";
        case midiNoteNum:   return "midiNote";
        case midiVelocity:  return "midiVelocity";
        case none:
        case ccBankSelect:
        case ccPolyMode:
        case numModSources:
        default:
        {
            if (src >= ccBankSelect && src <= ccPolyMode)
                return "cc" + juce::String (int (src - ccBankSelect));

            jassertfalse;
            return {};
        }
    }
}

FourOscPlugin::ModSource FourOscPlugin::idToModulationSource (juce::String idStr)
{
    if (idStr == "lfo1")            return lfo1;
    if (idStr == "lfo2")            return lfo2;
    if (idStr == "env1")            return env1;
    if (idStr == "env2")            return env2;
    if (idStr == "mpePressure")     return mpePressure;
    if (idStr == "mpeTimbre")       return mpeTimbre;
    if (idStr == "midiNote")        return midiNoteNum;
    if (idStr == "midiVelocity")    return midiVelocity;

    if (idStr.startsWith ("cc"))
        return ModSource (ccBankSelect + idStr.getTrailingIntValue());

    return none;
}

juce::Array<float> FourOscPlugin::getLiveModulationPositions (AutomatableParameter::Ptr param)
{
    juce::Array<float> positions;

    // Filter frequency is a special case, not only do we want to show modulation,
    // also want to show effect of key tracking and filter envelope
    if (param->paramID == "filterFreq" && isModulated (param))
    {
        for (int i = 0; i < getNumVoices(); i++)
            if (auto fov = dynamic_cast<FourOscVoice*> (getVoice (i)))
                fov->getLiveFilterFrequency (positions);
    }
    else if (isModulated (param))
    {
        for (int i = 0; i < getNumVoices(); i++)
            if (auto fov = dynamic_cast<FourOscVoice*> (getVoice (i)))
                fov->getLiveModulationPositions (param, positions);
    }
    return positions;
}

bool FourOscPlugin::isModulated (AutomatableParameter::Ptr param)
{
    if (param->paramID == "filterFreq" && (filterKeyValue.get() != 0 || filterAmountValue.get() != 0 ))
        return true;

    auto itr = modMatrix.find (param.get());
    if (itr != modMatrix.end())
    {
        for (auto d : itr->second.depths)
            if (d >= -1.0f)
                return true;

        return false;
    }
    jassertfalse;
    return false;
}

juce::Array<FourOscPlugin::ModSource> FourOscPlugin::getModulationSources (AutomatableParameter::Ptr param)
{
    auto itr = modMatrix.find (param.get());
    if (itr != modMatrix.end())
    {
        juce::Array<ModSource> res;
        for (int s = lfo1; s < numModSources; s++)
            if (itr->second.depths[s] >= -1.0f)
                res.add ((ModSource) s);

        return res;
    }
    jassertfalse;
    return {};
}

float FourOscPlugin::getModulationDepth (FourOscPlugin::ModSource src, AutomatableParameter::Ptr param)
{
    auto itr = modMatrix.find (param.get());
    if (itr != modMatrix.end())
        return itr->second.depths[src];
    jassertfalse;
    return -1000;
}

void FourOscPlugin::setModulationDepth (FourOscPlugin::ModSource src, AutomatableParameter::Ptr param, float depth)
{
    auto itr = modMatrix.find (param.get());
    if (itr != modMatrix.end())
    {
        itr->second.depths[src] = depth;
        itr->second.updateCachedInfo();
        return;
    }
    jassertfalse;
}

void FourOscPlugin::clearModulation (ModSource src, AutomatableParameter::Ptr param)
{
    auto itr = modMatrix.find (param.get());
    if (itr != modMatrix.end())
    {
        itr->second.depths[src] = -1000.0f;
        itr->second.updateCachedInfo();
        return;
    }
    jassertfalse;
}

float FourOscPlugin::paramValue (AutomatableParameter::Ptr param)
{
    jassert (param != nullptr);
    if (param == nullptr)
        return 0.0f;

    auto smoothItr = smoothers.find (param.get());
    if (smoothItr == smoothers.end())
        return param->getCurrentValue();

    float val = param->getCurrentNormalisedValue();
    smoothItr->second.setValue (val);
    return param->valueRange.convertFrom0to1 (smoothItr->second.getCurrentValue());
}

}
