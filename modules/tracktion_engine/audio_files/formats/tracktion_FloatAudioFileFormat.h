/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine {

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
    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>&,
                                                              const juce::AudioFormatWriterOptions&) override;
};

} // namespace tracktion::inline engine
