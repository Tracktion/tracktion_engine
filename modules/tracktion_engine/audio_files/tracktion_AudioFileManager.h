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
    std::unordered_map<HashCode, std::unique_ptr<KnownFile>> knownFiles;
    juce::CriticalSection knownFilesLock;

    KnownFile& findOrCreateKnown (const AudioFile&);
    void removeFile (HashCode);
    void clearFiles();

    juce::Array<AudioFile> filesToCheck;

    void handleAsyncUpdate();
    bool checkFileTime (KnownFile&);
    void callListeners (const AudioFile&);
    void callListenersOnMessageThread (const AudioFile&);

    friend class SmartThumbnail;
    std::unique_ptr<juce::AudioThumbnailCache> thumbnailCache;
    juce::Array<SmartThumbnail*> activeThumbnails;
    juce::CriticalSection activeThumbnailLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
};

}} // namespace tracktion { inline namespace engine
