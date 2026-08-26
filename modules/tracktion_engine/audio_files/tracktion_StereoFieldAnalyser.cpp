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
    currentReadings = {};
}

} // namespace tracktion::inline engine
