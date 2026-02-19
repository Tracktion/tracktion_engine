/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

#if TRACKTION_ENABLE_FFMPEG

class FFmpegEncoderAudioFormat : public juce::AudioFormat
{
public:
    /** Creates a FFmpegEncoderAudioFormat that expects to find a working FFmpeg
     executable at the location given.
     */
    FFmpegEncoderAudioFormat (const juce::File& ffmpegExecutableToUse);
    ~FFmpegEncoderAudioFormat() override;
    
    bool canHandleFile (const juce::File&) override;
    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool isCompressed() override;
    juce::StringArray getQualityOptions() override;
    
    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;

    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>&,
                                                              const juce::AudioFormatWriterOptions&) override;

private:
    juce::File ffmpegExe;
    class Writer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFmpegEncoderAudioFormat)
};

#endif

} // namespace tracktion::inline engine
