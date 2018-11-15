/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    Smart wrapper for writing to an audio file.
*/
class AudioFileWriter   : public juce::ReferenceCountedObject
{
public:
    AudioFileWriter (const AudioFile& file,
                     juce::AudioFormat* formatToUse,
                     int numChannels,
                     double sampleRate,
                     int bitsPerSample,
                     const juce::StringPairArray& metadata,
                     int quality);

    ~AudioFileWriter();

    bool isOpen() const noexcept                 { return writer != nullptr; }

    bool appendBuffer (juce::AudioBuffer<float>& buffer, int numSamples);
    bool appendBuffer (const int** buffer, int numSamples);

    void closeForWriting();

    AudioFile file;
    std::unique_ptr<juce::AudioFormatWriter> writer;

private:
    int samplesUntilFlush;
};

} // namespace tracktion_engine
