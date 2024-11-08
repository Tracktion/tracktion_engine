/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
    FallbackReader that wraps a MemoryMappedFile which usually improves read speeds.
*/
//==============================================================================
struct MemoryMappedFileReader    : public FallbackReader
{
    /** Creates a MemoryMappedFileReader for a  MappedFileAndReader. */
    MemoryMappedFileReader (std::unique_ptr<AudioFileUtils::MappedFileAndReader> mappedFileAndReader)
        : source (std::move (mappedFileAndReader))
    {
        sampleRate              = source->reader->sampleRate;
        bitsPerSample           = source->reader->bitsPerSample;
        lengthInSamples         = source->reader->lengthInSamples;
        numChannels             = source->reader->numChannels;
        usesFloatingPointData   = source->reader->usesFloatingPointData;
        metadataValues          = source->reader->metadataValues;
    }

    /** @internal */
    void setReadTimeout (int) override
    {
    }

    /** @internal */
    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->reader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                            startSampleInFile, numSamples);
    }

private:
    std::unique_ptr<AudioFileUtils::MappedFileAndReader> source;
};


//==============================================================================
//==============================================================================
/**
    FallbackReader that wraps a BufferingAudioReader to avoid reading on the audio thread.
*/
class BufferingAudioReaderWrapper   : public FallbackReader
{
public:
    BufferingAudioReaderWrapper (std::unique_ptr<juce::BufferingAudioReader> sourceReader)
        : source (std::move (sourceReader))
    {
        sampleRate              = source->sampleRate;
        bitsPerSample           = source->bitsPerSample;
        lengthInSamples         = source->lengthInSamples;
        numChannels             = source->numChannels;
        usesFloatingPointData   = source->usesFloatingPointData;
        metadataValues          = source->metadataValues;
    }

    /** @internal */
    void setReadTimeout (int timeoutMilliseconds) override
    {
        source->setReadTimeout (timeoutMilliseconds);
    }

    /** @internal */
    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                    startSampleInFile, numSamples);
    }

private:
    std::unique_ptr<juce::BufferingAudioReader> source;
};


}} // namespace tracktion { inline namespace engine
