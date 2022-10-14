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
inline int oddEven (int x)
{
    return (x % 2 == 0) ? 1 : -1;
}

//==============================================================================
static float sine (float phase)
{
    return std::sin (phase * 2 * juce::MathConstants<float>::pi);
}

static float triangle (float phase, float freq, double sampleRate)
{
    float sum = 0;
    int k = 1;
    while (freq * k < sampleRate / 2)
    {
        sum += std::pow (-1.0f, (k - 1.0f) / 2.0f) / (k * k) * std::sin (k * (phase * 2 * juce::MathConstants<float>::pi));
        k += 2;
    }
    return float (8.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi) * sum);
}

static float sawUp (float phase, float freq, double sampleRate)
{
    float sum = 0;
    int k = 1;
    while (freq * k < sampleRate / 2)
    {
        sum += oddEven (k) * std::sin (k * (phase * 2 * juce::MathConstants<float>::pi)) / k;
        k++;
    }
    return float (-2.0f / juce::MathConstants<float>::pi * sum);
}

static float sawDown (float phase, float freq, double sampleRate)
{
    float sum = 0;
    int k = 1;
    while (freq * k < sampleRate / 2)
    {
        sum += oddEven (k) * std::sin (k * (phase * 2 * juce::MathConstants<float>::pi)) / k;
        k++;
    }
    return float (2.0f / juce::MathConstants<float>::pi * sum);
}

//==============================================================================
void Oscillator::start()
{
    static juce::Random r;
    phase = r.nextFloat();
}

void Oscillator::setSampleRate (double sr)
{
    sampleRate = sr;

    if (lookupTables == nullptr || sampleRate != lookupTables->sampleRate)
        lookupTables = BandlimitedWaveLookupTables::getLookupTables (sampleRate);
}

void Oscillator::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (lookupTables != nullptr)
    {
        switch (wave)
        {
            case none:      break;
            case sine:      processSine (buffer, startSample, numSamples);  break;
            case square:    processSquare (buffer, startSample, numSamples);  break;
            case saw:       processLookup (buffer, startSample, numSamples, lookupTables->sawUpFunctions);    break;
            case triangle:  processLookup (buffer, startSample, numSamples, lookupTables->triangleFunctions); break;
            case noise:     processNoise (buffer, startSample, numSamples); break;
        }
    }
}

void Oscillator::processSine (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const float frequency = std::min (float (sampleRate) / 2.0f, 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f));
    const float period = 1.0f / float (frequency);
    const float periodInSamples = float (period * sampleRate);
    const float delta = 1.0f / periodInSamples;

    auto* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    for (int samp = 0; samp < numSamples; samp++)
    {
        float value = lookupTables->sineFunction[phase] * gain;
        for (int ch = 0; ch < numChannels; ch++)
            channels[ch][startSample + samp] += value;

        phase += delta;
        while (phase >= 1.0f)
            phase -= 1.0f;
    }
}

void Oscillator::processNoise (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    auto* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    for (int samp = 0; samp < numSamples; samp++)
    {
        float value = normalDistribution (generator) * gain;
        for (int ch = 0; ch < numChannels; ch++)
            channels[ch][startSample + samp] += value;
    }
}

void Oscillator::processLookup (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                                const juce::OwnedArray<juce::dsp::LookupTableTransform<float>>& tableSet)
{
    const float frequency = std::min (float (sampleRate) / 2.0f, 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f));
    const float period = 1.0f / float (frequency);
    const float periodInSamples = float (period * sampleRate);
    const float delta = 1.0f / periodInSamples;

    auto* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    int tableIndex = juce::jlimit (0, tableSet.size() - 1, int ((note - 0.5) / lookupTables->tablePerNumNotes));

    auto table = tableSet[tableIndex];
    jassert (table != nullptr);

    if (table != nullptr)
    {
        for (int samp = 0; samp < numSamples; samp++)
        {
            float value = table->processSampleUnchecked (phase) * gain;
            for (int ch = 0; ch < numChannels; ch++)
                channels[ch][startSample + samp] += value;

            phase += delta;
            while (phase >= 1.0f)
                phase -= 1.0f;
        }
    }
}

void Oscillator::processSquare (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const float frequency = std::min (float (sampleRate) / 2.0f, 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f));
    const float period = 1.0f / float (frequency);
    const float periodInSamples = float (period * sampleRate);
    const float delta = 1.0f / periodInSamples;

    auto* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    int tableIndex = juce::jlimit (0, lookupTables->sawUpFunctions.size() - 1, int ((note - 0.5) / lookupTables->tablePerNumNotes));

    auto saw1 = lookupTables->sawUpFunctions[tableIndex];
    auto saw2 = lookupTables->sawDownFunctions[tableIndex];

    jassert (saw1 != nullptr && saw2 != nullptr);

    if (saw1 != nullptr && saw2 != nullptr)
    {
        for (int samp = 0; samp < numSamples; samp++)
        {
            float phaseUp   = phase + 0.5f * pulseWidth;
            float phaseDown = phase - 0.5f * pulseWidth;

            if (phaseUp   > 1.0f) phaseUp   -= 1.0f;
            if (phaseDown < 0.0f) phaseDown += 1.0f;

            float value = (saw1->processSampleUnchecked (phaseUp) +
                           saw2->processSampleUnchecked (phaseDown)) * gain;

            for (int ch = 0; ch < numChannels; ch++)
                channels[ch][startSample + samp] += value;

            phase += delta;
            while (phase >= 1.0f)
                phase -= 1.0f;
        }
    }
}

//==============================================================================
MultiVoiceOscillator::MultiVoiceOscillator (int maxVoices)
{
    for (int i = 0; i < maxVoices * 2; i++)
        oscillators.add (new Oscillator());
}

void MultiVoiceOscillator::start()
{
    static juce::Random r;

    for (int i = 0; i < oscillators.size(); i += 2)
    {
        float phase = r.nextFloat();
        oscillators[i + 0]->start (phase);
        oscillators[i + 1]->start (phase);
    }
}

void MultiVoiceOscillator::setSampleRate (double sr)
{
    for (auto o : oscillators)
        o->setSampleRate (sr);
}

void MultiVoiceOscillator::setWave (Oscillator::Waves w)
{
    for (auto o : oscillators)
        o->setWave (w);
}

void MultiVoiceOscillator::setNote (float n)
{
    note = n;
}

void MultiVoiceOscillator::setGain (float g)
{
    gain = g;
}

void MultiVoiceOscillator::setPan (float p)
{
    pan = p;
}

void MultiVoiceOscillator::setPulseWidth (float p)
{
    for (auto o : oscillators)
        o->setPulseWidth (p);
}

void MultiVoiceOscillator::setNumVoices (int n)
{
    voices = n;
}

void MultiVoiceOscillator::setDetune (float d)
{
    detune = d;
}

void MultiVoiceOscillator::setSpread (float s)
{
    spread = s;
}

void MultiVoiceOscillator::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (voices == 1)
    {
        float leftGain  = 1.0f - pan;
        float rightGain = 1.0f + pan;

        for (int i = 0; i < 2; i++)
        {
            bool left = (i % 2) == 0;
            float panGain = left ? leftGain : rightGain;

            float* data = buffer.getWritePointer (left ? 0 : 1, startSample);
            float* dataPointers[] = {data};

            juce::AudioBuffer<float> channelBuffer (dataPointers, 1, numSamples);

            oscillators[i]->setGain (gain * panGain / voices);
            oscillators[i]->setNote (note);
            oscillators[i]->process (channelBuffer, 0, numSamples);
        }
    }
    else
    {
        for (int i = 0; i < voices * 2; i++)
        {
            int voiceIndex = (i / 2);
            float localPan = juce::jlimit (-1.0f, 1.0f, ((voiceIndex % 2 == 0) ? 1 : -1) * spread);

            float leftGain  = 1.0f - localPan;
            float rightGain = 1.0f + localPan;

            float base = note - detune / 2;
            float delta = detune / (voices - 1);
            bool left = (i % 2) == 0;
            float panGain = left ? leftGain : rightGain;

            float* data = buffer.getWritePointer (left ? 0 : 1, startSample);
            float* dataPointers[] = {data};

            juce::AudioBuffer<float> channelBuffer (dataPointers, 1, numSamples);

            oscillators[i]->setGain (gain * panGain / voices);
            oscillators[i]->setNote (base + delta * (i / 2));
            oscillators[i]->process (channelBuffer, 0, numSamples);
        }
    }
}

//==============================================================================
static juce::Array<BandlimitedWaveLookupTables*> tableCache;

BandlimitedWaveLookupTables::Ptr BandlimitedWaveLookupTables::getLookupTables (double sampleRate)
{
    for (auto table : tableCache)
        if (table->sampleRate == sampleRate)
            return table;

    Ptr table = new BandlimitedWaveLookupTables (sampleRate, 1024);
    return table;
}

BandlimitedWaveLookupTables::BandlimitedWaveLookupTables (double sr, int tableSize)
    : sampleRate (sr),
      sineFunction ([] (float in) { return sine (in); }, 0.0f, 1.0f, (size_t) tableSize)
{
    auto getMidiNoteInHertz = [](float noteNumber)
    {
        return 440.0f * std::pow (2.0f, (noteNumber - 69) / 12.0f);
    };

    auto start = juce::Time::getCurrentTime();

    for (float note = tablePerNumNotes + 0.5f; note < 127; note += tablePerNumNotes)
    {
        const float freq = getMidiNoteInHertz (note);

        triangleFunctions.add (new juce::dsp::LookupTableTransform<float> ([freq, sr] (float value)
                               { return triangle (value, freq, sr); }, 0.0f, 1.0f, (size_t) tableSize));

        sawUpFunctions.add (new juce::dsp::LookupTableTransform<float> ([freq, sr] (float value)
                            { return sawUp (value, freq, sr); }, 0.0f, 1.0f, (size_t) tableSize));

        sawDownFunctions.add (new juce::dsp::LookupTableTransform<float> ([freq, sr] (float value)
                              { return sawDown (value, freq, sr); }, 0.0f, 1.0f, (size_t) tableSize));
    }

    auto elapsed = (juce::Time::getCurrentTime() - start);
    DBG ("Generating waves: " + juce::String (elapsed.inMilliseconds()) + "ms");

    tableCache.add (this);
}

BandlimitedWaveLookupTables::~BandlimitedWaveLookupTables()
{
    tableCache.removeFirstMatchingValue (this);
}

}} // namespace tracktion { inline namespace engine
