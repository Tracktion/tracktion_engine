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
    inline constexpr float silenceFloorDb = LoudnessMeter::silenceFloorDb;

    static double rounded (double value, int decimals = 1)
    {
        const auto scale = std::pow (10.0, decimals);
        return std::round (value * scale) / scale;
    }

    static double safeDb (double gain)
    {
        return std::max ((double) silenceFloorDb, 20.0 * std::log10 (std::max (1.0e-10, gain)));
    }

    //==============================================================================
    /** Peak, true peak, RMS, R128 loudness, clipping and silence ratio. */
    struct LevelAnalyser final : public AudioFileAnalyser
    {
        void prepare (const AudioFileInfo& info) override
        {
            sampleRate = info.sampleRate;
            numChannels = std::max (1, info.numChannels);
            gatingBlockSamples = std::max (1, (int) std::llround (sampleRate * 0.1));

            meter.prepare (sampleRate, numChannels, maxBlockSize);
        }

        void process (const juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            const auto numChannelsToUse = std::min (numChannels, buffer.getNumChannels());

            // Sample peak, sum of squares and clipped-sample count
            for (int ch = 0; ch < numChannelsToUse; ++ch)
            {
                auto data = buffer.getReadPointer (ch);

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

            // Loudness and true peak
            meter.process (choc::buffer::createChannelArrayView (buffer.getArrayOfReadPointers(),
                                                                 (choc::buffer::ChannelCount) numChannelsToUse,
                                                                 (choc::buffer::FrameCount) numSamples));

            // The silence ratio's per-block unweighted energy, on the same
            // 100ms grid as the meter's gating blocks
            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < numChannelsToUse; ++ch)
                {
                    const double sample = buffer.getSample (ch, i);
                    plainBlockEnergy += sample * sample;
                }

                if (++samplesIntoBlock >= gatingBlockSamples)
                    finishSilenceBlock();
            }
        }

        void finishSilenceBlock()
        {
            const auto blockRms = std::sqrt (plainBlockEnergy / (double) (gatingBlockSamples * numChannels));

            if (safeDb (blockRms) < -60.0)
                ++silentBlocks;

            plainBlockEnergy = 0.0;
            ++totalBlocks;
            samplesIntoBlock = 0;
        }

        void addResults (juce::DynamicObject& result) override
        {
            if (samplesIntoBlock >= gatingBlockSamples / 2)
                finishSilenceBlock();

            meter.flush();
            const auto readings = meter.getReadings();

            const auto rms = totalSamplesPerChannel > 0
                                ? std::sqrt (sumOfSquares / (double) (totalSamplesPerChannel * numChannels))
                                : 0.0;

            result.setProperty ("peakDb", rounded (safeDb (peak)));
            result.setProperty ("truePeakDb", rounded (readings.truePeakDb));
            result.setProperty ("rmsDb", rounded (safeDb (rms)));
            result.setProperty ("clippedSamples", (juce::int64) clippedSamples);
            result.setProperty ("silenceRatio", totalBlocks > 0 ? rounded ((double) silentBlocks / (double) totalBlocks, 2) : 0.0);

            result.setProperty ("integratedLufs", readings.integratedValid ? juce::var (rounded (readings.integratedLufs)) : juce::var());
            result.setProperty ("maxMomentaryLufs", readings.momentaryValid ? juce::var (rounded (readings.maxMomentaryLufs)) : juce::var());
            result.setProperty ("maxShortTermLufs", readings.shortTermValid ? juce::var (rounded (readings.maxShortTermLufs)) : juce::var());
            result.setProperty ("loudnessRangeLu", readings.loudnessRangeValid ? juce::var (rounded (readings.loudnessRangeLu)) : juce::var());
        }

        static constexpr int maxBlockSize = 8192;

        LoudnessMeter meter;

        double sampleRate = 44100.0;
        int numChannels = 1;

        float peak = 0.0f;
        double sumOfSquares = 0.0;
        int64_t totalSamplesPerChannel = 0, clippedSamples = 0;

        int gatingBlockSamples = 4410, samplesIntoBlock = 0;
        double plainBlockEnergy = 0.0;
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
                                                        const std::vector<AudioFileAnalyser*>& analysers,
                                                        const std::function<bool()>& shouldAbort)
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
        if (shouldAbort && shouldAbort())
            return tl::unexpected (TRANS("Analysis cancelled"));

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

    return analyseAudioFile (engine, file, analysers, options.shouldAbort);
}

} // namespace tracktion::inline engine
