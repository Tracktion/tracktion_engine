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
//==============================================================================
class BufferedAudioReader : public juce::AudioFormatReader,
                            public juce::TimeSliceClient
{
public:
    BufferedAudioReader (std::unique_ptr<juce::AudioFormatReader>, juce::TimeSliceThread&);

    /** Destructor */
    ~BufferedAudioReader() override;

    /** Returns the proportion of the source that has been cached. */
    float getProportionComplete() const;

    /** @internal */
    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override;

    /** @internal */
    int useTimeSlice() override;

private:
    std::unique_ptr<juce::AudioFormatReader> source;
    juce::TimeSliceThread& thread;

    choc::buffer::ChannelArrayBuffer<float> data;
    std::atomic<choc::buffer::FrameCount> validEnd { 0 };
    const choc::buffer::FrameCount sourceLength { static_cast<choc::buffer::FrameCount> (source->lengthInSamples) };

    static constexpr choc::buffer::FrameCount chunkSize = 65'536;

    bool readNextChunk();
    bool readIntoBuffer (choc::buffer::FrameRange);
};

class BufferedFileReaderWrapper : public FallbackReader
{
public:
    BufferedFileReaderWrapper (std::shared_ptr<BufferedAudioReader> s)
        : source (std::move (s))
    {
        assert (source);

        sampleRate              = source->sampleRate;
        bitsPerSample           = source->bitsPerSample;
        lengthInSamples         = source->lengthInSamples;
        numChannels             = source->numChannels;
        usesFloatingPointData   = source->usesFloatingPointData;
        metadataValues          = source->metadataValues;
    }

    /** @internal */
    void setReadTimeout (int) override
    {}

    /** @internal */
    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                    startSampleInFile, numSamples);
    }

private:
    std::shared_ptr<BufferedAudioReader> source;
};

//==============================================================================
//==============================================================================
class BufferedAudioFileManager
{
public:
    BufferedAudioFileManager (Engine&);

    std::shared_ptr<BufferedAudioReader> get (juce::File);

private:
    Engine& engine;
    juce::TimeSliceThread readThread { "Audio file decompressing" };
    std::map<juce::File, std::shared_ptr<BufferedAudioReader>> cache;
    LambdaTimer timer { [this] { cleanUp(); } };

    void cleanUp();
    std::unique_ptr<juce::AudioFormatReader> createReader (juce::File);
};

}} // namespace tracktion { inline namespace engine
