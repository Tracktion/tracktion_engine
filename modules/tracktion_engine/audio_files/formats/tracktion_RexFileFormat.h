/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_ENABLE_REX

namespace tracktion_engine
{

class RexAudioFormat    : public juce::AudioFormat
{
public:
    RexAudioFormat();
    ~RexAudioFormat();

    juce::Array<int> getPossibleSampleRates() override    { return {}; }
    juce::Array<int> getPossibleBitDepths() override      { return {}; }

    bool canDoStereo() override     { return true; }
    bool canDoMono() override       { return true; }
    bool isCompressed() override    { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;
    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double sampleRateToUse,
                                              unsigned int numberOfChannels, int bitsPerSample,
                                              const juce::StringPairArray& metadataValues, int qualityOptionIndex) override;

    static const char* const rexTempo;
    static const char* const rexDenominator;
    static const char* const rexNumerator;
    static const char* const rexBeatPoints;

    static juce::String getErrorLoadingDLL();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexAudioFormat)
};

} // namespace tracktion_engine

#endif
