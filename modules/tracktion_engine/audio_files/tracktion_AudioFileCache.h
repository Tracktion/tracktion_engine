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

struct FallbackReader  : public juce::AudioFormatReader
{
    /** Constructor. */
    FallbackReader();

    /** Subclassed must override this to set the timeout.
        A value of less than 0 means wait forever,
        A value of 0 means don't wait
        A value greater than 0 means wait for the given number of ms
    */
    virtual void setReadTimeout (int timeoutMilliseconds) = 0;
};

//==============================================================================
/**
*/
class AudioFileCache
{
public:
    AudioFileCache (Engine&);
    ~AudioFileCache();

    //==============================================================================
    class Reader  : public juce::ReferenceCountedObject
    {
    public:
        ~Reader();

        using Ptr = juce::ReferenceCountedObjectPtr<Reader>;

        void setReadPosition (SampleCount) noexcept;
        SampleCount getReadPosition() const noexcept    { return readPos; }

        bool readSamples (int numSamples,
                          juce::AudioBuffer<float>& destBuffer,
                          const juce::AudioChannelSet& destBufferChannels,
                          int startOffsetInDestBuffer,
                          const juce::AudioChannelSet& sourceBufferChannels,
                          int timeoutMs);

        bool readSamples (int* const* destSamples,
                          int numDestChannels,
                          int startOffsetInDestBuffer,
                          int numSamples,
                          int timeoutMs);

        bool getRange (int numSamples,
                       float& lmax, float& lmin,
                       float& rmax, float& rmin,
                       int timeoutMs);

        void setLoopRange (SampleRange);

        int getNumChannels() const noexcept;
        double getSampleRate() const noexcept;

    private:
        friend class AudioFileCache;

        AudioFileCache& cache;
        void* file;
        std::atomic<SampleCount> readPos { 0 }, loopStart { 0 }, loopLength { 0 };
        std::unique_ptr<FallbackReader> fallbackReader;

        Reader (AudioFileCache&, void*, std::unique_ptr<FallbackReader>);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Reader)
    };

    /** Creates a Reader to read an AudioFile.
        This will use a memoery mapped reader for uncompressed formats.
    */
    Reader::Ptr createReader (const AudioFile&);

    /** @internal */
    Reader::Ptr createReader (const AudioFile&,
                              const std::function<std::unique_ptr<FallbackReader> (juce::AudioFormatReader* sourceReader,
                                                                                   juce::TimeSliceThread& timeSliceThread,
                                                                                   int samplesToBuffer)>&
                              createFallbackReader);

    //==============================================================================
    void setCacheSizeSamples (SampleCount samplesPerFile);
    SampleCount getCacheSizeSamples() const         { return cacheSizeSamples; }

    SampleCount getBytesInUse() const               { return totalBytesUsed; }

    bool hasCacheMissed (bool clearMissedFlag);

    /** Returns the amount of time spent reading files. */
    double getCpuUsage()                            { return cpuUsage.load (std::memory_order_relaxed); }

private:
    Engine& engine;
    SampleCount totalBytesUsed = 0, cacheSizeSamples = 0;
    bool cacheMissed = false;
    std::atomic<double> cpuUsage { 0 };

    class CacheBuffer;
    class CachedFile;
    juce::OwnedArray<CachedFile> activeFiles;
    int nextFileToService = 0;
    juce::ReadWriteLock fileListLock;

    CachedFile* getOrCreateCachedFile (const AudioFile&);
    bool serviceNextReader();
    void touchReaders();

    class MapperThread;
    std::unique_ptr<MapperThread> mapperThread;
    class RefresherThread;
    std::unique_ptr<RefresherThread> refresherThread;

    juce::TimeSliceThread backgroundReaderThread { "Preview Buffer" };

    void stopThreads();

    void purgeOldFiles();
    void purgeOrphanReaders();

    friend class AudioFileManager;
    void releaseFile (const AudioFile&);
    void releaseAllFiles();
    void validateFile (const AudioFile&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileCache)
};

}} // namespace tracktion { inline namespace engine
