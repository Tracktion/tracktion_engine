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
*/
struct AudioFileInfo
{
    AudioFileInfo();
    AudioFileInfo (const AudioFile&, juce::AudioFormatReader*, juce::AudioFormat*);

    static AudioFileInfo parse (const AudioFile&);

    juce::int64 hashCode = 0;
    juce::AudioFormat* format = nullptr;
    double sampleRate = 0;
    juce::int64 lengthInSamples = 0;
    int numChannels = 0;
    int bitsPerSample = 0;
    bool isFloatingPoint = false;
    bool needsCachedProxy = false;
    juce::StringPairArray metadata;
    juce::Time fileModificationTime;
    LoopInfo loopInfo;

    double getLengthInSeconds() const noexcept
    {
        if (sampleRate > 0)
            return lengthInSamples / sampleRate;

        return 0.0;
    }

    juce::String getLongDescription (Engine&) const;
};

//==============================================================================
/**
*/
class AudioFile
{
public:
    inline AudioFile() noexcept {}
    explicit AudioFile (const juce::File&) noexcept;
    AudioFile (const AudioFile&) noexcept;
    AudioFile& operator= (const AudioFile&) noexcept;
    ~AudioFile();

    const juce::File& getFile() const noexcept  { return file; }
    juce::int64 getHash() const noexcept        { return hash; }
    juce::String getHashString() const          { return juce::String::toHexString (hash); }

    inline bool operator== (const AudioFile& other) const noexcept      { return hash == other.hash; }
    inline bool operator!= (const AudioFile& other) const noexcept      { return hash != other.hash; }

    bool deleteFile() const;

    //==============================================================================
    bool isNull() const noexcept                { return hash == 0; }
    bool isValid() const;

    AudioFileInfo getInfo() const;

    juce::int64 getLengthInSamples() const;
    double getLength() const;
    int getNumChannels() const;
    double getSampleRate() const;
    int getBitsPerSample() const;
    bool isFloatingPoint() const;
    juce::StringPairArray getMetadata() const;

    juce::AudioFormat* getFormat() const;

    bool isWavFile() const;
    bool isAiffFile() const;
    bool isOggFile() const;
    bool isMp3File() const;
    bool isFlacFile() const;
    bool isRexFile() const;

private:
    juce::File file;
    juce::int64 hash = 0;
};

} // namespace tracktion_engine
