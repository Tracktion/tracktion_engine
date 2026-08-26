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
            transientBlockSamples = std::max (1, (int) std::llround (sampleRate * 0.05));

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
            // 100ms grid as the meter's gating blocks, and the transient
            // counter's finer 50ms grid
            for (int i = 0; i < numSamples; ++i)
            {
                double frameEnergy = 0.0;

                for (int ch = 0; ch < numChannelsToUse; ++ch)
                {
                    const double sample = buffer.getSample (ch, i);
                    frameEnergy += sample * sample;
                }

                plainBlockEnergy += frameEnergy;
                transientBlockEnergy += frameEnergy;

                if (++samplesIntoBlock >= gatingBlockSamples)
                    finishSilenceBlock();

                if (++samplesIntoTransientBlock >= transientBlockSamples)
                    finishTransientBlock();
            }
        }

        void finishTransientBlock()
        {
            // An onset is a block whose energy jumps well above the recent
            // average - the same idea as the engine's BeatDetect, kept coarse
            // on purpose: this is a density statistic, not a beat grid
            constexpr int historyBlocks = 20;   // 1s of history
            constexpr double onsetRatio = 4.0;  // 6dB above the running mean

            if ((int) energyHistory.size() >= historyBlocks)
            {
                const auto mean = historyEnergySum / (double) energyHistory.size();

                if (transientBlockEnergy > onsetRatio * std::max (1.0e-12, mean) && ! previousBlockWasOnset)
                {
                    ++numTransients;
                    previousBlockWasOnset = true;
                }
                else
                {
                    previousBlockWasOnset = false;
                }

                historyEnergySum -= energyHistory.front();
                energyHistory.pop_front();
            }

            energyHistory.push_back (transientBlockEnergy);
            historyEnergySum += transientBlockEnergy;
            transientBlockEnergy = 0.0;
            samplesIntoTransientBlock = 0;
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

            // Derived dynamics statistics: how squashed is this?
            // Crest = sample peak over RMS; PLR = true peak over integrated
            // loudness; PSR = true peak over the loudest short-term window
            auto dynamics = new juce::DynamicObject();
            dynamics->setProperty ("crestFactorDb", rounded (safeDb (peak) - safeDb (rms)));
            dynamics->setProperty ("plrDb", readings.integratedValid ? juce::var (rounded (readings.truePeakDb - readings.integratedLufs)) : juce::var());
            dynamics->setProperty ("psrDb", readings.shortTermValid ? juce::var (rounded (readings.truePeakDb - readings.maxShortTermLufs)) : juce::var());

            const auto seconds = (double) totalSamplesPerChannel / sampleRate;
            dynamics->setProperty ("transientsPerSecond", seconds > 1.0 ? juce::var (rounded ((double) numTransients / seconds, 2)) : juce::var());
            result.setProperty ("dynamics", juce::var (dynamics));
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

        int transientBlockSamples = 2205, samplesIntoTransientBlock = 0;
        double transientBlockEnergy = 0.0, historyEnergySum = 0.0;
        std::deque<double> energyHistory;
        int numTransients = 0;
        bool previousBlockWasOnset = false;
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
    /** A time x frequency-band level matrix with per-band summary stats and
        buildup detection: the "how does each part of the spectrum move over
        time" view the whole-file SpectralAnalyser average can't give.

        Levels are in dB relative to the loudest band/slice cell, so 0 is the
        loudest moment anywhere in the spectrogram.
    */
    struct SpectrogramAnalyser final : public AudioFileAnalyser
    {
        SpectrogramAnalyser (int numSlicesToUse, bool useThirdOctave)
            : requestedSlices (juce::jlimit (1, 1000, numSlicesToUse)),
              thirdOctave (useThirdOctave)
        {}

        struct Band
        {
            juce::String name;
            double lowHz, highHz;
        };

        void prepare (const AudioFileInfo& info) override
        {
            sampleRate = info.sampleRate;
            numChannels = std::max (1, info.numChannels);
            lengthSeconds = info.getLengthInSeconds();

            fifo.resize ((size_t) fftSize, 0.0f);
            window.resize ((size_t) fftSize);
            juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) fftSize,
                                                                      juce::dsp::WindowingFunction<float>::hann, false);

            if (thirdOctave)
            {
                static constexpr double centres[] = { 20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0,
                                                      200.0, 250.0, 315.0, 400.0, 500.0, 630.0, 800.0, 1000.0, 1250.0, 1600.0,
                                                      2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0, 10000.0, 12500.0, 16000.0, 20000.0 };
                const auto halfBand = std::pow (2.0, 1.0 / 6.0);

                for (auto centre : centres)
                    bands.push_back ({ juce::String ((int) centre) + "Hz", centre / halfBand, centre * halfBand });
            }
            else
            {
                bands = { { "sub", 20.0, 60.0 },        { "bass", 60.0, 150.0 },
                          { "lowMid", 150.0, 400.0 },   { "mid", 400.0, 1000.0 },
                          { "highMid", 1000.0, 2500.0 },{ "presence", 2500.0, 5000.0 },
                          { "high", 5000.0, 10000.0 },  { "air", 10000.0, 20000.0 } };
            }

            // Map each FFT bin to its band once
            const auto binHz = sampleRate / (double) fftSize;
            bandForBin.assign ((size_t) fftSize / 2, -1);

            for (size_t bin = 1; bin < bandForBin.size(); ++bin)
            {
                const auto frequency = binHz * (double) bin;

                for (size_t band = 0; band < bands.size(); ++band)
                {
                    if (frequency >= bands[band].lowHz && frequency < bands[band].highHz)
                    {
                        bandForBin[bin] = (int) band;
                        break;
                    }
                }
            }
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

            std::vector<double> bandEnergies (bands.size(), 0.0);

            for (size_t bin = 1; bin < bandForBin.size(); ++bin)
                if (const auto band = bandForBin[bin]; band >= 0)
                    bandEnergies[(size_t) band] += (double) frame[bin] * (double) frame[bin];

            frameBandEnergies.push_back (std::move (bandEnergies));

            // 50% hop
            std::copy (fifo.begin() + fftSize / 2, fifo.end(), fifo.begin());
            fifoPos = fftSize / 2;
        }

        void addResults (juce::DynamicObject& result) override
        {
            // Analyse a partial tail frame rather than dropping it
            if (fifoPos > fftSize / 2 || frameBandEnergies.empty())
            {
                if (fifoPos > 0)
                {
                    std::fill (fifo.begin() + fifoPos, fifo.end(), 0.0f);
                    fifoPos = fftSize;
                    analyseFrame();
                }

                if (frameBandEnergies.empty())
                {
                    result.setProperty ("spectrogram", juce::var());
                    return;
                }
            }

            const auto numFrames = (int) frameBandEnergies.size();
            const auto numSlices = std::min (requestedSlices, numFrames);
            const auto numBands = bands.size();

            // Mean band power per slice
            std::vector<std::vector<double>> slicePowers ((size_t) numSlices, std::vector<double> (numBands, 0.0));

            for (int slice = 0; slice < numSlices; ++slice)
            {
                const auto begin = slice * numFrames / numSlices;
                const auto end = std::max (begin + 1, (slice + 1) * numFrames / numSlices);

                for (auto frame = begin; frame < end; ++frame)
                    for (size_t band = 0; band < numBands; ++band)
                        slicePowers[(size_t) slice][band] += frameBandEnergies[(size_t) frame][band];

                for (size_t band = 0; band < numBands; ++band)
                    slicePowers[(size_t) slice][band] /= (double) (end - begin);
            }

            double maxPower = 0.0;

            for (auto& slice : slicePowers)
                for (auto power : slice)
                    maxPower = std::max (maxPower, power);

            const auto toDb = [maxPower] (double power)
            {
                if (maxPower <= 0.0)
                    return (double) silenceFloorDb;

                return std::max ((double) silenceFloorDb, 10.0 * std::log10 (std::max (1.0e-12, power / maxPower)));
            };

            const auto sliceSeconds = lengthSeconds > 0.0 ? lengthSeconds / (double) numSlices : 0.0;
            const auto sliceTime = [sliceSeconds] (int slice) { return rounded ((slice + 0.5) * sliceSeconds, 2); };

            auto spectrogram = new juce::DynamicObject();
            spectrogram->setProperty ("sliceSeconds", rounded (sliceSeconds, 3));
            spectrogram->setProperty ("numSlices", numSlices);

            juce::Array<juce::var> bandResults, buildups;

            std::vector<std::vector<double>> bandLevels (numBands, std::vector<double> ((size_t) numSlices));

            struct Candidate
            {
                double rise = 0.0;
                int from = 0, to = 0;
            };

            std::vector<Candidate> candidates (numBands);

            for (size_t band = 0; band < numBands; ++band)
            {
                auto& levels = bandLevels[band];
                juce::Array<juce::var> levelsDb;
                double peakDb = silenceFloorDb, minDb = 0.0, sum = 0.0;
                int peakSlice = 0;

                for (int slice = 0; slice < numSlices; ++slice)
                {
                    const auto db = toDb (slicePowers[(size_t) slice][band]);
                    levels[(size_t) slice] = db;
                    levelsDb.add (rounded (db));
                    sum += db;
                    minDb = std::min (minDb, db);

                    if (db > peakDb)
                    {
                        peakDb = db;
                        peakSlice = slice;
                    }
                }

                auto bandResult = new juce::DynamicObject();
                bandResult->setProperty ("name", bands[band].name);
                bandResult->setProperty ("lowHz", (int) bands[band].lowHz);
                bandResult->setProperty ("highHz", (int) bands[band].highHz);
                bandResult->setProperty ("meanDb", rounded (sum / (double) numSlices));
                bandResult->setProperty ("peakDb", rounded (peakDb));
                bandResult->setProperty ("peakSeconds", sliceTime (peakSlice));
                bandResult->setProperty ("rangeDb", rounded (peakDb - minDb));
                bandResult->setProperty ("levelsDb", levelsDb);
                bandResults.add (juce::var (bandResult));

                // Buildup candidate: the largest rise from a running minimum,
                // ignoring near-silent slices so a fade-in from digital
                // silence doesn't count
                constexpr double silenceGateDb = -50.0;
                double runningMin = 0.0;
                int runningMinSlice = -1;
                auto& candidate = candidates[band];

                for (int slice = 0; slice < numSlices; ++slice)
                {
                    const auto db = levels[(size_t) slice];

                    if (db <= silenceGateDb)
                        continue;

                    if (runningMinSlice >= 0 && db - runningMin > candidate.rise)
                    {
                        candidate.rise = db - runningMin;
                        candidate.from = runningMinSlice;
                        candidate.to = slice;
                    }

                    if (runningMinSlice < 0 || db < runningMin)
                    {
                        runningMin = db;
                        runningMinSlice = slice;
                    }
                }
            }

            // Second pass so leakage shadows can be recognised: a rising tone's
            // window skirts rise identically in the neighbouring bands, so a
            // buildup is dropped when an adjacent band also rose and sits well
            // above this one at the buildup's end - the louder band carries
            // the report. Quiet endings are dropped outright.
            constexpr double buildupThresholdDb = 6.0, significanceDb = -30.0, shadowMarginDb = 10.0;

            for (size_t band = 0; band < numBands; ++band)
            {
                const auto& candidate = candidates[band];

                if (candidate.rise < buildupThresholdDb
                     || bandLevels[band][(size_t) candidate.to] <= significanceDb)
                    continue;

                auto isShadowedBy = [&] (size_t other)
                {
                    return candidates[other].rise >= buildupThresholdDb
                            && bandLevels[other][(size_t) candidate.to]
                                 - bandLevels[band][(size_t) candidate.to] >= shadowMarginDb;
                };

                if ((band > 0 && isShadowedBy (band - 1))
                     || (band + 1 < numBands && isShadowedBy (band + 1)))
                    continue;

                auto buildup = new juce::DynamicObject();
                buildup->setProperty ("band", bands[band].name);
                buildup->setProperty ("fromSeconds", sliceTime (candidate.from));
                buildup->setProperty ("toSeconds", sliceTime (candidate.to));
                buildup->setProperty ("riseDb", rounded (candidate.rise));
                buildups.add (juce::var (buildup));
            }

            spectrogram->setProperty ("bands", bandResults);
            spectrogram->setProperty ("buildups", buildups);
            result.setProperty ("spectrogram", juce::var (spectrogram));
        }

        static constexpr int fftOrder = 12;
        static constexpr int fftSize = 1 << fftOrder;

        const int requestedSlices;
        const bool thirdOctave;

        double sampleRate = 44100.0, lengthSeconds = 0.0;
        int numChannels = 1;
        juce::dsp::FFT fft { fftOrder };
        std::vector<float> fifo, window;
        int fifoPos = 0;
        std::vector<Band> bands;
        std::vector<int> bandForBin;
        std::vector<std::vector<double>> frameBandEnergies;
    };

    //==============================================================================
    /** Stereo field statistics: correlation, width, balance and mono
        compatibility, overall plus a low-band pass for mono-bass checks.
    */
    struct StereoAnalyser final : public AudioFileAnalyser
    {
        void prepare (const AudioFileInfo& info) override
        {
            numChannels = info.numChannels;

            if (numChannels < 2)
                return;

            fullBand.prepare (info.sampleRate, 2, maxBlockSize);
            lowBand.prepare (info.sampleRate, 2, maxBlockSize);

            // A 2nd-order Butterworth low-pass at 200Hz for the low-band pass
            const double k = std::tan (juce::MathConstants<double>::pi * 200.0 / info.sampleRate);
            const double q = 0.70710678;
            const double a0 = 1.0 + k / q + k * k;

            for (auto& filter : lowpass)
            {
                filter.b0 = k * k / a0;
                filter.b1 = 2.0 * filter.b0;
                filter.b2 = filter.b0;
                filter.a1 = 2.0 * (k * k - 1.0) / a0;
                filter.a2 = (1.0 - k / q + k * k) / a0;
            }
        }

        void process (const juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            if (numChannels < 2 || buffer.getNumChannels() < 2)
                return;

            fullBand.process (buffer.getArrayOfReadPointers(), 2, numSamples);

            if ((int) filtered[0].size() < numSamples)
                for (auto& channel : filtered)
                    channel.resize ((size_t) numSamples);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto data = buffer.getReadPointer (ch);

                for (int i = 0; i < numSamples; ++i)
                    filtered[(size_t) ch][(size_t) i] = (float) lowpass[(size_t) ch].process (data[i]);
            }

            const float* channels[] = { filtered[0].data(), filtered[1].data() };
            lowBand.process (channels, 2, numSamples);
        }

        void addResults (juce::DynamicObject& result) override
        {
            if (numChannels < 2)
            {
                result.setProperty ("stereo", juce::var());
                return;
            }

            fullBand.flush();
            lowBand.flush();

            const auto full = fullBand.getReadings();
            const auto low = lowBand.getReadings();

            auto stereo = new juce::DynamicObject();
            stereo->setProperty ("correlation", full.overallValid ? juce::var (rounded (full.overallCorrelation, 2)) : juce::var());
            stereo->setProperty ("minCorrelation", full.windowValid ? juce::var (rounded (full.minCorrelation, 2)) : juce::var());
            stereo->setProperty ("width", full.overallValid ? juce::var (rounded (full.overallWidth, 2)) : juce::var());
            stereo->setProperty ("balance", full.overallValid ? juce::var (rounded (full.overallBalance, 2)) : juce::var());
            stereo->setProperty ("monoLossDb", full.overallValid ? juce::var (rounded (full.monoLossDb)) : juce::var());

            // Null unless the channels matched strongly enough at some lag for
            // the offset to mean anything
            stereo->setProperty ("delayMs", full.alignmentValid ? juce::var (rounded (full.interChannelDelayMs, 3)) : juce::var());
            stereo->setProperty ("delaySamples", full.alignmentValid ? juce::var (full.interChannelDelaySamples) : juce::var());
            stereo->setProperty ("polarityInverted", full.alignmentValid ? juce::var (full.polarityInverted) : juce::var());

            auto lowBandResult = new juce::DynamicObject();
            lowBandResult->setProperty ("correlation", low.overallValid ? juce::var (rounded (low.overallCorrelation, 2)) : juce::var());
            lowBandResult->setProperty ("width", low.overallValid ? juce::var (rounded (low.overallWidth, 2)) : juce::var());
            stereo->setProperty ("lowBand", juce::var (lowBandResult));

            result.setProperty ("stereo", juce::var (stereo));
        }

        static constexpr int maxBlockSize = 8192;

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

        int numChannels = 0;
        StereoFieldAnalyser fullBand, lowBand;
        std::array<Biquad, 2> lowpass;
        std::array<std::vector<float>, 2> filtered;
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
    if (options.stereo)     owned.push_back (std::make_unique<StereoAnalyser>());
    if (options.spectrogram) owned.push_back (std::make_unique<SpectrogramAnalyser> (options.spectrogramSlices, options.spectrogramThirdOctave));

    std::vector<AudioFileAnalyser*> analysers;

    for (auto& analyser : owned)
        analysers.push_back (analyser.get());

    return analyseAudioFile (engine, file, analysers, options.shouldAbort);
}

} // namespace tracktion::inline engine
