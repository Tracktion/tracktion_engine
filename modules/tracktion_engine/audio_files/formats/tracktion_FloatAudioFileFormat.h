/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

/**
    A raw, proprietary, simple floating point format used for freeze files, etc.
*/
class FloatAudioFormat   : public juce::AudioFormat
{
public:
    FloatAudioFormat();
    ~FloatAudioFormat() override;

    //==============================================================================
    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool canHandleFile (const juce::File&) override;

    //==============================================================================
    using juce::AudioFormat::createReaderFor;
    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;

    using juce::AudioFormat::createMemoryMappedReader;
    juce::MemoryMappedAudioFormatReader* createMemoryMappedReader (const juce::File&) override;

    using juce::AudioFormat::createWriterFor;
    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double sampleRate,
                                              unsigned int numChannels, int bitsPerSample,
                                              const juce::StringPairArray& metadataValues,
                                              int qualityOptionIndex) override;
};

}} // namespace tracktion { inline namespace engine
