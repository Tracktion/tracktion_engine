/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ProjectManager::ProjectManager()
    : engine (Engine::getInstance())
{
}

ProjectManager::~ProjectManager()
{
    CRASH_TRACER
    saveList();
    folders = {};
    jassert (openProjects.isEmpty());
    clearSingletonInstance();
}

JUCE_IMPLEMENT_SINGLETON (ProjectManager)

//==============================================================================
void ProjectManager::initialise()
{
    loadList();

    auto& storage = engine.getPropertyStorage();

    if (storage.getProperty (SettingID::findExamples, false))
    {
        storage.setProperty (SettingID::findExamples, var());

        File examplesDir (storage.getAppPrefsFolder().getChildFile ("examples"));

        Array<File> exampleProjects;
        examplesDir.findChildFiles (exampleProjects, File::findFiles, true,
                                    String ("*") + projectFileSuffix);

        for (auto& f : exampleProjects)
            addProjectToList (f, false, getActiveProjectsFolder(),
                              f.getFileName().containsIgnoreCase ("Spiralling") ? 0 : -1);

        saveList();
    }
}

//==============================================================================
static void ensureAllItemsHaveIDs (const ValueTree& folder)
{
    if (folder[IDs::uid].toString().isEmpty())
        ValueTree (folder).setProperty (IDs::uid, String::toHexString (Random().nextInt()), nullptr);

    for (int i = 0; i < folder.getNumChildren(); ++i)
        ensureAllItemsHaveIDs (folder.getChild(i));
}

void ProjectManager::loadList()
{
    const ScopedLock sl (lock);

    folders = {};

    auto xml = engine.getPropertyStorage().getXmlProperty (SettingID::projectList);

    if (xml != nullptr)
        folders = ValueTree::fromXml (*xml);

    if (! folders.hasType (IDs::ROOT))
        folders = ValueTree (IDs::ROOT);

    if (! getLibraryProjectsFolder().isValid())  folders.addChild (ValueTree (IDs::LIBRARY), -1, nullptr);
    if (! getActiveProjectsFolder().isValid())   folders.addChild (ValueTree (IDs::ACTIVE), 0, nullptr);

    jassert (getActiveProjectsFolder().isValid() && getLibraryProjectsFolder().isValid());

    if (xml == nullptr)   // import from T4 format:
    {
        xml = engine.getPropertyStorage().getXmlProperty (SettingID::projects);

        if (xml != nullptr)
        {
            auto oldT4 = ValueTree::fromXml (*xml);

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

static void stripProjectObjects (ValueTree v)
{
    v.removeProperty (IDs::project, nullptr);

    for (int i = 0; i < v.getNumChildren(); ++i)
        stripProjectObjects (v.getChild(i));
}

void ProjectManager::saveList()
{
    const ScopedLock sl (lock);

    auto foldersCopy = folders.createCopy();
    stripProjectObjects (foldersCopy);

    std::unique_ptr<XmlElement> xml (foldersCopy.createXml());
    engine.getPropertyStorage().setXmlProperty (SettingID::projectList, *xml);
}

static void findProjects (ProjectManager& pm, const ValueTree& folder, ReferenceCountedArray<Project>& list)
{
    if (auto p = pm.getProjectFrom (folder))
        list.add (p);

    for (int i = 0; i < folder.getNumChildren(); ++i)
        findProjects (pm, folder.getChild(i), list);
}

ReferenceCountedArray<Project> ProjectManager::getAllProjects (const ValueTree& folder)
{
    ReferenceCountedArray<Project> list;
    findProjects (*this, folder, list);
    return list;
}

ValueTree ProjectManager::getActiveProjectsFolder()     { return folders.getChildWithName (IDs::ACTIVE); }
ValueTree ProjectManager::getLibraryProjectsFolder()    { return folders.getChildWithName (IDs::LIBRARY); }

//==============================================================================
Project::Ptr ProjectManager::findProjectWithId (const ValueTree& folder, int pid)
{
    if (auto p = getProjectFrom (folder))
        if (p->getProjectID() == pid)
            return p;

    for (int i = 0; i < folder.getNumChildren(); ++i)
        if (auto p = findProjectWithId (folder.getChild (i), pid))
            return p;

    return {};
}

Project::Ptr ProjectManager::findProjectWithFile (const ValueTree& folder, const File& f)
{
    if (auto p = getProjectFrom (folder))
        if (p->getProjectFile() == f)
            return p;

    for (int i = 0; i < folder.getNumChildren(); ++i)
        if (auto p = findProjectWithFile (folder.getChild (i), f))
            return p;

    return {};
}

Project::Ptr ProjectManager::getProjectFrom (const ValueTree& v, bool createIfNotFound)
{
    if (auto p = dynamic_cast<Project*> (v.getProperty (IDs::project).getObject()))
        return p;

    if (createIfNotFound && v.hasType (IDs::PROJECT))
    {
        const File f (v[IDs::file]);

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
                ValueTree (v).setProperty (IDs::project, var (p.get()), nullptr);
                return p;
            }
        }
    }

    return {};
}

Project::Ptr ProjectManager::getProject (int pid)
{
    const ScopedLock sl (lock);

    for (auto* p : openProjects)
        if (p->getProjectID() == pid)
            return p;

    return findProjectWithId (folders, pid);
}

Project::Ptr ProjectManager::getProject (const File& f)
{
    const ScopedLock sl (lock);

    for (auto* p : openProjects)
        if (p->getProjectFile() == f)
            return p;

    return findProjectWithFile (folders, f);
}

//==============================================================================
Project::Ptr ProjectManager::addProjectToList (const File& f,
                                               bool shouldSaveList,
                                               ValueTree folderToAddTo,
                                               int index)
{
    if (f.existsAsFile() && isTracktionProjectFile (f))
    {
        const ScopedLock sl (lock);

        if (auto existing = findProjectWithFile (folders, f))
            return existing;

        auto p = createNewProject (f);

        if (p->isValid())
        {
            if (auto existing = findProjectWithId (folders, p->getProjectID()))
                return existing;

            ValueTree v (IDs::PROJECT);
            v.setProperty (IDs::file, f.getFullPathName(), nullptr);
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

static void removeProject (const ValueTree& folder, const Project::Ptr& toRemove)
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

void ProjectManager::removeProjectFromList (const File& f)
{
    const ScopedLock sl (lock);

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

static bool getValueTreeFor (const ValueTree& folder, const Project* proj, ValueTree& result, bool createIfNotFound = true)
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

ValueTree ProjectManager::findFolderContaining (const Project& p) const
{
    ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent();

    return result;
}

ValueTree ProjectManager::getFolderItemFor (const Project& p) const
{
    ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result;

    return {};
}

int ProjectManager::getFolderIndexFor (const Project& p) const
{
    ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent().indexOf (result);

    return -1;
}

void ProjectManager::updateProjectFile (Project& p, const File& f)
{
    ValueTree result;

    if (getValueTreeFor (folders, &p, result, false))
        result.setProperty (IDs::file, f.getFullPathName(), nullptr);
}

ProjectItem::Ptr ProjectManager::getProjectItem (ProjectItemID id)
{
    if (auto p = getProject (id.getProjectID()))
        return p->getProjectItemForID (id);

    return {};
}

ProjectItem::Ptr ProjectManager::getProjectItem (const Edit& ed)
{
    return getProjectItem (ed.getProjectItemID());
}

Project::Ptr ProjectManager::getProject (const Edit& ed)
{
    return getProject (ed.getProjectItemID().getProjectID());
}

File ProjectManager::findSourceFile (ProjectItemID id)
{
    if (auto i = getProjectItem (id))
        return i->getSourceFile();

    return {};
}

void ProjectManager::saveAllProjects()
{
    const ScopedLock sl (lock);

    for (auto p : getAllProjects (folders))
        p->save();
}

Project::Ptr ProjectManager::createNewProject (const juce::File& projectFile)
{
    return new Project (engine, *this, projectFile);
}

Project::Ptr ProjectManager::createNewProject (const File& projectFile, ValueTree folderToAddTo)
{
    const ScopedLock sl (lock);

    auto newProj = createNewProject (projectFile);
    newProj->createNewProjectId();
    newProj->setName (projectFile.getFileName().upToLastOccurrenceOf (".", false, false));
    newProj->setDescription (TRANS("Created") + ": " + Time::getCurrentTime().toString (true, false));

    if (newProj->save())
    {
        newProj = nullptr;
        newProj = addProjectToList (projectFile, true, folderToAddTo);

        if (newProj != nullptr)
        {
            if (newProj->getNumMediaItems() == 0)
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

Project::Ptr ProjectManager::createNewProjectFromTemplate (const String& name, const File& lastPath,
                                                           const File& archiveFile, ValueTree folder)
{
    auto extractPath = lastPath.getNonexistentChildFile (File::createLegalFileName (name), {});

    if (! extractPath.createDirectory())
        return {};

    TracktionArchiveFile archive (archiveFile);

    Project::Ptr proj;
    bool aborted = false;
    Array<File> filesCreated;
    archive.extractAllAsTask (extractPath, false, filesCreated, aborted);

    if (! aborted)
    {
        for (auto& f : filesCreated)
        {
            if (isTracktionProjectFile (f))
            {
                const ScopedLock sl (lock);

                auto p = createNewProject (f);

                auto oldID = p->getProjectID();
                p->createNewProjectId();

                {
                    auto newID = p->getProjectID();
                    p->redirectIDsFromProject (oldID, newID);
                    p->setName (name);
                    p->save();
                }

                auto newFileName = p->getProjectFile();
                p = nullptr;

                proj = addProjectToList (newFileName, true, folder);

                if (proj != nullptr)
                {
                    engine.getUIBehaviour().selectProjectInFocusedWindow (proj);
                    int editNum = 1;

                    for (int i = 0; i < proj->getNumMediaItems(); ++i)
                    {
                        auto mo = proj->getProjectItemAt (i);

                        if (mo->isEdit())
                            mo->setName (name + " " + TRANS("Edit") + " " + String (editNum++),
                                         ProjectItem::SetNameMode::forceRename);
                    }

                    proj->createDefaultFolders();
                    proj->refreshFolderStructure();
                    saveList();

                   #if JUCE_DEBUG
                    for (int i = 0; i < proj->getNumMediaItems(); ++i)
                        jassert (proj->getProjectItemAt (i));
                   #endif
                }

                break;
            }
        }
    }

    return proj;
}

Project::Ptr ProjectManager::createNewProjectInteractively (const String& name, const File& lastPath, ValueTree folderToAddTo)
{
    if (name.isNotEmpty())
    {
        auto& ui = engine.getUIBehaviour();
        auto fileName = File::createLegalFileName (name);

        auto projectFile = lastPath.getChildFile (fileName)
                                   .getChildFile (fileName + projectFileSuffix);

        if (projectFile.exists())
        {
            if (! ui.showOkCancelAlertBox (TRANS("Create project"),
                                           TRANS("This file already exists - do you want to open it?"),
                                           TRANS("Open")))
                return {};
        }
        else
        {
            auto parentDir = projectFile.getParentDirectory();

            if (! parentDir.exists())
                parentDir.createDirectory();

            if (parentDir.getNumberOfChildFiles (File::findFiles) + parentDir.getNumberOfChildFiles (File::findDirectories) > 0)
            {
                auto r = ui.showYesNoCancelAlertBox (
                                TRANS("Create project"),
                                TRANS("The directory in which you're trying to create this project is not empty.")
                                       + "\n\n"
                                       + TRANS("It's sensible to keep each project in its own directory, so "
                                               "would you like to create a new subdirectory for it called \"XZZX\"?")
                                            .replace ("XZZX", projectFile.getFileNameWithoutExtension()),
                                TRANS("Create a new subdirectory"),
                                TRANS("Use this directory anyway"),
                                TRANS("Cancel"));

                if (r == 0)
                    return {};

                if (r == 1)
                {
                    auto newDir = parentDir.getChildFile (projectFile.getFileNameWithoutExtension());

                    if (newDir.exists()
                         && newDir.getNumberOfChildFiles (File::findDirectories)
                              + newDir.getNumberOfChildFiles (File::findFiles) > 0)
                    {
                        ui.showWarningAlert (TRANS("Create project"),
                                             TRANS("The directory already existed and wasn't empty, so the project couldn't be created."));

                        return {};
                    }

                    if (! newDir.createDirectory())
                    {
                        ui.showWarningAlert (TRANS("Create project"),
                                             TRANS("Couldn't create the new directory")
                                               + ":\n\n" + newDir.getFullPathName());

                        return {};
                    }

                    projectFile = newDir.getChildFile (projectFile.getFileName());
                }
            }

            if (! projectFile.create())
            {
                ui.showWarningAlert (TRANS("Create project"),
                                     TRANS("Couldn't write to the file")
                                       + ":\n\n" + projectFile.getFullPathName());
                return {};
            }
        }

        return createNewProject (projectFile, folderToAddTo);
    }

    return {};
}

void ProjectManager::unpackArchiveAndAddToList (const File& archiveFile, ValueTree folder)
{
    TracktionArchiveFile archive (archiveFile);

    if (! archive.isValidArchive())
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
        return;
    }

    String text (TRANS("Choose a directory into which the archive \"XZZX\" should be unpacked")
                    .replace ("XZZX", archiveFile.getFileName()) + "...");

    auto lastPath = engine.getPropertyStorage().getDefaultLoadSaveDirectory ("projectfile");

    if (lastPath.existsAsFile())
        lastPath = lastPath.getParentDirectory();

    if (! lastPath.isDirectory())
        lastPath = archiveFile;
   #if JUCE_MODAL_LOOPS_PERMITTED
    FileChooser chooser (text, lastPath, "*");

    if (chooser.browseForDirectory())
   #endif
    {
       #if JUCE_MODAL_LOOPS_PERMITTED
        auto destDir = chooser.getResult();
       #else
        auto destDir = lastPath;
       #endif

        if (! destDir.createDirectory())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't create this target directory"));
            return;
        }

        destDir = destDir.getNonexistentChildFile (archiveFile.getFileNameWithoutExtension(), {}, false);

        if (! destDir.createDirectory())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't create this target directory"));
            return;
        }

        bool wasAborted;
        Array<File> newFiles;

        if (archive.extractAllAsTask (destDir, false, newFiles, wasAborted))
        {
            if (! wasAborted)
            {
                for (int i = newFiles.size(); --i >= 0;)
                    if (! isTracktionProjectFile (newFiles.getReference (i)))
                        newFiles.remove (i);

                if (newFiles.isEmpty())
                {
                    engine.getUIBehaviour().showWarningMessage (TRANS("This archive unpacked ok, but it didn't contain any project files!"));
                }
                else
                {
                    for (int i = newFiles.size(); --i >= 0;)
                        if (auto newProj = addProjectToList (newFiles.getReference (i), true, folder))
                            engine.getUIBehaviour().selectProjectInFocusedWindow (newProj);
                }

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
        }
        else
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Errors occurred whilst trying to unpack this archive"));
        }
    }
}

StringArray ProjectManager::getRecentProjects (bool printableFormat)
{
    StringArray files;
    files.addTokens (engine.getPropertyStorage().getProperty (SettingID::recentProjects).toString(), ";", {});
    files.trim();
    files.removeEmptyStrings();

    while (files.size() > 8)
        files.remove (0);

    for (int i = files.size(); --i >= 0;)
    {
        const File f (files.getReference (i));

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
            f = File (f).getFileNameWithoutExtension();

    return files;
}

void ProjectManager::addFileToRecentProjectsList (const File& f)
{
    StringArray files (getRecentProjects (false));

    for (auto& file : files)
        if (File (file) == f)
            return;

    if (auto p = getProject (f))
        if (findFolderContaining (*p).isValid())
            return;

    files.add (f.getFullPathName());

    engine.getPropertyStorage().setProperty (SettingID::recentProjects, files.joinIntoString (";"));
}

void ProjectManager::createNewProjectFolder (ValueTree parent, const String& name)
{
    ValueTree v (IDs::FOLDER);
    v.setProperty (IDs::name, name, nullptr);
    parent.addChild (v, 0, nullptr);
    ensureAllItemsHaveIDs (parent);
    saveList();
    engine.getUIBehaviour().updateAllProjectItemLists();
}

void ProjectManager::deleteProjectFolder (ValueTree folder)
{
    folder.getParent().removeChild (folder, nullptr);
}
