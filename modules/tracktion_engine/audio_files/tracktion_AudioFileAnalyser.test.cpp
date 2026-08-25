/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FILE_ANALYSER

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    namespace
    {
        /** Writes a stereo-or-mono test file whose samples come from a generator
            fn (channel, sampleIndex) -> value.
        */
        template<typename AudioFormatType, typename SampleGenerator>
        void writeTestFile (const juce::File& file, double sampleRate, int numChannels, int numSamples,
                            int bitDepth, SampleGenerator&& generator, int qualityOptionIndex = 0)
        {
            AudioFormatType format;
            file.deleteFile();
            std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            REQUIRE (stream != nullptr);

            auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                            .withSampleRate (sampleRate)
                                                            .withNumChannels (numChannels)
                                                            .withBitsPerSample (bitDepth)
                                                            .withQualityOptionIndex (qualityOptionIndex));
            REQUIRE (writer != nullptr);

            juce::AudioBuffer<float> buffer (numChannels, numSamples);

            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    buffer.setSample (ch, i, generator (ch, i));

            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        }

        juce::var analyse (const juce::File& file, const AudioAnalysisOptions& options = {})
        {
            auto result = analyseAudioFile (*Engine::getEngines()[0], file, options);
            REQUIRE (result.has_value());
            return *result;
        }

        double num (const juce::var& v, const char* key)
        {
            auto value = v.getProperty (key, juce::var());
            REQUIRE (! value.isVoid());
            return (double) value;
        }
    }

    TEST_CASE ("AudioFileAnalyser level stats for known signals")
    {
        SUBCASE ("a sine at a known amplitude gives exact peak, RMS and loudness in tolerance")
        {
            // 997Hz mono sine at 0.5 amplitude: peak -6.02dB, RMS -9.03dB.
            // BS.1770's -0.691 offset cancels the K-weighting's gain at 997Hz,
            // so the loudness equals the RMS power: -9.03 LUFS (the spec's
            // 0dBFS 997Hz reference reads -3.01 LUFS)
            juce::TemporaryFile file (".wav");
            writeTestFile<juce::WavAudioFormat> (file.getFile(), 44100.0, 1, 4 * 44100, 32, [] (int, int i)
            {
                return 0.5f * std::sin (juce::MathConstants<float>::twoPi * 997.0f * (float) i / 44100.0f);
            });

            auto v = analyse (file.getFile());
            CHECK_EQ (num (v, "sampleRate"), 44100.0);
            CHECK_EQ ((int) v.getProperty ("channels", 0), 1);
            CHECK_EQ (num (v, "seconds"), doctest::Approx (4.0).epsilon (0.01));

            CHECK_EQ (num (v, "peakDb"), doctest::Approx (-6.0).epsilon (0.02));
            CHECK_EQ (num (v, "rmsDb"), doctest::Approx (-9.0).epsilon (0.02));
            CHECK_EQ (num (v, "truePeakDb"), doctest::Approx (-6.0).epsilon (0.05));
            CHECK_EQ (num (v, "integratedLufs"), doctest::Approx (-9.0).epsilon (0.06));
            CHECK_EQ (num (v, "maxMomentaryLufs"), doctest::Approx (-9.0).epsilon (0.06));
            CHECK_EQ (num (v, "maxShortTermLufs"), doctest::Approx (-9.0).epsilon (0.06));
            CHECK_EQ ((juce::int64) v.getProperty ("clippedSamples", -1), (juce::int64) 0);
            CHECK_EQ (num (v, "silenceRatio"), 0.0);
        }

        SUBCASE ("a hard-clipped signal is flagged")
        {
            juce::TemporaryFile file (".wav");
            writeTestFile<juce::WavAudioFormat> (file.getFile(), 44100.0, 1, 44100, 32, [] (int, int i)
            {
                return juce::jlimit (-1.0f, 1.0f,
                                     1.4f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f));
            });

            auto v = analyse (file.getFile());
            CHECK_GT ((juce::int64) v.getProperty ("clippedSamples", 0), (juce::int64) 100);
            CHECK_EQ (num (v, "peakDb"), doctest::Approx (0.0).epsilon (0.01));
        }

        SUBCASE ("a half-silent file has a silence ratio of one half")
        {
            juce::TemporaryFile file (".wav");
            writeTestFile<juce::WavAudioFormat> (file.getFile(), 44100.0, 1, 4 * 44100, 32, [] (int, int i)
            {
                if (i >= 2 * 44100)
                    return 0.0f;

                return 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f);
            });

            auto v = analyse (file.getFile());
            CHECK_EQ (num (v, "silenceRatio"), doctest::Approx (0.5).epsilon (0.05));
        }
    }

    TEST_CASE ("AudioFileAnalyser spectral summary")
    {
        auto makeToneFile = [] (const juce::File& file, float frequency)
        {
            writeTestFile<juce::WavAudioFormat> (file, 44100.0, 1, 2 * 44100, 32, [frequency] (int, int i)
            {
                return 0.5f * std::sin (juce::MathConstants<float>::twoPi * frequency * (float) i / 44100.0f);
            });
        };

        SUBCASE ("a pure tone dominates the right band, centroid and balance")
        {
            juce::TemporaryFile file (".wav");
            makeToneFile (file.getFile(), 1000.0f);

            auto v = analyse (file.getFile());
            auto spectrum = v.getProperty ("spectrum", juce::var());
            auto bandHz = spectrum.getProperty ("bandHz", juce::var());
            auto bandDb = spectrum.getProperty ("bandDb", juce::var());
            REQUIRE (bandHz.size() == bandDb.size());
            REQUIRE (bandHz.size() > 20);

            int loudestBandHz = 0;
            double loudestDb = -1000.0;

            for (int i = 0; i < bandDb.size(); ++i)
            {
                if ((double) bandDb[i] > loudestDb)
                {
                    loudestDb = (double) bandDb[i];
                    loudestBandHz = (int) bandHz[i];
                }
            }

            CHECK_EQ (loudestBandHz, 1000);
            CHECK_EQ (loudestDb, 0.0);   // bands are relative to the loudest

            CHECK_EQ (num (spectrum, "centroidHz"), doctest::Approx (1000.0).epsilon (0.15));

            auto balance = spectrum.getProperty ("balance", juce::var());
            CHECK_GT (num (balance, "midPct"), 90.0);
        }

        SUBCASE ("centroid orders low against high content")
        {
            juce::TemporaryFile lowFile (".wav"), highFile (".wav");
            makeToneFile (lowFile.getFile(), 110.0f);
            makeToneFile (highFile.getFile(), 8000.0f);

            const auto lowCentroid = num (analyse (lowFile.getFile()).getProperty ("spectrum", juce::var()), "centroidHz");
            const auto highCentroid = num (analyse (highFile.getFile()).getProperty ("spectrum", juce::var()), "centroidHz");

            CHECK_LT (lowCentroid, 400.0);
            CHECK_GT (highCentroid, 4000.0);
        }
    }

    TEST_CASE ("AudioFileAnalyser dynamics envelope")
    {
        // Quiet first half, loud second half
        juce::TemporaryFile file (".wav");
        writeTestFile<juce::WavAudioFormat> (file.getFile(), 44100.0, 1, 2 * 44100, 32, [] (int, int i)
        {
            const auto amplitude = i < 44100 ? 0.05f : 0.8f;
            return amplitude * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f);
        });

        AudioAnalysisOptions options;
        options.envelopePoints = 10;

        auto envelope = analyse (file.getFile(), options).getProperty ("envelope", juce::var());
        auto rmsDb = envelope.getProperty ("rmsDb", juce::var());
        auto peakDb = envelope.getProperty ("peakDb", juce::var());

        REQUIRE (rmsDb.size() == 10);
        REQUIRE (peakDb.size() == 10);
        CHECK_EQ (num (envelope, "windowSeconds"), doctest::Approx (0.2).epsilon (0.01));

        // ~24dB step between the halves
        CHECK_LT ((double) rmsDb[1], (double) rmsDb[8] - 20.0);
        CHECK_LT ((double) peakDb[1], (double) peakDb[8] - 20.0);
        CHECK_EQ ((double) peakDb[8], doctest::Approx (-1.9).epsilon (0.1));
    }

    TEST_CASE ("AudioFileAnalyser handles other formats and channel counts")
    {
        auto stereoSin = [] (int, int i)
        {
            return 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f);
        };

        auto checkFile = [] (const juce::File& file)
        {
            auto v = analyse (file);
            CHECK_EQ ((int) v.getProperty ("channels", 0), 2);
            CHECK_EQ (num (v, "peakDb"), doctest::Approx (-6.0).epsilon (0.02));
            CHECK_EQ (num (v, "rmsDb"), doctest::Approx (-9.0).epsilon (0.02));
        };

        SUBCASE ("AIFF")
        {
            juce::TemporaryFile file (".aiff");
            writeTestFile<juce::AiffAudioFormat> (file.getFile(), 44100.0, 2, 44100, 24, stereoSin);
            checkFile (file.getFile());
        }

        SUBCASE ("FLAC")
        {
            juce::TemporaryFile file (".flac");
            writeTestFile<juce::FlacAudioFormat> (file.getFile(), 44100.0, 2, 44100, 24, stereoSin);
            checkFile (file.getFile());
        }

        SUBCASE ("OGG")
        {
            // Lossy, so the tolerances are wider than the uncompressed formats above
            // and the length is only approximate. Quality index 9 is 320kbps
            juce::TemporaryFile file (".ogg");
            writeTestFile<juce::OggVorbisAudioFormat> (file.getFile(), 44100.0, 2, 2 * 44100, 32,
                                                       [] (int, int i)
                                                       {
                                                           return 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                                                                     * 220.0f * (float) i / 44100.0f);
                                                       },
                                                       9);

            auto v = analyse (file.getFile());
            CHECK_EQ ((int) v.getProperty ("channels", 0), 2);
            CHECK_EQ (num (v, "sampleRate"), 44100.0);
            CHECK_EQ (num (v, "seconds"), doctest::Approx (2.0).epsilon (0.05));

            CHECK_EQ (num (v, "peakDb"), doctest::Approx (-6.0).epsilon (0.2));
            CHECK_EQ (num (v, "rmsDb"), doctest::Approx (-9.0).epsilon (0.05));
            CHECK_EQ (num (v, "integratedLufs"), doctest::Approx (-6.7).epsilon (0.15));

            // The tone still has to land in the right band and centroid
            auto spectrum = v.getProperty ("spectrum", juce::var());
            CHECK_EQ (num (spectrum, "centroidHz"), doctest::Approx (220.0).epsilon (0.4));
        }

        SUBCASE ("a missing file returns an error")
        {
            auto result = analyseAudioFile (*Engine::getEngines()[0],
                                            juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                .getChildFile ("does-not-exist.wav"),
                                            AudioAnalysisOptions());
            CHECK (! result.has_value());
        }

        SUBCASE ("an aborted analysis fails rather than returning partial results")
        {
            juce::TemporaryFile file (".wav");
            writeTestFile<juce::WavAudioFormat> (file.getFile(), 44100.0, 1, 4 * 44100, 32, [] (int, int i)
            {
                return 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f);
            });

            AudioAnalysisOptions options;
            options.shouldAbort = [] { return true; };

            auto result = analyseAudioFile (*Engine::getEngines()[0], file.getFile(), options);
            CHECK (! result.has_value());
        }
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FILE_ANALYSER
