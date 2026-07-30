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

namespace audio_analysis_utils
{
    inline constexpr float silenceFloorDb = -100.0f;

    static double rounded (double value, int decimals = 1)
    {
        const auto scale = std::pow (10.0, decimals);
        return std::round (value * scale) / scale;
    }

    static double safeDb (double gain)
    {
        return std::max ((double) silenceFloorDb, 20.0 * std::log10 (std::max (1.0e-10, gain)));
    }

    /** A direct-form biquad on doubles, for the K-weighting filters. */
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        double process (double x)
        {
            const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    /** The BS.1770-4 K-weighting: a high-shelf "head" pre-filter followed by
        an RLB high-pass, designed for the given sample rate from the analog
        prototypes published in the spec.
    */
    static std::pair<Biquad, Biquad> makeKWeighting (double sampleRate)
    {
        Biquad shelf, highpass;

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

        return { shelf, highpass };
    }

    /** BS.1770 channel weights: 1.0 for left/right/centre, 1.41 for surrounds.
        The LFE should be excluded, but plain audio files carry no layout info.
    */
    static double channelWeight (int channelIndex)
    {
        return channelIndex >= 3 ? 1.41 : 1.0;
    }

    static double powerToLoudness (double power)
    {
        return -0.691 + 10.0 * std::log10 (std::max (1.0e-12, power));
    }

    //==============================================================================
    /** Peak, true peak, RMS, R128 loudness, clipping and silence ratio. */
    struct LevelAnalyser final : public AudioFileAnalyser
    {
        void prepare (const AudioFileInfo& info) override
        {
            sampleRate = info.sampleRate;
            numChannels = std::max (1, info.numChannels);

            auto [shelf, highpass] = makeKWeighting (sampleRate);
            channels.clear();

            for (int i = 0; i < numChannels; ++i)
                channels.push_back ({ shelf, highpass, 0.0 });

            gatingBlockSamples = std::max (1, (int) std::llround (sampleRate * 0.1));

            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                              (size_t) numChannels, 2,
                              juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
            oversampler->initProcessing ((size_t) maxBlockSize);
        }

        void process (const juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            for (int start = 0; start < numSamples; start += maxBlockSize)
            {
                const auto numThisTime = std::min (maxBlockSize, numSamples - start);
                processBlock (buffer, start, numThisTime);
            }
        }

        void processBlock (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
        {
            // Sample peak, sum of squares and clipped-sample count
            for (int ch = 0; ch < std::min (numChannels, buffer.getNumChannels()); ++ch)
            {
                auto data = buffer.getReadPointer (ch, startSample);

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto value = std::abs (data[i]);
                    peak = std::max (peak, value);
                    sumOfSquares += (double) value * (double) value;

                    if (value > 0.999f)
                        ++clippedSamples;
                }
            }

            totalSamplesPerChannel += numSamples;

            // True peak from the 4x oversampled signal
            juce::dsp::AudioBlock<const float> block (buffer.getArrayOfReadPointers(),
                                                      (size_t) std::min (numChannels, buffer.getNumChannels()),
                                                      (size_t) startSample, (size_t) numSamples);
            auto upsampled = oversampler->processSamplesUp (block);

            for (size_t ch = 0; ch < upsampled.getNumChannels(); ++ch)
                for (size_t i = 0; i < upsampled.getNumSamples(); ++i)
                    truePeak = std::max (truePeak, std::abs (upsampled.getSample ((int) ch, (int) i)));

            // K-weighted energy in 100ms gating blocks, and the silence ratio's
            // per-block unweighted energy
            for (int i = 0; i < numSamples; ++i)
            {
                double weightedSquares = 0.0;

                for (int ch = 0; ch < std::min (numChannels, buffer.getNumChannels()); ++ch)
                {
                    auto& state = channels[(size_t) ch];
                    const double sample = buffer.getSample (ch, startSample + i);
                    const double filtered = state.highpass.process (state.shelf.process (sample));

                    weightedSquares += channelWeight (ch) * filtered * filtered;
                    state.plainBlockEnergy += sample * sample;
                }

                blockEnergy += weightedSquares;

                if (++samplesIntoBlock >= gatingBlockSamples)
                    finishGatingBlock();
            }
        }

        void finishGatingBlock()
        {
            blockEnergies.push_back (blockEnergy / (double) gatingBlockSamples);
            blockEnergy = 0.0;

            double plainEnergy = 0.0;

            for (auto& state : channels)
            {
                plainEnergy += state.plainBlockEnergy;
                state.plainBlockEnergy = 0.0;
            }

            const auto blockRms = std::sqrt (plainEnergy / (double) (gatingBlockSamples * (int) channels.size()));

            if (safeDb (blockRms) < -60.0)
                ++silentBlocks;

            ++totalBlocks;
            samplesIntoBlock = 0;
        }

        void addResults (juce::DynamicObject& result) override
        {
            if (samplesIntoBlock >= gatingBlockSamples / 2)
                finishGatingBlock();

            const auto rms = totalSamplesPerChannel > 0
                                ? std::sqrt (sumOfSquares / (double) (totalSamplesPerChannel * numChannels))
                                : 0.0;

            result.setProperty ("peakDb", rounded (safeDb (peak)));
            result.setProperty ("truePeakDb", rounded (safeDb (std::max (peak, truePeak))));
            result.setProperty ("rmsDb", rounded (safeDb (rms)));
            result.setProperty ("clippedSamples", clippedSamples);
            result.setProperty ("silenceRatio", totalBlocks > 0 ? rounded ((double) silentBlocks / (double) totalBlocks, 2) : 0.0);

            addLoudness (result);
        }

        void addLoudness (juce::DynamicObject& result)
        {
            // Momentary loudness: 400ms windows at the 100ms gating-block hop
            std::vector<double> momentaryPowers;
            double maxMomentary = silenceFloorDb, maxShortTerm = silenceFloorDb;

            for (size_t i = 3; i < blockEnergies.size(); ++i)
            {
                const auto power = (blockEnergies[i - 3] + blockEnergies[i - 2]
                                     + blockEnergies[i - 1] + blockEnergies[i]) / 4.0;
                momentaryPowers.push_back (power);
                maxMomentary = std::max (maxMomentary, powerToLoudness (power));
            }

            // Short-term: 3s windows at the same hop
            for (size_t i = 29; i < blockEnergies.size(); ++i)
            {
                double power = 0.0;

                for (size_t j = i - 29; j <= i; ++j)
                    power += blockEnergies[j];

                maxShortTerm = std::max (maxShortTerm, powerToLoudness (power / 30.0));
            }

            // Integrated: mean power of the momentary blocks that pass the
            // -70 LUFS absolute gate, then the relative gate 10 LU below the
            // mean of those
            auto gatedMean = [&momentaryPowers] (double gateLufs)
            {
                double sum = 0.0;
                int count = 0;

                for (auto power : momentaryPowers)
                {
                    if (powerToLoudness (power) > gateLufs)
                    {
                        sum += power;
                        ++count;
                    }
                }

                return count > 0 ? std::optional<double> (sum / count) : std::nullopt;
            };

            std::optional<double> integrated;

            if (auto absoluteGated = gatedMean (-70.0))
                if (auto relativeGated = gatedMean (powerToLoudness (*absoluteGated) - 10.0))
                    integrated = powerToLoudness (*relativeGated);

            result.setProperty ("integratedLufs", integrated ? juce::var (rounded (*integrated)) : juce::var());
            result.setProperty ("maxMomentaryLufs", momentaryPowers.empty() ? juce::var() : juce::var (rounded (maxMomentary)));
            result.setProperty ("maxShortTermLufs", blockEnergies.size() < 30 ? juce::var() : juce::var (rounded (maxShortTerm)));
        }

        struct ChannelState
        {
            Biquad shelf, highpass;
            double plainBlockEnergy = 0.0;
        };

        static constexpr int maxBlockSize = 8192;

        double sampleRate = 44100.0;
        int numChannels = 1;
        std::vector<ChannelState> channels;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

        float peak = 0.0f, truePeak = 0.0f;
        double sumOfSquares = 0.0;
        int64_t totalSamplesPerChannel = 0, clippedSamples = 0;

        int gatingBlockSamples = 4410, samplesIntoBlock = 0;
        double blockEnergy = 0.0;
        std::vector<double> blockEnergies;
        int silentBlocks = 0, totalBlocks = 0;
    };

    //==============================================================================
    /** Third-octave band energies, spectral centroid/rolloff and balance. */
    struct SpectralAnalyser final : public AudioFileAnalyser
    {
        void prepare (const AudioFileInfo& info) override
        {
            sampleRate = info.sampleRate;
            numChannels = std::max (1, info.numChannels);

            fifo.resize ((size_t) fftSize, 0.0f);
            averageSpectrum.assign ((size_t) fftSize / 2, 0.0);
            window.resize ((size_t) fftSize);
            juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) fftSize,
                                                                      juce::dsp::WindowingFunction<float>::hann, false);
        }

        void process (const juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            for (int i = 0; i < numSamples; ++i)
            {
                // Mono downmix
                float value = 0.0f;

                for (int ch = 0; ch < std::min (numChannels, buffer.getNumChannels()); ++ch)
                    value += buffer.getSample (ch, i);

                fifo[(size_t) fifoPos++] = value / (float) numChannels;

                if (fifoPos == fftSize)
                    analyseFrame();
            }
        }

        void analyseFrame()
        {
            std::vector<float> frame ((size_t) fftSize * 2, 0.0f);

            for (int i = 0; i < fftSize; ++i)
                frame[(size_t) i] = fifo[(size_t) i] * window[(size_t) i];

            fft.performFrequencyOnlyForwardTransform (frame.data(), true);

            for (size_t bin = 0; bin < averageSpectrum.size(); ++bin)
                averageSpectrum[bin] += (double) frame[bin] * (double) frame[bin];

            ++numFrames;

            // 50% hop
            std::copy (fifo.begin() + fftSize / 2, fifo.end(), fifo.begin());
            fifoPos = fftSize / 2;
        }

        void addResults (juce::DynamicObject& result) override
        {
            auto spectrum = new juce::DynamicObject();

            if (numFrames == 0)
            {
                // Analyse whatever partial frame we have rather than nothing
                if (fifoPos > 0)
                {
                    std::fill (fifo.begin() + fifoPos, fifo.end(), 0.0f);
                    fifoPos = fftSize;
                    analyseFrame();
                }

                if (numFrames == 0)
                {
                    result.setProperty ("spectrum", juce::var (spectrum));
                    return;
                }
            }

            const auto binHz = sampleRate / (double) fftSize;

            // Third-octave bands from 20Hz to 20kHz
            static constexpr double bandCentres[] = { 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0,
                                                      200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0,
                                                      2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0 };
            constexpr auto numBands = (int) std::size (bandCentres);
            const auto thirdOctave = std::pow (2.0, 1.0 / 6.0);   // half a band width each way

            juce::Array<juce::var> bandHz, bandDb;
            std::vector<double> bandEnergies ((size_t) numBands, 0.0);

            double totalEnergy = 0.0, weightedFrequency = 0.0;
            double lowEnergy = 0.0, midEnergy = 0.0, highEnergy = 0.0;

            for (size_t bin = 1; bin < averageSpectrum.size(); ++bin)
            {
                const auto frequency = binHz * (double) bin;
                const auto energy = averageSpectrum[bin] / (double) numFrames;

                totalEnergy += energy;
                weightedFrequency += energy * frequency;

                if (frequency < 250.0)          lowEnergy += energy;
                else if (frequency < 4000.0)    midEnergy += energy;
                else                            highEnergy += energy;

                for (int band = 0; band < numBands; ++band)
                {
                    if (frequency >= bandCentres[band] / thirdOctave && frequency < bandCentres[band] * thirdOctave)
                    {
                        bandEnergies[(size_t) band] += energy;
                        break;
                    }
                }
            }

            // Bands as dB relative to the loudest band
            const auto maxBandEnergy = *std::max_element (bandEnergies.begin(), bandEnergies.end());

            for (int band = 0; band < numBands; ++band)
            {
                bandHz.add ((int) bandCentres[band]);
                bandDb.add (maxBandEnergy > 0.0
                              ? rounded (std::max ((double) silenceFloorDb,
                                                   10.0 * std::log10 (std::max (1.0e-12, bandEnergies[(size_t) band] / maxBandEnergy))))
                              : (double) silenceFloorDb);
            }

            // 85% energy rolloff
            double rolloffHz = 0.0, cumulative = 0.0;

            for (size_t bin = 1; bin < averageSpectrum.size(); ++bin)
            {
                cumulative += averageSpectrum[bin] / (double) numFrames;

                if (cumulative >= totalEnergy * 0.85)
                {
                    rolloffHz = binHz * (double) bin;
                    break;
                }
            }

            spectrum->setProperty ("bandHz", bandHz);
            spectrum->setProperty ("bandDb", bandDb);
            spectrum->setProperty ("centroidHz", totalEnergy > 0.0 ? (int) std::llround (weightedFrequency / totalEnergy) : 0);
            spectrum->setProperty ("rolloffHz", (int) std::llround (rolloffHz));

            auto balance = new juce::DynamicObject();
            const auto toPercent = [totalEnergy] (double energy) { return totalEnergy > 0.0 ? (int) std::llround (100.0 * energy / totalEnergy) : 0; };
            balance->setProperty ("lowPct", toPercent (lowEnergy));
            balance->setProperty ("midPct", toPercent (midEnergy));
            balance->setProperty ("highPct", toPercent (highEnergy));
            spectrum->setProperty ("balance", juce::var (balance));

            result.setProperty ("spectrum", juce::var (spectrum));
        }

        static constexpr int fftOrder = 12;
        static constexpr int fftSize = 1 << fftOrder;

        double sampleRate = 44100.0;
        int numChannels = 1;
        juce::dsp::FFT fft { fftOrder };
        std::vector<float> fifo, window;
        int fifoPos = 0;
        std::vector<double> averageSpectrum;
        int numFrames = 0;
    };

    //==============================================================================
    /** A downsampled peak/RMS envelope exposing the file's dynamics over time. */
    struct EnvelopeAnalyser final : public AudioFileAnalyser
    {
        explicit EnvelopeAnalyser (int numPointsToUse) : numPoints (juce::jlimit (2, 1000, numPointsToUse)) {}

        void prepare (const AudioFileInfo& info) override
        {
            sampleRate = info.sampleRate;
            numChannels = std::max (1, info.numChannels);
            windowSamples = std::max ((SampleCount) 1, info.lengthInSamples / numPoints);
        }

        void process (const juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < std::min (numChannels, buffer.getNumChannels()); ++ch)
                {
                    const auto value = std::abs (buffer.getSample (ch, i));
                    windowPeak = std::max (windowPeak, value);
                    windowSumOfSquares += (double) value * (double) value;
                }

                if (++samplesIntoWindow >= windowSamples)
                    finishWindow();
            }
        }

        void finishWindow()
        {
            peakDb.add (rounded (safeDb (windowPeak)));
            rmsDb.add (rounded (safeDb (std::sqrt (windowSumOfSquares / (double) (samplesIntoWindow * numChannels)))));

            windowPeak = 0.0f;
            windowSumOfSquares = 0.0;
            samplesIntoWindow = 0;
        }

        void addResults (juce::DynamicObject& result) override
        {
            if (samplesIntoWindow > 0 && peakDb.size() < numPoints)
                finishWindow();

            auto envelope = new juce::DynamicObject();
            envelope->setProperty ("windowSeconds", rounded ((double) windowSamples / sampleRate, 3));
            envelope->setProperty ("peakDb", peakDb);
            envelope->setProperty ("rmsDb", rmsDb);
            result.setProperty ("envelope", juce::var (envelope));
        }

        const int numPoints;
        double sampleRate = 44100.0;
        int numChannels = 1;
        SampleCount windowSamples = 1, samplesIntoWindow = 0;
        float windowPeak = 0.0f;
        double windowSumOfSquares = 0.0;
        juce::Array<juce::var> peakDb, rmsDb;
    };
}

//==============================================================================
tl::expected<juce::var, juce::String> analyseAudioFile (Engine& engine, const juce::File& file,
                                                        const std::vector<AudioFileAnalyser*>& analysers)
{
    using namespace audio_analysis_utils;

    if (! file.existsAsFile())
        return tl::unexpected (TRANS("File not found: ") + file.getFullPathName());

    std::unique_ptr<juce::AudioFormatReader> reader (AudioFileUtils::createReaderFor (engine, file));

    if (reader == nullptr)
        return tl::unexpected (TRANS("Couldn't read the audio file: ") + file.getFullPathName());

    const auto info = AudioFile (engine, file).getInfo();

    for (auto analyser : analysers)
        analyser->prepare (info);

    constexpr int blockSize = 65536;
    const auto numChannels = std::max (1, (int) reader->numChannels);
    juce::AudioBuffer<float> buffer (numChannels, blockSize);

    for (SampleCount position = 0; position < (SampleCount) reader->lengthInSamples;)
    {
        const auto numThisTime = (int) std::min ((SampleCount) blockSize,
                                                 (SampleCount) reader->lengthInSamples - position);

        if (! reader->read (&buffer, 0, numThisTime, position, true, numChannels > 1))
            return tl::unexpected (TRANS("Couldn't read the audio file: ") + file.getFullPathName());

        for (auto analyser : analysers)
            analyser->process (buffer, numThisTime);

        position += numThisTime;
    }

    auto result = new juce::DynamicObject();
    result->setProperty ("sampleRate", info.sampleRate);
    result->setProperty ("channels", info.numChannels);
    result->setProperty ("seconds", rounded (info.getLengthInSeconds(), 3));

    for (auto analyser : analysers)
        analyser->addResults (*result);

    return juce::var (result);
}

tl::expected<juce::var, juce::String> analyseAudioFile (Engine& engine, const juce::File& file,
                                                        const AudioAnalysisOptions& options)
{
    using namespace audio_analysis_utils;

    std::vector<std::unique_ptr<AudioFileAnalyser>> owned;

    if (options.levels)     owned.push_back (std::make_unique<LevelAnalyser>());
    if (options.spectrum)   owned.push_back (std::make_unique<SpectralAnalyser>());
    if (options.envelope)   owned.push_back (std::make_unique<EnvelopeAnalyser> (options.envelopePoints));

    std::vector<AudioFileAnalyser*> analysers;

    for (auto& analyser : owned)
        analysers.push_back (analyser.get());

    return analyseAudioFile (engine, file, analysers);
}

} // namespace tracktion::inline engine
