/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
*/
struct AudioFileInfo
{
    AudioFileInfo (Engine&);
    AudioFileInfo (const AudioFile&, juce::AudioFormatReader*, juce::AudioFormat*);

    static AudioFileInfo parse (const AudioFile&);

    Engine* engine = nullptr;
    bool wasParsedOk = false;
    HashCode hashCode = 0;
    juce::AudioFormat* format = nullptr;
    double sampleRate = 0;
    SampleCount lengthInSamples = 0;
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

    juce::String getLongDescription() const;
};

//==============================================================================
/**
*/
class AudioFile
{
public:
    AudioFile() = delete;
    inline AudioFile (Engine& e) noexcept : engine (&e) {}
    explicit AudioFile (Engine&, const juce::File&) noexcept;
    AudioFile (const AudioFile&) noexcept;
    AudioFile& operator= (const AudioFile&) noexcept;
    ~AudioFile();

    const juce::File& getFile() const noexcept  { return file; }
    HashCode getHash() const noexcept           { return hash; }
    juce::String getHashString() const          { return juce::String::toHexString (hash); }

    inline bool operator== (const AudioFile& other) const noexcept      { return hash == other.hash; }
    inline bool operator!= (const AudioFile& other) const noexcept      { return hash != other.hash; }

    bool deleteFile() const;
    static bool deleteFiles (Engine&, const juce::Array<juce::File>& files);

    bool moveToTrash() const;

    //==============================================================================
    bool isNull() const noexcept                { return hash == 0; }
    bool isValid() const;

    AudioFileInfo getInfo() const;

    int64_t getLengthInSamples() const;
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

    Engine* engine = nullptr;

private:
    juce::File file;
    HashCode hash = 0;
};

}} // namespace tracktion { inline namespace engine
