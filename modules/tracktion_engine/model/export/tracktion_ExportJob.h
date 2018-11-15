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
class ExportJob  : public ThreadPoolJobWithProgress
{
public:
    enum class CompressionLevel
    {
        lossless = 0,
        highQuality,
        mediumQuality,
        lowestQuality,
        uncompressed
    };

    ExportJob (Edit*,
               const juce::File& destDir,
               const Project::Ptr& newProject,
               const Project::Ptr& srcProject,
               TracktionArchiveFile*,
               double handleSize_,
               bool keepEntireFiles,
               TracktionArchiveFile::CompressionType,
               juce::Array<juce::File>& filesForDeletion,
               juce::StringArray& failedFiles,
               bool includeLibraryFiles,
               bool includeClips);

    ~ExportJob();

    //==============================================================================
    JobStatus runJob() override;
    float getCurrentTaskProgress() override;

private:
    Edit* edit = nullptr;
    float progress = 0;
    juce::File destDir;
    Project::Ptr newProject, srcProject;
    double handleSize = 0;
    bool keepEntireFiles = false;
    TracktionArchiveFile::CompressionType compressionType;
    TracktionArchiveFile* archive = nullptr;
    juce::Array<juce::File>& filesForDeletion;
    juce::OwnedArray<Edit> edits;
    juce::StringArray& failedFiles;
    bool includeLibraryFiles = false;
    bool includeClips = false;

    void copyEditFilesToTempDir();
    void copyProjectFilesToTempDir();
    void createArchiveFromTempFiles();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportJob)
};

} // namespace tracktion_engine
