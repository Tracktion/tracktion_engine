/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static int getFloatFileHeaderInt()  { return (int) juce::ByteOrder::littleEndianInt ("TRKF"); }


//==============================================================================
class FloatAudioFormatReader  : public juce::AudioFormatReader
{
public:
    FloatAudioFormatReader (juce::InputStream* in)
        : AudioFormatReader (in, TRANS("Tracktion audio file"))
    {
        usesFloatingPointData = true;

        if (in->readInt() == getFloatFileHeaderInt())
        {
            dataStartOffset = in->readInt();
            sampleRate      = in->readInt();
            lengthInSamples = in->readInt();
            numChannels     = (unsigned int) in->readShort();
            bigEndian       = in->readShort() != 0;
            bitsPerSample   = 32;

            if (sampleRate < 32000 || sampleRate > 192000 || numChannels < 1 || numChannels > 16)
                sampleRate = 0;
        }
    }

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples)
    {
        clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                           startSampleInFile, numSamples, lengthInSamples);

        if (numSamples <= 0)
            return true;

        input->setPosition (4 * startSampleInFile * numChannels + dataStartOffset);
        const int bytesPerFrame = 4 * (int) numChannels;

        while (numSamples > 0)
        {
            const int tempBufSize = 480 * 3 * 4; // (keep this a multiple of 3)
            char tempBuffer [tempBufSize];

            const int numThisTime = std::min (tempBufSize / bytesPerFrame, numSamples);
            const int bytesRead = input->read (tempBuffer, numThisTime * bytesPerFrame);

            if (bytesRead < numThisTime * bytesPerFrame)
            {
                jassert (bytesRead >= 0);
                juce::zeromem (tempBuffer + bytesRead, (size_t) (numThisTime * bytesPerFrame - bytesRead));
            }

            if (bigEndian)
                ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::BigEndian>
                    ::read (destSamples, startOffsetInDestBuffer, numDestChannels, tempBuffer, (int) numChannels, numThisTime);
            else
                ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::LittleEndian>
                    ::read (destSamples, startOffsetInDestBuffer, numDestChannels, tempBuffer, (int) numChannels, numThisTime);

            startOffsetInDestBuffer += numThisTime;
            numSamples -= numThisTime;
        }

        return true;
    }

    int dataStartOffset;
    bool bigEndian;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatAudioFormatReader)
};

//==============================================================================
class FloatAudioFormatWriter  : public juce::AudioFormatWriter
{
public:
    FloatAudioFormatWriter (juce::OutputStream* out, double sampleRate, unsigned int numChannels_)
        : AudioFormatWriter (out,
                             TRANS("Tracktion audio file"),
                             sampleRate,
                             numChannels_,
                             32),
          lengthInSamples (0)
    {
        usesFloatingPointData = true;
        writeHeader();
    }

    ~FloatAudioFormatWriter()
    {
        output->setPosition (0);
        writeHeader();
    }

    //==============================================================================
    bool write (const int** data, int numSamps)
    {
        lengthInSamples += numSamps;

        for (int j = 0; j < numSamps; ++j)
        {
            for (unsigned int i = 0; i < numChannels; ++i)
            {
                if (const float* chan = (float*) (data[i]))
                {
                    float val = chan[j];
                    JUCE_UNDENORMALISE (val);
                    output->writeFloat (val);
                }
                else
                {
                    break;
                }
            }
        }

        return true;
    }

private:
    int lengthInSamples;

    void writeHeader()
    {
        const int headerSize = 512;
        output->writeInt (getFloatFileHeaderInt());
        output->writeInt (headerSize);
        output->writeInt (juce::roundToInt (sampleRate));
        output->writeInt (lengthInSamples);
        output->writeShort ((short)numChannels);
        output->writeShort (0); // big-endian

        while (output->getPosition() < headerSize)
            output->writeByte (0);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatAudioFormatWriter)
};

//==============================================================================
class MemoryMappedFloatReader   : public juce::MemoryMappedAudioFormatReader
{
public:
    MemoryMappedFloatReader (const juce::File& file, const FloatAudioFormatReader& reader)
        : MemoryMappedAudioFormatReader (file, reader,
                                         reader.dataStartOffset,
                                         reader.lengthInSamples * 4 * reader.numChannels,
                                         4 * (int) reader.numChannels),
          bigEndian (reader.bigEndian)
    {
    }

    bool readSamples (int** destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                           startSampleInFile, numSamples, lengthInSamples);

        if (map == nullptr || ! mappedSection.contains ({ startSampleInFile, startSampleInFile + numSamples }))
        {
            jassertfalse; // you must make sure that the window contains all the samples you're going to attempt to read.
            return false;
        }

        if (bigEndian)
            ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::BigEndian>
                ::read (destSamples, startOffsetInDestBuffer, numDestChannels, sampleToPointer (startSampleInFile), (int) numChannels, numSamples);
        else
            ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::LittleEndian>
                ::read (destSamples, startOffsetInDestBuffer, numDestChannels, sampleToPointer (startSampleInFile), (int) numChannels, numSamples);

        return true;
    }

    void getSample (juce::int64 sample, float* result) const noexcept override
    {
        if (map == nullptr || ! mappedSection.contains (sample))
        {
            jassertfalse; // you must make sure that the window contains all the samples you're going to attempt to read.

            juce::zeromem (result, sizeof (float) * numChannels);
            return;
        }

        const void* sourceData = sampleToPointer (sample);

        if (bigEndian)
            ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::BigEndian>
                ::read (&result, 0, 1, sourceData, 1, (int) numChannels);
        else
            ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::LittleEndian>
                ::read (&result, 0, 1, sourceData, 1, (int) numChannels);
    }

    void readMaxLevels (juce::int64 startSampleInFile, juce::int64 numSamples, juce::Range<float>* results, int numChannelsToRead) override
    {
        if (numSamples <= 0)
        {
            for (int i = 0; i < numChannelsToRead; ++i)
                results[i] = {};

            return;
        }

        if (map == nullptr || ! mappedSection.contains ({ startSampleInFile, startSampleInFile + numSamples }))
        {
            jassertfalse; // you must make sure that the window contains all the samples you're going to attempt to read.

            for (int i = 0; i < numChannelsToRead; ++i)
                results[i] = {};

            return;
        }

        switch (bitsPerSample)
        {
            case 8:     scanMinAndMax<juce::AudioData::UInt8> (startSampleInFile, numSamples, results, numChannelsToRead); break;
            case 16:    scanMinAndMax<juce::AudioData::Int16> (startSampleInFile, numSamples, results, numChannelsToRead); break;
            case 24:    scanMinAndMax<juce::AudioData::Int24> (startSampleInFile, numSamples, results, numChannelsToRead); break;
            case 32:    if (usesFloatingPointData) scanMinAndMax<juce::AudioData::Float32> (startSampleInFile, numSamples, results, numChannelsToRead);
                        else                       scanMinAndMax<juce::AudioData::Int32>   (startSampleInFile, numSamples, results, numChannelsToRead); break;
            default:    jassertfalse; break;
        }
    }

private:
    const bool bigEndian;

    template <typename SampleType>
    void scanMinAndMax (juce::int64 startSampleInFile, juce::int64 numSamples, juce::Range<float>* results, int numChannelsToRead) const
    {
        typedef juce::AudioData::Pointer<SampleType, juce::AudioData::LittleEndian, juce::AudioData::Interleaved, juce::AudioData::Const> SourceType;

        for (int i = 0; i < numChannelsToRead; ++i)
            results[i] = SourceType (sampleToPointer (startSampleInFile), (int) numChannels).findMinAndMax ((size_t) numSamples);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoryMappedFloatReader)
};

//==============================================================================
FloatAudioFormat::FloatAudioFormat()  : AudioFormat ("Tracktion audio file", ".trkaudio") {}
FloatAudioFormat::~FloatAudioFormat() {}

juce::Array<int> FloatAudioFormat::getPossibleSampleRates()     { return { 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 }; }
juce::Array<int> FloatAudioFormat::getPossibleBitDepths()       { return { 32 }; }

bool FloatAudioFormat::canDoStereo()    { return true; }
bool FloatAudioFormat::canDoMono()      { return true; }

bool FloatAudioFormat::canHandleFile (const juce::File& f)
{
    return f.hasFileExtension (".trkaudio")
       #if JUCE_WINDOWS
        || f.hasFileExtension ({})
       #endif
        || f.hasFileExtension (".freeze");
}

juce::AudioFormatReader* FloatAudioFormat::createReaderFor (juce::InputStream* in, bool deleteStreamIfOpeningFails)
{
    std::unique_ptr<FloatAudioFormatReader> r (new FloatAudioFormatReader (in));

    if (r->sampleRate > 0)
        return r.release();

    if (! deleteStreamIfOpeningFails)
        r->input = nullptr;

    return {};
}

juce::MemoryMappedAudioFormatReader* FloatAudioFormat::createMemoryMappedReader (const juce::File& file)
{
    if (auto fin = file.createInputStream())
    {
        FloatAudioFormatReader reader (fin);

        if (reader.lengthInSamples > 0)
            return new MemoryMappedFloatReader (file, reader);
    }

    return {};
}

juce::AudioFormatWriter* FloatAudioFormat::createWriterFor (juce::OutputStream* out,
                                                            double sampleRate,
                                                            unsigned int numChannels,
                                                            int /*bitsPerSample*/,
                                                            const juce::StringPairArray& /*metadataValues*/,
                                                            int /*qualityOptionIndex*/)
{
    return new FloatAudioFormatWriter (out, sampleRate, numChannels);
}
