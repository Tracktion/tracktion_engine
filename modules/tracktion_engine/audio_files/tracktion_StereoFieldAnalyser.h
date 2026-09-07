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
    A real-time-safe stereo field analyser: phase correlation, stereo balance
    and stereo width, over a short sliding window and over the whole
    measurement.

    Everything is derived from running sums of l*l, r*r and l*r, updated on the
    same 100ms block cadence as LoudnessMeter's gating blocks with a 400ms
    sliding window for the "current" readings. For buffers with more than two
    channels, the first pair is measured (standard correlation-meter practice);
    for mono, the single channel is treated as both sides, so the readings
    describe a perfectly centred signal.

    Real-time contract (identical to LoudnessMeter):
    - prepare() does all of the allocation and must not be called while
      processing.
    - process() is allocation- and lock-free.
    - getReadings() may be called from any thread while process() runs on the
      audio thread, returning one internally consistent seqlock'd snapshot.
*/
class StereoFieldAnalyser
{
public:
    //==============================================================================
    StereoFieldAnalyser() = default;

    /** Allocates for the given format and resets the measurement.
        Must be called before process(), and never whilst processing.
        Only the first two channels are analysed, but it's fine to prepare and
        feed wider buffers.
        @param maxBlockSize     accepted for signature parity with
                                LoudnessMeter::prepare - the analyser has no
                                per-block working buffers, so any block length
                                may be passed to process()
    */
    void prepare (double sampleRate, int numChannels, int maxBlockSize);

    /** Processes a block of audio, updating the published readings.
        Real-time safe: no allocation, no locks. The block can be any length.
        @see LoudnessMeter::process
    */
    void process (choc::buffer::ChannelArrayView<const float> block) noexcept;

    /** Processes a block of audio from raw channel pointers.
        @see process (choc::buffer::ChannelArrayView<const float>)
    */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** For offline use: finishes a partially-filled analysis block if it's at
        least half full, so a file's tail contributes to the measurement.
        Call this after the last process() call, not during streaming.
    */
    void flush() noexcept;

    /** Asks for the measurement, window history and held extremes to be
        restarted. Picked up by the processing thread at the start of its next
        block, so this never blocks.
    */
    void requestReset() noexcept;

    //==============================================================================
    /** A snapshot of the analyser's current readings.

        Correlation is the Pearson correlation of the two channels: +1 for
        identical (mono-compatible), 0 for unrelated, negative for
        phase-cancelling material. Balance is the energy split, -1 hard left to
        +1 hard right. Width is the side energy as a fraction of mid + side
        energy: 0 for mono, 0.5 for uncorrelated channels, 1 for pure side
        (out-of-phase) content. monoLossDb is how many dB the overall level
        drops when the two channels are summed to mono ((l+r)/2 against the
        per-channel average): 0 for centred material, 3 for uncorrelated or
        hard-panned material, large for phase-cancelling material.

        The alignment readings estimate a whole-signal timing offset between the
        two channels by cross-correlating them over a range of lags, which is
        what correlation alone cannot tell you: correlation says the channels
        disagree, alignment says by how much and in which direction. A positive
        delay means the right channel arrives later than the left.
    */
    struct Readings
    {
        float correlation        = 1.0f;    /**< The last 400ms window. */
        float minCorrelation     = 1.0f;    /**< Worst window since prepare() or the last reset. */
        float balance            = 0.0f;    /**< The last 400ms window, -1..+1. */
        float width              = 0.0f;    /**< The last 400ms window, 0..1. */

        float overallCorrelation = 1.0f;    /**< Since prepare() or the last reset. */
        float overallBalance     = 0.0f;    /**< Since prepare() or the last reset, -1..+1. */
        float overallWidth       = 0.0f;    /**< Since prepare() or the last reset, 0..1. */
        float monoLossDb         = 0.0f;    /**< Overall mono summing loss, in dB (positive = quieter in mono). */

        float interChannelDelayMs      = 0.0f;  /**< Right-channel delay relative to left, in ms (negative = left is late). */
        int   interChannelDelaySamples = 0;     /**< The same offset at the prepared sample rate. */
        bool  polarityInverted         = false; /**< True when the channels match best at inverted polarity. */

        bool windowValid         = false;   /**< False until 400ms of non-silent audio has been processed. */
        bool overallValid        = false;   /**< False until any non-silent block has been processed. */
        bool alignmentValid      = false;   /**< False until the channels match strongly enough at some lag to trust the estimate. */
    };

    /** Returns the current readings. Lock-free, callable from any thread. */
    Readings getReadings() const noexcept;

private:
    //==============================================================================
    struct BlockSums
    {
        double ll = 0.0, rr = 0.0, lr = 0.0;
    };

    static constexpr int windowBlocks = 4;          // 400ms at the 100ms hop
    static constexpr double silenceThreshold = 1.0e-12;

    // The lag search runs on a decimated copy of the signal: box-averaging down
    // to ~8kHz is a crude but adequate anti-alias filter, and gross channel
    // timing is a low-frequency question. That keeps the search to roughly 130k
    // multiply-accumulates per 100ms block instead of millions at full rate.
    static constexpr double alignmentSearchSeconds = 0.005;     // +/-5ms of lag
    static constexpr double alignmentWindowSeconds = 0.2;
    static constexpr double alignmentTargetRate = 8000.0;
    static constexpr float alignmentMinCorrelation = 0.5f;      // below this a "peak" is just noise
    static constexpr float alignmentPeakRatio = 1.2f;           // how far the peak must stand above the next-best one

    void processSamples (const float* const* channels, int numChannels, int startSample, int numSamples) noexcept;
    void pushAlignmentSample (double l, double r) noexcept;
    void finishBlock() noexcept;
    void updateReadings() noexcept;
    void updateAlignment() noexcept;
    void publish() noexcept;
    void resetMeasurement() noexcept;

    double sampleRate = 44100.0;
    int numChannels = 0, maxBlockSize = 0;

    int blockSamples = 4410, samplesIntoBlock = 0;
    BlockSums blockSums;
    std::array<BlockSums, windowBlocks> windowHistory = {};
    int windowPos = 0, numBlocksSeen = 0;

    BlockSums overallSums;

    // Decimated ring buffers for the lag search, plus linearised scratch copies
    // so the inner correlation loop is a straight walk with no modular indexing
    int decimationFactor = 1, decimateCount = 0;
    double decimateAccumL = 0.0, decimateAccumR = 0.0;
    std::vector<float> alignmentL, alignmentR, scratchL, scratchR, lagCurve;
    int alignmentWindowSamples = 0, maxLagSamples = 0;
    int alignmentPos = 0, alignmentFilled = 0;

    Readings currentReadings;
    crill::seqlock_object<Readings> publishedReadings { Readings() };
    std::atomic<bool> resetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoFieldAnalyser)
};

} // namespace tracktion::inline engine
