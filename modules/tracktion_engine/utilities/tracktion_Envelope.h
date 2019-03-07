/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

// An ADSR Envelope with exponential curves with the same API as the juce::ADSR
class Envelope
{
public:
    Envelope();
    
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
    
private:
	double sampleRate = 44100.0;
    
	float envelopeVal = 0.0f;

    float attackTime = 0.1f, decayTime = 0.1f, releaseTime = 0.1f;
    float sustainLevel = 0.0f;
    
    float attackCoeff = 0.0f, attackOffset = 0.0f, attackTCO {std::exp (-0.5f)};
    float decayCoeff = 0.0f, decayOffset = 0.0f, decayTCO {std::exp (-5.0f)};
    float releaseCoeff = 0.0f, releaseOffset = 0.0f, releaseTCO {std::exp (-5.0f)};

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
    
} // namespace tracktion_engine
