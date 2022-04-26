/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

class FODelay;
class FOChorus;

//==============================================================================
/** Smooths a value between 0 and 1 at a constant rate */
template <class T>
class ValueSmoother
{
public:
    void reset (double sr, double time)     { delta = 1.0f / T (sr * time); }
    T getCurrentValue()                     { return currentValue; }
    void setValue (T v)                     { targetValue = v; }
    void snapToValue()                      { currentValue = targetValue; }

    void process (int n)
    {
        if (targetValue != currentValue)
            for (int i = 0; i < n; i++)
                getNextValue();
    }

    T getNextValue()
    {
        if (currentValue < targetValue)
            currentValue = juce::jmin (targetValue, currentValue + delta);
        else if (currentValue > targetValue)
            currentValue = juce::jmax (targetValue, currentValue - delta);
        return currentValue;
    }

    void setValueUnsmoothed (T v)
    {
        targetValue = v;
        currentValue = v;
    }

private:
    T delta = 0, targetValue = 0, currentValue = 0;
};

//==============================================================================
class SimpleLFO
{
public:
    //==============================================================================
    enum WaveShape : int
    {
        none,
        sine,
        triangle,
        sawUp,
        sawDown,
        square,
        random
    };

    //==============================================================================
    struct Parameters
    {
        Parameters() = default;
        Parameters (float frequencyIn, float phaseOffsetIn, float offsetIn, float depthIn,
                    WaveShape waveShapeIn, float pulseWidthIn) :
            waveShape (waveShapeIn), frequency (frequencyIn), phaseOffset (phaseOffsetIn),
            offset (offsetIn), depth (depthIn), pulseWidth (pulseWidthIn) {}

        WaveShape waveShape = sine;
        float frequency = 0, phaseOffset = 0, offset = 0, depth = 0, pulseWidth = 0;
    };

    //==============================================================================
    void setSampleRate (double newSampleRate)       { sampleRate = newSampleRate; }
    void setParameters (Parameters newParameters)   { parameters = newParameters; }
    void reset()                                    { phase = 0; }

    void process (int numSamples)
    {
        double step = 0.0;
        if (parameters.frequency > 0.001f)
            step = parameters.frequency / sampleRate;

        for (int i = 0; i < numSamples; i++)
        {
            phase += step;
            if (phase >= 1.0)
                phase -= 1.0;

            float localPhase = 0.0f;

            if (parameters.phaseOffset != 0)
                localPhase = wrapValue ((float) std::fmod (phase + parameters.phaseOffset, 1.0f), 1.0f);
            else
                localPhase = wrapValue ((float) phase, 1.0f);

            jassert (localPhase >= 0.0f && localPhase <= 1.0f);

            if (parameters.waveShape == random)
            {
                if (localPhase < lastLocalPhase)
                    lastRandomVal = randomSource.nextFloat() * 2.0f - 1.0f;

                lastLocalPhase = localPhase;
            }
        }
    }

    float getCurrentValue()
    {
        float val = 0.0f, localPhase = 0.0f;

        if (parameters.phaseOffset != 0)
            localPhase = wrapValue ((float) std::fmod (phase + parameters.phaseOffset, 1.0f), 1.0f);
        else
            localPhase = wrapValue ((float) phase, 1.0f);

        jassert (localPhase >= 0.0f && localPhase <= 1.0f);

        switch (parameters.waveShape)
        {
            case none:      val = 0; break;
            case sine:      val = std::sin (localPhase * juce::MathConstants<float>::pi * 2); break;
            case triangle:  val = (localPhase < 0.5f) ? (4.0f * localPhase - 1.0f) : (-4.0f * localPhase + 3.0f); break;
            case sawUp:     val = localPhase * 2.0f - 1.0f; break;
            case sawDown:   val = (1.0f - localPhase) * 2.0f - 1.0f; break;
            case square:    val = (localPhase < parameters.pulseWidth) ? 1.0f : -1.0f; break;
            case random:    val = lastRandomVal; break;
        }

        return (val * parameters.depth + parameters.offset);
    }

private:
    Parameters parameters;

    double phase = 0, lastLocalPhase = 1, sampleRate = 0;
    float lastRandomVal = 0;

    juce::Random randomSource {1};

    inline float wrapValue (float v, float range)
    {
        while (v >= range) v -= range;
        while (v <  0)     v += range;
        return v;
    }
};

//==============================================================================
class FourOscPlugin  : public Plugin,
                       private juce::MPESynthesiser,
                       private juce::AsyncUpdater,
                       private LevelMeasurer::Client
{
public:
    FourOscPlugin (PluginCreationInfo);
    ~FourOscPlugin() override;

    bool isMono() const                                 { return voiceModeValue.get() == 0; }
    bool isLegato() const                               { return voiceModeValue.get() == 1; }
    bool isPoly() const                                 { return voiceModeValue.get() == 2; }

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("4OSC"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("4OSC"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "4OSC"; }
    juce::String getSelectableDescription() override    { return TRANS("4OSC"); }
    bool needsConstantBufferSize() override             { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;

    void reset() override;

    void applyToBuffer (const PluginRenderContext&) override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool isSynth() override                             { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    double getTailLength() const override               { return ampRelease->getCurrentValue(); }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    float getCurrentTempo()                             { return currentTempo; }

private:
    std::unordered_map<juce::String, juce::String> labels;

public:
    //==============================================================================
    struct OscParams
    {
        OscParams (FourOscPlugin& plugin, int oscNum);
        void attach();
        void detach();

        juce::CachedValue<int> waveShapeValue, voicesValue;
        juce::CachedValue<float> tuneValue, fineTuneValue, levelValue, pulseWidthValue,
                                 detuneValue, spreadValue, panValue;

        AutomatableParameter::Ptr tune, fineTune, level, pulseWidth, detune, spread, pan;

        void restorePluginStateFromValueTree (const juce::ValueTree& v)
        {
            juce::CachedValue<float>* cvsFloat[]  = { &tuneValue, &fineTuneValue, &levelValue, &pulseWidthValue,
                &detuneValue, &spreadValue, &panValue, nullptr };

            juce::CachedValue<int>* cvsInt[] { &waveShapeValue, &voicesValue, nullptr };

            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
            copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
        }
    };

    juce::OwnedArray<OscParams> oscParams;

    //==============================================================================
    struct LFOParams
    {
        LFOParams (FourOscPlugin& plugin, int lfoNum);
        void attach();
        void detach();

        juce::CachedValue<bool> syncValue;
        juce::CachedValue<int> waveShapeValue;
        juce::CachedValue<float> rateValue, beatValue, depthValue;

        AutomatableParameter::Ptr rate, depth;

        void restorePluginStateFromValueTree (const juce::ValueTree& v)
        {
            juce::CachedValue<float>* cvsFloat[]  = { &rateValue, &beatValue, &depthValue, nullptr };
            juce::CachedValue<int>* cvsInt[] { &waveShapeValue, nullptr };
            juce::CachedValue<bool>* cvsBool[] { &syncValue, nullptr };

            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
            copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
            copyPropertiesToNullTerminatedCachedValues (v, cvsBool);
        }
    };

    juce::OwnedArray<LFOParams> lfoParams;

    //==============================================================================
    struct MODEnvParams
    {
        MODEnvParams (FourOscPlugin& plugin, int envNum);
        void attach();
        void detach();

        juce::CachedValue<float> modAttackValue, modDecayValue, modSustainValue, modReleaseValue;
        AutomatableParameter::Ptr modAttack, modDecay, modSustain, modRelease;

        void restorePluginStateFromValueTree (const juce::ValueTree& v)
        {
            juce::CachedValue<float>* cvsFloat[]  = { &modAttackValue, &modDecayValue, &modSustainValue, &modReleaseValue, nullptr };

            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
        }
    };

    juce::OwnedArray<MODEnvParams> modEnvParams;

    //==============================================================================
    juce::CachedValue<float> ampAttackValue, ampDecayValue, ampSustainValue, ampReleaseValue, ampVelocityValue;
    juce::CachedValue<float> filterAttackValue, filterDecayValue, filterSustainValue, filterReleaseValue, filterFreqValue,
                             filterResonanceValue, filterAmountValue, filterKeyValue, filterVelocityValue;
    juce::CachedValue<int> filterTypeValue, filterSlopeValue;
    juce::CachedValue<bool> ampAnalogValue;

    AutomatableParameter::Ptr ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;
    AutomatableParameter::Ptr filterAttack, filterDecay, filterSustain, filterRelease, filterFreq, filterResonance, filterAmount, filterKey, filterVelocity;

    juce::CachedValue<bool> distortionOnValue, reverbOnValue, delayOnValue, chorusOnValue;

    juce::CachedValue<float> distortionValue;
    AutomatableParameter::Ptr distortion;

    juce::CachedValue<float> reverbSizeValue, reverbDampingValue, reverbWidthValue, reverbMixValue;
    AutomatableParameter::Ptr reverbSize, reverbDamping, reverbWidth, reverbMix;

    juce::CachedValue<float> delayValue, delayFeedbackValue, delayCrossfeedValue, delayMixValue;
    AutomatableParameter::Ptr delayFeedback, delayCrossfeed, delayMix;

    juce::CachedValue<float> chorusSpeedValue, chorusDepthValue, chorusWidthValue, chorusMixValue;
    AutomatableParameter::Ptr chorusSpeed, chorusDepth, chorusWidth, chorusMix;

    juce::CachedValue<int> voiceModeValue, voicesValue;
    juce::CachedValue<float> legatoValue, masterLevelValue;
    AutomatableParameter::Ptr legato, masterLevel;

    //==============================================================================

    enum ModSource : int
    {
        none = -1,
        lfo1 = 0,
        lfo2,
        env1,
        env2,
        mpePressure,
        mpeTimbre,
        midiNoteNum,
        midiVelocity,
        ccBankSelect,
        ccPolyMode = ccBankSelect + 127,
        numModSources
    };

    juce::String modulationSourceToName (ModSource src);
    juce::String modulationSourceToID (ModSource src);
    ModSource idToModulationSource (juce::String idStr);
    bool isModulated (AutomatableParameter::Ptr param);
    juce::Array<float> getLiveModulationPositions (AutomatableParameter::Ptr param);
    juce::Array<ModSource> getModulationSources (AutomatableParameter::Ptr param);
    float getModulationDepth (ModSource src, AutomatableParameter::Ptr param);
    void setModulationDepth (ModSource src, AutomatableParameter::Ptr param, float depth);
    void clearModulation (ModSource src, AutomatableParameter::Ptr param);

    struct ModAssign
    {
    public:
        ModAssign()
        {
            for (auto& d : depths)
                d = -1000.0f;
        }

        inline void updateCachedInfo()
        {
            int f = -1, l = -1;
            for (int i = 0; i < juce::numElementsInArray (depths); i++)
            {
                if (depths[i] >= -1.0f && f == -1)  f = i;
                if (depths[i] >= -1.0)              l = i;
            }

            firstModIndex = f;
            lastModIndex  = l;
        }

        inline bool isModulated()
        {
            return firstModIndex != -1 && lastModIndex != -1;
        }

        int firstModIndex = -1, lastModIndex = -1;
        float depths[numModSources] = {};
    };

    std::unordered_map<AutomatableParameter*, ModAssign> modMatrix;
    float controllerValues[128] = {0};

    float getLevel (int channel);

private:
    //==============================================================================
    void valueTreeChanged() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void handleAsyncUpdate() override;
    void handleController (int midiChannel, int controllerNumber, int controllerValue) override;

    void flushPluginStateToValueTree() override;

    void loadModMatrix();
    void setupTextFunctions();
    AutomatableParameter* addParam (const juce::String& paramID, const juce::String& name, juce::NormalisableRange<float> valueRange, juce::String label = {});

    void applyToBuffer (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void updateParams (juce::AudioBuffer<float>& buffer);
    void applyEffects (juce::AudioBuffer<float>& buffer);
    float paramValue (AutomatableParameter::Ptr param);

    tempo::Sequence::Position currentPos { createPosition (edit.tempoSequence) };
    juce::Reverb reverb;
    std::unique_ptr<FODelay> delay;
    std::unique_ptr<FOChorus> chorus;
    std::unordered_map<AutomatableParameter*, ValueSmoother<float>> smoothers;

    bool flushingState = false;
    float currentTempo = 0.0f;
    LevelMeasurer levelMeasurer;
    DbTimePair levels[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourOscPlugin)
};

}} // namespace tracktion { inline namespace engine
