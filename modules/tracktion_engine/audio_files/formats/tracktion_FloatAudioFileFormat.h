/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    A raw, proprietory, simple floating point format used for freeze files, etc.
*/
class FloatAudioFormat   : public juce::AudioFormat
{
public:
    FloatAudioFormat();
    ~FloatAudioFormat();

    //==============================================================================
    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool canHandleFile (const juce::File&) override;

    //==============================================================================
    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;

    juce::MemoryMappedAudioFormatReader* createMemoryMappedReader (const juce::File&) override;

    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double sampleRate,
                                              unsigned int numChannels, int bitsPerSample,
                                              const juce::StringPairArray& metadataValues,
                                              int qualityOptionIndex) override;
};

} // namespace tracktion_engine
