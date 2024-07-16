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

static juce::String readOutputFromSystem (juce::String cmd)
{
    juce::TemporaryFile tmpFile;
    juce::TemporaryFile batFile (".bat");
    
   #if JUCE_WINDOWS
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory (&si, sizeof (si));
    si.cb = sizeof (si);
    ZeroMemory (&pi, sizeof (pi));

    cmd = cmd + " > " + tmpFile.getFile().getFullPathName().quoted();

    if (cmd.length() > 32768)
    {
        jassertfalse;
        return {};
    }

    batFile.getFile ().replaceWithText (cmd);

    juce::String params = "/c " + batFile.getFile().getFullPathName().quoted();

    juce::HeapBlock<char> commandLine (params.getNumBytesAsUTF8() + 1);
    strcpy (commandLine.get(), params.toRawUTF8());

    juce::File cmdExe = juce::File::getSpecialLocation (juce::File::windowsSystemDirectory).getChildFile ("cmd.exe");
    jassert (cmdExe.existsAsFile());

    if (! CreateProcessA (cmdExe.getFullPathName().toRawUTF8(), commandLine.get(), nullptr, nullptr, 
                          false, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return {};
    
    WaitForSingleObject (pi.hProcess, INFINITE);

    CloseHandle (pi.hProcess);
    CloseHandle (pi.hThread);
   #else
    cmd << " > " << tmpFile.getFile().getFullPathName().quoted();
    auto result = std::system (cmd.toRawUTF8());
    juce::ignoreUnused (result);
   #endif

    auto contents = tmpFile.getFile().loadFileAsString();
    return contents;
}

class FFmpegEncoderAudioFormat::Writer : public juce::AudioFormatWriter
{
public:
    Writer (juce::OutputStream* destStream, const juce::String& formatName,
            const juce::File& exe, int vbr, int cbr,
            double sampleRateIn, unsigned int numberOfChannels,
            int bitsPerSampleIn, const juce::StringPairArray& md)
        : AudioFormatWriter (destStream, formatName, sampleRateIn,
                             numberOfChannels, (unsigned int) bitsPerSampleIn),
                             vbrLevel (vbr), cbrBitrate (cbr), ffmpeg (exe), metadata (md)
    {
        if (auto out = tempWav.getFile().createOutputStream())
            writer.reset (juce::WavAudioFormat().createWriterFor (out.release(), sampleRateIn, numChannels, bitsPerSampleIn, metadata, 0));
    }

    ~Writer() override
    {
        if (writer != nullptr)
        {
            writer = nullptr;
            
            if (! convertToMP3())
                convertToMP3();
        }
    }
    
    bool write (const int** samplesToWrite, int numSamples) override
    {
        return writer != nullptr && writer->write (samplesToWrite, numSamples);
    }
    
private:
    int vbrLevel, cbrBitrate;
    juce::TemporaryFile tempWav { ".wav" };
    std::unique_ptr<AudioFormatWriter> writer;

    bool runFFmpegChildProcess (const juce::TemporaryFile& tempMP3, const juce::StringArray& processArgs) const
    {
        juce::ChildProcess cp;

        DBG(processArgs.joinIntoString (" "));

        [[maybe_unused]] auto output = readOutputFromSystem (processArgs.joinIntoString (" "));
        DBG(output);
        
        return tempMP3.getFile().getSize() > 0;
    }
    
    bool convertToMP3() const
    {
        juce::TemporaryFile tempMP3 (".mp3");

        juce::StringArray args;

        args.add (ffmpeg.getFullPathName().quoted());
        args.add ("-i");
        args.add (tempWav.getFile().getFullPathName().quoted());
        args.add ("-codec:a");
        args.add ("libmp3lame");

        if (cbrBitrate == 0)
        {
            args.add ("-q:a");
            args.add (juce::String (vbrLevel));
        }
        else
        {
            args.add ("-b:a");
            args.add (juce::String::formatted ("%dk", cbrBitrate));
        }

        addMetadataArg (args, "id3title",       "title");
        addMetadataArg (args, "id3artist",      "artist");
        addMetadataArg (args, "id3album",       "album");
        addMetadataArg (args, "id3comment",     "comment");
        addMetadataArg (args, "id3date",        "date");
        addMetadataArg (args, "id3genre",       "genre");
        addMetadataArg (args, "id3trackNumber", "track");

        args.add (tempMP3.getFile().getFullPathName().quoted());

        if (runFFmpegChildProcess (tempMP3, args))
        {
            juce::FileInputStream fis (tempMP3.getFile());

            if (fis.openedOk() && output->writeFromInputStream (fis, -1) > 0)
            {
                output->flush();
                return true;
            }
        }
        
        return false;
    }

    void addMetadataArg (juce::StringArray& args, const char* key, const char* ffmpegFlag) const
    {
        auto value = metadata.getValue (key, {});

        if (value.isNotEmpty())
        {
            args.add ("-metadata");
            args.add (juce::String (ffmpegFlag) + "=" + value.quoted());
        }
    }

    const juce::File ffmpeg;
    const juce::StringPairArray metadata;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Writer)
};

//==============================================================================
FFmpegEncoderAudioFormat::FFmpegEncoderAudioFormat (const juce::File& ffmpeg)
    : AudioFormat ("MP3 file", ".mp3"), ffmpegExe (ffmpeg)
{
}

FFmpegEncoderAudioFormat::~FFmpegEncoderAudioFormat()
{
}

bool FFmpegEncoderAudioFormat::canHandleFile (const juce::File&)
{
    return false;
}

juce::Array<int> FFmpegEncoderAudioFormat::getPossibleSampleRates()
{
    return { 32000, 44100, 48000 };
}

juce::Array<int> FFmpegEncoderAudioFormat::getPossibleBitDepths()
{
    return { 16 };
}

bool FFmpegEncoderAudioFormat::canDoStereo()      { return true; }
bool FFmpegEncoderAudioFormat::canDoMono()        { return true; }
bool FFmpegEncoderAudioFormat::isCompressed()     { return true; }

juce::StringArray FFmpegEncoderAudioFormat::getQualityOptions()
{
    static const char* vbrOptions[] = { "VBR quality 0 (best)", "VBR quality 1", "VBR quality 2", "VBR quality 3",
        "VBR quality 4 (normal)", "VBR quality 5", "VBR quality 6", "VBR quality 7",
        "VBR quality 8", "VBR quality 9 (smallest)", nullptr };

    juce::StringArray opts (vbrOptions);

    const int cbrRates[] = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
    
    for (int i = 0; i < std::ssize (cbrRates); ++i)
        opts.add (juce::String (cbrRates[i]) + " Kb/s CBR");

    return opts;
}

juce::AudioFormatReader* FFmpegEncoderAudioFormat::createReaderFor (juce::InputStream*, const bool)
{
    return nullptr;
}

juce::AudioFormatWriter* FFmpegEncoderAudioFormat::createWriterFor (juce::OutputStream* streamToWriteTo,
                                                                    double sampleRateToUse,
                                                                    unsigned int numberOfChannels,
                                                                    int bitsPerSample,
                                                                    const juce::StringPairArray& metadataValues,
                                                                    int qualityOptionIndex)
{
    if (streamToWriteTo == nullptr)
        return nullptr;
    
    int vbr = 4;
    int cbr = 0;
    
    const juce::String qual (getQualityOptions() [qualityOptionIndex]);

    if (qual.contains ("VBR"))
        vbr = qual.retainCharacters ("0123456789").getIntValue();
    else
        cbr = qual.getIntValue();
    
    return new Writer (streamToWriteTo, getFormatName(), ffmpegExe, vbr, cbr, sampleRateToUse, numberOfChannels, bitsPerSample, metadataValues);
}

#endif

}} // namespace tracktion { inline namespace engine
