/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


TemporaryFileManager::TemporaryFileManager (Engine& e)  : engine (e)
{
    updateDir();
}

TemporaryFileManager::~TemporaryFileManager()
{
}

File TemporaryFileManager::getDefaultTempDir()
{
    return engine.getPropertyStorage().getAppCacheFolder().getChildFile ("Temporary");
}

bool TemporaryFileManager::setTempDirectory (const File& newFile)
{
    if (newFile == getDefaultTempDir())
    {
        setToDefaultDirectory();
    }
    else
    {
        String path (newFile.getFullPathName());
        const File parent (getDefaultTempDir().getParentDirectory());

        if (newFile.isAChildOf (parent))
            path = newFile.getRelativePathFrom (parent);

        engine.getPropertyStorage().setProperty (SettingID::tempDirectory, path);
        updateDir();

        jassert (newFile == tempDir);
    }

    return wasTempFolderSuccessfullyCreated();
}

void TemporaryFileManager::setToDefaultDirectory()
{
    tempDir = getDefaultTempDir();
    engine.getPropertyStorage().removeProperty (SettingID::tempDirectory);
}

void TemporaryFileManager::updateDir()
{
    String userFolder = engine.getPropertyStorage().getProperty (SettingID::tempDirectory).toString().trim();

    if (userFolder.isEmpty())
        setToDefaultDirectory();
    else
        tempDir = getDefaultTempDir().getSiblingFile (userFolder);

    if (! wasTempFolderSuccessfullyCreated())
    {
        tempDir = getDefaultTempDir();
        engine.getPropertyStorage().removeProperty (SettingID::tempDirectory);
    }
}

//==============================================================================
bool TemporaryFileManager::wasTempFolderSuccessfullyCreated() const
{
    return (tempDir.isDirectory() || tempDir.createDirectory())
             && tempDir.hasWriteAccess();
}

bool TemporaryFileManager::isDiskSpaceDangerouslyLow() const
{
    auto freeMb = tempDir.getBytesFreeOnVolume() / (1024 * 1024);
    return freeMb < 80;
}

static bool shouldDeleteTempFile (const File& f, bool spaceIsShort)
{
    auto fileName = f.getFileName();

    if (fileName.startsWith ("preview_"))
        return false;

    if (fileName.startsWith ("temp_"))
        return true;

    const double daysOld = (Time::getCurrentTime() - f.getLastAccessTime()).inDays();

    return daysOld > 10.0 || (spaceIsShort && daysOld > 1.0);
}

static void deleteEditPreviewsNotInUse (Array<File>& files)
{
    auto& pm = *ProjectManager::getInstance();
    StringArray ids;

    for (auto& p : pm.getAllProjects (pm.folders))
        for (auto& itemID : p->getAllProjectItemIDs())
            ids.add (itemID.toStringSuitableForFilename());

    for (int i = files.size(); --i >= 0;)
    {
        auto fileName = files.getUnchecked (i).getFileNameWithoutExtension();

        if (! fileName.startsWith ("preview_"))
            continue;

        auto itemID = fileName.fromFirstOccurrenceOf ("preview_", false, false);

        if (itemID.isNotEmpty() && ! ids.contains (itemID))
            files.removeAndReturn (i).deleteFile();
    }
}

// sorts files by last accesss, oldest first
struct FileDateSorter
{
    static int compareElements (const File& first, const File& second)
    {
        return (int) (first.getLastAccessTime().toMilliseconds()
                        - second.getLastAccessTime().toMilliseconds());
    }
};

void TemporaryFileManager::cleanUp()
{
    TRACKTION_LOG ("Cleaning up temp files..");

    Array<File> tempFiles;
    tempDir.findChildFiles (tempFiles, File::findFiles, true);

    deleteEditPreviewsNotInUse (tempFiles);

    int64 totalBytes = 0;

    for (auto& f : tempFiles)
        totalBytes += f.getSize();

    FileDateSorter sorter;
    tempFiles.sort (sorter);
    int numFiles = tempFiles.size();

    // allow ourselves to use up to 25% of the free space, or 500mb, whichever's smaller
    int64 maxSizeToKeep = tempDir.getBytesFreeOnVolume() / 4;
    if (maxSizeToKeep > 1024 * 1024 * 500)
        maxSizeToKeep = 1024 * 1024 * 500;

    for (auto& f : tempFiles)
    {
        if (shouldDeleteTempFile (f, totalBytes > maxSizeToKeep || numFiles > 500))
        {
            totalBytes -= f.getSize();
            f.deleteFile();
            --numFiles;
        }
    }

    for (DirectoryIterator i (getTempDirectory(), false, "edit_*", File::findDirectories); i.next();)
        if (i.getFile().getNumberOfChildFiles (File::findFilesAndDirectories) == 0)
            i.getFile().deleteRecursively();
}

File TemporaryFileManager::getTempFile (const String& filename) const
{
    return tempDir.getChildFile (filename);
}

File TemporaryFileManager::getUniqueTempFile (const String& prefix, const String& ext) const
{
    return tempDir.getChildFile (prefix + String::toHexString (Random::getSystemRandom().nextInt64()))
                  .withFileExtension (ext)
                  .getNonexistentSibling();
}

File TemporaryFileManager::getThumbnailsFolder() const
{
    return getTempDirectory().getChildFile ("thumbnails");
}
