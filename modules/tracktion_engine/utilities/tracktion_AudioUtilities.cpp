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

void sanitiseValues (juce::AudioBuffer<float>& buffer, int start, int numSamps,
                     float maxAbsValue, float minAbsThreshold)
{
    for (int j = buffer.getNumChannels(); --j >= 0;)
    {
        float* d = buffer.getWritePointer (j, start);
        jassert (numSamps <= buffer.getNumSamples());

        for (int i = numSamps; --i >= 0;)
        {
            const float n = *d;

            if (n >= minAbsThreshold)
            {
                if (n > maxAbsValue)
                    *d = maxAbsValue;
            }
            else if (n <= -minAbsThreshold)
            {
                if (n < -maxAbsValue)
                    *d = -maxAbsValue;
            }
            else
            {
                *d = 0;
            }

            ++d;
        }
    }
}

void resetFP() noexcept
{
   #if JUCE_WINDOWS
    _fpreset();
   #endif

   #if JUCE_MAC && JUCE_INTEL
    feclearexcept (FE_ALL_EXCEPT);
   #endif
}

static unsigned short getFPStatus() noexcept
{
    unsigned short sw = 0;

  #if JUCE_INTEL
   #if JUCE_32BIT
    #if JUCE_WINDOWS
     _asm fstsw WORD PTR sw
    #else
     asm ("fstsw %%ax" : "=a" (sw));
    #endif
   #endif
  #endif

    return sw;
}

inline void clearFP() noexcept
{
   #if JUCE_WINDOWS
    _clearfp();
   #endif

   #if JUCE_MAC && JUCE_INTEL
    __asm __volatile ("fclex");
   #endif
}

bool hasFloatingPointDenormaliseOccurred() noexcept
{
    if ((getFPStatus() & 0x02) != 0)
    {
        clearFP();
        return true;
    }

    return false;
}

void zeroDenormalisedValuesIfNeeded (juce::AudioBuffer<float>& buffer) noexcept
{
    juce::ignoreUnused (buffer);

   #if JUCE_INTEL
    if (hasFloatingPointDenormaliseOccurred())
        if (! buffer.hasBeenCleared())
            sanitiseValues (buffer, 0, buffer.getNumSamples(), 100.0f);
   #endif
}

void addAntiDenormalisationNoise (juce::AudioBuffer<float>& buffer, int start, int numSamples) noexcept
{
    juce::ignoreUnused (buffer, start, numSamples);

   #if JUCE_INTEL
    if (buffer.hasBeenCleared())
    {
        for (int i = buffer.getNumChannels(); --i >= 0;)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (i, start), 0.000000045f, numSamples);
    }
    else
    {
        for (int i = buffer.getNumChannels(); --i >= 0;)
            juce::FloatVectorOperations::add (buffer.getWritePointer (i, start), 0.000000045f, numSamples);
    }
   #endif
}

//==============================================================================
float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * (1.0f / 20.0f));
}

float gainToDb (float gain) noexcept
{
    return (gain > 0.0f) ? 20.0f * std::log10 (gain) : -100.0f;
}

static const float volScaleFactor = 20.0f;

float decibelsToVolumeFaderPosition (float db) noexcept
{
    return (db > -100.0f) ? std::exp ((db - 6.0f) * (1.0f / volScaleFactor)) : 0.0f;
}

float volumeFaderPositionToDB (float pos) noexcept
{
    return (pos > 0.0f) ? (volScaleFactor * std::log (pos)) + 6.0f : -100.0f;
}

float volumeFaderPositionToGain (float pos) noexcept
{
    return (pos > 0.0f) ? std::pow (10.0f, ((volScaleFactor * logf (pos)) + 6.0f) * (1.0f / 20.0f)) : 0.0f;
}

float gainToVolumeFaderPosition (float gain) noexcept
{
    return (gain > 0.0f) ? std::exp (((20.0f * std::log10 (gain)) - 6.0f) * (1.0f / volScaleFactor)) : 0.0f;
}

juce::String gainToDbString (float gain, float infLevel, int decPlaces)
{
    return juce::Decibels::toString (gainToDb (gain), decPlaces, infLevel);
}

float dbStringToDb (const juce::String& dbStr)
{
    if (dbStr.contains ("INF"))
        return -100.0f;

    return dbStr.retainCharacters ("0123456789.-").getFloatValue();
}

float dbStringToGain (const juce::String& dbStr)
{
    if (dbStr.contains ("INF"))
        return 0.0f;

    return dbToGain (dbStringToDb (dbStr));
}

juce::String getPanString (float pan)
{
    if (std::abs (pan) < 0.001f)
        return TRANS("Centre");

    const juce::String s (pan, 3);

    if (pan < 0.0f)
        return s + " " + TRANS("Left");

    return "+" + s + " " + TRANS("Right");
}

juce::String getSemitonesAsString (double semitones)
{
    juce::String t;

    if (semitones > 0)
        t << "+";

    if (std::abs (semitones - (int) semitones) < 0.01)
        t << (int) semitones;
    else
        t << juce::String (semitones, 2);

    if (std::abs (semitones) != 1)
        return TRANS("33 semitones").replace ("33", t);

    return TRANS("1 semitone").replace ("1", t);
}

static bool isNotSilent (float v) noexcept
{
    const float zeroThresh = 1.0f / 8000.0f;
    return v < -zeroThresh || v > zeroThresh;
}

bool isAudioDataAlmostSilent (const float* data, int num)
{
    return ! (isNotSilent (data[0])
               || isNotSilent (data[num / 2])
               || isNotSilent (getAudioDataMagnitude (data, num)));
}

float getAudioDataMagnitude (const float* data, int num)
{
    auto range = juce::FloatVectorOperations::findMinAndMax (data, num);

    return std::max (std::abs (range.getStart()),
                     std::abs (range.getEnd()));
}

//==============================================================================
void getGainsFromVolumeFaderPositionAndPan (float volSliderPos, float pan, const PanLaw panLaw,
                                            float& leftGain, float& rightGain) noexcept
{
    const float gain = volumeFaderPositionToGain (volSliderPos);

    if (panLaw == PanLawLinear)
    {
        const float pv = pan * gain;

        leftGain  = gain - pv;
        rightGain = gain + pv;
    }
    else
    {
        const float halfPi = juce::MathConstants<float>::pi * 0.5f;

        pan = (pan + 1.0f) * 0.5f; // Scale to range (0.0f, 1.0f) from (-1.0f, 1.0f) for simplicity

        leftGain    = std::sin ((1.0f - pan) * halfPi);
        rightGain   = std::sin (pan * halfPi);

        float ratio = 1.0f;

        switch (panLaw)
        {
            case PanLaw3dBCenter:       ratio = 1.0f;         break;
            case PanLaw2point5dBCenter: ratio = 2.5f / 3.0f;  break;
            case PanLaw4point5dBCenter: ratio = 1.5f;         break;
            case PanLaw6dBCenter:       ratio = 2.0f;         break;

            case PanLawDefault:
            case PanLawLinear:
                jassertfalse; // should have alread been handled?
                break;
                
            default:
                jassertfalse; // New pan law?
                break;
        }

        leftGain  = std::pow (leftGain,  ratio) * gain;
        rightGain = std::pow (rightGain, ratio) * gain;
    }
}

static PanLaw defaultPanLaw = PanLawLinear;

PanLaw getDefaultPanLaw() noexcept
{
    return defaultPanLaw;
}

void setDefaultPanLaw (const PanLaw panLaw)
{
    defaultPanLaw = panLaw;
}

juce::StringArray getPanLawChoices (bool includeDefault) noexcept
{
    juce::StringArray s;

    if (includeDefault)
        s.add ("(" + TRANS("Use Default") + ")");

    s.add (TRANS("Linear"));
    s.add (TRANS("-2.5 dB Center"));
    s.add (TRANS("-3.0 dB Center"));
    s.add (TRANS("-4.5 dB Center"));
    s.add (TRANS("-6.0 dB Center"));

    return s;
}

//==============================================================================
void convertIntsToFloats (juce::AudioBuffer<float>& buffer)
{
    for (int i = buffer.getNumChannels(); --i >= 0;)
    {
        float* d = buffer.getWritePointer (i);
        juce::FloatVectorOperations::convertFixedToFloat (d, (const int*) d, 1.0f / 0x7fffffff, buffer.getNumSamples());
    }
}

void convertFloatsToInts (juce::AudioBuffer<float>& buffer)
{
    for (int j = buffer.getNumChannels(); --j >= 0;)
    {
        int* d = (int*) buffer.getWritePointer(j);

        for (int i = buffer.getNumSamples(); --i >= 0;)
        {
            const double samp = *(const float*) d;

            if (samp <= -1.0)      *d++ = std::numeric_limits<int>::min();
            else if (samp >= 1.0)  *d++ = std::numeric_limits<int>::max();
            else                   *d++ = juce::roundToInt (std::numeric_limits<int>::max() * samp);
        }
    }
}

//==============================================================================
struct OneChannelDestination
{
    float* channel0;

    inline void apply (float value) noexcept
    {
        *channel0++ *= value;
    }
};

struct TwoChannelDestination
{
    float* channel0;
    float* channel1;

    inline void apply (float value) noexcept
    {
        *channel0++ *= value;
        *channel1++ *= value;
    }
};

void AudioFadeCurve::applyCrossfadeSection (juce::AudioBuffer<float>& buffer,
                                            int channel, int startSample, int numSamples,
                                            AudioFadeCurve::Type type, float startAlpha, float endAlpha)
{
    jassert (startSample >= 0 && startSample + numSamples <= buffer.getNumSamples());

    if (! buffer.hasBeenCleared())
    {
        OneChannelDestination d = { buffer.getWritePointer (channel, startSample) };
        AudioFadeCurve::renderBlockForType (d, numSamples, startAlpha, endAlpha, type);
    }
}

void AudioFadeCurve::applyCrossfadeSection (juce::AudioBuffer<float>& buffer,
                                            int startSample, int numSamples, AudioFadeCurve::Type type,
                                            float startAlpha, float endAlpha)
{
    jassert (startSample >= 0 && startSample + numSamples <= buffer.getNumSamples() && numSamples > 0);

    if (! buffer.hasBeenCleared())
    {
        if (buffer.getNumChannels() == 2)
        {
            TwoChannelDestination d = { buffer.getWritePointer (0, startSample),
                                        buffer.getWritePointer (1, startSample) };

            AudioFadeCurve::renderBlockForType (d, numSamples, startAlpha, endAlpha, type);
        }
        else
        {
            for (int i = buffer.getNumChannels(); --i >= 0;)
                applyCrossfadeSection (buffer, i, startSample, numSamples, type, startAlpha, endAlpha);
        }
    }
}

struct AddingFadeApplier
{
    float* dest;
    const float* src;

    inline void apply (float value) noexcept
    {
        *dest++ += *src++ * value;
    }
};

void AudioFadeCurve::addWithCrossfade (juce::AudioBuffer<float>& dest,
                                       const juce::AudioBuffer<float>& src,
                                       int destChannel, int destStartIndex,
                                       int sourceChannel, int sourceStartIndex,
                                       const int numSamples,
                                       const AudioFadeCurve::Type type,
                                       const float startAlpha,
                                       const float endAlpha)
{
    if (! src.hasBeenCleared())
    {
        if (startAlpha == endAlpha)
        {
            dest.addFrom (destChannel, destStartIndex, src, sourceChannel, sourceStartIndex, numSamples, endAlpha);
        }
        else
        {
            AddingFadeApplier d = { dest.getWritePointer (destChannel, destStartIndex),
                                    src.getReadPointer (sourceChannel, sourceStartIndex) };

            AudioFadeCurve::renderBlockForType (d, numSamples, startAlpha, endAlpha, type);
        }
    }
}

void AudioFadeCurve::drawFadeCurve (juce::Graphics& g, const AudioFadeCurve::Type fadeType,
                                    float x1, float x2, float top, float bottom, juce::Rectangle<int> clip)
{
    const float fadeLineThickness = 1.5f;

    juce::Path s;

    if (fadeType == AudioFadeCurve::linear)
    {
        s.startNewSubPath (x1, bottom);
        s.lineTo (x2, top);
    }
    else
    {
        float left  = (clip.getX() & ~3) - 3.0f;
        float right = clip.getRight() + 5.0f;

        left  = std::max (left,  std::min (x1, x2));
        right = std::min (right, std::max (x1, x2));

        s.startNewSubPath (left, x1 < x2 ? bottom : top);

        for (float x = left; x < right; x += 3.0f)
            s.lineTo (x, bottom - (bottom - top) * AudioFadeCurve::alphaToGainForType (fadeType, (x - x1) / (x2 - x1)));

        s.lineTo (right, x1 > x2 ? bottom : top);
    }

    g.strokePath (s, juce::PathStrokeType (fadeLineThickness));

    s.lineTo (x1, top);
    s.closeSubPath();

    g.setOpacity (0.3f);
    g.fillPath (s);
}

//==============================================================================
struct AudioScratchBuffer::Buffer
{
    juce::AudioBuffer<float> buffer { 2, 41000 };
    std::atomic<bool> isFree { true };
};

struct AudioScratchBuffer::BufferList   : private juce::DeletedAtShutdown
{
    BufferList()
    {
        for (int i = 8; --i >= 0;)
            buffers.add (new Buffer());
    }

    ~BufferList()
    {
        clearSingletonInstance();
    }

    JUCE_DECLARE_SINGLETON (BufferList, false)

    Buffer* get()
    {
        {
            const juce::ScopedLock sl (lock);

            for (auto b : buffers)
            {
                if (b->isFree)
                {
                    b->isFree = false;
                    return b;
                }
            }
        }

        auto newBuffer = new Buffer();
        newBuffer->isFree = false;

        const juce::ScopedLock sl (lock);
        buffers.add (newBuffer);
        return newBuffer;
    }

    juce::CriticalSection lock;
    juce::OwnedArray<Buffer> buffers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BufferList)
};

JUCE_IMPLEMENT_SINGLETON (AudioScratchBuffer::BufferList)

AudioScratchBuffer::AudioScratchBuffer (int numChans, int numSamples)
    : allocatedBuffer (BufferList::getInstance()->get()),
      buffer (allocatedBuffer->buffer)
{
    buffer.setSize (numChans, numSamples, false, false, true);
}

AudioScratchBuffer::AudioScratchBuffer (const juce::AudioBuffer<float>& srcBuffer)
  : allocatedBuffer (BufferList::getInstance()->get()),
    buffer (allocatedBuffer->buffer)
{
    const int chans = srcBuffer.getNumChannels();
    const int samps = srcBuffer.getNumSamples();

    buffer.setSize (chans, samps, false, false, true);

    for (int i = 0; i < chans; ++i)
        buffer.copyFrom (i, 0, srcBuffer.getReadPointer (i), samps);
}

AudioScratchBuffer::~AudioScratchBuffer() noexcept
{
    allocatedBuffer->isFree = true;
}

void AudioScratchBuffer::initialise()
{
    BufferList::getInstance();
}


//==============================================================================
//==============================================================================
#if TRACKTION_UNIT_TESTS

class PanLawTests : public juce::UnitTest
{
public:
    PanLawTests() : juce::UnitTest ("PanLaw", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        // N.B. Linear pan boosts L/R signals when panned (up to +6dB)
        // Equal power modes are 0dB hard L/R

        beginTest ("Linear");
        {
            testPanLaw (PanLawLinear, 0.0f, 0.0f, 0.0f, 0.0f);  // Zero
            testPanLaw (PanLawLinear, 1.0f, 0.0f, 1.0f, 1.0f);  // Unity
            testPanLaw (PanLawLinear, 1.0f, -1.0f, 2.0f, 0.0f); // Hard left
            testPanLaw (PanLawLinear, 1.0f, 1.0f, 0.0f, 2.0f);  // Hard right

            testPanLaw (PanLawLinear, 0.5f, 0.0f, 0.5f, 0.5f);  // Centre -6dB
            testPanLaw (PanLawLinear, 0.5f, -1.0f, 1.0f, 0.0f); // Hard left -6dB
            testPanLaw (PanLawLinear, 0.5f, 1.0f, 0.0f, 1.0f);  // Hard right -6dB

            testPanLaw (PanLawLinear, 1.0f, -0.5f, 1.5f, 0.5f); // 1/2 left
            testPanLaw (PanLawLinear, 1.0f, 0.5f, 0.5f, 1.5f);  // 1 right
        }
        
        beginTest ("-2.5dB");
        {
            const auto centreGain = juce::Decibels::decibelsToGain (-2.5f);
            testPanLaw (PanLaw2point5dBCenter, 0.0f, 0.0f, 0.0f, 0.0f);                 // Zero
            testPanLaw (PanLaw2point5dBCenter, 1.0f, 0.0f, centreGain, centreGain);     // Unity
            testPanLaw (PanLaw2point5dBCenter, 1.0f, -1.0f, 1.0f, 0.0f);                // Hard left
            testPanLaw (PanLaw2point5dBCenter, 1.0f, 1.0f, 0.0f, 1.0f);                 // Hard right
        }

        beginTest ("-3dB");
        {
            const auto centreGain = juce::Decibels::decibelsToGain (-3.0f);
            testPanLaw (PanLaw3dBCenter, 0.0f, 0.0f, 0.0f, 0.0f);                 // Zero
            testPanLaw (PanLaw3dBCenter, 1.0f, 0.0f, centreGain, centreGain);     // Unity
            testPanLaw (PanLaw3dBCenter, 1.0f, -1.0f, 1.0f, 0.0f);                // Hard left
            testPanLaw (PanLaw3dBCenter, 1.0f, 1.0f, 0.0f, 1.0f);                 // Hard right
        }

        beginTest ("-4.5dB");
        {
            const auto centreGain = juce::Decibels::decibelsToGain (-4.5f);
            testPanLaw (PanLaw4point5dBCenter, 0.0f, 0.0f, 0.0f, 0.0f);             // Zero
            testPanLaw (PanLaw4point5dBCenter, 1.0f, 0.0f, centreGain, centreGain); // Unity
            testPanLaw (PanLaw4point5dBCenter, 1.0f, -1.0f, 1.0f, 0.0f);            // Hard left
            testPanLaw (PanLaw4point5dBCenter, 1.0f, 1.0f, 0.0f, 1.0f);             // Hard right
        }

        beginTest ("-6dB");
        {
            const auto centreGain = juce::Decibels::decibelsToGain (-6.0f);
            testPanLaw (PanLaw6dBCenter, 0.0f, 0.0f, 0.0f, 0.0f);               // Zero
            testPanLaw (PanLaw6dBCenter, 1.0f, 0.0f, centreGain, centreGain);   // Unity
            testPanLaw (PanLaw6dBCenter, 1.0f, -1.0f, 1.0f, 0.0f);              // Hard left
            testPanLaw (PanLaw6dBCenter, 1.0f, 1.0f, 0.0f, 1.0f);               // Hard right
        }
    }

private:
    void testPanLaw (PanLaw pl, float gain, float pan,
                     float expectedLeftGain, float expectedRightGain)
    {
        expectGreaterOrEqual (gain, 0.0f);
        expectGreaterOrEqual (pan, -1.0f);
        expectLessOrEqual (pan, 1.0f);
        
        float leftGain = 0.0;
        float rightGain = 0.0;
        getGainsFromVolumeFaderPositionAndPan (gainToVolumeFaderPosition (gain), pan, pl,
                                               leftGain, rightGain);
        expectWithinAbsoluteError (leftGain, expectedLeftGain, 0.01f);
        expectWithinAbsoluteError (rightGain, expectedRightGain, 0.01f);
    }
};

static PanLawTests panLawTests;

#endif // TRACKTION_UNIT_TESTS

}} // namespace tracktion { inline namespace engine
