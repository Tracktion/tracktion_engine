/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Holds info about where temp files should go, and tidies up old ones when needed.
*/
class TemporaryFileManager
{
public:
    //==============================================================================
    TemporaryFileManager (Engine&);
    ~TemporaryFileManager();

    //==============================================================================
    bool wasTempFolderSuccessfullyCreated() const;
    bool isDiskSpaceDangerouslyLow() const;
    void cleanUp();

    const juce::File& getTempDirectory() const    { return tempDir; }

    juce::File getThumbnailsFolder() const;

    juce::File getTempFile (const juce::String& filename) const;
    juce::File getUniqueTempFile (const juce::String& prefix, const juce::String& ext) const;

    bool setTempDirectory (const juce::File& newFile);
    void setToDefaultDirectory();

    //==============================================================================
private:
    Engine& engine;

    juce::File tempDir;
    void updateDir();
    juce::File getDefaultTempDir();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TemporaryFileManager)
};

} // namespace tracktion_engine
