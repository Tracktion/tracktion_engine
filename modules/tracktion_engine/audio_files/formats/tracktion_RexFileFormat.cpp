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

#if TRACKTION_ENABLE_REX

const char* const RexAudioFormat::rexTempo          = "rex tempo";
const char* const RexAudioFormat::rexDenominator    = "rex denominator";
const char* const RexAudioFormat::rexNumerator      = "rex numerator";
const char* const RexAudioFormat::rexBeatPoints     = "rex beat points";

//==============================================================================
inline const char* rexErrorToString (REX::REXError e) noexcept
{
    switch (e)
    {
        case REX::kREXError_NoError:                   return "No error";
        case REX::kREXError_OperationAbortedByUser:    return "Operation aborted by user";
        case REX::kREXError_NoCreatorInfoAvailable:    return "No creator info available";
        case REX::kREXError_OutOfMemory:               return "Out of Memory";
        case REX::kREXError_FileCorrupt:               return "File is corrupt";
        case REX::kREXError_REX2FileTooNew:            return "REX2 file is too new";
        case REX::kREXError_FileHasZeroLoopLength:     return "File has a zero length for the loop";
        case REX::kREXImplError_DLLNotInitialized:     return "DLL Not Initialized";
        case REX::kREXImplError_DLLAlreadyInitialized: return "DLL Already Initialized";
        case REX::kREXImplError_InvalidHandle:         return "Invalid handle";
        case REX::kREXImplError_InvalidSize:           return "Invalid size";
        case REX::kREXImplError_InvalidArgument:       return "Invalid argument";
        case REX::kREXImplError_InvalidSlice:          return "Invalid slice";
        case REX::kREXImplError_InvalidSampleRate:     return "Invalid sample rate";
        case REX::kREXImplError_BufferTooSmall:        return "Buffer too small";
        case REX::kREXImplError_IsBeingPreviewed:      return "Is being previewed";
        case REX::kREXImplError_NotBeingPreviewed:     return "Not being previewed";
        case REX::kREXImplError_InvalidTempo:          return "Invalid tempo";
        default:                                       break;
    }

    return "Undefined";
}

static bool checkRexError (REX::REXError e)
{
    if (e == REX::kREXError_NoError)
        return true;

    TRACKTION_LOG_ERROR ("REX (Code " + juce::String ((int) e) + "): " + rexErrorToString (e));
    return false;
}

//==============================================================================
struct RexSystem
{
    RexSystem()
    {
        CRASH_TRACER
        const REX::REXError e = REX::REXInitializeDLL();

        if (e == REX::kREXImplError_DLLNotInitialized)
        {
            startupErrorMessage = TRANS("Error loading Propellerheads REX DLL!");
        }
        else if (e != REX::kREXError_NoError)
        {
            startupErrorMessage = TRANS("An unknown error occured with the Propellerheads REX format!");
        }

        initialised = checkRexError (e);
    }

    ~RexSystem()
    {
        CRASH_TRACER

        if (initialised)
            REX::REXUninitializeDLL();
    }

    juce::CriticalSection lock;
    bool initialised = false;
    juce::String startupErrorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexSystem)
};

static RexSystem& getRexSystem()
{
    static RexSystem rex;
    return rex;
}

struct RexHandleWrapper
{
    RexHandleWrapper (const void* rexData, size_t rexDataSize)
    {
        CRASH_TRACER
        ok = checkRexError (REX::REXCreate (&handle, (const char*) rexData,
                                            (REX::REX_int32_t) rexDataSize, nullptr, nullptr));
    }

    ~RexHandleWrapper()
    {
        CRASH_TRACER
        if (handle != 0)
            REX::REXDelete (&handle);
    }

    REX::REXHandle handle;
    bool ok = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexHandleWrapper)
};

//==============================================================================
class RexAudioFormatReader : public juce::AudioFormatReader
{
public:
    RexAudioFormatReader (juce::InputStream* sourceStream,  const juce::String& name)
        : juce::AudioFormatReader (sourceStream, name), buffer (2, 2)
    {
        CRASH_TRACER
        juce::MemoryBlock rexData;
        sourceStream->readIntoMemoryBlock (rexData);

        loadedOk = decompress (rexData.getData(), rexData.getSize());
    }

    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        CRASH_TRACER
        clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                           startSampleInFile, numSamples, lengthInSamples);

        if (numSamples > 0)
        {
            jassert (startOffsetInDestBuffer + numSamples <= buffer.getNumSamples());

            for (int i = 0; i < numDestChannels; ++i)
            {
                if (float* dest = (float*) destSamples[i])
                {
                    dest += startOffsetInDestBuffer;

                    if (i < buffer.getNumChannels())
                        memcpy (dest, buffer.getReadPointer (i, (int) startSampleInFile),
                                sizeof (float) * (size_t) numSamples);
                }
            }
        }

        return true;
    }

    bool loadedOk;

private:
    juce::AudioBuffer<float> buffer;

    bool decompress (const void* rexData, size_t rexDataSize)
    {
        CRASH_TRACER
        auto& rex = getRexSystem();

        const juce::ScopedLock lock (rex.lock);

        if (! rex.initialised)
            return false;

        RexHandleWrapper handle (rexData, rexDataSize);
        if (! handle.ok)
            return false;

        REX::REXInfo info;
        if (! checkRexError (REX::REXGetInfo (handle.handle, sizeof (info), &info)))
            return false;

        if (! checkRexError (REX::REXSetOutputSampleRate (handle.handle, info.fSampleRate)))
            return false;

        const double beats = info.fPPQLength / 15360.0;
        const double bps   = info.fTempo / (1000.0 * 60.0);

        lengthInSamples         = (SampleCount) ((beats / bps) * info.fSampleRate);
        sampleRate              = info.fSampleRate;
        bitsPerSample           = (unsigned int) info.fBitDepth;
        numChannels             = (unsigned int) info.fChannels;
        usesFloatingPointData   = true;

        buffer.setSize ((int) info.fChannels, (int) lengthInSamples);
        buffer.clear();

        juce::StringArray beatPoints;

        for (int j = 0; j < info.fSliceCount; ++j)
        {
            REX::REXSliceInfo slcInfo;
            if (! checkRexError (REX::REXGetSliceInfo (handle.handle, j, sizeof(slcInfo), &slcInfo)))
                return false;

            juce::AudioBuffer<float> sliceData (buffer.getNumChannels(), (int) slcInfo.fSampleLength);

            if (! checkRexError (REX::REXRenderSlice (handle.handle, j, slcInfo.fSampleLength, (float**)sliceData.getArrayOfWritePointers())))
                return false;

            auto offset = (SampleCount) ((slcInfo.fPPQPos / 15360.0) / (info.fTempo / (1000.0 * 60.0)) * info.fSampleRate);
            auto numSamples = (int) std::min (lengthInSamples - offset, (SampleCount) slcInfo.fSampleLength);

            if (numSamples > 0)
                for (int i = 0; i < sliceData.getNumChannels(); ++i)
                    buffer.addFrom (i, (int) offset, sliceData, i, 0, numSamples);

            beatPoints.add (juce::String (offset));
        }

        metadataValues.set (RexAudioFormat::rexDenominator, juce::String ((int) info.fTimeSignDenom));
        metadataValues.set (RexAudioFormat::rexNumerator,   juce::String ((int) info.fTimeSignNom));
        metadataValues.set (RexAudioFormat::rexTempo,       juce::String (info.fTempo / 1000.0));

        if (beatPoints.size() > 0)
            metadataValues.set (RexAudioFormat::rexBeatPoints, beatPoints.joinIntoString (";"));

        return true;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexAudioFormatReader)
};

//==============================================================================
RexAudioFormat::RexAudioFormat()
   : AudioFormat (NEEDS_TRANS("REX2 file"), ".rx2 .rex .rcy")
{
}

RexAudioFormat::~RexAudioFormat()
{
}

juce::AudioFormatReader* RexAudioFormat::createReaderFor (juce::InputStream* sourceStream, bool deleteStreamIfOpeningFails)
{
    CRASH_TRACER
    auto r = std::make_unique<RexAudioFormatReader> (sourceStream, getFormatName());

    if (r->loadedOk)
        return r.release();

    if (! deleteStreamIfOpeningFails)
        r->input = nullptr;

    return {};
}

juce::AudioFormatWriter* RexAudioFormat::createWriterFor (juce::OutputStream*, double, unsigned int, int, const juce::StringPairArray&, int)
{
    return {};
}

juce::String RexAudioFormat::getErrorLoadingDLL()
{
    return getRexSystem().startupErrorMessage;
}

#endif


}} // namespace tracktion { inline namespace engine
