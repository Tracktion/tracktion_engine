/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

ProjectManager::ProjectManager (Engine& e)
    : engine (e)
{
}

ProjectManager::~ProjectManager()
{
    CRASH_TRACER
    folders = {};
    jassert (openProjects.isEmpty());
}

//==============================================================================
void ProjectManager::initialise()
{
    loadList();

    auto& storage = engine.getPropertyStorage();

    if (storage.getProperty (SettingID::findExamples, false))
    {
        storage.setProperty (SettingID::findExamples, juce::var());

        auto examplesDir = storage.getAppPrefsFolder().getChildFile ("examples");

        auto exampleProjects = examplesDir.findChildFiles (juce::File::findFiles, true,
                                                           juce::String ("*") + projectFileSuffix);

        for (auto& f : exampleProjects)
            addProjectToList (f, false, getActiveProjectsFolder(),
                              f.getFileName().containsIgnoreCase ("Spiralling") ? 0 : -1);

        saveList();
    }
}

//==============================================================================
static void ensureAllItemsHaveIDs (const juce::ValueTree& folder)
{
    if (folder[IDs::uid].toString().isEmpty())
        juce::ValueTree (folder).setProperty (IDs::uid, juce::String::toHexString (juce::Random().nextInt()), nullptr);

    for (int i = 0; i < folder.getNumChildren(); ++i)
        ensureAllItemsHaveIDs (folder.getChild(i));
}

void ProjectManager::loadList()
{
    const juce::ScopedLock sl (lock);

    folders = {};

    auto xml = engine.getPropertyStorage().getXmlProperty (SettingID::projectList);

    if (xml != nullptr)
        folders = juce::ValueTree::fromXml (*xml);

    if (! folders.hasType (IDs::ROOT))
        folders = juce::ValueTree (IDs::ROOT);

    if (! getLibraryProjectsFolder().isValid())  folders.addChild (juce::ValueTree (IDs::LIBRARY), -1, nullptr);
    if (! getActiveProjectsFolder().isValid())   folders.addChild (juce::ValueTree (IDs::ACTIVE), 0, nullptr);

    jassert (getActiveProjectsFolder().isValid() && getLibraryProjectsFolder().isValid());

    if (xml == nullptr)   // import from T4 format:
    {
        xml = engine.getPropertyStorage().getXmlProperty (SettingID::projects);

        if (xml != nullptr)
        {
            auto oldT4 = juce::ValueTree::fromXml (*xml);

            {
                auto v = oldT4.getChildWithProperty (IDs::name, "Library Projects");

                for (int i = v.getNumChildren(); --i >= 0;)
                {
                    auto c = v.getChild(i);
                    v.removeChild (c, nullptr);
                    getLibraryProjectsFolder().addChild (c, 0, nullptr);
                }
            }

            {
                auto v = oldT4.getChildWithProperty (IDs::name, "Active Projects");

                for (int i = v.getNumChildren(); --i >= 0;)
                {
                    auto c = v.getChild(i);
                    v.removeChild (c, nullptr);
                    getActiveProjectsFolder().addChild (c, 0, nullptr);
                }
            }
        }
    }

    ensureAllItemsHaveIDs (folders);
}

static void stripProjectObjects (juce::ValueTree v)
{
    v.removeProperty (IDs::project, nullptr);

    for (int i = 0; i < v.getNumChildren(); ++i)
        stripProjectObjects (v.getChild(i));
}

void ProjectManager::saveList()
{
    const juce::ScopedLock sl (lock);

    auto foldersCopy = folders.createCopy();
    stripProjectObjects (foldersCopy);

    std::unique_ptr<juce::XmlElement> xml (foldersCopy.createXml());
    engine.getPropertyStorage().setXmlProperty (SettingID::projectList, *xml);
}

static void findProjects (ProjectManager& pm, const juce::ValueTree& folder,
                          juce::ReferenceCountedArray<Project>& list)
{
    if (auto p = pm.getProjectFrom (folder))
        list.add (p);

    for (int i = 0; i < folder.getNumChildren(); ++i)
        findProjects (pm, folder.getChild(i), list);
}

juce::ReferenceCountedArray<Project> ProjectManager::getAllProjects()
{
    const juce::ScopedLock sl (lock);
    juce::ReferenceCountedArray<Project> list;
    findProjects (*this, folders, list);
    return list;
}

juce::ReferenceCountedArray<Project> ProjectManager::getAllProjects (const juce::ValueTree& folder)
{
    const juce::ScopedLock sl (lock);
    juce::ReferenceCountedArray<Project> list;
    findProjects (*this, folder, list);
    return list;
}

juce::ValueTree ProjectManager::getActiveProjectsFolder()                      { return folders.getChildWithName (IDs::ACTIVE); }
juce::ValueTree ProjectManager::getLibraryProjectsFolder()                     { return folders.getChildWithName (IDs::LIBRARY); }

//==============================================================================
Project::Ptr ProjectManager::findProjectWithId (const juce::ValueTree& folder, ProjectID pid)
{
    if (auto p = getProjectFrom (folder))
        if (p->getProjectID() == pid)
            return p;

    for (int i = 0; i < folder.getNumChildren(); ++i)
        if (auto p = findProjectWithId (folder.getChild (i), pid))
            return p;

    return {};
}

Project::Ptr ProjectManager::findProjectWithFile (const juce::ValueTree& folder,
                                                  const juce::File& f)
{
    if (auto p = getProjectFrom (folder))
        if (p->getProjectFile() == f)
            return p;

    for (int i = 0; i < folder.getNumChildren(); ++i)
        if (auto p = findProjectWithFile (folder.getChild (i), f))
            return p;

    return {};
}

Project::Ptr ProjectManager::getProjectFrom (const juce::ValueTree& v,
                                             bool createIfNotFound)
{
    if (auto p = dynamic_cast<Project*> (v.getProperty (IDs::project).getObject()))
        return p;

    if (createIfNotFound && v.hasType (IDs::PROJECT))
    {
        const juce::File f (v[IDs::file]);

        if (f.exists())
        {
            Project::Ptr p;

            for (auto* proj : openProjects)
                if (proj->getProjectFile() == f)
                    p = proj;

            if (p == nullptr)
                p = createNewProject (f);

            if (p->isValid())
            {
                juce::ValueTree (v).setProperty (IDs::project, juce::var (p.get()), nullptr);
                return p;
            }
        }
    }

    return {};
}

Project::Ptr ProjectManager::getProject (ProjectID pid)
{
    const juce::ScopedLock sl (lock);

    for (auto p : openProjects)
        if (p->getProjectID() == pid)
            return p;

    return findProjectWithId (folders, pid);
}

Project::Ptr ProjectManager::getProject (const juce::File& f)
{
    const juce::ScopedLock sl (lock);

    for (auto p : openProjects)
        if (p->getProjectFile() == f)
            return p;

    return findProjectWithFile (folders, f);
}

//==============================================================================
Project::Ptr ProjectManager::addProjectToList (const juce::File& f,
                                               bool shouldSaveList,
                                               juce::ValueTree folderToAddTo,
                                               int index)
{
    if ((f.existsAsFile() && isTracktionProjectFile (f)) || f.isDirectory())
    {
        const juce::ScopedLock sl (lock);

        if (auto existing = findProjectWithFile (folders, f))
            return existing;

        // Reuse an already-open project for this file/folder if one exists
        auto p = getProject (f);

        if (p == nullptr)
            p = createNewProject (f);

        if (p->isValid())
        {
            if (p->getProjectID().isValid())
                if (auto existing = findProjectWithId (folders, p->getProjectID()))
                    return existing;

            auto v = createValueTree (IDs::PROJECT,
                                      IDs::file, f.getFullPathName());

            folderToAddTo.addChild (v, index, nullptr);
            ensureAllItemsHaveIDs (folderToAddTo);

            // NB: the object added may be a copy, so need to re-get this pointer
            p = getProject (f);

            if (shouldSaveList)
                saveList();

            return p;
        }
    }

    return {};
}

static void removeProject (const juce::ValueTree& folder, const Project::Ptr& toRemove)
{
    if (toRemove == nullptr)
        return;

    if (auto p = toRemove->projectManager.getProjectFrom (folder))
    {
        if (p == toRemove)
        {
            folder.getParent().removeChild (folder, nullptr);
            return;
        }
    }

    for (int i = 0; i < folder.getNumChildren(); ++i)
        removeProject (folder.getChild(i), toRemove);
}

void ProjectManager::removeProjectFromList (const juce::File& f)
{
    const juce::ScopedLock sl (lock);

    if (auto p = getProject (f))
    {
        if (! engine.getUIBehaviour().closeAllEditsBelongingToProject (*p))
            return;

        p->deselect();
        removeProject (folders, p);

        saveList();

        SelectionManager::deselectAllFromAllWindows();

        engine.getUIBehaviour().updateAllProjectItemLists();

        for (auto edit : engine.getUIBehaviour().getAllOpenEdits())
            if (edit != nullptr)
                edit->sendSourceFileUpdate();

        addFileToRecentProjectsList (f);
    }
}

void ProjectManager::clearProjects()
{
    const juce::ScopedLock sl (lock);

    folders.removeAllChildren (nullptr);
    openProjects.clear();
}

static bool getValueTreeFor (const juce::ValueTree& folder, const Project* proj,
                             juce::ValueTree& result, bool createIfNotFound = true)
{
    if (proj == nullptr)
        return false;

    if (auto p = proj->projectManager.getProjectFrom (folder, createIfNotFound))
    {
        if (p == proj)
        {
            result = folder;
            return true;
        }
    }

    for (int i = 0; i < folder.getNumChildren(); ++i)
        if (getValueTreeFor (folder.getChild(i), proj, result, createIfNotFound))
            return true;

    return false;
}

juce::ValueTree ProjectManager::findFolderContaining (const Project& p) const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent();

    return result;
}

juce::ValueTree ProjectManager::getFolderItemFor (const Project& p) const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result;

    return {};
}

int ProjectManager::getFolderIndexFor (const Project& p) const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent().indexOf (result);

    return -1;
}

void ProjectManager::updateProjectFile (Project& p, const juce::File& f)
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result, false))
        result.setProperty (IDs::file, f.getFullPathName(), nullptr);
}

ProjectItem::Ptr ProjectManager::getProjectItem (const ProjectItemRef& ref)
{
    if (auto pid = ref.getProjectItemID())
        if (auto p = getProject (pid->getProjectID()))
            return p->getProjectItemFor (ref);

    // For path-based refs, try the owner project
    if (auto p = ref.getProject())
        return p->getProjectItemFor (ref);

    // Path-based ref without an owner — search all projects
    if (! ref.isProjectItemID())
    {
        const juce::ScopedLock sl (lock);

        for (auto p : openProjects)
            if (auto item = p->getProjectItemFor (ref))
                return item;
    }

    return {};
}

ProjectItem::Ptr ProjectManager::getProjectItem (juce::File file)
{
    for (auto p : openProjects)
        if (auto pi = p->getProjectItemForFile (file))
            return pi;

    for (auto p : getAllProjects())
        if (auto pi = p->getProjectItemForFile (file))
            return pi;

    return {};
}

ProjectItem::Ptr ProjectManager::getProjectItem (ProjectItemID id)
{
    return getProjectItem (ProjectItemRef (id));
}

ProjectItem::Ptr ProjectManager::getProjectItem (const Edit& ed)
{
    return getProjectItem (ed.getProjectItemRef());
}

Project::Ptr ProjectManager::getProject (const Edit& ed)
{
    auto ref = ed.getProjectItemRef();

    if (auto pid = ref.getProjectItemID())
        if (auto p = getProject (pid->getProjectID()))
            return p;

    if (auto p = ref.getProject())
        return p;

    return {};
}

juce::File ProjectManager::findSourceFile (const ProjectItemRef& ref)
{
    if (auto i = getProjectItem (ref))
        return i->getSourceFile();

    return {};
}

juce::File ProjectManager::findSourceFile (ProjectItemID id)
{
    return findSourceFile (ProjectItemRef (id));
}

void ProjectManager::saveAllProjects()
{
    const juce::ScopedLock sl (lock);

    for (auto p : getAllProjects (folders))
        p->save();
}

Project::Ptr ProjectManager::createNewProject (const juce::File& projectFile)
{
    return new Project (engine, *this, projectFile);
}

Project::Ptr ProjectManager::createNewProject (const juce::File& projectFile,
                                                juce::ValueTree folderToAddTo,
                                                ProjectType projectType)
{
    // The backend is determined by whether projectFile is a directory, so for a
    // folder-based project the caller must create the directory before calling this
    jassert (projectType == ProjectType::folderBased ? projectFile.isDirectory()
                                                     : ! projectFile.isDirectory());
    juce::ignoreUnused (projectType);

    const juce::ScopedLock sl (lock);

    auto newProj = createNewProject (projectFile);
    newProj->createNewProjectId();
    newProj->setName (projectFile.getFileName().upToLastOccurrenceOf (".", false, false));
    newProj->setDescription (TRANS("Created") + ": " + juce::Time::getCurrentTime().toString (true, false));

    if (newProj->save())
    {
        newProj = nullptr;
        newProj = addProjectToList (projectFile, true, folderToAddTo);

        if (newProj != nullptr)
        {
            if (newProj->getNumProjectItems() == 0)
            {
                if (auto newEditProjectItem = newProj->createNewEdit())
                {
                    newEditProjectItem->setDescription ("(" + TRANS("Created as the default edit for this project") + ")");
                    newProj->save();
                }
            }

            newProj->createDefaultFolders();
            newProj->refreshFolderStructure();

            engine.getUIBehaviour().selectProjectInFocusedWindow (newProj);
        }
    }

    engine.getUIBehaviour().updateAllProjectItemLists();

    saveList();
    return newProj;
}

Project::Ptr ProjectManager::createNewProjectFromTemplate (const juce::String& name, const juce::File& lastPath,
                                                           const juce::File& archiveFile, juce::ValueTree folder,
                                                           ProjectType projectType)
{
    auto extractPath = lastPath.getNonexistentChildFile (juce::File::createLegalFileName (name), {});

    if (! extractPath.createDirectory())
        return {};

    Project::Ptr proj;
    bool aborted = false;
    juce::Array<juce::File> filesCreated;
    const bool isLegacy = isLegacyArchive (engine, archiveFile);

    if (isLegacy)
    {
        legacy::TracktionArchiveFile archive (engine, archiveFile);

        if (! archive.extractAllAsTask (extractPath, false, filesCreated, aborted))
            TRACKTION_LOG_ERROR("Unable to extract all files from archive: " + archiveFile.getFullPathName());
    }
    else
    {
        juce::ZipFile zip (archiveFile);
        auto result = zip.uncompressTo (extractPath, true);

        if (result.failed())
        {
            TRACKTION_LOG_ERROR("Unable to extract all files from archive: " + archiveFile.getFullPathName()
                                + " - " + result.getErrorMessage());
        }
        else
        {
            extractPath.findChildFiles (filesCreated, juce::File::findFiles, true);
        }
    }

    if (! aborted)
    {
        // New-style archives produce a folder-based project: extractPath itself is the project.
        // Legacy archives must always go through the file-based flow below so that ID remapping
        // and the optional conversion to a folder-based project happen.
        if (! isLegacy && isTracktionProjectFolder (extractPath))
        {
            const juce::ScopedLock sl (lock);
            proj = addProjectToList (extractPath, true, folder);
        }
        else
        {
            for (auto& f : filesCreated)
            {
                if (isTracktionProjectFile (f))
                {
                    const juce::ScopedLock sl (lock);

                    // Always open as file-based first for ID remapping
                    auto p = createNewProject (f);

                    if (projectType == ProjectType::folderBased)
                    {
                        proj = convertToFolderBasedProject (*p);
                        p = nullptr;

                        if (proj != nullptr)
                            addProjectToList (proj->getDefaultDirectory(), true, folder);

                        f.deleteFile();
                    }
                    else
                    {
                        auto oldID = p->getProjectID();
                        auto newID = FileBasedProject::generateNewProjectId (p->projectManager);

                        {
                            // If this is an old style project, we need to make it available to the ProjectManager
                            // as some internal plugins use that to resolve file paths
                            p->redirectIDsFromProject (oldID, newID);
                            p->setNewProjectId (newID);

                            p->setName (name);
                            p->save();
                        }

                        auto newFileName = p->getProjectFile();
                        p = nullptr;
                        proj = addProjectToList (newFileName, true, folder);
                    }

                    break;
                }
            }
        }

        if (proj != nullptr)
        {
            engine.getUIBehaviour().selectProjectInFocusedWindow (proj);
            int editNum = 1;

            for (int i = 0; i < proj->getNumProjectItems(); ++i)
            {
                auto mo = proj->getProjectItemAt (i);

                if (mo->isEdit())
                    mo->setName (name + " " + TRANS("Edit") + " " + juce::String (editNum++),
                                 ProjectItem::SetNameMode::forceRenameSynchronous);
            }

            proj->createDefaultFolders();
            proj->refreshFolderStructure();
            saveList();

           #if JUCE_DEBUG
            for (int i = 0; i < proj->getNumProjectItems(); ++i)
                jassert (proj->getProjectItemAt (i));
           #endif
        }
    }

    if (! proj)
        TRACKTION_LOG_ERROR("Unable to create new project: " + archiveFile.getFullPathName());

    return proj;
}

void ProjectManager::createNewProjectInteractively (const juce::String& name,
                                                     const juce::File& lastPath,
                                                     juce::ValueTree folderToAddTo,
                                                     ProjectType projectType,
                                                     std::function<void (Project::Ptr)> callback)
{
    if (name.isEmpty())
    {
        if (callback)
            callback ({});

        return;
    }

    auto& ui = engine.getUIBehaviour();
    auto fileName = juce::File::createLegalFileName (name);

    if (projectType == ProjectType::folderBased)
    {
        auto projectFolder = lastPath.getChildFile (fileName);

        if (projectFolder.isDirectory())
        {
            ui.showOkCancelAlertBoxAsync (TRANS("Create project"),
                                          TRANS("This folder already exists - do you want to open it?"),
                                          TRANS("Open"),
                                          TRANS("Cancel"),
                                          [this, projectFolder, folderToAddTo, projectType, callback] (bool okPressed)
                                          {
                                              if (callback)
                                                  callback (okPressed ? createNewProject (projectFolder, folderToAddTo, projectType) : Project::Ptr {});
                                          });
            return;
        }

        if (! projectFolder.createDirectory().wasOk())
        {
            ui.showWarningAlert (TRANS("Create project"),
                                 TRANS("Couldn't create the project folder")
                                   + ":\n\n" + projectFolder.getFullPathName());

            if (callback)
                callback ({});

            return;
        }

        if (callback)
            callback (createNewProject (projectFolder, folderToAddTo, projectType));

        return;
    }

    auto projectFile = lastPath.getChildFile (fileName)
                               .getChildFile (fileName + projectFileSuffix);

    if (projectFile.exists())
    {
        ui.showOkCancelAlertBoxAsync (TRANS("Create project"),
                                      TRANS("This file already exists - do you want to open it?"),
                                      TRANS("Open"),
                                      TRANS("Cancel"),
                                      [this, projectFile, folderToAddTo, projectType, callback] (bool okPressed)
                                      {
                                          if (callback)
                                              callback (okPressed ? createNewProject (projectFile, folderToAddTo, projectType) : Project::Ptr {});
                                      });
        return;
    }

    auto parentDir = projectFile.getParentDirectory();

    if (! parentDir.exists())
        parentDir.createDirectory();

    if (parentDir.getNumberOfChildFiles (juce::File::findFiles)
         + parentDir.getNumberOfChildFiles (juce::File::findDirectories) > 0)
    {
        ui.showYesNoCancelAlertBoxAsync (
            TRANS("Create project"),
            TRANS("The directory in which you're trying to create this project is not empty.")
                   + "\n\n"
                   + TRANS("It's sensible to keep each project in its own directory, so "
                           "would you like to create a new subdirectory for it called \"XZZX\"?")
                        .replace ("XZZX", projectFile.getFileNameWithoutExtension()),
            TRANS("Create a new subdirectory"),
            TRANS("Use this directory anyway"),
            TRANS("Cancel"),
            [this, projectFile, parentDir, folderToAddTo, projectType, callback] (int r) mutable
            {
                if (r == 0)
                {
                    if (callback)
                        callback ({});

                    return;
                }

                if (r == 1)
                {
                    auto newDir = parentDir.getChildFile (projectFile.getFileNameWithoutExtension());

                    if (newDir.exists()
                         && newDir.getNumberOfChildFiles (juce::File::findDirectories)
                              + newDir.getNumberOfChildFiles (juce::File::findFiles) > 0)
                    {
                        engine.getUIBehaviour().showWarningAlert (TRANS("Create project"),
                                                                  TRANS("The directory already existed and wasn't empty, so the project couldn't be created."));
                        if (callback)
                            callback ({});

                        return;
                    }

                    if (! newDir.createDirectory())
                    {
                        engine.getUIBehaviour().showWarningAlert (TRANS("Create project"),
                                                                  TRANS("Couldn't create the new directory")
                                                                    + ":\n\n" + newDir.getFullPathName());
                        if (callback)
                            callback ({});

                        return;
                    }

                    projectFile = newDir.getChildFile (projectFile.getFileName());
                }

                if (! projectFile.create())
                {
                    engine.getUIBehaviour().showWarningAlert (TRANS("Create project"),
                                                              TRANS("Couldn't write to the file")
                                                                + ":\n\n" + projectFile.getFullPathName());
                    if (callback)
                        callback ({});

                    return;
                }

                if (callback)
                    callback (createNewProject (projectFile, folderToAddTo, projectType));
            });

        return;
    }

    if (! projectFile.create())
    {
        ui.showWarningAlert (TRANS("Create project"),
                             TRANS("Couldn't write to the file")
                               + ":\n\n" + projectFile.getFullPathName());
        if (callback)
            callback ({});

        return;
    }

    if (callback)
        callback (createNewProject (projectFile, folderToAddTo, projectType));
}

void ProjectManager::unpackArchiveAndAddToList (const juce::File& archiveFile, const juce::File& destDir, juce::ValueTree folder)
{
    if (! destDir.createDirectory())
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't create this target directory"));
        return;
    }

    auto extractDir = destDir.getNonexistentChildFile (archiveFile.getFileNameWithoutExtension(), {}, false);

    if (! extractDir.createDirectory())
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't create this target directory"));
        return;
    }

    juce::Array<juce::File> newFiles;

    if (isLegacyArchive (engine, archiveFile))
    {
        legacy::TracktionArchiveFile archive (engine, archiveFile);

        bool wasAborted = false;

        if (! archive.extractAllAsTask (extractDir, false, newFiles, wasAborted))
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
            return;
        }

        if (wasAborted)
            return;

        for (int i = newFiles.size(); --i >= 0;)
            if (! isTracktionProjectFile (newFiles.getReference (i)))
                newFiles.remove (i);

        if (newFiles.isEmpty())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("This archive unpacked ok, but it didn't contain any project files!"));
            return;
        }
    }
    else
    {
        juce::ZipFile zip (archiveFile);

        if (zip.getNumEntries() == 0 || ! zip.uncompressTo (extractDir, true).wasOk())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
            return;
        }

        // Folder-based project: the extracted directory itself is the project
        if (isTracktionProjectFolder (extractDir))
        {
            if (auto newProj = addProjectToList (extractDir, true, folder))
                engine.getUIBehaviour().selectProjectInFocusedWindow (newProj);

            saveList();
            return;
        }

        // File-based project: collect .tracktion files from the extraction
        extractDir.findChildFiles (newFiles, juce::File::findFiles, true);

        for (int i = newFiles.size(); --i >= 0;)
            if (! isTracktionProjectFile (newFiles.getReference (i)))
                newFiles.remove (i);

        if (newFiles.isEmpty())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("This archive unpacked ok, but it didn't contain any project files!"));
            return;
        }
    }

    for (int i = newFiles.size(); --i >= 0;)
        if (auto newProj = addProjectToList (newFiles.getReference (i), true, folder))
            engine.getUIBehaviour().selectProjectInFocusedWindow (newProj);

    for (auto& f : newFiles)
    {
        if (auto proj = getProject (f))
        {
            proj->createDefaultFolders();
            proj->refreshFolderStructure();
        }
    }

    saveList();
}

juce::StringArray ProjectManager::getRecentProjects (bool printableFormat)
{
    juce::StringArray files;
    files.addTokens (engine.getPropertyStorage().getProperty (SettingID::recentProjects).toString(), ";", {});
    files.trim();
    files.removeEmptyStrings();

    while (files.size() > 8)
        files.remove (0);

    for (int i = files.size(); --i >= 0;)
    {
        const juce::File f (files.getReference (i));

        if (! f.existsAsFile())
        {
            files.remove (i);
        }
        else
        {
            if (auto p = getProject (f))
                if (findFolderContaining (*p).isValid())
                    files.remove (i);
        }
    }

    if (printableFormat)
        for (auto& f : files)
            f = juce::File (f).getFileNameWithoutExtension();

    return files;
}

void ProjectManager::addFileToRecentProjectsList (const juce::File& f)
{
    auto files = getRecentProjects (false);

    for (auto& file : files)
        if (juce::File (file) == f)
            return;

    if (auto p = getProject (f))
        if (findFolderContaining (*p).isValid())
            return;

    files.add (f.getFullPathName());

    engine.getPropertyStorage().setProperty (SettingID::recentProjects, files.joinIntoString (";"));
}

void ProjectManager::createNewProjectFolder (juce::ValueTree parent, const juce::String& name)
{
    auto v = createValueTree (IDs::FOLDER,
                              IDs::name, name);

    parent.addChild (v, 0, nullptr);
    ensureAllItemsHaveIDs (parent);
    saveList();
    engine.getUIBehaviour().updateAllProjectItemLists();
}

void ProjectManager::deleteProjectFolder (juce::ValueTree folder)
{
    folder.getParent().removeChild (folder, nullptr);
}

} // namespace tracktion::inline engine
