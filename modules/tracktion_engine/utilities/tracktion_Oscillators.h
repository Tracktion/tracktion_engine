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
class BandlimitedWaveLookupTables : public juce::ReferenceCountedObject
{
public:
    ~BandlimitedWaveLookupTables();

    using Ptr = juce::ReferenceCountedObjectPtr<BandlimitedWaveLookupTables>;

    static Ptr getLookupTables (double sampleRate);

    double sampleRate = 44100.0;

    //==============================================================================
    juce::dsp::LookupTableTransform<float> sineFunction;

    juce::OwnedArray<juce::dsp::LookupTableTransform<float>> triangleFunctions, sawUpFunctions, sawDownFunctions;

    const int tablePerNumNotes = 3;

private:
    BandlimitedWaveLookupTables (double sampleRate, int tableSize);
};

//==============================================================================
class Oscillator
{
public:
    //==============================================================================
    enum Waves
    {
        none = 0,
        sine,
        square,
        saw,
        triangle,
        noise,
    };

    //==============================================================================
    Oscillator() = default;

    void start();
    void start (float p)            { phase = p;        }

    void setSampleRate (double sr);
    void setWave (Waves w)          { wave  = w;        }
    void setNote (float n)          { note = n;         }
    void setGain (float g)          { gain = g;         }
    void setPulseWidth (float p)    { pulseWidth = p;   }

    void process (juce::AudioSampleBuffer& buffer, int startSample, int numSamples);

private:
    //==============================================================================
    void processSine (juce::AudioSampleBuffer& buffer, int startSample, int numSamples);
    void processSquare (juce::AudioSampleBuffer& buffer, int startSample, int numSamples);
    void processNoise (juce::AudioSampleBuffer& buffer, int startSample, int numSamples);

    void processLookup (juce::AudioSampleBuffer& buffer, int startSample, int numSamples,
                        const juce::OwnedArray<juce::dsp::LookupTableTransform<float>>& tableSet);

    //==============================================================================
    Waves wave = sine;
    float phase = 0, gain = 1, note = 69, pulseWidth = 0.5;
    double sampleRate = 44100.0;

    BandlimitedWaveLookupTables::Ptr lookupTables;

    std::default_random_engine generator;
    std::normal_distribution<float> normalDistribution {0.0f, 0.1f};
};

//==============================================================================
class MultiVoiceOscillator
{
public:
    MultiVoiceOscillator (int maxVoices = 8);

    void start();
    void setSampleRate (double sr);
    void setWave (Oscillator::Waves w);
    void setNote (float n);
    void setGain (float g);
    void setPan (float p);
    void setPulseWidth (float p);

    void setNumVoices (int n);
    void setDetune (float d);
    void setSpread (float s);

    void process (juce::AudioSampleBuffer& buffer, int startSample, int numSamples);

private:
    juce::OwnedArray<Oscillator> oscillators;

    int voices = 1;
    float detune = 0, spread = 0, gain = 1.0f, note = 69.0f, pan = 0.0f;
};

} // namespace tracktion_engine
