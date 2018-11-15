/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Keeps a list of available wave formats and can create a format object
    for a file.
*/
class AudioFileFormatManager
{
public:
    AudioFileFormatManager();
    ~AudioFileFormatManager();

    //==============================================================================
    const juce::Array<juce::AudioFormat*>& getWriteFormats() const     { return writeFormats; }

    void addLameFormat (std::unique_ptr<juce::AudioFormat> lameForArray,
                        std::unique_ptr<juce::AudioFormat> lameForAccess);

    void addFormat (std::function<juce::AudioFormat*()> formatCreator, bool isWritable, bool isMemoryMappable);

    //==============================================================================
    juce::AudioFormat* getFormatFromFileName (const juce::File&) const;
    juce::AudioFormat* getNamedFormat (const juce::String& formatName) const;

    bool canOpen (const juce::File&) const;
    juce::String getValidFileExtensions() const;

    juce::AudioFormat* getDefaultFormat() const     { return wavFormat.get(); }
    juce::AudioFormat* getWavFormat() const         { return wavFormat.get(); }
    juce::AudioFormat* getAiffFormat() const        { return aiffFormat.get(); }
    juce::AudioFormat* getFrozenFileFormat() const  { return floatFormat.get(); }
    juce::AudioFormat* getOggFormat() const         { return oggFormat.get(); }
    juce::AudioFormat* getFlacFormat() const        { return flacFormat.get(); }
    juce::AudioFormat* getNativeAudioFormat() const { return nativeAudioFormat.get(); }
    juce::AudioFormat* getLameFormat() const        { return lameFormat.get(); }

   #if TRACKTION_ENABLE_REX
    juce::AudioFormat* getRexFormat() const         { return rexFormat.get(); }
   #endif

    juce::AudioFormatManager readFormatManager, writeFormatManager, memoryMappedFormatManager;

private:
    juce::Array<juce::AudioFormat*> allFormats, readFormats, writeFormats;
    juce::OwnedArray<juce::AudioFormat> additionalFormats;

    std::unique_ptr<juce::AudioFormat> wavFormat, aiffFormat, floatFormat, nativeAudioFormat,
                                       mp3ReadFormat, oggFormat, flacFormat, lameFormat;

   #if TRACKTION_ENABLE_REX
    std::unique_ptr<juce::AudioFormat> rexFormat;
   #endif
};

} // namespace tracktion_engine
