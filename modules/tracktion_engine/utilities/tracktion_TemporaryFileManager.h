/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

/**
    Holds info about where temp files should go, and tidies up old ones when needed.

    You shouldn't have to ever create your own instance of this class - the
    Engine has a TemporaryFileManager object that is shared.

    @see Engine::getTemporaryFileManager()
*/
class TemporaryFileManager
{
public:
    //==============================================================================
    /** You shouldn't have to ever create your own instance of this class - the
        Engine itself has a TemporaryFileManager that is shared.
    */
    TemporaryFileManager (Engine&);

    ~TemporaryFileManager();

    //==============================================================================
    /** */
    bool wasTempFolderSuccessfullyCreated() const;

    /** */
    bool isDiskSpaceDangerouslyLow() const;

    /** */
    juce::int64 getMaxSpaceAllowedForTempFiles() const;

    /** */
    int getMaxNumTempFiles() const;

    /** */
    void cleanUp();

    /** */
    const juce::File& getTempDirectory() const;

    /** */
    bool setTempDirectory (const juce::File& newFile);

    /** */
    void ressetToDefaultLocation();

    /** */
    juce::File getThumbnailsFolder() const;

    /** */
    juce::File getTempFile (const juce::String& filename) const;

    /** */
    juce::File getUniqueTempFile (const juce::String& prefix, const juce::String& ext) const;

    //==============================================================================
    /** */
    static AudioFile getFileForCachedClipRender (const AudioClipBase&, juce::int64 hash);

    /** */
    static AudioFile getFileForCachedCompRender (const AudioClipBase& clip, juce::int64 takeHash);

    /** */
    static AudioFile getFileForCachedFileRender (Edit&, juce::int64 hash);

    /** */
    static juce::File getFreezeFileForDevice (Edit&, OutputDevice&);

    /** */
    static juce::File getFreezeFileForTrack (const AudioTrack&);

    /** */
    static juce::Array<juce::File> getFrozenTrackFiles (Edit&);

    /** */
    static void purgeOrphanFreezeAndProxyFiles (Edit&);

    /** */
    void purgeOrphanEditTempFolders (ProjectManager&);

    //==============================================================================
private:
    Engine& engine;
    juce::File tempDir;

    void updateDir();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TemporaryFileManager)
};

} // namespace tracktion_engine
