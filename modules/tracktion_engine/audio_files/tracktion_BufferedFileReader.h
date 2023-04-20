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
/** @internal

    An AudioFormatReader that uses a background thread to pre-read data from
    another reader.

    N.B. This class is in development and not ready for public use yet!!
*/
class BufferedFileReader    : public juce::AudioFormatReader,
                              private juce::TimeSliceClient
{
public:
    /** Creates a reader.

        @param sourceReader     the source reader to wrap. This BufferingAudioReader
                                takes ownership of this object and will delete it later
                                when no longer needed
        @param timeSliceThread  the thread that should be used to do the background reading.
                                Make sure that the thread you supply is running, and won't
                                be deleted while the reader object still exists.
        @param samplesToBuffer  the total number of samples to buffer ahead.
                                Pass -1 to buffer the whole source
    */
    BufferedFileReader (juce::AudioFormatReader* sourceReader,
                        juce::TimeSliceThread& timeSliceThread,
                        int samplesToBuffer);

    /** Destructor. */
    ~BufferedFileReader() override;

    /** Sets a number of milliseconds that the reader can block for in its readSamples()
        method before giving up and returning silence.

        A value of less that 0 means "wait forever".
        The default timeout is 0 which means don't wait at all.
    */
    void setReadTimeout (int timeoutMilliseconds) noexcept;

    /** Returns true if this has been initialised to buffer the whole file
        once that is complete, false otherwise.
    */
    bool isFullyBuffered() const;

    //==============================================================================
    /** @internal */
    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override;

private:
    struct BufferedBlock
    {
        BufferedBlock (juce::AudioFormatReader&);

        void update (juce::AudioFormatReader&, juce::Range<juce::int64> range, size_t slotIndex);

        juce::Range<juce::int64> range;
        juce::AudioBuffer<float> buffer;
        bool allSamplesRead = false;
        std::atomic<juce::uint32> lastUseTime { 0 };
        std::atomic<int> slotIndex { -1 };
    };

    struct ScopedSlotAccess
    {
        ScopedSlotAccess (BufferedFileReader&, size_t slotIndex);
        ~ScopedSlotAccess();

        static ScopedSlotAccess fromPosition (BufferedFileReader&, juce::int64 position);

        BufferedBlock* getBlock() const     { return block; }
        void setBlock (BufferedBlock*);

    private:
        BufferedFileReader& reader;
        const size_t slotIndex = 0;
        BufferedBlock* block = nullptr;
    };

    int useTimeSlice() override;

    enum class PositionStatus
    {
        positionChangedByAudioThread,
        nextChunkScheduled,
        blocksFull,
        fullyLoaded
    };

    PositionStatus readNextBufferChunk();

    static constexpr int samplesPerBlock = 32768;

    std::unique_ptr<juce::AudioFormatReader> source;
    juce::TimeSliceThread& thread;
    std::atomic<juce::int64> nextReadPosition { 0 };
    int timeoutMs = 0;

    std::vector<std::unique_ptr<BufferedBlock>> blocks;
    std::vector<std::atomic<BufferedBlock*>> slots;
    std::vector<std::atomic<bool>> slotsInUse;

    size_t numBlocksToBuffer = 0;
    std::atomic<size_t> numBlocksBuffered { 0 }, nextSlotScheduled { 0 };
    const bool isFullyBuffering = false;

    size_t getSlotIndexFromSamplePosition (juce::int64 samplePos);
    juce::Range<juce::int64> getSlotRange (size_t slotIndex);
    void markSlotUseState (size_t slotIndex, bool isInUse);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BufferedFileReader)
};

}} // namespace tracktion { inline namespace engine
