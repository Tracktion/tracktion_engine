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
class AudioFileManager   : private juce::AsyncUpdater
{
public:
    AudioFileManager (Engine&);
    ~AudioFileManager();

    AudioFile getAudioFile (ProjectItemID);
    AudioFileInfo getInfo (const AudioFile&);

    void checkFileForChangesAsync (const AudioFile&);
    void checkFileForChanges (const AudioFile&);
    void checkFilesForChanges();
    void forceFileUpdate (const AudioFile&);
    void validateFile (const AudioFile&, bool updateInfo);

    void releaseFile (const AudioFile&);
    void releaseAllFiles();

    juce::AudioThumbnailCache& getAudioThumbnailCache()     { return *thumbnailCache; }

    Engine& engine;
    AudioProxyGenerator proxyGenerator;
    AudioFileCache cache;

private:
    struct KnownFile;
    juce::HashMap<juce::int64, KnownFile*> knownFiles;
    juce::CriticalSection knownFilesLock;

    KnownFile& findOrCreateKnown (const AudioFile&);
    void removeFile (juce::int64 hash);
    void clearFiles();

    juce::Array<AudioFile> filesToCheck;

    void handleAsyncUpdate();
    bool checkFileTime (KnownFile&);
    void callListeners (const AudioFile&);

    friend class SmartThumbnail;
    std::unique_ptr<juce::AudioThumbnailCache> thumbnailCache;
    juce::Array<SmartThumbnail*> activeThumbnails;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
};

} // namespace tracktion_engine
