/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

//==============================================================================
// An ADSR Envelope with exponential curves with the same API as the juce::ADSR
class ExpEnvelope
{
public:
    ExpEnvelope();

    //==============================================================================
    /** Holds the parameters being used by an ADSR object. */
    struct Parameters
    {
        /** Attack time in seconds. */
        float attack  = 0.1f;

        /** Decay time in seconds. */
        float decay   = 0.1f;

        /** Sustain level. */
        float sustain = 1.0f;

        /** Release time in seconds. */
        float release = 0.1f;
    };

    /** Sets the parameters that will be used by an ADSR object.

     You must have called setSampleRate() with the correct sample rate before
     this otherwise the values may be incorrect!

     @see getParameters
     */
    void setParameters (const Parameters& newParameters)
    {
        setAttackTime (newParameters.attack);
        setDecayTime (newParameters.decay);
        setSustainLevel (newParameters.sustain);
        setReleaseTime (newParameters.release);
    }

    inline void setSampleRate (double d)    { sampleRate = d; }

    //==============================================================================
    /** Resets the envelope to an idle state. */
    inline void reset()
    {
        currentState = State::idle;
        calculateReleaseTime();
        envelopeVal = 0.0;
    }

    /** Starts the attack phase of the envelope. */
    inline void noteOn()                    { currentState = State::attack; }

    /** Starts the release phase of the envelope. */
    inline void noteOff()
    {
        if (envelopeVal > 0)
            currentState = State::release;
        else
            currentState = State::idle;
    }

    /** Returns true if the envelope is in its attack, decay, sustain or release stage. */
    inline bool isActive() const            { return currentState != State::idle; }

    //==============================================================================
    /** Returns the next sample value for an ADSR object.

        @see applyEnvelopeToBuffer
     */
    inline float getNextSample()
    {
        switch (currentState)
        {
            case State::idle:
            {
                envelopeVal = 0.0f;
                break;
            }
            case State::attack:
            {
                envelopeVal = attackOffset + envelopeVal * attackCoeff;

                if (envelopeVal >= 1.0f || attackTime <= 0.0f)
                {
                    envelopeVal = 1.0f;
                    currentState = State::decay;
                    break;
                }
                break;
            }
            case State::decay:
            {
                envelopeVal = decayOffset + envelopeVal * decayCoeff;

                if (envelopeVal <= sustainLevel || decayTime <= 0.0f)
                {
                    envelopeVal = sustainLevel;
                    currentState = State::sustain;
                    break;
                }
                break;
            }
            case State::sustain:
            {
                envelopeVal = sustainLevel;
                break;
            }
            case State::release:
            {
                envelopeVal = releaseOffset + envelopeVal * releaseCoeff;

                if (envelopeVal <= 0.0f || releaseTime <= 0.0f)
                {
                    envelopeVal = 0.0f;
                    currentState = State::idle;
                    break;
                }
                break;
            }
        }

        return envelopeVal;
    }

    /** This method will conveniently apply the next numSamples number of envelope values
        to an AudioBuffer.

        @see getNextSample
     */
    template<typename FloatType>
    void applyEnvelopeToBuffer (juce::AudioBuffer<FloatType>& buffer, int startSample, int numSamples)
    {
        jassert (startSample + numSamples <= buffer.getNumSamples());

        auto numChannels = buffer.getNumChannels();

        while (--numSamples >= 0)
        {
            auto env = getNextSample();

            for (int i = 0; i < numChannels; ++i)
                buffer.getWritePointer (i)[startSample] *= env;

            ++startSample;
        }
    }

    void setAnalog (bool a)
    {
        if (a != analog)
        {
            analog = a;
            if (analog)
            {
                attackTCO  = std::exp (-0.5f);
                decayTCO   = std::exp (-5.0f);
                releaseTCO = std::exp (-5.0f);
            }
            else
            {
                attackTCO = decayTCO = releaseTCO = std::pow (10.0f, -96.0f / 20.0f);
            }
        }
    }

private:
    double sampleRate = 44100.0;

    float envelopeVal = 0.0f;

    float attackTime = 0.1f, decayTime = 0.1f, releaseTime = 0.1f;
    float sustainLevel = 0.0f;

    float attackCoeff = 0.0f, attackOffset = 0.0f, attackTCO {std::exp (-0.5f)};
    float decayCoeff = 0.0f, decayOffset = 0.0f, decayTCO {std::exp (-5.0f)};
    float releaseCoeff = 0.0f, releaseOffset = 0.0f, releaseTCO {std::exp (-5.0f)};

    bool analog = true;

    enum class State { idle, attack, decay, sustain, release };
    State currentState = State::idle;

    void calculateAttackTime();
    void calculateDecayTime();
    void calculateReleaseTime();

    inline void setAttackTime (float d)
    {
        if (! almostEqual (attackTime, d))
        {
            attackTime = d;
            calculateAttackTime();
        }
    }

    inline void setDecayTime (float d)
    {
        if (! almostEqual (decayTime, d))
        {
            decayTime = d;
            calculateDecayTime();
        }
    }

    inline void setReleaseTime (float d)
    {
        if (! almostEqual (releaseTime, d))
        {
            releaseTime = d;
            calculateReleaseTime();
        }
    }

    inline void setSustainLevel (float d)
    {
        if (! almostEqual (sustainLevel, d))
        {
            sustainLevel = d;
            calculateDecayTime();
            if (currentState != State::release)
                calculateReleaseTime();
        }
    }
};

//==============================================================================
// An ADSR Envelope with linear curves with the same API as the juce::ADSR
class LinEnvelope
{
public:
    enum class State { idle, attack, decay, sustain, release };

    //==============================================================================
    /** Holds the parameters being used by an ADSR object. */
    struct Parameters
    {
        /** Attack time in seconds. */
        float attack  = 0.1f;

        /** Decay time in seconds. */
        float decay   = 0.1f;

        /** Sustain level. */
        float sustain = 1.0f;

        /** Release time in seconds. */
        float release = 0.1f;
    };

    /** Sets the parameters that will be used by an ADSR object.
        You must have called setSampleRate() with the correct sample rate before
        this otherwise the values may be incorrect!
        @see getParameters
     */
    void setParameters (const Parameters& newParameters)
    {
        currentParameters = newParameters;

        sustainLevel = newParameters.sustain;
        calculateRates (newParameters);
    }

    void setSampleRate (double sr)              { sampleRate = sr;      }

    float getEnvelopeValue()                    { return envelopeVal;   }
    State getState()                            { return currentState;  }

    void noteOn()
    {
        if (attackRate > 0.0f)
        {
            currentState = State::attack;
        }
        else if (decayRate > 0.0f)
        {
            currentState = State::decay;
            envelopeVal = 1.0f;
        }
        else if (sustainLevel > 0.0f)
        {
            currentState = State::sustain;
            envelopeVal = sustainLevel;
        }
        else if (releaseRate > 0.0f)
        {
            currentState = State::release;
        }
        else
        {
            currentState = State::idle;
            envelopeVal = 0.0f;
        }
    }

    void noteOff()
    {
        if (releaseRate > 0)
        {
            currentState = State::release;
        }
        else
        {
            currentState = State::idle;
            envelopeVal = 0.0f;
        }
    }

    void reset()
    {
        currentState = State::idle;
        envelopeVal = 0.0f;
    }

    //==============================================================================
    /** Returns the next sample value for an ADSR object.
        @see applyEnvelopeToBuffer
     */
    float getNextSample()
    {
        if (currentState == State::idle)
            return 0.0f;

        if (currentState == State::attack)
        {
            if (attackRate > 0.0f)
                envelopeVal += attackRate;
            else
                envelopeVal = 1.0f;

            if (envelopeVal >= 1.0f)
            {
                envelopeVal = 1.0f;

                if (decayRate > 0.0f)
                    currentState = State::decay;
                else
                    currentState = State::sustain;
            }
        }
        else if (currentState == State::decay)
        {
            if (decayRate > 0.0f)
                envelopeVal -= decayRate;
            else
                envelopeVal = sustainLevel;

            if (envelopeVal <= sustainLevel)
            {
                envelopeVal = sustainLevel;
                currentState = State::sustain;
            }
        }
        else if (currentState == State::sustain)
        {
            envelopeVal = sustainLevel;
        }
        else if (currentState == State::release)
        {
            if (releaseRate > 0.0f)
                envelopeVal -= releaseRate;
            else
                envelopeVal = 0.0f;

            if (envelopeVal <= 0.0f)
                reset();
        }

        return envelopeVal;
    }

    /** This method will conveniently apply the next numSamples number of envelope values
        to an AudioBuffer.
        @see getNextSample
     */
    template<typename FloatType>
    void applyEnvelopeToBuffer (juce::AudioBuffer<FloatType>& buffer, int startSample, int numSamples)
    {
        jassert (startSample + numSamples <= buffer.getNumSamples());

        auto numChannels = buffer.getNumChannels();

        while (--numSamples >= 0)
        {
            auto env = getNextSample();

            for (int i = 0; i < numChannels; ++i)
                buffer.getWritePointer (i)[startSample] *= env;

            ++startSample;
        }
    }

protected:
    void calculateRates (const Parameters& parameters)
    {
        // need to call setSampleRate() first!
        jassert (sampleRate > 0.0);

        attackRate  = (parameters.attack  > 0.0f ? static_cast<float> (1.0f / (parameters.attack * sampleRate))  : 0.0f);
        decayRate   = (parameters.decay   > 0.0f ? static_cast<float> (1.0f / (parameters.decay * sampleRate))   : 0.0f);
        releaseRate = (parameters.release > 0.0f ? static_cast<float> (1.0f / (parameters.release * sampleRate)) : 0.0f);
    }

    State currentState = State::idle;
    Parameters currentParameters;
    double sampleRate = 44100.0;
    float envelopeVal = 0.0f;

    float sustainLevel = 0.0f;
    float attackRate = 0.0f, decayRate = 0.0f, releaseRate = 0.0f;
};

}} // namespace tracktion { inline namespace engine
