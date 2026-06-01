/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <ranges>

namespace tracktion::inline engine {

bool isLegacyArchive (Engine& engine, const juce::File& file)
{
    legacy::TracktionArchiveFile archive (engine, file);
    return archive.isValidArchive();
}

bool isArchive (Engine& engine, const juce::File& file)
{
    if (isLegacyArchive (engine, file))
        return true;

    // Check if it's a valid zip containing at least one .tracktionedit file
    juce::ZipFile zip (file);

    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (auto entry = zip.getEntry (i))
            if (entry->filename.endsWithIgnoreCase (".tracktionedit"))
                return true;

    return false;
}

//==============================================================================
ArchiveJob::ArchiveJob (Source src,
                        const juce::File& dest,
                        CompressionLevel level)
    : ThreadPoolJobWithProgress (TRANS("Archiving") + "..."),
      source (std::move (src)),
      destZipFile (dest),
      compressionLevel (level)
{
}

ArchiveJob::~ArchiveJob() = default;

float ArchiveJob::getCurrentTaskProgress()
{
    return progress.load();
}

bool ArchiveJob::canCancel() const
{
    return true;
}

juce::String ArchiveJob::getError() const
{
    return errorMessage;
}

juce::ThreadPoolJob::JobStatus ArchiveJob::runJob()
{
    CRASH_TRACER
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

    if (! copyToTempDir())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to copy project files");

        return jobHasFinished;
    }

    if (shouldExit())
        return jobHasFinished;

    if (! consolidate())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to consolidate project");

        return jobHasFinished;
    }

    if (shouldExit())
        return jobHasFinished;

    if (! createZip())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to create archive");

        return jobHasFinished;
    }

    return jobHasFinished;
}

bool ArchiveJob::runSynchronously()
{
    CRASH_TRACER

    if (! copyToTempDir())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to copy project files");

        return false;
    }

    if (! consolidate())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to consolidate project");

        return false;
    }

    if (! createZip())
    {
        if (errorMessage.isEmpty())
            errorMessage = TRANS("Failed to create archive");

        return false;
    }

    return true;
}

//==============================================================================
static Engine& getEngineFromSource (const ArchiveJob::Source& source)
{
    return std::visit ([] (auto* ptr) -> Engine& { return ptr->engine; }, source);
}

bool ArchiveJob::copyToTempDir()
{
    progress = 0.0f;

    auto& engine = getEngineFromSource (source);

    // Create temp directory
    tempDir = choc::file::TempFile (choc::file::TempFile::createRandomFilename ("tracktion_archive", ""));
    auto tempDirFile = juce::File (tempDir.file.string());

    if (auto project = std::get_if<Project*> (&source))
    {
        // Flush any pending changes before copying
        (*project)->save();

        // Copy the whole project folder to temp
        auto srcDir = (*project)->getDefaultDirectory();

        if ((*project)->isFolderBased())
        {
            if (! srcDir.copyDirectoryTo (tempDirFile.getChildFile (srcDir.getFileName())))
            {
                errorMessage = TRANS("Failed to copy project folder");
                return false;
            }

            auto copiedProjectDir = tempDirFile.getChildFile (srcDir.getFileName());
            tempProject = engine.getProjectManager().createNewProject (copiedProjectDir);
        }
        else
        {
            // File-based project: copy to temp and convert to folder-based
            auto projectFile = (*project)->getProjectFile();
            auto projDir = (*project)->getDefaultDirectory();
            auto copiedDir = tempDirFile.getChildFile (projDir.getFileName());

            if (! srcDir.copyDirectoryTo (copiedDir))
            {
                errorMessage = TRANS("Failed to copy project directory");
                return false;
            }

            progress = 0.1f;

            auto copiedProjectFile = copiedDir.getChildFile (projectFile.getFileName());
            auto copiedProject = engine.getProjectManager().createNewProject (copiedProjectFile);

            if (copiedProject == nullptr)
            {
                errorMessage = TRANS("Failed to open copied project");
                return false;
            }

            tempProject = convertToFolderBasedProject (*copiedProject);

            if (tempProject == nullptr)
            {
                errorMessage = TRANS("Failed to convert project to folder-based format");
                return false;
            }
        }
    }
    else if (auto projectItem = std::get_if<ProjectItem*> (&source))
    {
        // Single edit: copy the edit and its referenced files to temp
        auto& pm = engine.getProjectManager();
        auto srcFile = (*projectItem)->getSourceFile();

        if (! srcFile.existsAsFile())
        {
            errorMessage = TRANS("Source edit file doesn't exist");
            return false;
        }

        // Create a folder-based project in temp
        auto projectDir = tempDirFile.getChildFile (srcFile.getFileNameWithoutExtension() + "_project");

        if (! projectDir.createDirectory())
        {
            errorMessage = TRANS("Couldn't create temporary project directory");
            return false;
        }

        tempProject = pm.createNewProject (projectDir);

        if (tempProject == nullptr)
        {
            errorMessage = TRANS("Couldn't create temporary project");
            return false;
        }

        tempProject->createNewProjectId();

        // Copy the edit file into the project
        auto destEditFile = projectDir.getChildFile (srcFile.getFileName());

        if (! srcFile.copyFileTo (destEditFile))
        {
            errorMessage = TRANS("Failed to copy edit file");
            return false;
        }

        auto destEditItem = tempProject->createNewItem (destEditFile,
                                                        ProjectItem::editItemType(),
                                                        (*projectItem)->getName(),
                                                        (*projectItem)->getDescription(),
                                                        (*projectItem)->getCategory(),
                                                        true);

        if (! destEditItem)
        {
            errorMessage = TRANS("Failed create new Edit");
            return false;
        }

        progress = 0.1f;

        // Copy referenced media files
        auto edit = loadEditForExamining (engine.getProjectManager(),
                                         (*projectItem)->getProjectItemRef());

        if (edit != nullptr)
        {
            struct ExportableUpdate
            {
                ProjectItemRef oldRef;
                ProjectItemRef newRef;
            };

            struct FileToCopy
            {
                juce::File mediaSrc;
                juce::String itemType, itemName, itemDescription;
                ProjectItem::Category itemCategory;
                ProjectItemRef oldRef;
            };

            // Pass 1: Collect all files to copy
            std::vector<FileToCopy> filesToCopy;

            for (auto e : Exportable::addAllExportables (*edit))
            {
                for (auto& ref : e->getReferencedItems())
                {
                    if (auto item = pm.getProjectItem (ref.itemRef))
                    {
                        auto mediaSrc = item->getSourceFile();

                        if (mediaSrc.existsAsFile() && ! mediaSrc.isAChildOf (projectDir))
                            filesToCopy.push_back ({ mediaSrc, item->getType(), item->getName(),
                                                     item->getDescription(), item->getCategory(),
                                                     ref.itemRef });
                    }
                    else
                    {
                        // Path-based ref without owner (folder-based projects) - resolve directly
                        auto srcProject = (*projectItem)->getProject();
                        auto mediaSrc = srcProject != nullptr
                                            ? ref.itemRef.resolve (engine, srcProject->getDefaultDirectory())
                                            : ref.itemRef.resolve (engine);

                        if (mediaSrc.existsAsFile() && ! mediaSrc.isAChildOf (projectDir))
                            filesToCopy.push_back ({ mediaSrc, ProjectItem::waveItemType(),
                                                     mediaSrc.getFileNameWithoutExtension(),
                                                     {}, ProjectItem::Category::imported,
                                                     ref.itemRef });
                    }
                }
            }

            // Pass 2: Copy files with progress
            std::vector<ExportableUpdate> exportablesToUpdate;
            auto totalFiles = (int) filesToCopy.size();

            for (int i = 0; i < totalFiles; ++i)
            {
                if (shouldExit())
                    return false;

                auto& f = filesToCopy[(size_t) i];

                auto srcProject = (*projectItem)->getProject();
                auto srcProjectDir = srcProject != nullptr ? srcProject->getDefaultDirectory()
                                                           : juce::File();

                juce::File destMedia;

                if (srcProjectDir != juce::File() && f.mediaSrc.isAChildOf (srcProjectDir))
                {
                    // Preserve the source project's layout (e.g. "Imported/foo.ogg",
                    // "Recorded/take 3.wav", or any custom subfolder structure).
                    auto relPath = f.mediaSrc.getRelativePathFrom (srcProjectDir);
                    destMedia = projectDir.getChildFile (relPath).getNonexistentSibling (true);
                    destMedia.getParentDirectory().createDirectory();
                }
                else
                {
                    // External file — drop it into the appropriate category folder.
                    auto mediaDir = tempProject->getDirectoryForMedia (f.itemCategory);
                    destMedia = mediaDir.getChildFile (f.mediaSrc.getFileName()).getNonexistentSibling (true);
                }

                if (f.mediaSrc.copyFileTo (destMedia))
                {
                    auto destItem = tempProject->createNewItem (destMedia,
                                                                f.itemType,
                                                                f.itemName,
                                                                f.itemDescription,
                                                                f.itemCategory,
                                                                true);
                    exportablesToUpdate.push_back (ExportableUpdate (f.oldRef, destItem->getProjectItemRef()));
                }

                progress = 0.1f + (0.2f * (float) (i + 1) / (float) totalFiles);
            }

            // Copy any extra loose files the Edit depends on that aren't tracked
            // as ProjectItems (e.g. plugin patch folders). These are preserved
            // with their layout relative to the source project dir so references
            // into them can be relocated after extraction.
            if (auto srcProject = (*projectItem)->getProject())
            {
                auto srcProjectDir = srcProject->getDefaultDirectory();

                for (auto& extraFile : engine.getEngineBehaviour().getExtraFilesToArchive (*edit))
                {
                    if (! extraFile.existsAsFile() || ! extraFile.isAChildOf (srcProjectDir))
                        continue;

                    auto relPath = extraFile.getRelativePathFrom (srcProjectDir);
                    auto destFile = projectDir.getChildFile (relPath);

                    if (destFile.existsAsFile())
                        continue;

                    destFile.getParentDirectory().createDirectory();
                    extraFile.copyFileTo (destFile);
                }
            }

            // Reassign refs on the dest Edit, then let it destruct on the
            // message thread before consolidate() runs.
            bool destEditLoadFailed = false;

            juce::MessageManager::callSync ([&engine, &destEditItem, &exportablesToUpdate, &destEditLoadFailed]
                                            {
                                                auto destEdit = loadEditForExamining (engine.getProjectManager(),
                                                                                      destEditItem->getProjectItemRef());

                                                if (destEdit == nullptr)
                                                {
                                                    destEditLoadFailed = true;
                                                    return;
                                                }

                                                for (auto destExportable : Exportable::addAllExportables (*destEdit))
                                                {
                                                    for (const auto& destReferencedItem : destExportable->getReferencedItems())
                                                    {
                                                        if (auto foundRef = std::ranges::find (exportablesToUpdate, destReferencedItem.itemRef, &ExportableUpdate::oldRef);
                                                            foundRef != exportablesToUpdate.end())
                                                        {
                                                            std::cout << "Reassigning:\n"
                                                                      << "\t" << destReferencedItem.itemRef.toString() << "\n"
                                                                      << "\t" << foundRef->newRef.toString() << std::endl;
                                                            destExportable->reassignReferencedItem (destReferencedItem, foundRef->newRef, 0.0);
                                                        }
                                                    }
                                                }

                                                // Persist the reassigned references before destEdit destructs:
                                                // consolidate() reloads each edit from disk, so without this save it
                                                // would see the original (pre-relocation) references and re-resolve
                                                // them against their source locations instead of the archived copies.
                                                EditFileOperations (*destEdit).writeToFile (destEditItem->getSourceFile(), false);

                                                // destEdit destructs here on the message thread.
                                            });

            if (destEditLoadFailed)
            {
                errorMessage = TRANS("Couldn't create new edit");
                return false;
            }
        }

        // Destroy the original `edit` on the message thread before continuing.
        juce::MessageManager::callSync ([&edit] { edit.reset(); });

        tempProject->save();
    }

    if (tempProject != nullptr)
        tempProject->setTemporary (true);

    progress = 0.3f;
    return true;
}

bool ArchiveJob::consolidate()
{
    if (tempProject == nullptr)
    {
        errorMessage = TRANS("No project to consolidate");
        return false;
    }

    progress = 0.3f;

    std::pair<int, juce::String> numImportedAndError;
    juce::MessageManager::callSync ([this, &numImportedAndError]
                                    {
                                        numImportedAndError = ProjectUtilities::consolidateProject (*tempProject);
                                    });

    auto [numImported, error] = numImportedAndError;
    juce::ignoreUnused (numImported);

    if (error.isNotEmpty())
    {
        errorMessage = error;
        return false;
    }

    progress = 0.5f;
    return true;
}

bool ArchiveJob::createZip()
{
    progress = 0.5f;

    // Find the root dir to zip
    juce::File rootDir;

    if (tempProject->isFolderBased())
        rootDir = tempProject->getDefaultDirectory();
    else
        rootDir = juce::File (tempDir.file.string());

    // Save the project before zipping
    tempProject->save();

    // Add archive_type to project_info.json
    {
        auto infoFile = rootDir.getChildFile ("project_info.json");
        auto infoJson = juce::JSON::parse (infoFile);
        auto obj = infoJson.getDynamicObject();

        if (obj == nullptr)
        {
            obj = new juce::DynamicObject();
            infoJson = juce::var (obj);
        }

        obj->setProperty ("archive_type", 1);
        infoFile.replaceWithText (juce::JSON::toString (infoJson));
    }

    // Collect all files to zip
    juce::Array<juce::File> files;
    rootDir.findChildFiles (files, juce::File::findFiles, true);

    if (files.isEmpty())
    {
        errorMessage = TRANS("No files to archive");
        return false;
    }

    auto stream = std::make_shared<std::ofstream> (destZipFile.getFullPathName().toStdString(),
                                                    std::ios::binary);

    if (! stream->is_open())
    {
        errorMessage = TRANS("Couldn't create archive file");
        return false;
    }

    auto level = static_cast<choc::zip::ZipWriter::CompressionLevel> (compressionLevel);
    choc::zip::ZipWriter writer (stream);
    auto totalFiles = files.size();

    for (int i = 0; i < totalFiles; ++i)
    {
        auto& f = files.getReference (i);
        auto relativePath = f.getRelativePathFrom (rootDir)
                             .replaceCharacter ('\\', '/');

        std::ifstream fileStream (f.getFullPathName().toStdString(), std::ios::binary);

        if (! fileStream.is_open())
        {
            errorMessage = TRANS("Failed to read file: ") + f.getFileName();
            return false;
        }

        writer.addFileFromStream (relativePath.toStdString(), fileStream, level);
        progress = 0.5f + (0.5f * static_cast<float> (i + 1) / static_cast<float> (totalFiles));
    }

    writer.flush();
    return true;
}

} // namespace tracktion::inline engine
