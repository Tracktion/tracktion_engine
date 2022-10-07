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

TemporaryFileManager::TemporaryFileManager (Engine& e)  : engine (e)
{
    updateDir();
}

TemporaryFileManager::~TemporaryFileManager()
{
}

static juce::File getDefaultTempFolder (Engine& engine)
{
    return engine.getPropertyStorage().getAppCacheFolder().getChildFile ("Temporary");
}

const juce::File& TemporaryFileManager::getTempDirectory() const
{
    return tempDir;
}

bool TemporaryFileManager::setTempDirectory (const juce::File& newFile)
{
    auto defaultDir = getDefaultTempFolder (engine);

    if (newFile == defaultDir)
    {
        ressetToDefaultLocation();
    }
    else
    {
        auto path = newFile.getFullPathName();
        auto parent = defaultDir.getParentDirectory();

        if (newFile.isAChildOf (parent))
            path = newFile.getRelativePathFrom (parent);

        engine.getPropertyStorage().setProperty (SettingID::tempDirectory, path);
        updateDir();

        jassert (newFile == tempDir);
    }

    return wasTempFolderSuccessfullyCreated();
}

void TemporaryFileManager::ressetToDefaultLocation()
{
    tempDir = getDefaultTempFolder (engine);
    engine.getPropertyStorage().removeProperty (SettingID::tempDirectory);
}

void TemporaryFileManager::updateDir()
{
    auto defaultDir = getDefaultTempFolder (engine);
    auto userFolder = engine.getPropertyStorage().getProperty (SettingID::tempDirectory).toString().trim();

    if (userFolder.isEmpty())
        ressetToDefaultLocation();
    else
        tempDir = defaultDir.getSiblingFile (userFolder);

    if (! wasTempFolderSuccessfullyCreated())
    {
        tempDir = defaultDir;
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

int64_t TemporaryFileManager::getMaxSpaceAllowedForTempFiles() const
{
    int64_t minAbsoluteSize = 1024 * 1024 * 750;
    int64_t minProportionOfDisk = tempDir.getBytesFreeOnVolume() / 4;

    return std::min (minProportionOfDisk, minAbsoluteSize);
}

int TemporaryFileManager::getMaxNumTempFiles() const
{
    return 1000;
}

static bool shouldDeleteTempFile (const juce::File& f, bool spaceIsShort)
{
    auto fileName = f.getFileName();

    if (fileName.startsWith ("preview_"))
        return false;

    if (fileName.startsWith ("temp_"))
        return true;

    auto daysOld = (juce::Time::getCurrentTime() - f.getLastAccessTime()).inDays();

    return daysOld > 60.0 || (spaceIsShort && daysOld > 1.0);
}

static void deleteEditPreviewsNotInUse (Engine& engine, juce::Array<juce::File>& files)
{
    auto& pm = engine.getProjectManager();
    juce::StringArray ids;

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

void TemporaryFileManager::cleanUp()
{
    CRASH_TRACER
    TRACKTION_LOG ("Cleaning up temp files..");

    auto tempFiles = tempDir.findChildFiles (juce::File::findFiles, true);

    deleteEditPreviewsNotInUse (engine, tempFiles);

    int64_t totalBytes = 0;

    for (auto& f : tempFiles)
        totalBytes += f.getSize();

    std::sort (tempFiles.begin(), tempFiles.end(),
               [] (const juce::File& first, const juce::File& second) -> bool
               {
                   return first.getLastAccessTime().toMilliseconds() < second.getLastAccessTime().toMilliseconds();
               });

    auto numFiles = tempFiles.size();
    auto maxNumFiles = getMaxNumTempFiles();
    auto maxSizeToKeep = getMaxSpaceAllowedForTempFiles();

    for (auto& f : tempFiles)
    {
        if (shouldDeleteTempFile (f, totalBytes > maxSizeToKeep || numFiles > maxNumFiles))
        {
            totalBytes -= f.getSize();
            f.deleteFile();
            --numFiles;
        }
    }

    for (auto entry : juce::RangedDirectoryIterator (getTempDirectory(), false, "edit_*", juce::File::findDirectories))
        if (entry.getFile().getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
            entry.getFile().deleteRecursively();
}

juce::File TemporaryFileManager::getTempFile (const juce::String& filename) const
{
    return tempDir.getChildFile (filename);
}

juce::File TemporaryFileManager::getUniqueTempFile (const juce::String& prefix, const juce::String& ext) const
{
    return tempDir.getChildFile (prefix + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()))
                  .withFileExtension (ext)
                  .getNonexistentSibling();
}

juce::File TemporaryFileManager::getThumbnailsFolder() const
{
    return getTempDirectory().getChildFile ("thumbnails");
}

//==============================================================================
static juce::String getClipProxyPrefix()                { return "clip_"; }
static juce::String getFileProxyPrefix()                { return "proxy_"; }
static juce::String getDeviceFreezePrefix (Edit& edit)  { return "freeze_" + edit.getProjectItemID().toStringSuitableForFilename() + "_"; }
static juce::String getTrackFreezePrefix()              { return "trackFreeze_"; }
static juce::String getCompPrefix()                     { return "comp_"; }

static AudioFile getCachedEditFile (Edit& edit, const juce::String& prefix, HashCode hash)
{
    return AudioFile (edit.engine, edit.getTempDirectory (true).getChildFile (prefix + juce::String::toHexString (hash) + ".wav"));
}

static AudioFile getCachedClipFileWithPrefix (const AudioClipBase& clip, const juce::String& prefix, HashCode hash)
{
    return getCachedEditFile (clip.edit, prefix + "0_" + clip.itemID.toString() + "_", hash);
}

AudioFile TemporaryFileManager::getFileForCachedClipRender (const AudioClipBase& clip, HashCode hash)
{
    return getCachedClipFileWithPrefix (clip, getClipProxyPrefix(), hash);
}

AudioFile TemporaryFileManager::getFileForCachedCompRender (const AudioClipBase& clip, HashCode takeHash)
{
    return getCachedClipFileWithPrefix (clip, getCompPrefix(), takeHash);
}

AudioFile TemporaryFileManager::getFileForCachedFileRender (Edit& edit, HashCode hash)
{
    return getCachedEditFile (edit, getFileProxyPrefix(), hash);
}

juce::File TemporaryFileManager::getFreezeFileForDevice (Edit& edit, OutputDevice& device)
{
    return edit.getTempDirectory (true)
             .getChildFile (getDeviceFreezePrefix (edit) + juce::String (device.getDeviceID()) + ".freeze");
}

juce::String TemporaryFileManager::getDeviceIDFromFreezeFile (Edit& edit, const juce::File& deviceFreezeFile)
{
    const auto fileName = deviceFreezeFile.getFileName();
    jassert (fileName.startsWith (getDeviceFreezePrefix (edit)));
    jassert (fileName.endsWith (".freeze"));

    return fileName.fromLastOccurrenceOf (getDeviceFreezePrefix (edit), false, false)
                   .upToFirstOccurrenceOf (".", false, false);
}

juce::File TemporaryFileManager::getFreezeFileForTrack (const AudioTrack& track)
{
    return track.edit.getTempDirectory (true)
             .getChildFile (getTrackFreezePrefix() + "0_" + track.itemID.toString() + ".freeze");
}

juce::Array<juce::File> TemporaryFileManager::getFrozenTrackFiles (Edit& edit)
{
    return edit.getTempDirectory (false)
             .findChildFiles (juce::File::findFiles, false, getDeviceFreezePrefix (edit) + "*");
}

static ProjectItemID getProjectItemIDFromFilename (const juce::String& name)
{
    auto tokens = juce::StringArray::fromTokens (name.fromFirstOccurrenceOf ("_", false, false), "_", {});

    if (tokens.size() <= 1)
        return {};

    return ProjectItemID (tokens[0] + "_" + tokens[1]);
}

void TemporaryFileManager::purgeOrphanEditTempFolders (ProjectManager& pm)
{
    CRASH_TRACER
    juce::Array<juce::File> filesToDelete;
    juce::StringArray reasonsForDeletion;

    for (auto entry : juce::RangedDirectoryIterator (getTempDirectory(), false, "edit_*", juce::File::findDirectories))
    {
        auto itemID = getProjectItemIDFromFilename (entry.getFile().getFileName());

        if (itemID.isValid())
        {
            if (pm.getProjectItem (itemID) == nullptr)
            {
                filesToDelete.add (entry.getFile());

                auto pp = pm.getProject (itemID.getProjectID());

                if (itemID.getProjectID() == 0)
                    reasonsForDeletion.add ("Invalid project ID");
                else if (pp == nullptr)
                    reasonsForDeletion.add ("Can't find project");
                else if (pp->getProjectItemForID (itemID) == nullptr)
                    reasonsForDeletion.add ("Can't find source media");
                else
                    reasonsForDeletion.add ("Unknown");
            }
        }
    }

    for (int i = filesToDelete.size(); --i >= 0;)
    {
        TRACKTION_LOG ("Purging edit folder: " + filesToDelete.getReference (i).getFileName() + " - " + reasonsForDeletion[i]);
        filesToDelete.getReference (i).deleteRecursively();
    }
}

static EditItemID getEditItemIDFromFilename (const juce::String& name)
{
   auto tokens = juce::StringArray::fromTokens (name.fromFirstOccurrenceOf ("_", false, false), "_", {});

    if (tokens.isEmpty())
        return {};

    // TODO: think about backwards-compatibility for these strings - was a two-part
    // ID separated by an underscore
    return EditItemID::fromVar (tokens[0] + tokens[1]);
}

void TemporaryFileManager::purgeOrphanFreezeAndProxyFiles (Edit& edit)
{
    CRASH_TRACER
    juce::Array<juce::File> filesToDelete;

    for (auto entry : juce::RangedDirectoryIterator (edit.getTempDirectory (false), false, "*"))
    {
        auto name = entry.getFile().getFileName();
        auto itemID = getEditItemIDFromFilename (name);

        if (itemID.isValid())
        {
            if (name.startsWith (getClipProxyPrefix())
                 || name.startsWith (getCompPrefix()))
            {
                auto clip = findClipForID (edit, itemID);

                if (auto acb = dynamic_cast<AudioClipBase*> (clip))
                {
                    if (! acb->isUsingFile (AudioFile (edit.engine, entry.getFile())))
                        filesToDelete.add (entry.getFile());
                }
                else if (clip == nullptr)
                {
                    filesToDelete.add (entry.getFile());
                }
            }
            else if (name.startsWith (getFileProxyPrefix()))
            {
                // XXX
            }
            else if (name.startsWith (getTrackFreezePrefix()))
            {
                if (auto at = dynamic_cast<AudioTrack*> (findTrackForID (edit, itemID)))
                    if (! at->isFrozen (Track::individualFreeze))
                        filesToDelete.add (entry.getFile());
            }
        }
        else if (name.startsWith (RenderManager::getFileRenderPrefix()))
        {
            if (! edit.areAnyClipsUsingFile (AudioFile (edit.engine, entry.getFile())))
                filesToDelete.add (entry.getFile());
        }
    }

    for (auto& f : filesToDelete)
    {
        DBG ("Purging temp file: " << f.getFileName());
        AudioFile (edit.engine, f).deleteFile();
    }
}

}} // namespace tracktion { inline namespace engine
