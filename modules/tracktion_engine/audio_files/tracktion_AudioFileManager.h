/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

class TracktionThumbnailCache;

//==============================================================================
/**
*/
class AudioFileManager   : private juce::AsyncUpdater
{
public:
    AudioFileManager (Engine&);
    ~AudioFileManager();

    AudioFile getAudioFile (ProjectItemRef);
    AudioFileInfo getInfo (const AudioFile&);

    void checkFileForChangesAsync (const AudioFile&);
    void checkFileForChanges (const AudioFile&);
    void checkFilesForChanges();
    void forceFileUpdate (const AudioFile&);
    void validateFile (const AudioFile&, bool updateInfo);

    void releaseFile (const AudioFile&);
    void releaseAllFiles();

    /// Register a memory buffer as a virtual audio file.
    /// The buffer data must remain valid for the lifetime of the registration.
    void registerMemoryBuffer (const std::string& filename,
                               choc::buffer::InterleavedView<const float> buffer,
                               double sampleRate);

    /// Unregister a previously registered memory buffer.
    void unregisterMemoryBuffer (const std::string& filename);

    /// If the given AudioFile is backed by a registered memory buffer, creates a
    /// AudioFormatReaderWithTimeout for it. Returns nullptr otherwise.
    std::unique_ptr<AudioFormatReaderWithTimeout> createMemoryReader (const AudioFile&) const;

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

    friend class TracktionThumbnailCache;
    friend class SmartThumbnail;
    std::unique_ptr<juce::AudioThumbnailCache> thumbnailCache;
    std::set<size_t> thumbnailTypeHashes;
    std::unordered_map<const juce::AudioThumbnailBase*, SmartThumbnail*> thumbnailMap;
    juce::Array<SmartThumbnail*> activeThumbnails;
    juce::CriticalSection activeThumbnailLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
};

} // namespace tracktion::inline engine
