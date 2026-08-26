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

namespace stereo_field_utils
{
    /** Pearson correlation from sums of squares and products, with the
        conventions used by hardware correlation meters: silence on both sides
        has no defined value (the caller keeps the last one), and one silent
        side reads as uncorrelated.
    */
    static std::optional<float> correlationFromSums (double ll, double rr, double lr, double threshold)
    {
        if (ll + rr < threshold)
            return std::nullopt;

        if (ll < threshold || rr < threshold)
            return 0.0f;

        return (float) juce::jlimit (-1.0, 1.0, lr / std::sqrt (ll * rr));
    }

    static float balanceFromSums (double ll, double rr)
    {
        const auto total = ll + rr;
        return total > 0.0 ? (float) juce::jlimit (-1.0, 1.0, (rr - ll) / total) : 0.0f;
    }

    /** Side energy as a fraction of mid + side energy:
        sum(((l-r)/2)^2) / (sum(((l+r)/2)^2) + sum(((l-r)/2)^2))
        which reduces to (ll + rr - 2lr) / (2 (ll + rr)).
    */
    static float widthFromSums (double ll, double rr, double lr)
    {
        const auto total = ll + rr;
        return total > 0.0 ? (float) juce::jlimit (0.0, 1.0, (total - 2.0 * lr) / (2.0 * total)) : 0.0f;
    }

    /** dB lost when summing to mono: the energy of (l+r)/2 against the
        per-channel average energy. 0 for identical channels, ~3 for
        uncorrelated or hard-panned material, large when phase-cancelling.
    */
    static float monoLossDbFromSums (double ll, double rr, double lr)
    {
        const auto total = ll + rr;

        if (total <= 0.0)
            return 0.0f;

        const auto ratio = std::max (1.0e-10, (total + 2.0 * lr) / (2.0 * total));
        return (float) std::min (100.0, std::max (0.0, -10.0 * std::log10 (ratio)));
    }
}

//==============================================================================
void StereoFieldAnalyser::prepare (double newSampleRate, int newNumChannels, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = std::max (1, newNumChannels);
    maxBlockSize = std::max (1, newMaxBlockSize);
    blockSamples = std::max (1, (int) std::llround (sampleRate * 0.1));

    decimationFactor = std::max (1, (int) std::llround (sampleRate / alignmentTargetRate));
    const auto decimatedRate = sampleRate / decimationFactor;
    alignmentWindowSamples = std::max (2, (int) std::llround (decimatedRate * alignmentWindowSeconds));
    maxLagSamples = juce::jlimit (1, alignmentWindowSamples / 4,
                                  (int) std::llround (decimatedRate * alignmentSearchSeconds));

    alignmentL.assign ((size_t) alignmentWindowSamples, 0.0f);
    alignmentR.assign ((size_t) alignmentWindowSamples, 0.0f);
    scratchL.assign ((size_t) alignmentWindowSamples, 0.0f);
    scratchR.assign ((size_t) alignmentWindowSamples, 0.0f);
    lagCurve.assign ((size_t) (2 * maxLagSamples + 1), 0.0f);

    resetRequested.store (false, std::memory_order_relaxed);
    resetMeasurement();
    publish();
}

void StereoFieldAnalyser::process (choc::buffer::ChannelArrayView<const float> block) noexcept
{
    processSamples (block.data.channels, (int) block.getNumChannels(),
                    (int) block.data.offset, (int) block.getNumFrames());
}

void StereoFieldAnalyser::process (const float* const* channelData, int numChannelsToUse, int numSamples) noexcept
{
    processSamples (channelData, numChannelsToUse, 0, numSamples);
}

void StereoFieldAnalyser::processSamples (const float* const* channelData, int numChannelsToUse,
                                          int startSample, int numSamples) noexcept
{
    if (channelData == nullptr || numSamples <= 0)
        return;

    if (resetRequested.exchange (false, std::memory_order_relaxed))
        resetMeasurement();

    numChannelsToUse = std::min (numChannelsToUse, numChannels);

    if (numChannelsToUse <= 0)
        return;

    auto left = channelData[0] + startSample;
    auto right = (numChannelsToUse >= 2 ? channelData[1] : channelData[0]) + startSample;

    for (int i = 0; i < numSamples; ++i)
    {
        const double l = left[i], r = right[i];

        blockSums.ll += l * l;
        blockSums.rr += r * r;
        blockSums.lr += l * r;

        pushAlignmentSample (l, r);

        if (++samplesIntoBlock >= blockSamples)
            finishBlock();
    }

    publish();
}

void StereoFieldAnalyser::flush() noexcept
{
    if (resetRequested.exchange (false, std::memory_order_relaxed))
        resetMeasurement();

    // The formulas are all energy ratios, so a partial block needs no scaling
    if (samplesIntoBlock >= blockSamples / 2)
    {
        finishBlock();
        publish();
    }
}

void StereoFieldAnalyser::requestReset() noexcept
{
    resetRequested.store (true, std::memory_order_relaxed);
}

StereoFieldAnalyser::Readings StereoFieldAnalyser::getReadings() const noexcept
{
    return publishedReadings.load();
}

//==============================================================================
void StereoFieldAnalyser::finishBlock() noexcept
{
    overallSums.ll += blockSums.ll;
    overallSums.rr += blockSums.rr;
    overallSums.lr += blockSums.lr;

    windowHistory[(size_t) windowPos] = blockSums;
    windowPos = (windowPos + 1) % windowBlocks;
    ++numBlocksSeen;

    blockSums = {};
    samplesIntoBlock = 0;

    updateReadings();
    updateAlignment();
}

void StereoFieldAnalyser::updateReadings() noexcept
{
    using namespace stereo_field_utils;

    BlockSums window;
    const auto blocksInWindow = std::min (numBlocksSeen, windowBlocks);

    for (int i = 0; i < blocksInWindow; ++i)
    {
        const auto& block = windowHistory[(size_t) i];
        window.ll += block.ll;
        window.rr += block.rr;
        window.lr += block.lr;
    }

    // The window readings hold their last values over silence, and are only
    // valid once a full 400ms of non-silent audio is in the window
    if (const auto correlation = correlationFromSums (window.ll, window.rr, window.lr, silenceThreshold))
    {
        currentReadings.correlation = *correlation;
        currentReadings.balance = balanceFromSums (window.ll, window.rr);
        currentReadings.width = widthFromSums (window.ll, window.rr, window.lr);
        currentReadings.windowValid = numBlocksSeen >= windowBlocks;

        if (currentReadings.windowValid)
            currentReadings.minCorrelation = std::min (currentReadings.minCorrelation, *correlation);
    }
    else
    {
        currentReadings.windowValid = false;
    }

    if (const auto correlation = correlationFromSums (overallSums.ll, overallSums.rr, overallSums.lr, silenceThreshold))
    {
        currentReadings.overallCorrelation = *correlation;
        currentReadings.overallBalance = balanceFromSums (overallSums.ll, overallSums.rr);
        currentReadings.overallWidth = widthFromSums (overallSums.ll, overallSums.rr, overallSums.lr);
        currentReadings.monoLossDb = monoLossDbFromSums (overallSums.ll, overallSums.rr, overallSums.lr);
        currentReadings.overallValid = true;
    }
}

void StereoFieldAnalyser::pushAlignmentSample (double l, double r) noexcept
{
    if (alignmentWindowSamples <= 0)
        return;

    decimateAccumL += l;
    decimateAccumR += r;

    if (++decimateCount < decimationFactor)
        return;

    const auto scale = 1.0 / decimationFactor;
    alignmentL[(size_t) alignmentPos] = (float) (decimateAccumL * scale);
    alignmentR[(size_t) alignmentPos] = (float) (decimateAccumR * scale);

    alignmentPos = (alignmentPos + 1) % alignmentWindowSamples;
    alignmentFilled = std::min (alignmentFilled + 1, alignmentWindowSamples);

    decimateCount = 0;
    decimateAccumL = 0.0;
    decimateAccumR = 0.0;
}

void StereoFieldAnalyser::updateAlignment() noexcept
{
    // Hold the previous estimate until there's a full window to search
    if (alignmentWindowSamples <= 0 || alignmentFilled < alignmentWindowSamples)
        return;

    const auto n = alignmentWindowSamples;

    // Linearise the ring buffers oldest-to-newest so the inner loop can index
    // straight through them
    for (int i = 0; i < n; ++i)
    {
        const auto src = (size_t) ((alignmentPos + i) % n);
        scratchL[(size_t) i] = alignmentL[src];
        scratchR[(size_t) i] = alignmentR[src];
    }

    double energyL = 0.0, energyR = 0.0;

    for (int i = 0; i < n; ++i)
    {
        energyL += (double) scratchL[(size_t) i] * scratchL[(size_t) i];
        energyR += (double) scratchR[(size_t) i] * scratchR[(size_t) i];
    }

    if (energyL < silenceThreshold || energyR < silenceThreshold)
    {
        currentReadings.alignmentValid = false;
        return;
    }

    // A single normalisation from the whole-window energies, rather than a
    // per-lag one: the lag range is a small fraction of the window, so the
    // difference is negligible and it keeps the search to one pass per lag
    const auto norm = 1.0 / std::sqrt (energyL * energyR);

    auto correlationAtLag = [this, n, norm] (int lag)
    {
        // sum over l[i] * r[i + lag], across the overlapping region only
        const auto first = std::max (0, -lag);
        const auto last = std::min (n, n - lag);
        double sum = 0.0;

        for (int i = first; i < last; ++i)
            sum += (double) scratchL[(size_t) i] * scratchR[(size_t) (i + lag)];

        return sum * norm;
    };

    const auto numLags = 2 * maxLagSamples + 1;
    auto bestIndex = 0;
    auto bestMagnitude = -1.0f;

    for (int i = 0; i < numLags; ++i)
    {
        const auto value = (float) correlationAtLag (i - maxLagSamples);
        lagCurve[(size_t) i] = value;

        if (std::abs (value) > bestMagnitude)
        {
            bestMagnitude = std::abs (value);
            bestIndex = i;
        }
    }

    if (bestMagnitude < alignmentMinCorrelation)
    {
        // Independent channels produce a low, flat curve with no real peak -
        // reporting its argmax would be inventing a number
        currentReadings.alignmentValid = false;
        return;
    }

    // A sustained tone correlates just as well at every multiple of its period,
    // so a tall peak is not by itself enough: require it to stand clear of the
    // best rival outside its own lobe, otherwise the offset is ambiguous
    const auto exclusion = std::max (2, maxLagSamples / 10);
    auto runnerUpMagnitude = 0.0f;

    for (int i = 0; i < numLags; ++i)
        if (std::abs (i - bestIndex) > exclusion)
            runnerUpMagnitude = std::max (runnerUpMagnitude, std::abs (lagCurve[(size_t) i]));

    if (bestMagnitude < runnerUpMagnitude * alignmentPeakRatio)
    {
        currentReadings.alignmentValid = false;
        return;
    }

    const auto bestLag = bestIndex - maxLagSamples;
    const auto bestValue = lagCurve[(size_t) bestIndex];

    // Parabolic interpolation about the peak, so the estimate is not limited to
    // whole decimated samples. Work on the curve scaled by the peak's sign so an
    // inverted-polarity match interpolates the same way.
    const auto sign = bestValue < 0.0f ? -1.0f : 1.0f;
    auto refinedLag = (double) bestLag;

    if (bestIndex > 0 && bestIndex < numLags - 1)
    {
        const auto y0 = sign * lagCurve[(size_t) (bestIndex - 1)];
        const auto y1 = sign * bestValue;
        const auto y2 = sign * lagCurve[(size_t) (bestIndex + 1)];
        const auto denom = y0 - 2.0f * y1 + y2;

        if (std::abs (denom) > 1.0e-12f)
            refinedLag += juce::jlimit (-1.0, 1.0, (double) (0.5f * (y0 - y2) / denom));
    }

    const auto delayInSourceSamples = refinedLag * decimationFactor;

    currentReadings.interChannelDelaySamples = (int) std::llround (delayInSourceSamples);
    currentReadings.interChannelDelayMs = (float) (delayInSourceSamples * 1000.0 / sampleRate);
    currentReadings.polarityInverted = bestValue < 0.0f;
    currentReadings.alignmentValid = true;
}

void StereoFieldAnalyser::publish() noexcept
{
    publishedReadings.store (currentReadings);
}

void StereoFieldAnalyser::resetMeasurement() noexcept
{
    samplesIntoBlock = 0;
    blockSums = {};
    windowHistory = {};
    windowPos = 0;
    numBlocksSeen = 0;
    overallSums = {};

    decimateCount = 0;
    decimateAccumL = 0.0;
    decimateAccumR = 0.0;
    alignmentPos = 0;
    alignmentFilled = 0;
    std::fill (alignmentL.begin(), alignmentL.end(), 0.0f);
    std::fill (alignmentR.begin(), alignmentR.end(), 0.0f);

    currentReadings = {};
}

} // namespace tracktion::inline engine
