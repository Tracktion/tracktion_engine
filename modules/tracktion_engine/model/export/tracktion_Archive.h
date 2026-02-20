/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** Returns true if the file is a legacy .trkarch archive. */
bool isLegacyArchive (Engine&, const juce::File&);

/** Returns true if the file is any supported archive format (legacy or zip). */
bool isArchive (Engine&, const juce::File&);

//==============================================================================
/** Job that archives a Project or single Edit into a standard zip file.

    For folder-based projects: consolidates then zips.
    For file-based projects: converts to folder-based in a temp dir first,
    then consolidates and zips.
*/
class ArchiveJob  : public ThreadPoolJobWithProgress
{
public:
    /** What to archive. */
    using Source = std::variant<Project*, ProjectItem*>;

    /** Compression level for the zip archive (maps to zlib levels 0-9, -1 = default). */
    enum class CompressionLevel
    {
        uncompressed  = 0,
        fastest       = 1,
        normal        = 6,
        best          = 9,
        defaultLevel  = -1
    };

    ArchiveJob (Source source,
                const juce::File& destZipFile,
                CompressionLevel);

    ~ArchiveJob() override;

    //==============================================================================
    JobStatus runJob() override;
    float getCurrentTaskProgress() override;
    bool canCancel() const override;

    /** Runs the archive process synchronously on the calling thread.
        Useful when already on the message thread (avoids MessageManager::callSync deadlock).
        Returns true on success. Call getError() for details on failure. */
    bool runSynchronously();

    /** After runJob(), returns error message (empty on success). */
    juce::String getError() const;

private:
    Source source;
    juce::File destZipFile;
    CompressionLevel compressionLevel;
    std::atomic<float> progress { 0.0f };
    juce::String errorMessage;

    // Steps
    bool copyToTempDir();
    bool consolidate();
    bool createZip();

    // Temp state
    choc::file::TempFile tempDir;
    Project::Ptr tempProject;
    std::vector<std::unique_ptr<Edit>> tempEdits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArchiveJob)
};

} // namespace tracktion::inline engine
