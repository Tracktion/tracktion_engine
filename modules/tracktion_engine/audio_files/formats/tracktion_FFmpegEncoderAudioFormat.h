/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if TRACKTION_ENABLE_FFMPEG

class FFmpegEncoderAudioFormat : public juce::AudioFormat
{
public:
    /** Creates a FFmpegEncoderAudioFormat that expects to find a working FFmpeg
     executable at the location given.
     */
    FFmpegEncoderAudioFormat (const juce::File& ffmpegExecutableToUse);
    ~FFmpegEncoderAudioFormat();
    
    bool canHandleFile (const juce::File&);
    juce::Array<int> getPossibleSampleRates();
    juce::Array<int> getPossibleBitDepths();
    bool canDoStereo();
    bool canDoMono();
    bool isCompressed();
    juce::StringArray getQualityOptions();
    
    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails);

    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double sampleRateToUse,
                                              unsigned int numberOfChannels, int bitsPerSample,
                                              const juce::StringPairArray& metadataValues, int qualityOptionIndex);

    using AudioFormat::createWriterFor;
    
private:
    juce::File ffmpegExe;
    class Writer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFmpegEncoderAudioFormat)
};

#endif

}} // namespace tracktion { inline namespace engine
