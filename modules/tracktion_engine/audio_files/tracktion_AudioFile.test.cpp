/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class AudioFileTests    : public juce::UnitTest
{
public:
    AudioFileTests()
        : juce::UnitTest ("AudioFile", "Tracktion")
    {
    }

    void runTest() override
    {
        runFileInfoTest();
    }

private:
    void runFileInfoTest()
    {
        // Create a file
        // Get the info
        // Write to it
        // Get info again
        // Check length and sample rate

        beginTest ("AudioFileInfo update after writing");

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
            expectEquals (info.sampleRate, 0.0);
            expectEquals<SampleCount> (info.lengthInSamples, 0);
            expectEquals (info.getLengthInSeconds(), 0.0);
        }

        // Write 1s silence
        {
            const auto numSamplesToWrite = static_cast<int> (sampleRate);

            AudioFileWriter writer (audioFile, &format, numChannels, sampleRate, bitDepth, {}, 0);
            expect (writer.isOpen());
            
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
            expectEquals (info.sampleRate, sampleRate);
            expectEquals (info.lengthInSamples, static_cast<SampleCount> (sampleRate));
            expectEquals (info.getLengthInSeconds(), 1.0);
        }
    }
};

static AudioFileTests audioFileTests;

#endif

}} // namespace tracktion { inline namespace engine
