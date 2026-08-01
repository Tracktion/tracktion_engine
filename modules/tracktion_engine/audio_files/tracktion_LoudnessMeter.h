/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

//==============================================================================
/**
    A real-time-safe EBU R128 / ITU-R BS.1770-4 loudness meter.

    It measures momentary (400ms), short-term (3s) and gated integrated loudness,
    loudness range (EBU Tech 3342) and 4x-oversampled true peak and sample peak,
    and can be driven either from an audio callback or from an offline file pass.

    Real-time contract:
    - prepare() does all of the allocation and must not be called while
      processing.
    - process() is allocation- and lock-free, and takes no locks internally.
    - getReadings() is lock-free and may be called from any thread (typically a
      UI timer) while process() is running on the audio thread.

    Unlike a straight offline implementation, the integrated measurement doesn't
    keep the whole history of gating blocks: momentary powers are accumulated
    into a fixed 0.1 LU histogram (the libebur128 approach), so the memory is
    constant and the gated result is recomputed in constant time every 100ms.
    Only the two gate decisions are quantised, to at most the 0.1 LU bin width,
    which is far inside the EBU R128 tolerance. Loudness range works the same
    way, from a second histogram of short-term loudness.
*/
class LoudnessMeter
{
public:
    //==============================================================================
    LoudnessMeter() = default;

    /** The value used for readings which haven't seen any audio yet. */
    static constexpr float silenceFloorDb = -100.0f;

    //==============================================================================
    /** Allocates for the given format and resets the measurement.
        Must be called before process(), and never whilst processing.
        @param maxBlockSize     the largest block process() will be given in one
                                go internally - longer blocks are chunked, so this
                                only needs to be a sensible working size
    */
    void prepare (double sampleRate, int numChannels, int maxBlockSize);

    /** Processes a block of audio, updating the published readings.
        Real-time safe: no allocation, no locks. The block can be any length -
        it's chunked internally to the prepared maximum block size. Channels
        beyond the number passed to prepare() are ignored.
    */
    void process (choc::buffer::ChannelArrayView<const float> block) noexcept;

    /** Processes a block of audio from raw channel pointers.
        @see process (choc::buffer::ChannelArrayView<const float>)
    */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** For offline use: finishes a partially-filled gating block if it's at
        least half full, so a file's tail contributes to the measurement.
        Call this after the last process() call, not during streaming.
    */
    void flush() noexcept;

    /** Asks for the integrated measurement, held maxima and peaks to be
        restarted. The request is picked up by the processing thread at the start
        of its next block, so this never blocks. The K-weighting filter state is
        deliberately left running, to avoid a reset causing a filter transient.
    */
    void requestReset() noexcept;

    //==============================================================================
    /** A snapshot of the meter's current readings. */
    struct Readings
    {
        float momentaryLufs     = silenceFloorDb;   /**< The last 400ms window. */
        float shortTermLufs     = silenceFloorDb;   /**< The last 3s window. */
        float integratedLufs    = silenceFloorDb;   /**< Gated, since prepare() or the last reset. */
        float maxMomentaryLufs  = silenceFloorDb;   /**< Held maximum momentary value. */
        float maxShortTermLufs  = silenceFloorDb;   /**< Held maximum short-term value. */
        float truePeakDb        = silenceFloorDb;   /**< Held maximum, 4x oversampled. */
        float samplePeakDb      = silenceFloorDb;   /**< Held maximum sample value. */
        float loudnessRangeLu   = 0.0f;             /**< EBU Tech 3342 loudness range, in LU. */

        bool momentaryValid     = false;            /**< False until 400ms has been processed. */
        bool shortTermValid     = false;            /**< False until 3s has been processed. */
        bool integratedValid    = false;            /**< False until a block passes the gates. */
        bool loudnessRangeValid = false;            /**< False until a short-term block passes the gates. */
    };

    /** Returns the current readings. Lock-free, callable from any thread. */
    Readings getReadings() const noexcept;

private:
    //==============================================================================
    /** A direct-form biquad on doubles, for the K-weighting filters. */
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        double process (double x) noexcept
        {
            const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    struct ChannelState
    {
        Biquad shelf, highpass;
    };

    static constexpr int momentaryBlocks = 4;       // 400ms at the 100ms hop
    static constexpr int shortTermBlocks = 30;      // 3s
    static constexpr int numHistogramBins = 800;    // [-70, +10) LUFS
    static constexpr double histogramFloorLufs = -70.0;
    static constexpr double histogramBinWidthLu = 0.1;

    static void setKWeightingCoefficients (Biquad& shelf, Biquad& highpass, double sampleRate);

    void processSamples (const float* const* channels, int numChannels, int startSample, int numSamples) noexcept;
    void processChunk (const float* const* channels, int numChannels, int startSample, int numSamples) noexcept;
    void finishGatingBlock() noexcept;
    double sumOfLastBlockPowers (int numBlocks) const noexcept;
    void addToHistogram (double power, double loudness) noexcept;
    void updateIntegrated() noexcept;
    void addToShortTermHistogram (double power, double loudness) noexcept;
    void updateLoudnessRange() noexcept;
    void publishPeaks() noexcept;
    void resetMeasurement() noexcept;

    double sampleRate = 44100.0;
    int numChannels = 0, maxBlockSize = 0;
    std::vector<ChannelState> channels;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    int gatingBlockSamples = 4410, samplesIntoBlock = 0;
    double blockEnergy = 0.0;
    std::array<double, shortTermBlocks> blockPowers = {};
    int blockPowerPos = 0;
    int64_t numGatingBlocks = 0;

    std::array<double, numHistogramBins> binPowerSums = {};
    std::array<int64_t, numHistogramBins> binCounts = {};
    double histogramPowerSum = 0.0;
    int64_t histogramCount = 0;

    // The loudness range only needs counts - the percentiles come from those,
    // and its relative gate uses a running mean of the gated short-term powers
    std::array<int64_t, numHistogramBins> shortTermBinCounts = {};
    double shortTermPowerSum = 0.0;
    int64_t shortTermCount = 0;

    float peak = 0.0f, truePeak = 0.0f;

    std::atomic<float> publishedMomentary { silenceFloorDb }, publishedShortTerm { silenceFloorDb },
                       publishedIntegrated { silenceFloorDb }, publishedMaxMomentary { silenceFloorDb },
                       publishedMaxShortTerm { silenceFloorDb }, publishedTruePeakDb { silenceFloorDb },
                       publishedSamplePeakDb { silenceFloorDb }, publishedLoudnessRange { 0.0f };
    std::atomic<int64_t> publishedNumGatingBlocks { 0 };
    std::atomic<bool> publishedIntegratedValid { false }, publishedLoudnessRangeValid { false };
    std::atomic<bool> resetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};

} // namespace tracktion::inline engine
