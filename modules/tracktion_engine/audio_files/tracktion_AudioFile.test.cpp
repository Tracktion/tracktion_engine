/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FILE

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE("WavAudioFormat 7 hrs")
    {
        using namespace std::literals;
        auto numChannels = 2;
        auto duration = TimeDuration (7h);
        auto sampleRate = 44100.0;
        auto numSamples = toSamples (duration, sampleRate);

        auto headerSizeBytes = static_cast<size_t> (5'888);
        auto totalSizeBytes = headerSizeBytes + static_cast<size_t> (numChannels * numSamples) * sizeof (int16_t);
        auto mb = juce::MemoryBlock (totalSizeBytes);
        auto os = std::unique_ptr<juce::OutputStream> (std::make_unique<juce::MemoryOutputStream> (mb, false));
        auto w = juce::WavAudioFormat().createWriterFor (os,
                                                         juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                                                                                         .withNumChannels (numChannels));
        auto blockSize = 2048 * 4;
        juce::AudioBuffer<float> temp (numChannels, blockSize);
        temp.clear();

        for (int64_t i = 0; i < numSamples;)
        {
            auto numLeft = numSamples - i;
            auto numThisTime = std::min (static_cast<int64_t> (blockSize), numLeft);

            temp.setSize (numChannels, static_cast<int> (numThisTime), true, false, true);
            w->writeFromAudioSampleBuffer (temp, 0, temp.getNumSamples());

            i += numThisTime;
        }

        w.reset();
        auto is = std::make_unique<juce::MemoryInputStream> (mb, false);
        auto reader = std::unique_ptr<juce::AudioFormatReader> (juce::WavAudioFormat().createReaderFor (is.release(), true));
        CHECK_EQ(reader->lengthInSamples, numSamples);
        CHECK_EQ(reader->numChannels, numChannels);
        CHECK_EQ(reader->sampleRate, sampleRate);
        CHECK_EQ(reader->usesFloatingPointData, false);
        CHECK_EQ(reader->bitsPerSample, 16);
    }
    TEST_CASE("AudioFileInfo channel layout round-trip")
    {
        auto& engine = *Engine::getEngines().getFirst();

        juce::WavAudioFormat format;
        juce::TemporaryFile tempFile (format.getFileExtensions()[0]);

        AudioFile audioFile (engine, tempFile.getFile());
        auto channelLayout = juce::AudioChannelSet::create5point1();
        auto numChannels = channelLayout.size();
        auto sampleRate = 44100.0;
        auto bitDepth = 16;

        // Write a short multi-channel WAV with 5.1 layout
        {
            AudioFileWriter writer (audioFile, &format, numChannels, sampleRate, bitDepth, {}, 0, channelLayout);
            REQUIRE(writer.isOpen());

            juce::AudioBuffer<float> buffer (numChannels, 512);
            buffer.clear();
            writer.appendBuffer (buffer, buffer.getNumSamples());
        }

        // Read back and check the channel layout is preserved
        {
            auto info = audioFile.getInfo();
            CHECK_EQ(info.numChannels, numChannels);
            CHECK_EQ(info.channelLayout, channelLayout);
        }
    }

    TEST_CASE("AudioFileInfo discrete channel layout")
    {
        auto& engine = *Engine::getEngines().getFirst();

        juce::WavAudioFormat format;
        juce::TemporaryFile tempFile (format.getFileExtensions()[0]);

        AudioFile audioFile (engine, tempFile.getFile());
        auto numChannels = 4;
        auto sampleRate = 48000.0;
        auto bitDepth = 24;

        // Write a 4-channel WAV with no specific layout (discrete)
        {
            AudioFileWriter writer (audioFile, &format, numChannels, sampleRate, bitDepth, {}, 0);
            REQUIRE(writer.isOpen());

            juce::AudioBuffer<float> buffer (numChannels, 512);
            buffer.clear();
            writer.appendBuffer (buffer, buffer.getNumSamples());
        }

        // Read back — should get discrete channels (or default mapping)
        {
            auto info = audioFile.getInfo();
            CHECK_EQ(info.numChannels, numChannels);
            // Without explicit layout, WAV reader returns discreteChannels or a default set
            auto isDiscrete = info.channelLayout == juce::AudioChannelSet::discreteChannels (numChannels);
            auto isCanonical = info.channelLayout == juce::AudioChannelSet::canonicalChannelSet (numChannels);
            CHECK((isDiscrete || isCanonical));
        }
    }

    TEST_CASE("AudioFileInfo channel layout write round-trip")
    {
        auto& engine = *Engine::getEngines().getFirst();

        juce::WavAudioFormat format;
        juce::TemporaryFile srcTempFile (format.getFileExtensions()[0]);
        juce::TemporaryFile dstTempFile (format.getFileExtensions()[0]);

        AudioFile srcAudioFile (engine, srcTempFile.getFile());
        AudioFile dstAudioFile (engine, dstTempFile.getFile());
        auto channelLayout = juce::AudioChannelSet::create5point1();
        auto numChannels = channelLayout.size();
        auto sampleRate = 44100.0;
        auto bitDepth = 16;
        auto numSamples = 512;

        // Step 1: Write a 5.1 WAV file
        {
            AudioFileWriter writer (srcAudioFile, &format, numChannels, sampleRate, bitDepth, {}, 0, channelLayout);
            REQUIRE(writer.isOpen());

            juce::AudioBuffer<float> buffer (numChannels, numSamples);
            buffer.clear();
            writer.appendBuffer (buffer, buffer.getNumSamples());
        }

        // Step 2: Read back via AudioFileInfo, then use that info to write a second file
        {
            auto info = srcAudioFile.getInfo();
            REQUIRE(info.channelLayout == channelLayout);

            AudioFileWriter writer (dstAudioFile, &format, info.numChannels,
                                    info.sampleRate, info.bitsPerSample,
                                    info.metadata, 0, info.channelLayout);
            REQUIRE(writer.isOpen());

            // Copy audio data through
            auto reader = std::unique_ptr<juce::AudioFormatReader> (
                AudioFileUtils::createReaderFor (engine, srcAudioFile.getFile()));
            REQUIRE(reader != nullptr);
            writer.writeFromAudioReader (*reader, 0, reader->lengthInSamples);
        }

        // Step 3: Read the second file and verify the layout survived
        {
            auto info = dstAudioFile.getInfo();
            CHECK_EQ(info.numChannels, numChannels);
            CHECK_EQ(info.channelLayout, channelLayout);
            CHECK_EQ(info.lengthInSamples, static_cast<SampleCount> (numSamples));
        }
    }
}


TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("AudioFileInfo update after writing")
    {
        // Create a file
        // Get the info
        // Write to it
        // Get info again
        // Check length and sample rate

        auto& engine = *Engine::getEngines().getFirst();

        juce::WavAudioFormat format;
        juce::TemporaryFile tempFile (format.getFileExtensions()[0]);

        AudioFile audioFile (engine, tempFile.getFile());
        const int numChannels = 2;
        const double sampleRate = 44100.0;
        const int bitDepth = 16;

        // Check file is invalid
        {
            auto info = audioFile.getInfo();
            CHECK_EQ (info.sampleRate, 0.0);
            CHECK_EQ (info.lengthInSamples, static_cast<SampleCount> (0));
            CHECK_EQ (info.getLengthInSeconds(), 0.0);
        }

        // Write 1s silence
        {
            const auto numSamplesToWrite = static_cast<int> (sampleRate);

            AudioFileWriter writer (audioFile, &format, numChannels, sampleRate, bitDepth, {}, 0);
            CHECK (writer.isOpen());

            if (writer.isOpen())
            {
                juce::AudioBuffer<float> buffer (numChannels, numSamplesToWrite);
                buffer.clear();
                writer.appendBuffer (buffer, buffer.getNumSamples());
            }
        }

        // Check file is now valid
        {
            auto info = audioFile.getInfo();
            CHECK_EQ (info.sampleRate, sampleRate);
            CHECK_EQ (info.lengthInSamples, static_cast<SampleCount> (sampleRate));
            CHECK_EQ (info.getLengthInSeconds(), 1.0);
        }
    }
}

} // namespace tracktion::inline engine

#endif
