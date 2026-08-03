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

namespace loudness_utils
{
    static double safeDb (double gain)
    {
        return std::max ((double) LoudnessMeter::silenceFloorDb,
                         20.0 * std::log10 (std::max (1.0e-10, gain)));
    }

    static double powerToLoudness (double power)
    {
        return -0.691 + 10.0 * std::log10 (std::max (1.0e-12, power));
    }

    /** BS.1770 channel weights for a buffer in the usual ITU order
        (L, R, C, LFE, Ls, Rs...): 1.0 for the front channels, 1.41 for the
        surrounds, and the LFE excluded from the measurement entirely.

        Plain buffers carry no layout information, so the LFE has to be found by
        position. Only the 5.1 and 7.1 layouts have one, and in both it sits at
        index 3; 5.0 and below are taken to have no LFE.
    */
    static double channelWeight (int channelIndex, int totalChannels)
    {
        if (totalChannels >= 6 && channelIndex == 3)
            return 0.0;

        return channelIndex >= 3 ? 1.41 : 1.0;
    }
}

//==============================================================================
void LoudnessMeter::setKWeightingCoefficients (Biquad& shelf, Biquad& highpass, double sampleRate)
{
    // The BS.1770-4 K-weighting: a high-shelf "head" pre-filter followed by an
    // RLB high-pass, designed for the given sample rate from the analog
    // prototypes published in the spec.
    {
        const double f0 = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double q = 0.7071752369554196;

        const double k = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        shelf.b0 = (vh + vb * k / q + k * k) / a0;
        shelf.b1 = 2.0 * (k * k - vh) / a0;
        shelf.b2 = (vh - vb * k / q + k * k) / a0;
        shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        shelf.a2 = (1.0 - k / q + k * k) / a0;
    }

    {
        const double f0 = 38.13547087602444;
        const double q = 0.5003270373238773;

        const double k = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
        const double a0 = 1.0 + k / q + k * k;

        highpass.b0 = 1.0;
        highpass.b1 = -2.0;
        highpass.b2 = 1.0;
        highpass.a1 = 2.0 * (k * k - 1.0) / a0;
        highpass.a2 = (1.0 - k / q + k * k) / a0;
    }
}

//==============================================================================
void LoudnessMeter::prepare (double newSampleRate, int newNumChannels, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = std::max (1, newNumChannels);
    maxBlockSize = std::max (1, newMaxBlockSize);
    gatingBlockSamples = std::max (1, (int) std::llround (sampleRate * 0.1));

    channels.assign ((size_t) numChannels, {});

    for (auto& state : channels)
        setKWeightingCoefficients (state.shelf, state.highpass, sampleRate);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                      (size_t) numChannels, 2,
                      juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    oversampler->initProcessing ((size_t) maxBlockSize);

    resetRequested.store (false, std::memory_order_relaxed);
    resetMeasurement();
}

void LoudnessMeter::process (choc::buffer::ChannelArrayView<const float> block) noexcept
{
    processSamples (block.data.channels, (int) block.getNumChannels(),
                    (int) block.data.offset, (int) block.getNumFrames());
}

void LoudnessMeter::process (const float* const* channelData, int numChannelsToUse, int numSamples) noexcept
{
    processSamples (channelData, numChannelsToUse, 0, numSamples);
}

void LoudnessMeter::processSamples (const float* const* channelData, int numChannelsToUse,
                                    int startSample, int numSamples) noexcept
{
    if (channelData == nullptr || channels.empty() || numSamples <= 0)
        return;

    if (resetRequested.exchange (false, std::memory_order_relaxed))
        resetMeasurement();

    numChannelsToUse = std::min (numChannelsToUse, numChannels);

    if (numChannelsToUse <= 0)
        return;

    for (int start = 0; start < numSamples; start += maxBlockSize)
        processChunk (channelData, numChannelsToUse, startSample + start,
                      std::min (maxBlockSize, numSamples - start));

    publishPeaks();
}

void LoudnessMeter::processChunk (const float* const* channelData, int numChannelsToUse,
                                  int startSample, int numSamples) noexcept
{
    using namespace loudness_utils;

    // Sample peak
    for (int ch = 0; ch < numChannelsToUse; ++ch)
    {
        auto data = channelData[ch] + startSample;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (data[i]));
    }

    // True peak from the 4x oversampled signal
    juce::dsp::AudioBlock<const float> block (channelData, (size_t) numChannelsToUse,
                                              (size_t) startSample, (size_t) numSamples);
    auto upsampled = oversampler->processSamplesUp (block);

    for (size_t ch = 0; ch < std::min ((size_t) numChannelsToUse, upsampled.getNumChannels()); ++ch)
    {
        auto data = upsampled.getChannelPointer (ch);

        for (size_t i = 0; i < upsampled.getNumSamples(); ++i)
            truePeak = std::max (truePeak, std::abs (data[i]));
    }

    // K-weighted energy in 100ms gating blocks
    for (int i = 0; i < numSamples; ++i)
    {
        double weightedSquares = 0.0;

        for (int ch = 0; ch < numChannelsToUse; ++ch)
        {
            auto& state = channels[(size_t) ch];
            const double sample = channelData[ch][startSample + i];
            const double filtered = state.highpass.process (state.shelf.process (sample));

            weightedSquares += channelWeight (ch, numChannelsToUse) * filtered * filtered;
        }

        blockEnergy += weightedSquares;

        if (++samplesIntoBlock >= gatingBlockSamples)
            finishGatingBlock();
    }
}

void LoudnessMeter::finishGatingBlock() noexcept
{
    using namespace loudness_utils;

    // Note the division by the full block length even for a partial block
    // flushed at the end of a file - this matches the offline analyser
    const auto blockPower = blockEnergy / (double) gatingBlockSamples;
    blockEnergy = 0.0;
    samplesIntoBlock = 0;

    blockPowers[(size_t) blockPowerPos] = blockPower;
    blockPowerPos = (blockPowerPos + 1) % shortTermBlocks;
    ++numGatingBlocks;

    if (numGatingBlocks >= momentaryBlocks)
    {
        const auto momentaryPower = sumOfLastBlockPowers (momentaryBlocks) / (double) momentaryBlocks;
        const auto momentaryLufs = powerToLoudness (momentaryPower);

        currentReadings.momentaryLufs = (float) momentaryLufs;
        currentReadings.maxMomentaryLufs = std::max (currentReadings.maxMomentaryLufs, (float) momentaryLufs);

        addToHistogram (momentaryPower, momentaryLufs);
        updateIntegrated();
    }

    if (numGatingBlocks >= shortTermBlocks)
    {
        const auto shortTermPower = sumOfLastBlockPowers (shortTermBlocks) / (double) shortTermBlocks;
        const auto shortTermLufs = powerToLoudness (shortTermPower);

        currentReadings.shortTermLufs = (float) shortTermLufs;
        currentReadings.maxShortTermLufs = std::max (currentReadings.maxShortTermLufs, (float) shortTermLufs);

        addToShortTermHistogram (shortTermPower, shortTermLufs);
        updateLoudnessRange();
    }

    currentReadings.momentaryValid = numGatingBlocks >= momentaryBlocks;
    currentReadings.shortTermValid = numGatingBlocks >= shortTermBlocks;

    publish();
}

double LoudnessMeter::sumOfLastBlockPowers (int numBlocks) const noexcept
{
    double sum = 0.0;

    for (int i = 1; i <= numBlocks; ++i)
        sum += blockPowers[(size_t) ((blockPowerPos - i + shortTermBlocks) % shortTermBlocks)];

    return sum;
}

void LoudnessMeter::addToHistogram (double power, double loudness) noexcept
{
    // The absolute gate is the histogram's lower bound
    if (loudness <= histogramFloorLufs)
        return;

    const auto bin = juce::jlimit (0, numHistogramBins - 1,
                                   (int) ((loudness - histogramFloorLufs) / histogramBinWidthLu));

    binPowerSums[(size_t) bin] += power;
    ++binCounts[(size_t) bin];

    histogramPowerSum += power;
    ++histogramCount;
}

void LoudnessMeter::updateIntegrated() noexcept
{
    using namespace loudness_utils;

    if (histogramCount == 0)
    {
        currentReadings.integratedValid = false;
        return;
    }

    // The relative gate sits 10 LU below the mean of the absolutely-gated blocks
    const auto relativeGateLufs = powerToLoudness (histogramPowerSum / (double) histogramCount) - 10.0;
    const auto firstBin = juce::jlimit (0, numHistogramBins,
                                        (int) std::ceil ((relativeGateLufs - histogramFloorLufs) / histogramBinWidthLu));

    double sum = 0.0;
    int64_t count = 0;

    for (int bin = firstBin; bin < numHistogramBins; ++bin)
    {
        sum += binPowerSums[(size_t) bin];
        count += binCounts[(size_t) bin];
    }

    if (count == 0)
    {
        currentReadings.integratedValid = false;
        return;
    }

    currentReadings.integratedLufs = (float) powerToLoudness (sum / (double) count);
    currentReadings.integratedValid = true;
}

void LoudnessMeter::addToShortTermHistogram (double power, double loudness) noexcept
{
    if (loudness <= histogramFloorLufs)
        return;

    const auto bin = juce::jlimit (0, numHistogramBins - 1,
                                   (int) ((loudness - histogramFloorLufs) / histogramBinWidthLu));

    ++shortTermBinCounts[(size_t) bin];
    shortTermPowerSum += power;
    ++shortTermCount;
}

void LoudnessMeter::updateLoudnessRange() noexcept
{
    using namespace loudness_utils;

    if (shortTermCount == 0)
    {
        currentReadings.loudnessRangeValid = false;
        return;
    }

    // EBU Tech 3342: the same -70 LUFS absolute gate as the integrated
    // measurement, but a relative gate 20 LU down, then the spread between the
    // 10th and 95th percentiles of what's left
    const auto relativeGateLufs = powerToLoudness (shortTermPowerSum / (double) shortTermCount) - 20.0;
    const auto firstBin = juce::jlimit (0, numHistogramBins,
                                        (int) std::ceil ((relativeGateLufs - histogramFloorLufs) / histogramBinWidthLu));

    int64_t total = 0;

    for (int bin = firstBin; bin < numHistogramBins; ++bin)
        total += shortTermBinCounts[(size_t) bin];

    if (total == 0)
    {
        currentReadings.loudnessRangeValid = false;
        return;
    }

    auto loudnessAtPercentile = [this, firstBin, total] (double percentile)
    {
        const auto target = (int64_t) std::llround ((double) (total - 1) * percentile);
        int64_t accumulated = 0;

        for (int bin = firstBin; bin < numHistogramBins; ++bin)
        {
            accumulated += shortTermBinCounts[(size_t) bin];

            if (accumulated > target)
                return histogramFloorLufs + bin * histogramBinWidthLu;
        }

        return histogramFloorLufs + (numHistogramBins - 1) * histogramBinWidthLu;
    };

    const auto range = loudnessAtPercentile (0.95) - loudnessAtPercentile (0.1);

    currentReadings.loudnessRangeLu = (float) std::max (0.0, range);
    currentReadings.loudnessRangeValid = true;
}

void LoudnessMeter::publishPeaks() noexcept
{
    using namespace loudness_utils;

    currentReadings.samplePeakDb = (float) safeDb (peak);
    currentReadings.truePeakDb = (float) safeDb (std::max (peak, truePeak));

    publish();
}

void LoudnessMeter::publish() noexcept
{
    publishedReadings.store (currentReadings);
}

void LoudnessMeter::flush() noexcept
{
    if (samplesIntoBlock >= gatingBlockSamples / 2)
        finishGatingBlock();

    publishPeaks();
}

void LoudnessMeter::requestReset() noexcept
{
    resetRequested.store (true, std::memory_order_relaxed);
}

void LoudnessMeter::resetMeasurement() noexcept
{
    samplesIntoBlock = 0;
    blockEnergy = 0.0;
    blockPowers.fill (0.0);
    blockPowerPos = 0;
    numGatingBlocks = 0;

    binPowerSums.fill (0.0);
    binCounts.fill (0);
    histogramPowerSum = 0.0;
    histogramCount = 0;

    shortTermBinCounts.fill (0);
    shortTermPowerSum = 0.0;
    shortTermCount = 0;

    peak = 0.0f;
    truePeak = 0.0f;

    currentReadings = Readings();
    publish();
}

LoudnessMeter::Readings LoudnessMeter::getReadings() const noexcept
{
    return publishedReadings.load();
}

} // namespace tracktion::inline engine
