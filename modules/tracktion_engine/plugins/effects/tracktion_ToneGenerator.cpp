/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
juce::StringArray ToneGeneratorPlugin::getOscTypeNames()
{
    return { "Sin", "Triangle", "Saw Up", "Saw Down", "Square", "Noise" };
}

//==============================================================================
ToneGeneratorPlugin::ToneGeneratorPlugin (PluginCreationInfo info)
    : Plugin (info)
{
    initialiseOscilators();
    
    auto um = getUndoManager();

    oscType.referTo (state, IDs::oscType, um, static_cast<float> (OscType::sin));
    bandLimit.referTo (state, IDs::bandLimit, um, 0.0f);
    frequency.referTo (state, IDs::frequency, um, 220.0f);
    level.referTo (state, IDs::level, um, 1.0f);

    oscTypeParam    = createDiscreteParameter (*this, "oscType",      TRANS("OSC Type"),   { 0.0f, (float)  getOscTypeNames().size() - 1 },  oscType,    getOscTypeNames());
    bandLimitParam  = createDiscreteParameter (*this, "bandLimit",    TRANS("Band Limit"), { 0.0f, 1.0f },                                   bandLimit,  { NEEDS_TRANS("Aliased"), NEEDS_TRANS("Band Limited") });
    frequencyParam  = createSuffixedParameter (*this, "frequency",    TRANS("Frequency"),  { 1.0f, 22050.0f }, 1000.0f,                      frequency,  {});
    levelParam      = createSuffixedParameter (*this, "level",        TRANS("Level"),      { 0.00001f, 1.0f }, 0.5f,                         level,      {});
    
    addAutomatableParameter (oscTypeParam);
    addAutomatableParameter (bandLimitParam);
    addAutomatableParameter (frequencyParam);
    addAutomatableParameter (levelParam);
}

ToneGeneratorPlugin::~ToneGeneratorPlugin()
{
    notifyListenersOfDeletion();
}

const char* ToneGeneratorPlugin::xmlTypeName = "toneGenerator";

void ToneGeneratorPlugin::initialise (const PluginInitialisationInfo& info)
{
    scratch.setSize (1, info.blockSizeSamples);
    auto samplesPerBlock = static_cast<uint32_t> (info.blockSizeSamples);

    sine.prepare        ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
    triangle.prepare    ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
    sawUp.prepare       ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
    sawDown.prepare     ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
    square.prepare      ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
    noise.prepare       ({ sampleRate, static_cast<uint32_t> (samplesPerBlock), 1 });
}

void ToneGeneratorPlugin::deinitialise()
{
}

void ToneGeneratorPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK
    float gain = levelParam->getCurrentValue();

    if (juce::isWithin (gain, 0.0f, 0.00001f))
    {
        fc.destBuffer->clear();
        return;
    }

    int numSamples = fc.bufferNumSamples;
    scratch.setSize (1, numSamples, false, false, true);

    bandLimitOsc = getBoolParamValue (*bandLimitParam);
    float freq = frequencyParam->getCurrentValue();
    sine.setFrequency (freq);
    triangle.setFrequency (freq);
    sawUp.setFrequency (freq);
    sawDown.setFrequency (freq);
    square.setFrequency (freq);

    scratch.clear();
    juce::dsp::AudioBlock<float> block (scratch);
    juce::dsp::ProcessContextReplacing<float> context (block);

    switch (getTypedParamValue<OscType> (*oscTypeParam))
    {
        case OscType::sin:      sine.process (context);     break;
        case OscType::triangle: triangle.process (context); break;
        case OscType::sawUp:    sawUp.process (context);    break;
        case OscType::sawDown:  sawDown.process (context);  break;
        case OscType::square:   square.process (context);   break;
        case OscType::noise:    noise.process (context);    break;
    };

    scratch.applyGain (gain);

    for (int i = fc.destBuffer->getNumChannels(); --i >= 0;)
        fc.destBuffer->copyFrom (i, fc.bufferStartSample, scratch, 0, 0, numSamples);
}

void ToneGeneratorPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[] = { &oscType, &bandLimit, &frequency, &level, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

//==============================================================================
namespace ToneGenHelpers
{
    inline int oddEven (int x)
    {
        return (x % 2 == 0) ? 1 : -1;
    }
}

void ToneGeneratorPlugin::initialiseOscilators()
{
    auto sineFunc = [&] (float in) -> float
    {
        return std::sin (in);
    };

    auto triangleFunc = [&] (float in) -> float
    {
        if (bandLimitOsc.load())
        {
            const double f = frequencyParam->getCurrentValue();
            const double sr = sampleRate;

            double sum = 0;
            int k = 1;

            while (f * k < sr / 2)
            {
                sum += std::pow (-1, (k - 1) / 2.0f) / (k * k) * sin (k * in);
                k += 2;
            }

            return float (8.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi) * sum);
        }

        return (in < 0 ? in / -juce::MathConstants<float>::pi : in / juce::MathConstants<float>::pi) * 2 - 1;
    };

    auto sawUpFunc = [&] (float in) -> float
    {
        if (bandLimitOsc.load())
        {
            const double f = frequencyParam->getCurrentValue();
            const double sr = sampleRate;

            double sum = 0;
            int k = 1;

            while (f * k < sr / 2)
            {
                sum += ToneGenHelpers::oddEven (k) * std::sin (k * in) / k;
                k++;
            }

            return float (-2.0f / juce::MathConstants<float>::pi * sum);
        }

        return ((in + juce::MathConstants<float>::pi) / (2 * juce::MathConstants<float>::pi)) * 2 - 1;
    };

    auto sawDownFunc = [&] (float in) -> float
    {
        if (bandLimitOsc.load())
        {
            const double f = frequencyParam->getCurrentValue();
            const double sr = sampleRate;

            double sum = 0;
            int k = 1;

            while (f * k < sr / 2)
            {
                sum += ToneGenHelpers::oddEven (k) * std::sin (k * in) / k;
                k++;
            }

            return float (2.0f / juce::MathConstants<float>::pi * sum);
        }

        return -(((in + juce::MathConstants<float>::pi) / (2 * juce::MathConstants<float>::pi)) * 2 - 1);
    };

    auto squareFunc = [&] (float in) -> float
    {
        if (bandLimitOsc.load())
        {
            const double f = frequencyParam->getCurrentValue();
            const double sr = sampleRate;

            double sum = 0;
            int i = 1;

            while (f * (2 * i - 1) < sr / 2)
            {
                sum += std::sin ((2 * i - 1) * in) / ((2 * i - 1));
                i++;
            }

            return float (4.0f / juce::MathConstants<float>::pi * sum);
        }

        return in < 0 ? -1.0f : 1.0f;
    };

    auto noiseFunc = [&] (float) -> float
    {
        const float mean = 0.0f;
        const float stddev = 0.1f;

        static std::default_random_engine generator;
        static std::normal_distribution<float> dist (mean, stddev);

        return dist (generator);
    };

    sine.initialise (sineFunc);
    triangle.initialise (triangleFunc);
    sawUp.initialise (sawUpFunc);
    sawDown.initialise (sawDownFunc);
    square.initialise (squareFunc);
    noise.initialise (noiseFunc);
}

}} // namespace tracktion { inline namespace engine
