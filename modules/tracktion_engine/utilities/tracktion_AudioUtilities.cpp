/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
            FloatVectorOperations::fill (buffer.getWritePointer (i, start), 0.000000045f, numSamples);
    }
    else
    {
        for (int i = buffer.getNumChannels(); --i >= 0;)
            FloatVectorOperations::add (buffer.getWritePointer (i, start), 0.000000045f, numSamples);
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

String gainToDbString (float gain, float infLevel, int decPlaces)
{
    return Decibels::toString (gainToDb (gain), decPlaces, infLevel);
}

float dbStringToDb (const String& dbStr)
{
    if (dbStr.contains ("INF"))
        return -100.0f;

    return dbStr.retainCharacters ("0123456789.-").getFloatValue();
}

float dbStringToGain (const String& dbStr)
{
    if (dbStr.contains ("INF"))
        return 0.0f;

    return dbToGain (dbStringToDb (dbStr));
}

String getPanString (float pan)
{
    if (std::abs (pan) < 0.001f)
        return TRANS("Centre");

    const String s (pan, 3);

    if (pan < 0.0f)
        return s + " " + TRANS("Left");

    return "+" + s + " " + TRANS("Right");
}

String getSemitonesAsString (double semitones)
{
    String t;

    if (semitones > 0)
        t << "+";

    if (std::abs (semitones - (int) semitones) < 0.01)
        t << (int) semitones;
    else
        t << String (semitones, 2);

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
    auto range = FloatVectorOperations::findMinAndMax (data, num);

    return jmax (std::abs (range.getStart()),
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
        const float halfPi = juce::float_Pi * 0.5f;

        pan = (pan + 1.0f) * 0.5f; // Scale to range (0.0f, 1.0f) from (-1.0f, 1.0f) for simplicity

        leftGain    = std::sin ((1.0f - pan) * halfPi);
        rightGain   = std::sin (pan * halfPi);

        float ratio;

        switch (panLaw)
        {
            case PanLaw3dBCenter:       ratio = 1.0f;         break;
            case PanLaw2point5dBCenter: ratio = 2.5f / 3.0f;  break;
            case PanLaw4point5dBCenter: ratio = 1.5f;         break;
            case PanLaw6dBCenter:       ratio = 2.0f;         break;

            default:
                jassertfalse; // New pan law?
                ratio = 1.0f;
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

StringArray getPanLawChoices (bool includeDefault) noexcept
{
    StringArray s;

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
        FloatVectorOperations::convertFixedToFloat (d, (const int*) d, 1.0f / 0x7fffffff, buffer.getNumSamples());
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
            else                   *d++ = roundToInt (std::numeric_limits<int>::max() * samp);
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

void AudioFadeCurve::drawFadeCurve (Graphics& g, const AudioFadeCurve::Type fadeType,
                                    float x1, float x2, float top, float bottom, Rectangle<int> clip)
{
    const float fadeLineThickness = 1.5f;

    Path s;

    if (fadeType == AudioFadeCurve::linear)
    {
        s.startNewSubPath (x1, bottom);
        s.lineTo (x2, top);
    }
    else
    {
        float left  = (clip.getX() & ~3) - 3.0f;
        float right = clip.getRight() + 5.0f;

        left  = jmax (left,  jmin (x1, x2));
        right = jmin (right, jmax (x1, x2));

        s.startNewSubPath (left, x1 < x2 ? bottom : top);

        for (float x = left; x < right; x += 3.0f)
            s.lineTo (x, bottom - (bottom - top) * AudioFadeCurve::alphaToGainForType (fadeType, (x - x1) / (x2 - x1)));

        s.lineTo (right, x1 > x2 ? bottom : top);
    }

    g.strokePath (s, PathStrokeType (fadeLineThickness));

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

struct AudioScratchBuffer::BufferList   : private DeletedAtShutdown
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
            const ScopedLock sl (lock);

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

        const ScopedLock sl (lock);
        buffers.add (newBuffer);
        return newBuffer;
    }

    CriticalSection lock;
    OwnedArray<Buffer> buffers;

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

//==============================================================================
ChannelIndex::ChannelIndex (int index, juce::AudioChannelSet::ChannelType c) noexcept
    : indexInDevice (index), channel (c)
{
}

bool ChannelIndex::operator== (const ChannelIndex& other) const
{
    return indexInDevice == other.indexInDevice && channel == other.channel;
}

bool ChannelIndex::operator != (const ChannelIndex& other) const
{
    return indexInDevice != other.indexInDevice || channel != other.channel;
}

String createDescriptionOfChannels (const std::vector<ChannelIndex>& channels)
{
    String desc;

    for (const auto& ci : channels)
    {
        if (desc.isNotEmpty())
            desc << ", ";

        desc << String (ci.indexInDevice) + " (" + AudioChannelSet::getAbbreviatedChannelTypeName (ci.channel) + ")";
    }

    return desc;
}

juce::AudioChannelSet createChannelSet (const std::vector<ChannelIndex>& channels)
{
    juce::AudioChannelSet channelSet;

    for (const auto& ci : channels)
        channelSet.addChannel (ci.channel);

    return channelSet;
}

juce::AudioChannelSet::ChannelType channelTypeFromAbbreviatedName (const juce::String& abbreviatedName)
{
    struct NamedChannelTypeCache
    {
        NamedChannelTypeCache()
        {
            for (int i = 0; i < AudioChannelSet::discreteChannel0; ++i)
            {
                const auto channelType = static_cast<AudioChannelSet::ChannelType> (i);
                const String name (AudioChannelSet::getAbbreviatedChannelTypeName (channelType));

                if (name.isNotEmpty())
                    map[name] = channelType;
            }
        }

        std::map<String, AudioChannelSet::ChannelType> map;
    };

    static NamedChannelTypeCache cache;
    const auto result = cache.map.find (abbreviatedName);

    if (result != cache.map.end())
        return result->second;

    return juce::AudioChannelSet::unknown;
}

juce::AudioChannelSet channelSetFromSpeakerArrangmentString (const juce::String& arrangement)
{
    AudioChannelSet cs;

    for (auto& channel : StringArray::fromTokens (arrangement, false))
    {
        const auto ct = channelTypeFromAbbreviatedName (channel);

        if (ct != AudioChannelSet::unknown)
            cs.addChannel (ct);
    }

    return cs;
}
