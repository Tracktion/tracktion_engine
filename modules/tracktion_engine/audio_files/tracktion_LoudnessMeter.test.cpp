/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LOUDNESS_METER

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    namespace loudness_meter_tests
    {
        constexpr double testSampleRate = 44100.0;

        /** Builds a mono buffer from a generator fn (sampleIndex) -> value. */
        template<typename SampleGenerator>
        juce::AudioBuffer<float> makeBuffer (int numSamples, SampleGenerator&& generator)
        {
            juce::AudioBuffer<float> buffer (1, numSamples);

            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (0, i, generator (i));

            return buffer;
        }

        static float sine (int i, double frequency, float amplitude)
        {
            return amplitude * std::sin (juce::MathConstants<float>::twoPi * (float) frequency
                                          * (float) i / (float) testSampleRate);
        }

        /** Tone bursts at different levels separated by silence, so that both
            the absolute and the relative gate have something to reject.
        */
        static juce::AudioBuffer<float> makeVariedLevelSignal()
        {
            const auto oneSecond = (int) testSampleRate;

            return makeBuffer (12 * oneSecond, [] (int i)
            {
                const auto section = i / oneSecond;

                switch (section)
                {
                    case 0: case 1: case 2:     return sine (i, 1000.0, 0.5f);       // -6dBFS
                    case 3: case 4:             return 0.0f;                         // digital silence
                    case 5: case 6: case 7:     return sine (i, 300.0, 0.05f);       // -26dBFS
                    case 8:                     return sine (i, 1000.0, 0.0002f);    // below the absolute gate
                    default:                    return sine (i, 700.0, 0.25f);       // -12dBFS
                }
            });
        }

        static void processInBlocks (LoudnessMeter& meter, const juce::AudioBuffer<float>& buffer,
                                     const std::vector<int>& blockSizes)
        {
            const auto numChannels = buffer.getNumChannels();

            // Test-side scratch to build the offset channel pointers the raw
            // pointer overload takes - allocated once, outside the block loop
            std::vector<const float*> channels ((size_t) numChannels, nullptr);
            int position = 0;

            for (size_t i = 0; position < buffer.getNumSamples(); ++i)
            {
                const auto numThisTime = std::min (blockSizes[i % blockSizes.size()],
                                                   buffer.getNumSamples() - position);

                for (int ch = 0; ch < numChannels; ++ch)
                    channels[(size_t) ch] = buffer.getReadPointer (ch) + position;

                meter.process (channels.data(), numChannels, numThisTime);
                position += numThisTime;
            }

            meter.flush();
        }

        /** The same, but through the choc view overload, so the views have a
            non-zero frame offset.
        */
        static void processInBlocksAsView (LoudnessMeter& meter, const juce::AudioBuffer<float>& buffer,
                                           const std::vector<int>& blockSizes)
        {
            auto view = choc::buffer::createChannelArrayView (buffer.getArrayOfReadPointers(),
                                                              (choc::buffer::ChannelCount) buffer.getNumChannels(),
                                                              (choc::buffer::FrameCount) buffer.getNumSamples());
            int position = 0;

            for (size_t i = 0; position < buffer.getNumSamples(); ++i)
            {
                const auto numThisTime = std::min (blockSizes[i % blockSizes.size()],
                                                   buffer.getNumSamples() - position);

                meter.process (view.getFrameRange ({ (choc::buffer::FrameCount) position,
                                                     (choc::buffer::FrameCount) (position + numThisTime) }));
                position += numThisTime;
            }

            meter.flush();
        }

        //==============================================================================
        /** A direct, allocating reference implementation of BS.1770-4 integrated
            loudness which keeps every gating block, to check the meter's fixed
            histogram against.
        */
        static double referenceIntegratedLufs (const juce::AudioBuffer<float>& buffer)
        {
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

            Biquad shelf, highpass;

            {
                const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
                const double k = std::tan (juce::MathConstants<double>::pi * f0 / testSampleRate);
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
                const double f0 = 38.13547087602444, q = 0.5003270373238773;
                const double k = std::tan (juce::MathConstants<double>::pi * f0 / testSampleRate);
                const double a0 = 1.0 + k / q + k * k;

                highpass.b0 = 1.0;
                highpass.b1 = -2.0;
                highpass.b2 = 1.0;
                highpass.a1 = 2.0 * (k * k - 1.0) / a0;
                highpass.a2 = (1.0 - k / q + k * k) / a0;
            }

            auto powerToLoudness = [] (double power) { return -0.691 + 10.0 * std::log10 (std::max (1.0e-12, power)); };

            const auto gatingBlockSamples = (int) std::llround (testSampleRate * 0.1);
            std::vector<double> blockEnergies;
            double blockEnergy = 0.0;
            int samplesIntoBlock = 0;

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto filtered = highpass.process (shelf.process ((double) buffer.getSample (0, i)));
                blockEnergy += filtered * filtered;

                if (++samplesIntoBlock >= gatingBlockSamples)
                {
                    blockEnergies.push_back (blockEnergy / (double) gatingBlockSamples);
                    blockEnergy = 0.0;
                    samplesIntoBlock = 0;
                }
            }

            if (samplesIntoBlock >= gatingBlockSamples / 2)
                blockEnergies.push_back (blockEnergy / (double) gatingBlockSamples);

            std::vector<double> momentaryPowers;

            for (size_t i = 3; i < blockEnergies.size(); ++i)
                momentaryPowers.push_back ((blockEnergies[i - 3] + blockEnergies[i - 2]
                                             + blockEnergies[i - 1] + blockEnergies[i]) / 4.0);

            auto gatedMean = [&] (double gateLufs)
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

            auto absoluteGated = gatedMean (-70.0);
            REQUIRE (absoluteGated.has_value());
            auto relativeGated = gatedMean (powerToLoudness (*absoluteGated) - 10.0);
            REQUIRE (relativeGated.has_value());

            return powerToLoudness (*relativeGated);
        }
    }

    //==============================================================================
    TEST_CASE ("LoudnessMeter known signal levels")
    {
        using namespace loudness_meter_tests;

        // 997Hz sine at 0.5 amplitude: BS.1770's -0.691 offset cancels the
        // K-weighting's gain at 997Hz, so the loudness equals the RMS power
        auto buffer = makeBuffer (4 * (int) testSampleRate, [] (int i) { return sine (i, 997.0, 0.5f); });

        LoudnessMeter meter;
        meter.prepare (testSampleRate, 1, 8192);
        processInBlocks (meter, buffer, { 512 });

        auto readings = meter.getReadings();
        REQUIRE (readings.momentaryValid);
        REQUIRE (readings.shortTermValid);
        REQUIRE (readings.integratedValid);

        CHECK_EQ (readings.momentaryLufs, doctest::Approx (-9.03).epsilon (0.01));
        CHECK_EQ (readings.shortTermLufs, doctest::Approx (-9.03).epsilon (0.01));
        CHECK_EQ (readings.integratedLufs, doctest::Approx (-9.03).epsilon (0.01));
        CHECK_EQ (readings.maxMomentaryLufs, doctest::Approx (-9.03).epsilon (0.01));
        CHECK_EQ (readings.maxShortTermLufs, doctest::Approx (-9.03).epsilon (0.01));
        CHECK_EQ (readings.samplePeakDb, doctest::Approx (-6.02).epsilon (0.01));
        CHECK_EQ (readings.truePeakDb, doctest::Approx (-6.02).epsilon (0.02));
        CHECK_GE (readings.truePeakDb, readings.samplePeakDb);
    }

    TEST_CASE ("LoudnessMeter readings before enough audio has been processed")
    {
        using namespace loudness_meter_tests;

        LoudnessMeter meter;
        meter.prepare (testSampleRate, 1, 8192);

        auto readings = meter.getReadings();
        CHECK (! readings.momentaryValid);
        CHECK (! readings.shortTermValid);
        CHECK (! readings.integratedValid);

        // 500ms is enough for momentary, but not for short-term
        auto buffer = makeBuffer ((int) (testSampleRate / 2), [] (int i) { return sine (i, 1000.0, 0.5f); });
        processInBlocks (meter, buffer, { 256 });

        readings = meter.getReadings();
        CHECK (readings.momentaryValid);
        CHECK (! readings.shortTermValid);
        CHECK (readings.integratedValid);
    }

    TEST_CASE ("LoudnessMeter is invariant to the block sizes it's fed")
    {
        using namespace loudness_meter_tests;

        auto buffer = makeVariedLevelSignal();

        LoudnessMeter oneBigBlock, variedBlocks;
        oneBigBlock.prepare (testSampleRate, 1, 8192);
        variedBlocks.prepare (testSampleRate, 1, 8192);

        processInBlocks (oneBigBlock, buffer, { buffer.getNumSamples() });
        processInBlocks (variedBlocks, buffer, { 1, 7, 64, 4095, 8192, 33, 1024, 3, 6000 });

        const auto a = oneBigBlock.getReadings();
        const auto b = variedBlocks.getReadings();

        CHECK_EQ (a.integratedLufs, doctest::Approx (b.integratedLufs).epsilon (0.0001));
        CHECK_EQ (a.momentaryLufs, doctest::Approx (b.momentaryLufs).epsilon (0.0001));
        CHECK_EQ (a.shortTermLufs, doctest::Approx (b.shortTermLufs).epsilon (0.0001));
        CHECK_EQ (a.maxMomentaryLufs, doctest::Approx (b.maxMomentaryLufs).epsilon (0.0001));
        CHECK_EQ (a.maxShortTermLufs, doctest::Approx (b.maxShortTermLufs).epsilon (0.0001));
        CHECK_EQ (a.loudnessRangeLu, doctest::Approx (b.loudnessRangeLu).epsilon (0.0001));
        CHECK_EQ (a.truePeakDb, doctest::Approx (b.truePeakDb).epsilon (0.001));
        CHECK_EQ (a.samplePeakDb, doctest::Approx (b.samplePeakDb).epsilon (0.0001));
    }

    TEST_CASE ("LoudnessMeter loudness range")
    {
        using namespace loudness_meter_tests;

        SUBCASE ("a constant level has almost no loudness range")
        {
            auto buffer = makeBuffer (20 * (int) testSampleRate, [] (int i) { return sine (i, 1000.0, 0.5f); });

            LoudnessMeter meter;
            meter.prepare (testSampleRate, 1, 8192);
            processInBlocks (meter, buffer, { 1024 });

            auto readings = meter.getReadings();
            REQUIRE (readings.loudnessRangeValid);
            CHECK_LT (readings.loudnessRangeLu, 0.5f);
        }

        SUBCASE ("two sections 20 LU apart give a loudness range of about 20 LU")
        {
            const auto tenSeconds = 10 * (int) testSampleRate;

            auto buffer = makeBuffer (2 * tenSeconds, [] (int i)
            {
                return sine (i, 1000.0, i < tenSeconds ? 0.5f : 0.05f);
            });

            LoudnessMeter meter;
            meter.prepare (testSampleRate, 1, 8192);
            processInBlocks (meter, buffer, { 1024 });

            auto readings = meter.getReadings();
            REQUIRE (readings.loudnessRangeValid);
            CHECK_EQ (readings.loudnessRangeLu, doctest::Approx (20.0).epsilon (0.05));
        }

        SUBCASE ("it isn't valid until there are short-term blocks")
        {
            auto buffer = makeBuffer (2 * (int) testSampleRate, [] (int i) { return sine (i, 1000.0, 0.5f); });

            LoudnessMeter meter;
            meter.prepare (testSampleRate, 1, 8192);
            processInBlocks (meter, buffer, { 1024 });

            auto readings = meter.getReadings();
            CHECK (! readings.shortTermValid);
            CHECK (! readings.loudnessRangeValid);
        }
    }

    TEST_CASE ("LoudnessMeter choc view and raw pointer overloads agree")
    {
        using namespace loudness_meter_tests;

        auto mono = makeVariedLevelSignal();
        juce::AudioBuffer<float> stereo (2, mono.getNumSamples());

        for (int i = 0; i < mono.getNumSamples(); ++i)
        {
            stereo.setSample (0, i, mono.getSample (0, i));
            stereo.setSample (1, i, 0.5f * mono.getSample (0, i));
        }

        const std::vector<int> blockSizes { 480, 1024, 37 };

        LoudnessMeter fromPointers, fromView;
        fromPointers.prepare (testSampleRate, 2, 8192);
        fromView.prepare (testSampleRate, 2, 8192);

        processInBlocks (fromPointers, stereo, blockSizes);
        processInBlocksAsView (fromView, stereo, blockSizes);

        const auto a = fromPointers.getReadings();
        const auto b = fromView.getReadings();

        // The same work in the same order, so these should be identical
        CHECK_EQ (a.integratedLufs, b.integratedLufs);
        CHECK_EQ (a.momentaryLufs, b.momentaryLufs);
        CHECK_EQ (a.shortTermLufs, b.shortTermLufs);
        CHECK_EQ (a.maxMomentaryLufs, b.maxMomentaryLufs);
        CHECK_EQ (a.maxShortTermLufs, b.maxShortTermLufs);
        CHECK_EQ (a.loudnessRangeLu, b.loudnessRangeLu);
        CHECK_EQ (a.truePeakDb, b.truePeakDb);
        CHECK_EQ (a.samplePeakDb, b.samplePeakDb);
        CHECK (b.integratedValid);
    }

    TEST_CASE ("LoudnessMeter histogram matches a full-history reference")
    {
        using namespace loudness_meter_tests;

        auto buffer = makeVariedLevelSignal();

        LoudnessMeter meter;
        meter.prepare (testSampleRate, 1, 8192);
        processInBlocks (meter, buffer, { 1024 });

        auto readings = meter.getReadings();
        REQUIRE (readings.integratedValid);

        // The only approximation is the 0.1 LU quantisation of the two gate
        // decisions, so this should be well inside the EBU R128 tolerance
        CHECK_LT (std::abs ((double) readings.integratedLufs - referenceIntegratedLufs (buffer)), 0.05);
    }

    TEST_CASE ("LoudnessMeter restarts cleanly after a reset")
    {
        using namespace loudness_meter_tests;

        auto loud = makeBuffer (4 * (int) testSampleRate, [] (int i) { return sine (i, 1000.0, 0.5f); });    // -9 LUFS
        auto quiet = makeBuffer (4 * (int) testSampleRate, [] (int i) { return sine (i, 1000.0, 0.05f); });  // -29 LUFS

        LoudnessMeter meter;
        meter.prepare (testSampleRate, 1, 8192);
        processInBlocks (meter, loud, { 512 });

        CHECK_EQ (meter.getReadings().integratedLufs, doctest::Approx (-9.03).epsilon (0.01));

        meter.requestReset();

        // The request is only picked up by the processing thread
        CHECK_EQ (meter.getReadings().integratedLufs, doctest::Approx (-9.03).epsilon (0.01));

        processInBlocks (meter, quiet, { 512 });

        auto readings = meter.getReadings();
        REQUIRE (readings.integratedValid);
        CHECK_EQ (readings.integratedLufs, doctest::Approx (-29.0).epsilon (0.02));
        CHECK_EQ (readings.maxMomentaryLufs, doctest::Approx (-29.0).epsilon (0.02));
        CHECK_EQ (readings.maxShortTermLufs, doctest::Approx (-29.0).epsilon (0.02));
        CHECK_EQ (readings.samplePeakDb, doctest::Approx (-26.02).epsilon (0.01));

        // The short-term histogram is cleared too, so the loud section doesn't
        // leave a 20 LU range behind
        REQUIRE (readings.loudnessRangeValid);
        CHECK_LT (readings.loudnessRangeLu, 0.5f);
    }

    TEST_CASE ("LoudnessMeter readings can be polled whilst processing")
    {
        using namespace loudness_meter_tests;

        auto buffer = makeVariedLevelSignal();

        LoudnessMeter meter;
        meter.prepare (testSampleRate, 1, 8192);

        std::atomic<bool> processing { true }, sane { true };
        std::atomic<int> numPolls { 0 };

        // Assertions stay on the main thread; the poller just reads
        std::thread poller ([&]
        {
            while (processing.load())
            {
                auto readings = meter.getReadings();

                if (readings.momentaryLufs > 0.0f || readings.samplePeakDb > 0.0f)
                    sane = false;

                ++numPolls;
            }
        });

        for (int i = 0; i < 4; ++i)
            processInBlocks (meter, buffer, { 256, 512, 1024 });

        processing = false;
        poller.join();

        CHECK (sane.load());
        CHECK_GT (numPolls.load(), 0);
        CHECK (meter.getReadings().integratedValid);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LOUDNESS_METER
