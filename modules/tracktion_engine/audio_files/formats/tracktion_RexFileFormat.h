/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_ENABLE_REX

namespace tracktion::inline engine {

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
    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>&,
                                                              const juce::AudioFormatWriterOptions&) override;

    static const char* const rexTempo;
    static const char* const rexDenominator;
    static const char* const rexNumerator;
    static const char* const rexBeatPoints;

    static juce::String getErrorLoadingDLL();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RexAudioFormat)
};

} // namespace tracktion::inline engine

#endif
