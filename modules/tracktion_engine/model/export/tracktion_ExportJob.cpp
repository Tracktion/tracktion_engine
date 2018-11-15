/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ExportJob::ExportJob (Edit* edit_,
                      const File& destDir_,
                      const Project::Ptr& newProject_,
                      const Project::Ptr& srcProject_,
                      TracktionArchiveFile* archive_,
                      double handleSize_,
                      bool keepEntireFiles_,
                      TracktionArchiveFile::CompressionType compressionType_,
                      Array<File>& filesForDeletion_,
                      StringArray& failedFiles_,
                      bool includeLibraryFiles_,
                      bool includeClips_)
    : ThreadPoolJobWithProgress (TRANS("Exporting") + "..."),
      edit (edit_),
      destDir (destDir_),
      newProject (newProject_),
      srcProject (srcProject_),
      handleSize (handleSize_),
      keepEntireFiles (keepEntireFiles_),
      compressionType (compressionType_),
      archive (archive_),
      filesForDeletion (filesForDeletion_),
      failedFiles (failedFiles_),
      includeLibraryFiles (includeLibraryFiles_),
      includeClips (includeClips_)
{
    jassert (srcProject != nullptr);
    jassert (newProject != nullptr);
}

ExportJob::~ExportJob()
{
}

//==============================================================================
ThreadPoolJob::JobStatus ExportJob::runJob()
{
    CRASH_TRACER

    FloatVectorOperations::disableDenormalisedNumberSupport();

    if (edit != nullptr)
        copyEditFilesToTempDir();
    else
        copyProjectFilesToTempDir();

    if (archive != nullptr && ! shouldExit())
        createArchiveFromTempFiles();

    if (shouldExit())
    {
        if (archive != nullptr)
        {
            archive->flush();
            filesForDeletion.add (archive->getFile());
            filesForDeletion.add (destDir);
        }
        else
        {
            destDir.findChildFiles (filesForDeletion, File::findFiles, true);
        }

        failedFiles.clear();
    }

    return jobHasFinished;
}

//==============================================================================
void ExportJob::copyProjectFilesToTempDir()
{
    bool showedSpaceWarning = false;

    // exporting the whole project
    auto newProjectFile = destDir.getChildFile (srcProject->getProjectFile().getFileName());

    if (srcProject->getProjectFile().copyFileTo (newProjectFile))
    {
        newProjectFile.setReadOnly (false);

        auto destProject = srcProject->projectManager.createNewProject (newProjectFile);
        destProject->createNewProjectId();

        for (int i = 0; i < destProject->getNumMediaItems(); ++i)
        {
            progress = ((archive != nullptr) ? 0.5f : 1.0f) * i
                          / (float) destProject->getNumMediaItems();

            if (shouldExit())
                break;

            auto srcObject  = srcProject->getProjectItemAt (i);
            auto destObject = destProject->getProjectItemAt (i);

            if (srcObject != nullptr && destObject != nullptr
                 && srcObject->getSourceFile().existsAsFile())
            {
                auto dest = destDir.getChildFile (srcObject->getSourceFile().getFileName())
                                   .getNonexistentSibling (true);

                auto bytesFree = dest.getBytesFreeOnVolume();
                auto bytesNeeded = jmax (2 * srcObject->getSourceFile().getSize(),
                                         (int64) (1024 * 1024 * 50));

                if (bytesFree > 0
                     && bytesFree < bytesNeeded
                     && ! showedSpaceWarning)
                {
                    showedSpaceWarning = true;
                    Engine::getInstance().getUIBehaviour().showWarningAlert (TRANS("Exporting"),
                                                                             TRANS("Disk space is critically low!")
                                                                               + "\n\n"
                                                                               + TRANS("Not all files may be exported correctly."));
                }

                if (! srcObject->getSourceFile().copyFileTo (dest))
                {
                    failedFiles.add (dest.getFileName());
                }
                else
                {
                    dest.setReadOnly (false);
                    destObject->setSourceFile (dest);
                }
            }
        }

        callBlocking ([this, &destProject]()
                      {
                          destProject->redirectIDsFromProject (srcProject->getProjectID(), destProject->getProjectID());
                          destProject->save();
                      });
    }
    else
    {
        failedFiles.add (srcProject->getProjectFile().getFullPathName());
    }
}

//==============================================================================
void ExportJob::copyEditFilesToTempDir()
{
    if (! includeClips)
    {
        for (auto t : getClipTracks (*edit))
        {
            for (int j = t->getClips().size(); --j >= 0;)
            {
                auto clip = t->getClips().getUnchecked (j);

                if (clip->type != TrackItem::Type::video
                      && clip->type != TrackItem::Type::marker)
                {
                    clip->removeFromParentTrack();
                }
            }
        }
    }

    auto allExportables = Exportable::addAllExportables (*edit);

    if (keepEntireFiles)
        handleSize = 10000.0;

    auto& projectManager = srcProject->projectManager;
    ReferencedMaterialList refList (projectManager, handleSize);

    for (auto exportable : allExportables)
        for (auto& i : exportable->getReferencedItems())
            refList.add (i);

    int numThingsCopied = 0;
    const int totalThingsToCopy = refList.getTotalNumThingsToExport();

    for (auto exportable : allExportables)
    {
        if (shouldExit())
            break;

        for (auto ref : exportable->getReferencedItems())
        {
            progress = ((archive != nullptr ? 0.5f : 1.0f) * numThingsCopied) / totalThingsToCopy;

            if (shouldExit())
                break;

            double start = 0.0, length = 0.0;
            auto newFilename = refList.getReassignedFileName (ref.itemID, ref.firstTimeUsed,
                                                              start, length);

            if (length > 0.0 && newFilename.isNotEmpty())
            {
                auto oldSourceMedia (projectManager.getProjectItem (ref.itemID));

                if (oldSourceMedia != nullptr
                     && (includeLibraryFiles || ! oldSourceMedia->getProject()->isLibraryProject()))
                {
                    ProjectItem::Ptr newSourceItem;

                    // the file we're creating as a section of the whole thing
                    auto newFile = destDir.getChildFile (newFilename);

                    if (! newFile.exists())
                    {
                        File actualNewFile;

                        if (! oldSourceMedia->copySectionToNewFile (newFile, actualNewFile, start, length))
                        {
                            failedFiles.add (oldSourceMedia->getFileName());
                            TRACKTION_LOG_ERROR ("Failed to copy file during edit archive: " + newFile.getFullPathName());
                        }
                        else
                        {
                            newFile = actualNewFile;

                            newSourceItem = newProject->createNewItem (newFile,
                                                                       oldSourceMedia->getType(),
                                                                       oldSourceMedia->getName(),
                                                                       oldSourceMedia->getDescription(),
                                                                       oldSourceMedia->getCategory(),
                                                                       true);

                            if (newSourceItem != nullptr)
                                newSourceItem->copyAllPropertiesFrom (*oldSourceMedia);
                        }

                        ++numThingsCopied;
                    }
                    else
                    {
                        newSourceItem = newProject->getProjectItemForFile (newFile);
                    }

                    auto newID = newSourceItem != nullptr ? newSourceItem->getID() : ProjectItemID();

                    callBlocking ([=]() { exportable->reassignReferencedItem (ref, newID, start); });
                }
            }
            else
            {
                // couldn't create the new clip, so avoid pointing at the old one
                callBlocking ([&]() { exportable->reassignReferencedItem (ref, {}, start); });
            }
        }
    }

    // put the new edit at the top of the list
    newProject->moveProjectItem (newProject->getIndexOf (edit->getProjectItemID()), 0);

    if (edit != nullptr)
        callBlocking ([this] { EditFileOperations (*edit).save (true, true, false); });
}

//==============================================================================
void ExportJob::createArchiveFromTempFiles()
{
    if (archive != nullptr && ! shouldExit())
    {
        if (newProject != nullptr)
        {
            newProject->save();
            newProject->unlockFile();
        }

        destDir.findChildFiles (filesForDeletion, File::findFiles, true);

        for (int i = 0; i < filesForDeletion.size(); ++i)
        {
            progress = 0.5f + 0.5f * i / filesForDeletion.size();

            if (shouldExit())
                break;

            auto compression = TracktionArchiveFile::CompressionType::zip;
            const File f (filesForDeletion[i]);

            if (AudioFile (f).isValid())
                compression = compressionType;

            if (! archive->addFile (f, destDir, compression))
                failedFiles.add (f.getFileName());
        }

        filesForDeletion.clear();
        filesForDeletion.add (destDir);
    }
}

//==============================================================================
float ExportJob::getCurrentTaskProgress()
{
    return progress;
}
