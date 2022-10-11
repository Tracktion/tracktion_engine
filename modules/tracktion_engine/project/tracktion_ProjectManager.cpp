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
    juce::ReferenceCountedArray<Project> list;
    findProjects (*this, folders, list);
    return list;
}

juce::ReferenceCountedArray<Project> ProjectManager::getAllProjects (const juce::ValueTree& folder)
{
    juce::ReferenceCountedArray<Project> list;
    findProjects (*this, folder, list);
    return list;
}

juce::ValueTree ProjectManager::getActiveProjectsFolder()     { return folders.getChildWithName (IDs::ACTIVE); }
juce::ValueTree ProjectManager::getLibraryProjectsFolder()    { return folders.getChildWithName (IDs::LIBRARY); }

//==============================================================================
Project::Ptr ProjectManager::findProjectWithId (const juce::ValueTree& folder, int pid)
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

Project::Ptr ProjectManager::getProject (int pid)
{
    const juce::ScopedLock sl (lock);

    for (auto* p : openProjects)
        if (p->getProjectID() == pid)
            return p;

    return findProjectWithId (folders, pid);
}

Project::Ptr ProjectManager::getProject (const juce::File& f)
{
    const juce::ScopedLock sl (lock);

    for (auto* p : openProjects)
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
    if (f.existsAsFile() && isTracktionProjectFile (f))
    {
        const juce::ScopedLock sl (lock);

        if (auto existing = findProjectWithFile (folders, f))
            return existing;

        auto p = createNewProject (f);

        if (p->isValid())
        {
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
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent();

    return result;
}

juce::ValueTree ProjectManager::getFolderItemFor (const Project& p) const
{
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result;

    return {};
}

int ProjectManager::getFolderIndexFor (const Project& p) const
{
    juce::ValueTree result;

    if (getValueTreeFor (folders, &p, result))
        return result.getParent().indexOf (result);

    return -1;
}

void ProjectManager::updateProjectFile (Project& p, const juce::File& f)
{
    juce::ValueTree result;

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

juce::File ProjectManager::findSourceFile (ProjectItemID id)
{
    if (auto i = getProjectItem (id))
        return i->getSourceFile();

    return {};
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

Project::Ptr ProjectManager::createNewProject (const juce::File& projectFile, juce::ValueTree folderToAddTo)
{
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
                                                           const juce::File& archiveFile, juce::ValueTree folder)
{
    auto extractPath = lastPath.getNonexistentChildFile (juce::File::createLegalFileName (name), {});

    if (! extractPath.createDirectory())
        return {};

    TracktionArchiveFile archive (engine, archiveFile);

    Project::Ptr proj;
    bool aborted = false;
    juce::Array<juce::File> filesCreated;
    archive.extractAllAsTask (extractPath, false, filesCreated, aborted);

    if (! aborted)
    {
        for (auto& f : filesCreated)
        {
            if (isTracktionProjectFile (f))
            {
                const juce::ScopedLock sl (lock);

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

                    for (int i = 0; i < proj->getNumProjectItems(); ++i)
                    {
                        auto mo = proj->getProjectItemAt (i);

                        if (mo->isEdit())
                            mo->setName (name + " " + TRANS("Edit") + " " + juce::String (editNum++),
                                         ProjectItem::SetNameMode::forceRename);
                    }

                    proj->createDefaultFolders();
                    proj->refreshFolderStructure();
                    saveList();

                   #if JUCE_DEBUG
                    for (int i = 0; i < proj->getNumProjectItems(); ++i)
                        jassert (proj->getProjectItemAt (i));
                   #endif
                }

                break;
            }
        }
    }

    return proj;
}

Project::Ptr ProjectManager::createNewProjectInteractively (const juce::String& name,
                                                            const juce::File& lastPath,
                                                            juce::ValueTree folderToAddTo)
{
    if (name.isNotEmpty())
    {
        auto& ui = engine.getUIBehaviour();
        auto fileName = juce::File::createLegalFileName (name);

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

            if (parentDir.getNumberOfChildFiles (juce::File::findFiles)
                 + parentDir.getNumberOfChildFiles (juce::File::findDirectories) > 0)
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
                         && newDir.getNumberOfChildFiles (juce::File::findDirectories)
                              + newDir.getNumberOfChildFiles (juce::File::findFiles) > 0)
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

void ProjectManager::unpackArchiveAndAddToList (const juce::File& archiveFile, juce::ValueTree folder)
{
    TracktionArchiveFile archive (engine, archiveFile);

    if (! archive.isValidArchive())
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
        return;
    }

    auto text = TRANS("Choose a directory into which the archive \"XZZX\" should be unpacked")
                  .replace ("XZZX", archiveFile.getFileName()) + "...";

    auto lastPath = engine.getPropertyStorage().getDefaultLoadSaveDirectory ("projectfile");

    if (lastPath.existsAsFile())
        lastPath = lastPath.getParentDirectory();

    if (! lastPath.isDirectory())
        lastPath = archiveFile;
   #if JUCE_MODAL_LOOPS_PERMITTED
    juce::FileChooser chooser (text, lastPath, "*");

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
        juce::Array<juce::File> newFiles;

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

}} // namespace tracktion { inline namespace engine
