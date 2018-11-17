/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if JUCE_WINDOWS && TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 #pragma comment (lib, "libelastiqueProV3.lib")
 #pragma comment (lib, "libelastiqueEfficientV3.lib")
 #pragma comment (lib, "libelastiqueSOLOIST.lib")
 #pragma comment (lib, "libResample.lib")
 #pragma comment (lib, "libzplVecLib.lib")

 #pragma comment (lib, "ippcore_l.lib")
 #pragma comment (lib, "ipps_l.lib")
 #pragma comment (lib, "ippvm_l.lib")
#endif

TimeStretcher::ElastiqueProOptions::ElastiqueProOptions (const String& string)
{
    if (string.isEmpty())
        return;

    if (string.startsWith ("1/"))
    {
        StringArray tokens;
        tokens.addTokens (string, "/", {});

        if (tokens.size() == 5)
        {
            midSideStereo        = tokens[1].getIntValue() != 0;
            syncTimeStrPitchShft = tokens[2].getIntValue() != 0;
            preserveFormants     = tokens[3].getIntValue() != 0;
            envelopeOrder        = tokens[4].getIntValue();
            return;
        }
    }

    jassertfalse; //unknown string format
}

String TimeStretcher::ElastiqueProOptions::toString() const
{
    // version / midside / sync / preserve / order
    return String::formatted ("1/%d/%d/%d/%d",
                              midSideStereo        ? 1 : 0,
                              syncTimeStrPitchShft ? 1 : 0,
                              preserveFormants     ? 1 : 0,
                              envelopeOrder);
}

bool TimeStretcher::ElastiqueProOptions::operator== (const ElastiqueProOptions& other) const
{
    return midSideStereo        == other.midSideStereo
        && syncTimeStrPitchShft == other.syncTimeStrPitchShft
        && preserveFormants     == other.preserveFormants
        && envelopeOrder        == other.envelopeOrder;
}

bool TimeStretcher::ElastiqueProOptions::operator!= (const ElastiqueProOptions& other) const
{
    return ! operator== (other);
}

//==============================================================================
struct TimeStretcher::Stretcher
{
    virtual ~Stretcher() {}

    virtual bool isOk() const = 0;
    virtual void reset() = 0;
    virtual bool setSpeedAndPitch (float speedRatio, float semitonesUp) = 0;
    virtual int getFramesNeeded() const = 0;
    virtual int getMaxFramesNeeded() const = 0;
    virtual void processData (const float* const* inChannels, int numSamples, float* const* outChannels) = 0;
    virtual void flush (float* const* outChannels) = 0;
};

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE

struct ElastiqueStretcher  : public TimeStretcher::Stretcher
{
    ElastiqueStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels,
                        TimeStretcher::Mode mode, TimeStretcher::ElastiqueProOptions options, float minFactor)
        : elastiqueMode (mode),
          elastiqueProOptions (options)
    {
        CRASH_TRACER
        int res = CElastiqueProV3If::CreateInstance (elastique, samplesPerBlock, numChannels, (float) sourceSampleRate,
                                                     getElastiqueMode (mode), minFactor);
        jassert (res == 0); juce::ignoreUnused (res);

        if (elastique == nullptr)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("Cannot create Elastique");
        }
        else
        {
            // This must be called first, before any other functions and can not
            // be called again
            maxFramesNeeded = elastique->GetMaxFramesNeeded();

            if (mode == TimeStretcher::elastiquePro)
                elastique->SetStereoInputMode (elastiqueProOptions.midSideStereo ? CElastiqueProV3If::kMSMode : CElastiqueProV3If::kPlainStereoMode);
        }
    }

    ~ElastiqueStretcher()
    {
        if (elastique != nullptr)
            CElastiqueProV3If::DestroyInstance (elastique);
    }

    bool isOk() const override      { return elastique != nullptr; }
    void reset() override           { elastique->Reset(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        float pitch = jlimit (0.25f, 4.0f, Pitch::semitonesToRatio (semitonesUp));
        bool sync  = (elastiqueMode == TimeStretcher::elastiquePro) ? elastiqueProOptions.syncTimeStrPitchShft : false;
        int r = elastique->SetStretchPitchQFactor (speedRatio, pitch, sync);
        jassert (r == 0); juce::ignoreUnused (r);

        if (elastiqueMode == TimeStretcher::elastiquePro && elastiqueProOptions.preserveFormants)
        {
            elastique->SetEnvelopeFactor (pitch);
            elastique->SetEnvelopeOrder (elastiqueProOptions.envelopeOrder);
        }

        return r == 0;
    }

    int getFramesNeeded() const override
    {
        const int framesNeeded = elastique->GetFramesNeeded();
        jassert (framesNeeded <= maxFramesNeeded);
        return framesNeeded;
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    void processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        int err = elastique->ProcessData ((float**) inChannels, numSamples, (float**) outChannels);
        jassert (err == 0); juce::ignoreUnused (err);
    }

    void flush (float* const* outChannels) override
    {
        elastique->FlushBuffer ((float**) outChannels);
    }

private:
    CElastiqueProV3If* elastique = nullptr;
    TimeStretcher::Mode elastiqueMode;
    TimeStretcher::ElastiqueProOptions elastiqueProOptions;

    int maxFramesNeeded;

    static CElastiqueProV3If::ElastiqueMode_t getElastiqueMode (TimeStretcher::Mode mode)
    {
        switch (mode)
        {
            case TimeStretcher::elastiqueEfficient:     return CElastiqueProV3If::kV3Eff;
            case TimeStretcher::elastiqueMobile:        return CElastiqueProV3If::kV3mobile;
            case TimeStretcher::elastiqueMonophonic:    return CElastiqueProV3If::kV3Monophonic;
            default:                                    return CElastiqueProV3If::kV3Pro;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElastiqueStretcher)
};
#endif

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH

#include "../3rd_party/soundtouch/include/SoundTouch.h"

struct SoundTouchStretcher  : public TimeStretcher::Stretcher,
                              private soundtouch::SoundTouch
{
    SoundTouchStretcher (double sourceSampleRate, int samplesPerBlock,
                         int chans, bool betterQuality)
        : numChannels (chans), samplesPerOutputBuffer (samplesPerBlock)
    {
        CRASH_TRACER
        setChannels (static_cast<unsigned int> (numChannels));
        setSampleRate (static_cast<unsigned int> (sourceSampleRate));

        if (betterQuality)
        {
            setSetting (SETTING_USE_AA_FILTER, 1);
            setSetting (SETTING_AA_FILTER_LENGTH, 64);
            setSetting (SETTING_USE_QUICKSEEK, 0);
            setSetting (SETTING_SEQUENCE_MS, 60);
            setSetting (SETTING_SEEKWINDOW_MS, 25);
        }
    }

    bool isOk() const override      { return true; }
    void reset() override           { clear(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        setTempo (1.0f / speedRatio);
        setPitchSemiTones (semitonesUp);
        return true;
    }

    int getFramesNeeded() const override
    {
        int ready = (int) numSamples();

        if (ready < samplesPerOutputBuffer)
            return getSetting (SETTING_INITIAL_LATENCY) + samplesPerOutputBuffer;

        return 0;
    }

    int getMaxFramesNeeded() const override
    {
        return getSetting (SETTING_INITIAL_LATENCY);
    }

    void processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        int offset = 0;
        int numNeeded = samplesPerOutputBuffer;
        bool hasWritten = false;

        while (numNeeded > 0)
        {
            const int numDone = readOutput (outChannels, offset, numNeeded);

            if (numDone == 0)
            {
                if (hasWritten)
                {
                    jassertfalse;
                    break;
                }

                hasWritten = true;
                writeInput (inChannels, numSamples);
            }

            offset += numDone;
            numNeeded -= numDone;
        }

        if (numNeeded > 0)
            for (int chan = 0; chan < numChannels; ++chan)
                FloatVectorOperations::clear (outChannels[chan] + offset, numNeeded);
    }

    void flush (float* const* /*outChannels*/) override
    {
    }

private:
    int numChannels = 0, samplesPerOutputBuffer = 0;

    int readOutput (float* const* outChannels, int offset, int numNeeded)
    {
        float* interleaved = ptrBegin();
        const int num = jmin (numNeeded, (int) numSamples());

        for (int chan = 0; chan < numChannels; ++chan)
        {
            const float* src = interleaved + chan;
            float* dest = outChannels[chan] + offset;

            for (int i = 0; i < num; ++i)
            {
                dest[i] = *src;
                src += numChannels;
            }
        }

        receiveSamples (static_cast<unsigned int> (num));
        return num;
    }

    void writeInput (const float* const* inChannels, int numSamples)
    {
        if (numChannels > 1)
        {
            AudioScratchBuffer scratch (1, numSamples * numChannels);
            float* interleaved = scratch.buffer.getWritePointer(0);

            for (int chan = 0; chan < numChannels; ++chan)
            {
                float* dest = interleaved + chan;
                const float* src = inChannels[chan];

                for (int i = 0; i < numSamples; ++i)
                {
                    *dest = src[i];
                    dest += numChannels;
                }
            }

            putSamples (interleaved, static_cast<unsigned int> (numSamples));
        }
        else
        {
            putSamples (inChannels[0], static_cast<unsigned int> (numSamples));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundTouchStretcher)
};
#endif

//==============================================================================
TimeStretcher::TimeStretcher() {}
TimeStretcher::~TimeStretcher() {}

static String getMelodyne()             { return "Melodyne"; }
static String getElastiquePro()         { return "Elastique (" + TRANS("Pro") + ")"; }
static String getElastiqueEfficeint()   { return "Elastique (" + TRANS("Efficient") + ")"; }
static String getElastiqueMobile()      { return "Elastique (" + TRANS("Mobile") + ")"; }
static String getElastiqueMono()        { return "Elastique (" + TRANS("Monophonic") + ")"; }
static String getSoundTouchNormal()     { return "SoundTouch (" + TRANS("Normal") + ")"; }
static String getSoundTouchBetter()     { return "SoundTouch (" + TRANS("Better") + ")"; }

TimeStretcher::Mode TimeStretcher::checkModeIsAvailable (Mode m)
{
    switch (m)
    {
        case elastiqueTransient:
        case elastiqueTonal:
       #if ! TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
       #endif
       #if ! TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
       #endif
            return defaultMode;
       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
            return m;
       #endif
       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
            return m;
       #endif
        default:
            return m;
    }
}

StringArray TimeStretcher::getPossibleModes (Engine& e, bool excludeMelodyne)
{
    StringArray s;

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    s.add (getElastiquePro());
    s.add (getElastiqueEfficeint());
    s.add (getElastiqueMobile());
    s.add (getElastiqueMono());
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    s.add (getSoundTouchNormal());
    s.add (getSoundTouchBetter());
   #endif

   #if TRACKTION_ENABLE_ARA
    if (! excludeMelodyne && e.getPluginManager().getARACompatiblePlugDescriptions().size() > 0)
        s.add (getMelodyne());
   #else
    juce::ignoreUnused (e, excludeMelodyne);
   #endif

    return s;
}

TimeStretcher::Mode TimeStretcher::getModeFromName (Engine& e, const String& name)
{
   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    if (name == getElastiquePro())          return elastiquePro;
    if (name == getElastiqueEfficeint())    return elastiqueEfficient;
    if (name == getElastiqueMobile())       return elastiqueMobile;
    if (name == getElastiqueMono())         return elastiqueMonophonic;
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    if (name == getSoundTouchNormal())      return soundtouchNormal;
    if (name == getSoundTouchBetter())      return soundtouchBetter;
   #endif

   #if TRACKTION_ENABLE_ARA
    if (name == getMelodyne())              return melodyne;
   #endif

    return getPossibleModes (e, false).contains (name) ? defaultMode
                                                       : disabled;
}

String TimeStretcher::getNameOfMode (const Mode mode)
{
    switch (mode)
    {
        case elastiquePro:          return getElastiquePro();
        case elastiqueEfficient:    return getElastiqueEfficeint();
        case elastiqueMobile:       return getElastiqueMobile();
        case elastiqueMonophonic:   return getElastiqueMono();
        case soundtouchNormal:      return getSoundTouchNormal();
        case soundtouchBetter:      return getSoundTouchBetter();
        case melodyne:              return getMelodyne();
        default:                    jassertfalse; break;
    }

    return {};
}

bool TimeStretcher::isMelodyne (Mode mode)
{
   #if TRACKTION_ENABLE_ARA
    return mode == melodyne;
   #else
    juce::ignoreUnused (mode);
    return false;
   #endif
}

bool TimeStretcher::isInitialised() const
{
    return stretcher != nullptr;
}

void TimeStretcher::initialise (double sourceSampleRate, int samplesPerBlock,
                                int numChannels, Mode mode, ElastiqueProOptions options, bool realtime)
{
    juce::ignoreUnused (sourceSampleRate, numChannels, mode, options, realtime);
    jassert (! isMelodyne (mode));

    samplesPerBlockRequested = samplesPerBlock;

    CRASH_TRACER
    jassert (stretcher == nullptr);

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE || TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    switch (mode)
    {
       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
            stretcher = new ElastiqueStretcher (sourceSampleRate, samplesPerBlock, numChannels,
                                                mode, options, realtime ? 0.25f : 0.1f);
            break;
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
            juce::ignoreUnused (options, realtime);
            stretcher = new SoundTouchStretcher (sourceSampleRate, samplesPerBlock, numChannels,
                                                 mode == soundtouchBetter);
            break;
       #endif

        default:
            break;
    }
   #endif

    if (stretcher != nullptr)
        if (! stretcher->isOk())
            stretcher = nullptr;
}

bool TimeStretcher::canProcessFor (const Mode mode)
{
    return mode != TimeStretcher::disabled;
}

void TimeStretcher::reset()
{
    if (stretcher != nullptr)
        stretcher->reset();
}

bool TimeStretcher::setSpeedAndPitch (float speedRatio, float semitonesUp)
{
    if (stretcher != nullptr)
        return stretcher->setSpeedAndPitch (speedRatio, semitonesUp);

    return false;
}

int TimeStretcher::getFramesNeeded() const
{
    if (stretcher != nullptr)
        return stretcher->getFramesNeeded();

    return 0;
}

int TimeStretcher::getMaxFramesNeeded() const
{
    if (stretcher != nullptr)
        return stretcher->getMaxFramesNeeded();

    return 0;
}

void TimeStretcher::processData (const float* const* inChannels, int numSamples, float* const* outChannels)
{
    if (stretcher != nullptr)
        stretcher->processData (inChannels, numSamples, outChannels);
}

void TimeStretcher::processData (AudioFifo& inFifo, int numSamples, AudioFifo& outFifo)
{
    AudioScratchBuffer inBuffer (inFifo.getNumChannels(), numSamples);
    AudioScratchBuffer outBuffer (outFifo.getNumChannels(), samplesPerBlockRequested);

    inFifo.read (inBuffer.buffer, 0, numSamples);

    if (stretcher != nullptr)
    {
        const float* const* inChannels = inBuffer.buffer.getArrayOfReadPointers();
        float* const* outChannels = outBuffer.buffer.getArrayOfWritePointers();

        stretcher->processData (inChannels, numSamples, outChannels);
    }

    outFifo.write (outBuffer.buffer, 0, samplesPerBlockRequested);
}

void TimeStretcher::flush (float* const* outChannels)
{
    if (stretcher != nullptr)
        stretcher->flush (outChannels);
}
