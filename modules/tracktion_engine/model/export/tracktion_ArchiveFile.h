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
class TracktionArchiveFile
{
public:
    TracktionArchiveFile (const juce::File& file);
    ~TracktionArchiveFile();

    bool isValidArchive() const;
    juce::File getFile() const;

    enum class CompressionType
    {
        none                    = 0,
        zip                     = 1,
        lossless                = 2,
        lossyGoodQuality        = 3,
        lossyMediumQuality      = 4,
        lossyLowQuality         = 5
    };

    int getNumFiles() const;
    juce::String getOriginalFileName (int index) const;

    int indexOfFile (const juce::String& name) const;

    /** Create a stream to read one of the archived objects. */
    std::unique_ptr<juce::InputStream> createStoredInputStream (int index) const;

    bool extractFile (int index, const juce::File& destDirectory,
                      juce::File& fileCreated, bool askBeforeOverwriting);
    bool extractAll (const juce::File& destDirectory,
                     juce::Array<juce::File>& filesCreated);
    bool extractAllAsTask (const juce::File& destDirectory,
                           bool warnAboutOverwrite,
                           juce::Array<juce::File>& filesCreated,
                           bool& wasAborted);

    bool addFile (const juce::File&, const juce::File& rootDirectory, CompressionType);
    bool addFile (const juce::File&, const juce::String& filenameToUse, CompressionType);

    void addFileInfo (const juce::String& filename,
                      const juce::String& itemName,
                      const juce::String& itemValue);

    void flush();

public:
    //==============================================================================
    struct IndexEntry
    {
        IndexEntry() {}
        IndexEntry (juce::InputStream&);
        ~IndexEntry();

        void write (juce::OutputStream&);

        // original name - name of the file that went in
        // stored name - name of the compressed file that's stored
        // length = length of the stored object
        // offset - offset from start of file of the stored object
        juce::String originalName, storedName;
        juce::int64 offset = 0, length = 0;
        // extra names and values - for extra metadata
        juce::StringArray extraNames, extraValues;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IndexEntry)
    };

private:
    juce::File file;
    juce::int64 indexOffset = 8;
    bool valid = false, needToWriteIndex;

    juce::OwnedArray<IndexEntry> entries;
    void readIndex();

    static int getOggQuality (CompressionType);
    static int getMagicNumber();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracktionArchiveFile)
};

} // namespace tracktion_engine
