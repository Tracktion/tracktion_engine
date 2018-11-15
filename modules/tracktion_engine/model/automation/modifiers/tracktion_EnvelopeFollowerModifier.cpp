/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


//==============================================================================
/**
    Calculates the RMS of a continuous signal.
    N.B. this isn't a mathematically correct RMS but a reasonable estimate.
*/
class RunningRMS
{
public:
    /** Creates an empty RunningRMS. */
    RunningRMS() = default;

    /** Sets the sample rate to use for detection. */
    inline void setSampleRate (float newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        update();
    }

    /** Returns the current RMS for a new input sample. */
    inline float processSingleSample (float in) noexcept
    {
        state += (square (in) - state) * coefficient;
        return std::sqrt (state);
    }

private:
    float state = 0.0f;
    float coefficient = 1.0f;
    float sampleRate = 44100.0f;

    void update() noexcept
    {
        coefficient = 1.0f - std::exp (-2.0f * float_Pi * 100.0f / sampleRate);
        state = 0.0f;
    }
};

//==============================================================================
/**
    Envelope follower with adjustable attack/release parameters as well as several
    detection and time constant modes.
*/
class EnvelopeFollowerModifier::EnvelopeFollower
{
public:
    //==============================================================================
    /** The available detection modes. */
    enum DetectionMode
    {
        peakMode = 0,
        msMode,
        rmsMode
    };

    /** The available time constant modes modes. */
    enum TimeConstantMode
    {
        digitalTC = 0,
        slowDigitalTC,
        analogTC
    };

    //==============================================================================
    /** Creates a default EnvelopeFollower. */
    EnvelopeFollower() = default;

    /** Sets the sample rate to use.
        This should be called before any of the parameter methods.
    */
    void setSampleRate (float newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        rms.setSampleRate (newSampleRate);

        attackCoeff     = getDelta (attackTimeMs);
        releaseCoeff    = getDelta (releaseTimeMs);
        holdSamples     = (int) msToSamples (holdTimeMs, sampleRate);

        reset();
    }

    /** Resets the detection and current envelope. */
    void reset() noexcept
    {
        envelope = 0.0f;
    }

    //==============================================================================
    /** Sets the attack time. */
    void setAttackTime (float attackMs)
    {
        attackTimeMs = attackMs;
        attackCoeff = getDelta (attackTimeMs);
    }

    /** Sets the hold time. */
    void setHoldTime (float holdMs)
    {
        holdTimeMs = holdMs;
        holdSamples = (int) msToSamples (holdMs, sampleRate);
    }

    /** Sets the release time. */
    void setReleaseTime (float releaseMs)
    {
        releaseTimeMs = releaseMs;
        releaseCoeff = getDelta (releaseTimeMs);
    }

    /** Sets the detection mode to use. */
    void setDetectMode (DetectionMode newDetectionMode)
    {
        detectMode = newDetectionMode;
    }

    /** Sets the time constant to be used. */
    void setTimeConstantMode (TimeConstantMode newTC)
    {
        if (! setIfDifferent (timeConstantMode, newTC))
            return;

        switch (timeConstantMode)
        {
            case digitalTC:     timeConstant = -2.0f; break; // log (1%)
            case slowDigitalTC: timeConstant = -1.0f; break; // log (10%)
            case analogTC:      timeConstant = -0.43533393574791066201247090699309f; break; // log (36.7%)
        }

        attackCoeff   = getDelta (attackTimeMs);
        releaseCoeff  = getDelta (releaseTimeMs);
    }

    //==============================================================================
    /** Returns the envelope for a new sample. */
    float processSingleSample (float input)
    {
        switch (detectMode)
        {
            case peakMode:
            {
                input = std::abs (input);
                break;
            }
            case msMode:
            {
                input = square (std::abs (input));
                break;
            }
            case rmsMode:
            {
                input = rms.processSingleSample (input);
                break;
            }
        }

        if (input > envelope)
        {
            envelope = attackCoeff * (envelope - input) + input;
            holdSamplesLeft = holdSamples;
        }
        else if (holdSamplesLeft > 0)
        {
            --holdSamplesLeft;
        }
        else
        {
            envelope = releaseCoeff * (envelope - input) + input;
        }

        jassert (envelope == 0.0f || std::isnormal (envelope));
        envelope = jlimit (0.0f, 1.0f, envelope);

        return envelope;
    }

protected:
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
    float attackTimeMs = 0.0f;
    float holdTimeMs = 0.0f;
    float releaseTimeMs = 0.0f;
    float sampleRate = 44100.0f;
    float timeConstant = -2.0f;
    int holdSamples = 0, holdSamplesLeft = 0;
    DetectionMode detectMode = peakMode;
    TimeConstantMode timeConstantMode = digitalTC;
    RunningRMS rms;

    inline float getDelta (float timeMs)
    {
        return std::exp (timeConstant / (timeMs * sampleRate * 0.001f));
    }

    inline double msToSamples (double numMiliseconds, double sr)
    {
        return numMiliseconds * 0.001 * sr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeFollower)
};

//==============================================================================
struct EnvelopeFollowerModifier::EnvelopeFollowerModifierAudioNode  : public SingleInputAudioNode
{
    EnvelopeFollowerModifierAudioNode (AudioNode* input, EnvelopeFollowerModifier& efm)
        : SingleInputAudioNode (input),
          envelopeFollowerModifier (&efm)
    {
    }

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override
    {
        return keepAudio || SingleInputAudioNode::purgeSubNodes (keepAudio, keepMidi);
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        SingleInputAudioNode::prepareAudioNodeToPlay (info);
        envelopeFollowerModifier->baseClassInitialise (info);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        SingleInputAudioNode::renderOver (rc);
        envelopeFollowerModifier->applyToBuffer (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    EnvelopeFollowerModifier::Ptr envelopeFollowerModifier;
};

//==============================================================================
EnvelopeFollowerModifier::EnvelopeFollowerModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    gainDb.referTo (state, IDs::gainDb, um, 0.0f);
    attack.referTo (state, IDs::attack, um, 100.0f);
    hold.referTo (state, IDs::hold, um, 0.0f);
    release.referTo (state, IDs::release, um, 500.0f);
    depth.referTo (state, IDs::depth, um, 1.0f);
    offset.referTo (state, IDs::offset, um, 0.0f);
    lowPassEnabled.referTo (state, IDs::lowPassEnabled, um, false);
    highPassEnabled.referTo (state, IDs::highPassEnabled, um, false);
    lowPassFrequency.referTo (state, IDs::lowPassFrequency, um, 2000.0f);
    highPassFrequency.referTo (state, IDs::highPassFrequency, um, 500.0f);

    auto addDiscreteParam = [this] (const String& paramID, const String& name,
                                    Range<float> valueRange, CachedValue<float>& val, const StringArray& labels) -> AutomatableParameter*
    {
        auto* p = new DiscreteLabelledParameter (paramID, name, *this, valueRange, labels.size(), labels);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    auto addParam = [this] (const String& paramID, const String& name, NormalisableRange<float> valueRange, float centreVal, CachedValue<float>& val, const String& suffix) -> AutomatableParameter*
    {
        valueRange.setSkewForCentre (centreVal);
        auto* p = new SuffixedParameter (paramID, name, *this, valueRange, suffix);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    const NormalisableRange<float> freqRange (20.0f, 20000.0f);
    gainDbParam             = addParam          ("gainDb",              TRANS("Gain"),                  { -20.0f, 20.0f, 0.001f }, 0.0f,    gainDb,             "dB");
    attackParam             = addParam          ("attack",              TRANS("Attack"),                { 1.0f, 5000.0f }, 50.0f,           attack,             "ms");
    holdParam               = addParam          ("hold",                TRANS("Hold"),                  { 0.0f, 5000.0f }, 50.0f,           hold,               "ms");
    releaseParam            = addParam          ("release",             TRANS("Release"),               { 1.0f, 5000.0f }, 50.0f,           release,            "ms");
    depthParam              = addParam          ("depth",               TRANS("Depth"),                 { -1.0f, 1.0f }, 0.0f,              depth,              {});
    offsetParam             = addParam          ("offset",              TRANS("Offset"),                { 0.0f, 1.0f }, 0.5f,               offset,             {});
    lowPassEnabledParam     = addDiscreteParam  ("lowPassEnabled",      TRANS("Low-pass Enabled"),      { 0.0f, 1.0f },                     lowPassEnabled,     getEnabledNames());
    highPassEnabledParam    = addDiscreteParam  ("highPassEnabled",     TRANS("High-pass Enabled"),     { 0.0f, 1.0f },                     highPassEnabled,    getEnabledNames());
    lowPassFrequencyParam   = addParam          ("lowPassFrequency",    TRANS("Low-pass Frequency"),    freqRange, 700.0f,                  lowPassFrequency,   "Hz");
    highPassFrequencyParam  = addParam          ("highPassFrequency",   TRANS("High-pass Frequency"),   freqRange, 700.0f,                  highPassFrequency,  "Hz");

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);

    envelopeFollower = std::make_unique<EnvelopeFollower>();
}

EnvelopeFollowerModifier::~EnvelopeFollowerModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

//==============================================================================
float EnvelopeFollowerModifier::getCurrentValue()
{
    return (getEnvelopeValue() * depthParam->getCurrentValue()) + offsetParam->getCurrentValue();
}

AutomatableParameter::ModifierAssignment* EnvelopeFollowerModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

StringArray EnvelopeFollowerModifier::getAudioInputNames()
{
    return { TRANS("Audio input left"), TRANS("Audio input right") };
}

AudioNode* EnvelopeFollowerModifier::createPostFXAudioNode (AudioNode* an)
{
    return new EnvelopeFollowerModifierAudioNode (an, *this);
}

void EnvelopeFollowerModifier::initialise (const PlaybackInitialisationInfo& info)
{
    prepareToPlay (info.sampleRate);
}

void EnvelopeFollowerModifier::deinitialise()
{
    reset();
}

void EnvelopeFollowerModifier::applyToBuffer (const AudioRenderContext& rc)
{
    if (rc.destBuffer == nullptr)
        return;

    updateParameterStreams (rc.getEditTime().editRange1.getStart());

    if (rc.didPlayheadJump())
        reset();

    AudioBuffer<float> ab (rc.destBuffer->getArrayOfWritePointers(),
                           rc.destBuffer->getNumChannels(),
                           rc.bufferStartSample,
                           rc.bufferNumSamples);
    processBlock (ab);
}

//==============================================================================
EnvelopeFollowerModifier::Assignment::Assignment (const juce::ValueTree& v, const EnvelopeFollowerModifier& evm)
    : AutomatableParameter::ModifierAssignment (evm.edit, v),
      envelopeFollowerModifierID (evm.itemID)
{
}

bool EnvelopeFollowerModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const EnvelopeFollowerModifier*> (&source))
        return mod->itemID == envelopeFollowerModifierID;

    return false;
}

EnvelopeFollowerModifier::Ptr EnvelopeFollowerModifier::Assignment::getEnvelopeFollowerModifier() const
{
    if (auto mod = findModifierTypeForID<EnvelopeFollowerModifier> (edit, envelopeFollowerModifierID))
        return mod;

    return {};
}

//==============================================================================
void EnvelopeFollowerModifier::prepareToPlay (double sampleRate)
{
    envelopeFollower->setSampleRate ((float) sampleRate);
    currentSampleRate = sampleRate;
}

void EnvelopeFollowerModifier::processBlock (const juce::AudioBuffer<float>& ab)
{
    envelopeFollower->setAttackTime (attackParam->getCurrentValue());
    envelopeFollower->setHoldTime (holdParam->getCurrentValue());
    envelopeFollower->setReleaseTime (releaseParam->getCurrentValue());
    const float gainVal = Decibels::decibelsToGain (gainDbParam->getCurrentValue());
    const bool lowPass = getBoolParamValue (*lowPassEnabledParam);
    const bool highPass = getBoolParamValue (*highPassEnabledParam);

    const int numChannels = ab.getNumChannels();
    const int numSamples = ab.getNumSamples();
    const float** samples = ab.getArrayOfReadPointers();
    float envelope = 0.0f;

    AudioScratchBuffer scratch (1, numSamples);
    float* scratchData = scratch.buffer.getWritePointer (0);

    // Find max of all channels
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;

        for (int c = 0; c < numChannels; ++c)
            sample = jmax (sample, std::abs (samples[c][i]));

        scratchData[i] = sample * gainVal;
    }

    // Filter if required
    if (setIfDifferent (currentLowPassFrequency, lowPassFrequencyParam->getCurrentValue()))
        lowPassFilter.setCoefficients (IIRCoefficients::makeLowPass (currentSampleRate, currentLowPassFrequency));

    if (setIfDifferent (currentHighPassFrequency, highPassFrequencyParam->getCurrentValue()))
        highPassFilter.setCoefficients (IIRCoefficients::makeHighPass (currentSampleRate, currentHighPassFrequency));

    if (lowPass)
        lowPassFilter.processSamples (scratchData, numSamples);

    if (highPass)
        highPassFilter.processSamples (scratchData, numSamples);

    // Process envelope
    for (int i = 0; i < numSamples; ++i)
        envelope = envelopeFollower->processSingleSample (scratchData[i]);

    envelopeValue.store (envelope, std::memory_order_release);
}

void EnvelopeFollowerModifier::reset()
{
    envelopeFollower->reset();
}

//==============================================================================
void EnvelopeFollowerModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}
