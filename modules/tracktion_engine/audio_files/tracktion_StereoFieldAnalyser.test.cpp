/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_STEREO_FIELD_ANALYSER

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    namespace stereo_field_tests
    {
        constexpr double testSampleRate = 44100.0;

        /** Builds a stereo buffer from a generator fn (sampleIndex) -> {l, r}. */
        template<typename SampleGenerator>
        juce::AudioBuffer<float> makeStereoBuffer (int numSamples, SampleGenerator&& generator)
        {
            juce::AudioBuffer<float> buffer (2, numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto [l, r] = generator (i);
                buffer.setSample (0, i, l);
                buffer.setSample (1, i, r);
            }

            return buffer;
        }

        static float sine (int i, double frequency, float amplitude)
        {
            return amplitude * std::sin (juce::MathConstants<float>::twoPi * (float) frequency
                                          * (float) i / (float) testSampleRate);
        }

        static void processInBlocks (StereoFieldAnalyser& analyser, const juce::AudioBuffer<float>& buffer,
                                     const std::vector<int>& blockSizes)
        {
            const auto numChannels = buffer.getNumChannels();
            std::vector<const float*> channels ((size_t) numChannels, nullptr);
            int position = 0;

            for (size_t i = 0; position < buffer.getNumSamples(); ++i)
            {
                const auto numThisTime = std::min (blockSizes[i % blockSizes.size()],
                                                   buffer.getNumSamples() - position);

                for (int ch = 0; ch < numChannels; ++ch)
                    channels[(size_t) ch] = buffer.getReadPointer (ch) + position;

                analyser.process (channels.data(), numChannels, numThisTime);
                position += numThisTime;
            }

            analyser.flush();
        }

        static StereoFieldAnalyser::Readings analyse (const juce::AudioBuffer<float>& buffer,
                                                      const std::vector<int>& blockSizes = { 512 })
        {
            StereoFieldAnalyser analyser;
            analyser.prepare (testSampleRate, buffer.getNumChannels(), 8192);
            processInBlocks (analyser, buffer, blockSizes);
            return analyser.getReadings();
        }
    }

    //==============================================================================
    TEST_CASE ("StereoFieldAnalyser known signal correlations")
    {
        using namespace stereo_field_tests;
        const auto numSamples = 2 * (int) testSampleRate;

        SUBCASE ("identical channels are perfectly correlated, mono-width and centred")
        {
            auto readings = analyse (makeStereoBuffer (numSamples, [] (int i)
            {
                const auto value = sine (i, 997.0, 0.5f);
                return std::pair (value, value);
            }));

            REQUIRE (readings.windowValid);
            REQUIRE (readings.overallValid);
            CHECK_EQ (readings.correlation, doctest::Approx (1.0).epsilon (0.001));
            CHECK_EQ (readings.minCorrelation, doctest::Approx (1.0).epsilon (0.001));
            CHECK_EQ (readings.overallCorrelation, doctest::Approx (1.0).epsilon (0.001));
            CHECK_EQ (readings.width, doctest::Approx (0.0).epsilon (0.001));
            CHECK_EQ (readings.balance, doctest::Approx (0.0).epsilon (0.001));
            CHECK_EQ (readings.monoLossDb, doctest::Approx (0.0).epsilon (0.01));
        }

        SUBCASE ("a scaled copy still correlates fully, but tips the balance")
        {
            auto readings = analyse (makeStereoBuffer (numSamples, [] (int i)
            {
                const auto value = sine (i, 997.0, 0.5f);
                return std::pair (value, 0.5f * value);
            }));

            REQUIRE (readings.windowValid);
            CHECK_EQ (readings.correlation, doctest::Approx (1.0).epsilon (0.001));

            // rr = ll/4, so the energy balance is (0.25 - 1) / 1.25
            CHECK_EQ (readings.balance, doctest::Approx (-0.6).epsilon (0.001));
            CHECK_EQ (readings.overallBalance, doctest::Approx (-0.6).epsilon (0.001));
        }

        SUBCASE ("inverted channels are anti-correlated, pure side, and cancel in mono")
        {
            auto readings = analyse (makeStereoBuffer (numSamples, [] (int i)
            {
                const auto value = sine (i, 997.0, 0.5f);
                return std::pair (value, -value);
            }));

            REQUIRE (readings.windowValid);
            CHECK_EQ (readings.correlation, doctest::Approx (-1.0).epsilon (0.001));
            CHECK_EQ (readings.minCorrelation, doctest::Approx (-1.0).epsilon (0.001));
            CHECK_EQ (readings.width, doctest::Approx (1.0).epsilon (0.001));
            CHECK_GE (readings.monoLossDb, 20.0f);
        }

        SUBCASE ("independent noise is roughly uncorrelated, half-width, and loses ~3dB in mono")
        {
            juce::Random random (42);

            auto readings = analyse (makeStereoBuffer (8 * (int) testSampleRate, [&] (int)
            {
                return std::pair (random.nextFloat() - 0.5f, random.nextFloat() - 0.5f);
            }));

            REQUIRE (readings.windowValid);
            CHECK_LT (std::abs (readings.overallCorrelation), 0.02f);
            CHECK_LT (std::abs (readings.correlation), 0.05f);
            CHECK_LT (std::abs (readings.minCorrelation), 0.1f);
            CHECK_EQ (readings.overallWidth, doctest::Approx (0.5).epsilon (0.02));
            CHECK_EQ (readings.monoLossDb, doctest::Approx (3.01).epsilon (0.05));
        }

        SUBCASE ("hard-panned material reads as one-sided and loses 3dB in mono")
        {
            auto readings = analyse (makeStereoBuffer (numSamples, [] (int i)
            {
                return std::pair (0.0f, sine (i, 997.0, 0.5f));
            }));

            REQUIRE (readings.windowValid);
            CHECK_EQ (readings.balance, doctest::Approx (1.0).epsilon (0.001));
            CHECK_EQ (readings.correlation, doctest::Approx (0.0).epsilon (0.001));
            CHECK_EQ (readings.monoLossDb, doctest::Approx (3.01).epsilon (0.01));
        }
    }

    TEST_CASE ("StereoFieldAnalyser mono input reads as a centred signal")
    {
        using namespace stereo_field_tests;

        juce::AudioBuffer<float> mono (1, 2 * (int) testSampleRate);

        for (int i = 0; i < mono.getNumSamples(); ++i)
            mono.setSample (0, i, sine (i, 997.0, 0.5f));

        auto readings = analyse (mono);

        REQUIRE (readings.windowValid);
        CHECK_EQ (readings.correlation, doctest::Approx (1.0).epsilon (0.001));
        CHECK_EQ (readings.width, doctest::Approx (0.0).epsilon (0.001));
        CHECK_EQ (readings.balance, doctest::Approx (0.0).epsilon (0.001));
        CHECK_EQ (readings.monoLossDb, doctest::Approx (0.0).epsilon (0.01));
    }

    TEST_CASE ("StereoFieldAnalyser only measures the first stereo pair")
    {
        using namespace stereo_field_tests;
        const auto numSamples = 2 * (int) testSampleRate;

        juce::AudioBuffer<float> surround (6, numSamples);
        surround.clear();
        juce::Random random (7);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = sine (i, 997.0, 0.5f);
            surround.setSample (0, i, value);
            surround.setSample (1, i, value);

            // Uncorrelated noise on the other channels must not affect anything
            for (int ch = 2; ch < 6; ++ch)
                surround.setSample (ch, i, random.nextFloat() - 0.5f);
        }

        auto readings = analyse (surround);

        REQUIRE (readings.windowValid);
        CHECK_EQ (readings.correlation, doctest::Approx (1.0).epsilon (0.001));
        CHECK_EQ (readings.width, doctest::Approx (0.0).epsilon (0.001));
        CHECK_EQ (readings.balance, doctest::Approx (0.0).epsilon (0.001));
    }

    TEST_CASE ("StereoFieldAnalyser is invariant to the block sizes it's fed")
    {
        using namespace stereo_field_tests;
        juce::Random random (11);

        auto buffer = makeStereoBuffer (5 * (int) testSampleRate, [&] (int i)
        {
            return std::pair (sine (i, 400.0, 0.4f) + 0.1f * (random.nextFloat() - 0.5f),
                              sine (i, 400.0, 0.3f) - 0.1f * (random.nextFloat() - 0.5f));
        });

        const auto a = analyse (buffer, { buffer.getNumSamples() });
        const auto b = analyse (buffer, { 1, 7, 64, 4095, 8192, 33, 1024, 3, 6000 });

        CHECK_EQ (a.correlation, doctest::Approx (b.correlation).epsilon (0.0001));
        CHECK_EQ (a.minCorrelation, doctest::Approx (b.minCorrelation).epsilon (0.0001));
        CHECK_EQ (a.balance, doctest::Approx (b.balance).epsilon (0.0001));
        CHECK_EQ (a.width, doctest::Approx (b.width).epsilon (0.0001));
        CHECK_EQ (a.overallCorrelation, doctest::Approx (b.overallCorrelation).epsilon (0.0001));
        CHECK_EQ (a.overallBalance, doctest::Approx (b.overallBalance).epsilon (0.0001));
        CHECK_EQ (a.overallWidth, doctest::Approx (b.overallWidth).epsilon (0.0001));
        CHECK_EQ (a.monoLossDb, doctest::Approx (b.monoLossDb).epsilon (0.0001));
    }

    TEST_CASE ("StereoFieldAnalyser choc view and raw pointer overloads agree")
    {
        using namespace stereo_field_tests;

        auto buffer = makeStereoBuffer (3 * (int) testSampleRate, [] (int i)
        {
            return std::pair (sine (i, 500.0, 0.5f), sine (i, 800.0, 0.5f));
        });

        StereoFieldAnalyser fromPointers, fromView;
        fromPointers.prepare (testSampleRate, 2, 8192);
        fromView.prepare (testSampleRate, 2, 8192);

        const std::vector<int> blockSizes { 480, 1024, 37 };
        processInBlocks (fromPointers, buffer, blockSizes);

        {
            auto view = choc::buffer::createChannelArrayView (buffer.getArrayOfReadPointers(),
                                                              (choc::buffer::ChannelCount) 2,
                                                              (choc::buffer::FrameCount) buffer.getNumSamples());
            int position = 0;

            for (size_t i = 0; position < buffer.getNumSamples(); ++i)
            {
                const auto numThisTime = std::min (blockSizes[i % blockSizes.size()],
                                                   buffer.getNumSamples() - position);

                fromView.process (view.getFrameRange ({ (choc::buffer::FrameCount) position,
                                                        (choc::buffer::FrameCount) (position + numThisTime) }));
                position += numThisTime;
            }

            fromView.flush();
        }

        const auto a = fromPointers.getReadings();
        const auto b = fromView.getReadings();

        CHECK_EQ (a.correlation, b.correlation);
        CHECK_EQ (a.minCorrelation, b.minCorrelation);
        CHECK_EQ (a.balance, b.balance);
        CHECK_EQ (a.width, b.width);
        CHECK_EQ (a.overallCorrelation, b.overallCorrelation);
        CHECK_EQ (a.monoLossDb, b.monoLossDb);
        CHECK (b.windowValid);
    }

    TEST_CASE ("StereoFieldAnalyser validity and silence handling")
    {
        using namespace stereo_field_tests;

        SUBCASE ("nothing is valid before any audio")
        {
            StereoFieldAnalyser analyser;
            analyser.prepare (testSampleRate, 2, 8192);

            auto readings = analyser.getReadings();
            CHECK (! readings.windowValid);
            CHECK (! readings.overallValid);
        }

        SUBCASE ("the window isn't valid until 400ms has been processed")
        {
            StereoFieldAnalyser analyser;
            analyser.prepare (testSampleRate, 2, 8192);

            // 300ms: three full blocks, no full window yet
            auto buffer = makeStereoBuffer ((int) (testSampleRate * 0.3), [] (int i)
            {
                const auto value = sine (i, 997.0, 0.5f);
                return std::pair (value, value);
            });

            processInBlocks (analyser, buffer, { 512 });

            auto readings = analyser.getReadings();
            CHECK (! readings.windowValid);
            CHECK (readings.overallValid);
        }

        SUBCASE ("digital silence invalidates the window but leaves held values alone")
        {
            const auto oneSecond = (int) testSampleRate;

            auto buffer = makeStereoBuffer (3 * oneSecond, [] (int i)
            {
                if (i >= oneSecond)
                    return std::pair (0.0f, 0.0f);

                const auto value = sine (i, 997.0, 0.5f);
                return std::pair (value, -value);
            });

            auto readings = analyse (buffer);

            CHECK (! readings.windowValid);
            CHECK (readings.overallValid);
            CHECK_EQ (readings.correlation, doctest::Approx (-1.0).epsilon (0.001));
            CHECK_EQ (readings.minCorrelation, doctest::Approx (-1.0).epsilon (0.001));
            CHECK_EQ (readings.overallCorrelation, doctest::Approx (-1.0).epsilon (0.001));
        }
    }

    TEST_CASE ("StereoFieldAnalyser restarts cleanly after a reset")
    {
        using namespace stereo_field_tests;
        const auto numSamples = 2 * (int) testSampleRate;

        auto wide = makeStereoBuffer (numSamples, [] (int i)
        {
            const auto value = sine (i, 997.0, 0.5f);
            return std::pair (value, -value);
        });

        auto centred = makeStereoBuffer (numSamples, [] (int i)
        {
            const auto value = sine (i, 997.0, 0.5f);
            return std::pair (value, value);
        });

        StereoFieldAnalyser analyser;
        analyser.prepare (testSampleRate, 2, 8192);
        processInBlocks (analyser, wide, { 512 });

        CHECK_EQ (analyser.getReadings().minCorrelation, doctest::Approx (-1.0).epsilon (0.001));

        analyser.requestReset();

        // The request is only picked up by the processing thread
        CHECK_EQ (analyser.getReadings().minCorrelation, doctest::Approx (-1.0).epsilon (0.001));

        processInBlocks (analyser, centred, { 512 });

        auto readings = analyser.getReadings();
        REQUIRE (readings.windowValid);
        CHECK_EQ (readings.correlation, doctest::Approx (1.0).epsilon (0.001));
        CHECK_EQ (readings.minCorrelation, doctest::Approx (1.0).epsilon (0.001));
        CHECK_EQ (readings.overallCorrelation, doctest::Approx (1.0).epsilon (0.001));
        CHECK_EQ (readings.monoLossDb, doctest::Approx (0.0).epsilon (0.01));
    }

    TEST_CASE ("StereoFieldAnalyser readings can be polled whilst processing")
    {
        using namespace stereo_field_tests;

        auto buffer = makeStereoBuffer (4 * (int) testSampleRate, [] (int i)
        {
            return std::pair (sine (i, 400.0, 0.4f), sine (i, 600.0, 0.4f));
        });

        StereoFieldAnalyser analyser;
        analyser.prepare (testSampleRate, 2, 8192);

        std::atomic<bool> processing { true }, sane { true };
        std::atomic<int> numPolls { 0 };

        // Assertions stay on the main thread; the poller just reads
        std::thread poller ([&]
        {
            while (processing.load())
            {
                auto readings = analyser.getReadings();

                if (readings.correlation < -1.0f || readings.correlation > 1.0f
                     || readings.width < 0.0f || readings.width > 1.0f)
                    sane = false;

                ++numPolls;
            }
        });

        for (int i = 0; i < 4; ++i)
            processInBlocks (analyser, buffer, { 256, 512, 1024 });

        processing = false;
        poller.join();

        CHECK (sane.load());
        CHECK_GT (numPolls.load(), 0);
        CHECK (analyser.getReadings().windowValid);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_STEREO_FIELD_ANALYSER
