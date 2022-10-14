/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if JUCE_WINDOWS && TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 #pragma comment (lib, "libelastiquePro.lib")
 #pragma comment (lib, "libelastiqueEfficient.lib")
 #pragma comment (lib, "libelastiqueSOLOIST.lib")
 #pragma comment (lib, "libResample.lib")
 #pragma comment (lib, "libzplVecLib.lib")
#endif

#if JUCE_WINDOWS && TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE_IPP
 #pragma comment (lib, "ippcore_l.lib")
 #pragma comment (lib, "ipps_l.lib")
 #pragma comment (lib, "ippvm_l.lib")
#endif

#if JUCE_WINDOWS
 #define NOMINMAX
#endif

namespace tracktion { inline namespace engine
{

TimeStretcher::ElastiqueProOptions::ElastiqueProOptions (const juce::String& string)
{
    if (string.isEmpty())
        return;

    if (string.startsWith ("1/"))
    {
        juce::StringArray tokens;
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

juce::String TimeStretcher::ElastiqueProOptions::toString() const
{
    // version / midside / sync / preserve / order
    return juce::String::formatted ("1/%d/%d/%d/%d",
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
    virtual int processData (const float* const* inChannels, int numSamples, float* const* outChannels) = 0;
    virtual int flush (float* const* outChannels) = 0;
};

//==============================================================================
#if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE

struct ElastiqueStretcher  : public TimeStretcher::Stretcher
{
    ElastiqueStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels,
                        TimeStretcher::Mode mode, TimeStretcher::ElastiqueProOptions options, float minFactor)
        : elastiqueMode (mode),
          elastiqueProOptions (options),
          samplesPerOutputBuffer (samplesPerBlock)
    {
        CRASH_TRACER
        jassert (sourceSampleRate > 0.0);
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

    ~ElastiqueStretcher() override
    {
        if (elastique != nullptr)
            CElastiqueProV3If::DestroyInstance (elastique);
    }

    bool isOk() const override      { return elastique != nullptr; }
    void reset() override           { elastique->Reset(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        float pitch = juce::jlimit (0.25f, 4.0f, Pitch::semitonesToRatio (semitonesUp));
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

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        int err = elastique->ProcessData ((float**) inChannels, numSamples, (float**) outChannels);
        jassert (err == 0); juce::ignoreUnused (err);
        return samplesPerOutputBuffer;
    }

    int flush (float* const* outChannels) override
    {
        elastique->FlushBuffer ((float**) outChannels);
        return samplesPerOutputBuffer;
    }

private:
    CElastiqueProV3If* elastique = nullptr;
    TimeStretcher::Mode elastiqueMode;
    TimeStretcher::ElastiqueProOptions elastiqueProOptions;
    const int samplesPerOutputBuffer;
    int maxFramesNeeded;

    static CElastiqueProV3If::ElastiqueMode_t getElastiqueMode (TimeStretcher::Mode mode)
    {
        switch (mode)
        {
            case TimeStretcher::elastiquePro:           return CElastiqueProV3If::kV3Pro;
            case TimeStretcher::elastiqueEfficient:     return CElastiqueProV3If::kV3Eff;
            case TimeStretcher::elastiqueMobile:        return CElastiqueProV3If::kV3mobile;
            case TimeStretcher::elastiqueMonophonic:    return CElastiqueProV3If::kV3Monophonic;
            case TimeStretcher::disabled:               [[ fallthrough ]];
            case TimeStretcher::elastiqueTransient:     [[ fallthrough ]];
            case TimeStretcher::elastiqueTonal:         [[ fallthrough ]];
            case TimeStretcher::soundtouchNormal:       [[ fallthrough ]];
            case TimeStretcher::soundtouchBetter:       [[ fallthrough ]];
            case TimeStretcher::melodyne:               [[ fallthrough ]];
            case TimeStretcher::rubberbandMelodic:      [[ fallthrough ]];
            case TimeStretcher::rubberbandPercussive:   [[ fallthrough ]];
            default:
                jassertfalse;
                return CElastiqueProV3If::kV3Pro;
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
        inputOutputSampleRatio = getInputOutputSampleRatio();
        
        return true;
    }

    int getFramesNeeded() const override
    {
        const int numAvailable = (int) numSamples();
        const int numRequiredForOneBlock = juce::roundToInt (samplesPerOutputBuffer * inputOutputSampleRatio);
        
        return std::max (0, numRequiredForOneBlock - numAvailable);
    }

    int getMaxFramesNeeded() const override
    {
        // This was derived by experimentation
        return 8192;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        CRASH_TRACER
        jassert (numSamples <= getFramesNeeded());
        writeInput (inChannels, numSamples);

        const int numAvailable = (int) soundtouch::SoundTouch::numSamples();
        jassert (numAvailable >= 0);
        
        const int numToRead = std::min (numAvailable, samplesPerOutputBuffer);
        
        if (numToRead > 0)
            return readOutput (outChannels, 0, numToRead);

        return 0;
    }

    int flush (float* const* outChannels) override
    {
        CRASH_TRACER
        if (! hasDoneFinalBlock)
        {
            soundtouch::SoundTouch::flush();
            hasDoneFinalBlock = true;
        }

        const int numAvailable = (int) numSamples();
        const int numToRead = std::min (numAvailable, samplesPerOutputBuffer);
        
        return readOutput (outChannels, 0, numToRead);
    }

private:
    int numChannels = 0, samplesPerOutputBuffer = 0;
    bool hasDoneFinalBlock = false;
    double inputOutputSampleRatio = 1.0;
    
    int readOutput (float* const* outChannels, int offset, int numNeeded)
    {
        float* interleaved = ptrBegin();
        auto num = std::min (numNeeded, (int) numSamples());

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
#if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wextra-semi"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wsign-compare"
 #pragma GCC diagnostic ignored "-Wcast-align"
 #if ! __clang__
  #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #endif
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wpedantic"
 #pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#define WIN32_LEAN_AND_MEAN 1
#define Point CarbonDummyPointName
#define Component CarbonDummyCompName

}} // namespace tracktion { inline namespace engine
#if TRACKTION_BUILD_RUBBERBAND
 #if __has_include(<rubberband/single/RubberBandSingle.cpp>)
  #include <rubberband/single/RubberBandSingle.cpp>
 #elif __has_include("../3rd_party/rubberband/single/RubberBandSingle.cpp")
  #include "../3rd_party/rubberband/single/RubberBandSingle.cpp"
 #else
  #error "TRACKTION_BUILD_RUBBERBAND enabled but not found in the search path!"
 #endif
#else
 #if __has_include(<rubberband/rubberband/RubberBandStretcher.h>)
  #include <rubberband/rubberband/RubberBandStretcher.h>
 #elif __has_include("../3rd_party/rubberband/rubberband/RubberBandStretcher.h")
  #include "../3rd_party/rubberband/rubberband/RubberBandStretcher.h"
 #else
  #error "TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND enabled but not found in the search path!"
 #endif
#endif
namespace tracktion { inline namespace engine {
    
#undef WIN32_LEAN_AND_MEAN
#undef Point
#undef Component

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

struct RubberBandStretcher  : public TimeStretcher::Stretcher
{
    static int getOptionFlags (bool percussive)
    {
        if (percussive)
            return RubberBand::RubberBandStretcher::OptionProcessRealTime
                | RubberBand::RubberBandStretcher::OptionPitchHighConsistency
                | RubberBand::RubberBandStretcher::PercussiveOptions;

        return RubberBand::RubberBandStretcher::OptionProcessRealTime
            | RubberBand::RubberBandStretcher::OptionPitchHighConsistency
            | RubberBand::RubberBandStretcher::OptionWindowShort;
    }
    
    RubberBandStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels, bool percussive)
        : rubberBandStretcher ((size_t) sourceSampleRate, (size_t) numChannels,
                               getOptionFlags (percussive)),
          samplesPerOutputBuffer (samplesPerBlock)
    {
    }

    bool isOk() const override
    {
        return true;
    }

    void reset() override
    {
        rubberBandStretcher.reset();
    }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        const float pitch = juce::jlimit (0.25f, 4.0f, tracktion::engine::Pitch::semitonesToRatio (semitonesUp));

        rubberBandStretcher.setPitchScale (pitch);
        rubberBandStretcher.setTimeRatio (speedRatio);
        
        if (numSamplesToDrop == -1)
        {
            // This is the first speed and pitch change so set up the padding and dropping
            numSamplesToDrop = int (rubberBandStretcher.getLatency());
            int numSamplesToPad = juce::roundToInt (numSamplesToDrop * pitch);
            
            if (numSamplesToPad > 0)
            {
                AudioScratchBuffer scratch (int (rubberBandStretcher.getChannelCount()), samplesPerOutputBuffer);
                scratch.buffer.clear();
                
                while (numSamplesToPad > 0)
                {
                    const int numThisTime = std::min (numSamplesToPad, samplesPerOutputBuffer);
                    rubberBandStretcher.process (scratch.buffer.getArrayOfReadPointers(), numThisTime, false);
                    numSamplesToPad -= numThisTime;
                }
            }
            
            jassert (numSamplesToPad == 0);
        }
        
        const bool r = rubberBandStretcher.getPitchScale() == pitch
                    && rubberBandStretcher.getTimeRatio() == speedRatio;
        jassert (r);
        juce::ignoreUnused (r);

        return r;
    }

    int getFramesNeeded() const override
    {
        return (int) rubberBandStretcher.getSamplesRequired();
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        jassert (numSamples <= getFramesNeeded());
        rubberBandStretcher.process (inChannels, (size_t) numSamples, false);

        // Once there is output, read this in to the output buffer
        int numAvailable = rubberBandStretcher.available();
        jassert (numAvailable >= 0);
        
        if (numSamplesToDrop > 0)
        {
            auto numToDropThisTime = std::min (numSamplesToDrop, std::min (numAvailable, samplesPerOutputBuffer));
            rubberBandStretcher.retrieve (outChannels, (size_t) numToDropThisTime);
            numSamplesToDrop -= numToDropThisTime;
            jassert (numSamplesToDrop >= 0);
            
            numAvailable -= numToDropThisTime;
        }
        
        if (numAvailable > 0)
            return (int) rubberBandStretcher.retrieve (outChannels, (size_t) numAvailable);
        
        return 0;
    }

    int flush (float* const* outChannels) override
    {
        // Empty the FIFO in to the stretcher and mark the last block as final
        if (! hasDoneFinalBlock)
        {
            float* inChannels[32] = { nullptr };
            rubberBandStretcher.process (inChannels, 0, true);
            hasDoneFinalBlock = true;
        }
        
        // Then get the rest of the data out of the stretcher
        const int numAvailable = rubberBandStretcher.available();
        const int numThisBlock = std::min (numAvailable, samplesPerOutputBuffer);
        
        if (numThisBlock > 0)
            return (int) rubberBandStretcher.retrieve (outChannels, (size_t) numThisBlock);

        return 0;
    }

private:
    RubberBand::RubberBandStretcher rubberBandStretcher;
    const int maxFramesNeeded = 8192;
    const int samplesPerOutputBuffer = 0;
    int numSamplesToDrop = -1;
    bool hasDoneFinalBlock = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RubberBandStretcher)
};

#endif // TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND

//==============================================================================
TimeStretcher::TimeStretcher() {}
TimeStretcher::~TimeStretcher() {}

static juce::String getMelodyne()             { return "Melodyne"; }
static juce::String getElastiquePro()         { return "Elastique (" + TRANS("Pro") + ")"; }
static juce::String getElastiqueEfficeint()   { return "Elastique (" + TRANS("Efficient") + ")"; }
static juce::String getElastiqueMobile()      { return "Elastique (" + TRANS("Mobile") + ")"; }
static juce::String getElastiqueMono()        { return "Elastique (" + TRANS("Monophonic") + ")"; }
static juce::String getSoundTouchNormal()     { return "SoundTouch (" + TRANS("Normal") + ")"; }
static juce::String getSoundTouchBetter()     { return "SoundTouch (" + TRANS("Better") + ")"; }
static juce::String getRubberBandMelodic()    { return "RubberBand (" + TRANS("Melodic") + ")"; }
static juce::String getRubberBandPercussive() { return "RubberBand (" + TRANS("Percussive") + ")"; }

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
       #if ! TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
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
       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
            return m;
       #endif
        case disabled:
        case melodyne:
        default:
            return m;
    }
}

juce::StringArray TimeStretcher::getPossibleModes (Engine& e, bool excludeMelodyne)
{
    juce::StringArray s;

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
    
   #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
    s.add (getRubberBandMelodic());
    s.add (getRubberBandPercussive());
   #endif

   #if TRACKTION_ENABLE_ARA
    if (! excludeMelodyne && e.getPluginManager().getARACompatiblePlugDescriptions().size() > 0)
        s.add (getMelodyne());
   #else
    juce::ignoreUnused (e, excludeMelodyne);
   #endif

    return s;
}

TimeStretcher::Mode TimeStretcher::getModeFromName (Engine& e, const juce::String& name)
{
   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    if (name == getElastiquePro())          return elastiquePro;
    if (name == getElastiqueEfficeint())    return elastiqueEfficient;
    if (name == getElastiqueMobile())       return elastiqueMobile;
    if (name == getElastiqueMono())         return elastiqueMonophonic;
   #endif

   #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
     if (name == getRubberBandMelodic())    return rubberbandMelodic;
     if (name == getRubberBandPercussive()) return rubberbandPercussive;
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

juce::String TimeStretcher::getNameOfMode (const Mode mode)
{
    switch (mode)
    {
        case elastiquePro:          return getElastiquePro();
        case elastiqueEfficient:    return getElastiqueEfficeint();
        case elastiqueMobile:       return getElastiqueMobile();
        case elastiqueMonophonic:   return getElastiqueMono();
        case soundtouchNormal:      return getSoundTouchNormal();
        case soundtouchBetter:      return getSoundTouchBetter();
        case rubberbandMelodic:     return getRubberBandMelodic();
        case rubberbandPercussive:  return getRubberBandPercussive();
        case melodyne:              return getMelodyne();
        case disabled:
        case elastiqueTransient:
        case elastiqueTonal:
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

   #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE || TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND || TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    switch (mode)
    {
       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        case elastiquePro:
        case elastiqueEfficient:
        case elastiqueMobile:
        case elastiqueMonophonic:
            stretcher.reset (new ElastiqueStretcher (sourceSampleRate, samplesPerBlock, numChannels,
                                                     mode, options, realtime ? 0.25f : 0.1f));
            break;
       #else
        case elastiquePro:          [[fallthrough]];
        case elastiqueEfficient:    [[fallthrough]];
        case elastiqueMobile:       [[fallthrough]];
        case elastiqueMonophonic:
            break;
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        case soundtouchNormal:
        case soundtouchBetter:
            juce::ignoreUnused (options, realtime);
            stretcher.reset (new SoundTouchStretcher (sourceSampleRate, samplesPerBlock, numChannels,
                                                      mode == soundtouchBetter));
            break;
       #else
        case soundtouchNormal:      [[fallthrough]];
        case soundtouchBetter:
            break;
       #endif
            
       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        case rubberbandMelodic:
        case rubberbandPercussive:
            juce::ignoreUnused (options, realtime);
            stretcher.reset (new tracktion::engine::RubberBandStretcher (sourceSampleRate, samplesPerBlock, numChannels,
                                                                        mode == rubberbandPercussive));
            break;
       #else
        case rubberbandMelodic:     [[fallthrough]];
        case rubberbandPercussive:
            break;
       #endif

        case disabled:              [[fallthrough]];
        case melodyne:              [[fallthrough]];
        case elastiqueTransient:    [[fallthrough]];
        case elastiqueTonal:        [[fallthrough]];
        default:
            break;
    }
   #endif

    if (stretcher != nullptr)
        if (! stretcher->isOk())
            stretcher.reset();
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

int TimeStretcher::processData (const float* const* inChannels, int numSamples, float* const* outChannels)
{
    if (stretcher != nullptr)
        return stretcher->processData (inChannels, numSamples, outChannels);

    return 0;
}

int TimeStretcher::processData (AudioFifo& inFifo, int numSamples, AudioFifo& outFifo)
{
    if (stretcher == nullptr)
        return 0;
    
    jassert (numSamples == stretcher->getFramesNeeded());
    AudioScratchBuffer inBuffer (inFifo.getNumChannels(), numSamples);
    AudioScratchBuffer outBuffer (outFifo.getNumChannels(), samplesPerBlockRequested);

    auto inChannels = inBuffer.buffer.getArrayOfReadPointers();
    auto outChannels = outBuffer.buffer.getArrayOfWritePointers();

    inFifo.read (inBuffer.buffer, 0, numSamples);
    const int numOutputFrames = stretcher->processData (inChannels, numSamples, outChannels);
    outFifo.write (outBuffer.buffer, 0, numOutputFrames);

    return numOutputFrames;
}

int TimeStretcher::flush (float* const* outChannels)
{
    if (stretcher != nullptr)
        return stretcher->flush (outChannels);
    
    return 0;
}

}} // namespace tracktion { inline namespace engine
