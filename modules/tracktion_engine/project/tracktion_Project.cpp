/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


// a combined version number and file identifier for the project file
static const char* magicNumberV1 = "TP01";

//==============================================================================
Project::Project (Engine& e, ProjectManager& pm, const File& projectFile)
   : engine (e), projectManager (pm), file (projectFile)
{
    jassert (isTracktionProjectFile (file));

    for (auto* p : projectManager.openProjects)
    {
        juce::ignoreUnused (p);
        jassert (p->getProjectFile() != file);
    }

    projectManager.openProjects.add (this);

    lockFile();
    load();
}

Project::~Project()
{
    projectManager.openProjects.removeFirstMatchingValue (this);
    save();
    notifyListenersOfDeletion();
}

void Project::lockFile()
{
    if (fileLockingStream == nullptr)
        fileLockingStream = file.createInputStream();
}

void Project::unlockFile()
{
    fileLockingStream = nullptr;
}

void Project::load()
{
    CRASH_TRACER
    readOnly = ! (file.hasWriteAccess() && ! file.isDirectory());
    projectId = 0;

    auto in = getInputStream();

    if (in != nullptr && readProjectHeader (*in))
    {
        in->setPosition (objectOffset);
        int num = in->readInt();

        jassert (num >= 0 && num < 20000); // vague sanity check

        if (num < 20000)
        {
            while (--num >= 0)
            {
                ObjectInfo o;
                o.itemID = in->readInt();
                o.fileOffset = in->readInt();

                jassert (o.itemID != 0);
                jassert (o.fileOffset > 0);

                if (o.fileOffset > 0 && o.itemID != 0)
                    objects.add (o);
            }
        }
    }
    else
    {
        stream = nullptr;
        projectId = 0;
    }

    hasChanged = false;
}

void Project::refreshProjectPropertiesFromFile()
{
    stream = nullptr;

    if (auto in = getInputStream())
        readProjectHeader (*in, false);
}

bool Project::readProjectHeader (InputStream& in, bool clearObjectInfo)
{
    CRASH_TRACER

    if (clearObjectInfo)
        objects.clear();

    char n[4] = { 0 };
    in.read (n, 4);

    if (strncmp (n, magicNumberV1, 4) == 0)
    {
        projectId = in.readInt();
        objectOffset = in.readInt();
        indexOffset = in.readInt();

        int numProps = in.readInt();
        properties.clear();

        while (--numProps >= 0)
        {
            auto propName = in.readString();
            auto size = in.readInt();

            MemoryBlock mem ((size_t) size);
            in.read (mem.getData(), size);

            properties.set (propName, mem.toString());
        }

        return objectOffset > 0 && indexOffset > 0;
    }

    return false;
}

bool Project::loadProjectItem (ObjectInfo& o)
{
    if (o.fileOffset > 0)
    {
        if (auto in = getInputStream())
        {
            in->setPosition (o.fileOffset);
            o.item = new ProjectItem (engine, ProjectItemID (o.itemID, projectId), in);
            return true;
        }
    }

    jassertfalse;
    return false;
}

void Project::loadAllProjectItems()
{
    CRASH_TRACER
    const ScopedLock sl (objectLock);

    for (auto& o : objects)
        if (o.item == nullptr)
            if (! loadProjectItem (o))
                break;
}

BufferedInputStream* Project::getInputStream()
{
    if (stream == nullptr && file.getSize() > 0)
        if (auto in = file.createInputStream())
            stream = new BufferedInputStream (in, 16384, true);

    return stream;
}

void Project::handleAsyncUpdate()
{
    save();
}

bool Project::save()
{
    CRASH_TRACER

    if (isValid() && ! isReadOnly())
    {
        if (! hasChanged)
            return true;

        const ScopedLock sl (objectLock);

        loadAllProjectItems();

        auto tempFile = file.getParentDirectory().getNonexistentChildFile ("temp", ".tmp");

        if (auto out = std::unique_ptr<FileOutputStream> (tempFile.createOutputStream()))
        {
            saveTo (*out);
            out.reset();

            stream = nullptr;
            unlockFile();

            // try this twice
            if (tempFile.moveFileTo (file) || tempFile.moveFileTo (file))
            {
                hasChanged = false;

                DBG (Time::getCurrentTime().toString (false, true)
                        + " Saved: " + file.getFullPathName());
            }
            else
            {
                jassertfalse;
                hasChanged = true;

                bool b = tempFile.deleteFile();
                jassert (b); juce::ignoreUnused (b);

                DBG (String ("!!couldn't save ") + file.getFullPathName());
            }

            lockFile();
        }

        cancelPendingUpdate();
        return ! hasChanged;
    }

    return false;
}

//==============================================================================
void Project::saveTo (FileOutputStream& out)
{
    if (! isValid())
        return;

    out.write (magicNumberV1, 4);
    out.writeInt (getProjectID());
    out.writeInt (0);
    out.writeInt (0);
    out.writeInt (properties.size());

    for (int i = 0; i < properties.size(); ++i)
    {
        out.writeString (properties.getName(i).toString());

        auto value = properties.getValueAt (i).toString();
        auto utf8 = value.toUTF8();
        auto numBytes = value.getNumBytesAsUTF8() + 1;

        out.writeInt ((int) numBytes);
        out.write (utf8, numBytes);
    }

    for (auto& o : objects)
    {
        if (auto c = o.item)
        {
            o.fileOffset = (int) out.getPosition();
            c->writeToStream (out);
        }
    }

    objectOffset = (int) out.getPosition();

    out.writeInt (objects.size());

    for (auto& o : objects)
    {
        out.writeInt (o.itemID);
        out.writeInt (o.fileOffset);
    }

    indexOffset = (int) out.getPosition();
    ProjectSearchIndex searchIndex (*this);

    for (auto& o : objects)
        if (auto c = o.item)
            searchIndex.addClip (c);

    searchIndex.writeToStream (out);

    out.setPosition (8);
    out.writeInt (objectOffset);
    out.writeInt (indexOffset);
}

//==============================================================================
bool Project::isValid() const
{
    return projectId != 0;
}

bool Project::isReadOnly() const
{
    return readOnly;
}

int Project::getProjectID() const
{
    return projectId;
}

String Project::getProjectProperty (const String& name) const
{
    const ScopedLock sl (propertyLock);
    return properties [name];
}

void Project::setProjectProperty (const String& name, const String& value)
{
    const ScopedLock sl (propertyLock);
    properties.set (name, value);
    changed();
}

String Project::getName() const
{
    return getProjectProperty ("name");
}

String Project::getDescription() const
{
    return getProjectProperty ("description");
}

void Project::setName (const String& newName)
{
    if (getName() != newName)
    {
        setProjectProperty ("name", newName.substring (0, 64));
        engine.getUIBehaviour().updateAllProjectItemLists();

        File dst = file.getParentDirectory().getChildFile(File::createLegalFileName(newName) + file.getFileExtension());
        stream = nullptr;

        unlockFile();

        if (file.moveFileTo (dst) || file.moveFileTo (dst))
            file = dst;

        projectManager.updateProjectFile (*this, file);
        lockFile();

        projectManager.saveList();
    }
}

void Project::setDescription (const String& newDesc)
{
    setProjectProperty ("description", String (newDesc).substring (0, 512));
}

void Project::createNewProjectId()
{
    int newID = Random::getSystemRandom().nextInt (9999999);

    while (projectManager.getProject (newID))
    {
        jassertfalse;
        newID = Random::getSystemRandom().nextInt (9999999);
    }

    projectId = newID;
    hasChanged = true;
}

void Project::redirectIDsFromProject (int oldProjId, int newProjId)
{
    for (int k = 0; k < getNumMediaItems(); ++k)
    {
        if (auto mo = getProjectItemAt (k))
        {
            if (mo->isEdit())
            {
                Edit ed (engine,
                         loadEditFromProjectManager (projectManager, mo->getID()),
                         Edit::forExamining, nullptr, 1);

                for (auto exportable : Exportable::addAllExportables (ed))
                {
                    int i = 0;

                    for (auto& item : exportable->getReferencedItems())
                    {
                         if (item.itemID == oldProjId)
                             exportable->reassignReferencedItem (item, item.itemID.withNewProjectID (newProjId), 0.0);

                        ++i;
                    }
                }

                EditFileOperations (ed).save (false, true, false);
            }
        }
    }

    changed();
}

bool Project::isLibraryProject() const
{
    return projectManager.findFolderContaining (*this) == projectManager.getLibraryProjectsFolder();
}

void Project::changed()
{
    hasChanged = true;
    triggerAsyncUpdate();
    Selectable::changed();
}

int Project::getNumMediaItems()
{
    return objects.size();
}

ProjectItemID Project::getProjectItemID (int i)
{
    const ScopedLock sl (objectLock);

    if (isPositiveAndBelow (i, objects.size()))
        return ProjectItemID (objects.getReference(i).itemID, projectId);

    return {};
}

Array<int> Project::getAllItemIDs() const
{
    Array<int> a;

    for (auto& o : objects)
        a.add (o.itemID);

    return a;
}

juce::Array<ProjectItemID> Project::getAllProjectItemIDs() const
{
    Array<ProjectItemID> dest;

    const ScopedLock sl (objectLock);

    for (auto& o : objects)
        dest.add (ProjectItemID (o.itemID, projectId));

    return dest;
}

ProjectItem::Ptr Project::getProjectItemAt (int i)
{
    const ScopedLock sl (objectLock);

    if (isPositiveAndBelow (i, objects.size()))
    {
        auto& o = objects.getReference(i);

        if (o.item == nullptr)
            loadProjectItem (o);

        return o.item;
    }

    return {};
}

ProjectItem::Ptr Project::getProjectItemForID (ProjectItemID targetId)
{
    const ScopedLock sl (objectLock);
    return getProjectItemAt (getIndexOf (targetId));
}

ProjectItem::Ptr Project::getProjectItemForFile (const File& fileToFind)
{
    const ScopedLock sl (objectLock);

    for (auto& o : objects)
    {
        if (o.item == nullptr)
            if (! loadProjectItem (o))
                continue;

        if (o.item->isForFile (fileToFind))
            return o.item;
    }

    return {};
}

int Project::getIndexOf (ProjectItemID mo) const
{
    const ScopedLock sl (objectLock);

    if (mo.getProjectID() == getProjectID())
    {
        auto itemID = mo.getItemID();

        for (int i = objects.size(); --i >= 0;)
            if (objects.getReference(i).itemID == itemID)
                return i;
    }

    return -1;
}

void Project::moveProjectItem (int indexToMoveFrom, int indexToMoveTo)
{
    if (indexToMoveTo != indexToMoveFrom)
    {
        const ScopedLock sl (objectLock);

        if (indexToMoveFrom >= 0 && indexToMoveFrom < objects.size())
        {
            objects.move (indexToMoveFrom, jlimit (0, objects.size(), indexToMoveTo));
            changed();
        }
    }
}

ProjectItem::Ptr Project::createNewItem (const File& fileToReference,
                                         const String& type,
                                         const String& name,
                                         const String& description,
                                         const ProjectItem::Category cat,
                                         bool atTopOfList)
{
    jassert (type.isNotEmpty());

    if (isValid() && ! isReadOnly())
    {
        if (auto mo = getProjectItemForFile (fileToReference))
            if (mo->getID().isValid() && mo->getType() == type)
                return mo;

        ObjectInfo o;
        o.item = new ProjectItem (engine, name, type, description, {}, cat, 0,
                                  ProjectItemID::createNewID (getProjectID()));
        o.itemID = o.item->getID().getItemID();
        o.fileOffset = 0;

        {
            const ScopedLock sl (objectLock);

            if (atTopOfList)
                objects.insert (0, o);
            else
                objects.add (o);
        }

        o.item->setSourceFile (fileToReference);
        o.item->verifyLength();

        changed();
        return o.item;
    }

    return {};
}

ProjectItem::Ptr Project::quickAddProjectItem (const String& relPathName,
                                               const String& type,
                                               const String& name,
                                               const String& description,
                                               const ProjectItem::Category cat,
                                               ProjectItemID newID)
{
    ObjectInfo o;
    o.item = new ProjectItem (engine, name, type, description, {}, cat, 0, newID);
    o.itemID = o.item->getID().getItemID();
    o.fileOffset = 0;
    o.item->file = relPathName;

    {
        const ScopedLock sl (objectLock);
        objects.add (o);
    }

    changed();
    return o.item;
}

bool Project::removeProjectItem (ProjectItemID item, bool deleteSourceMaterial)
{
    if (isValid() && ! isReadOnly())
    {
        {
            const ScopedLock sl (objectLock);

            const int index = getIndexOf (item);
            jassert (index >= 0);

            if (index >= 0)
            {
                auto& o = objects.getReference (index);

                if (o.item != nullptr)
                {
                    o.item->deselect();

                    if (deleteSourceMaterial)
                        if (! o.item->deleteSourceFile())
                            return false;
                }

                objects.remove (index);
            }
        }

        changed();
        return true;
    }

    return false;
}

File Project::getDirectoryForMedia (ProjectItem::Category category) const
{
    File dir = getDefaultDirectory();

    switch (category)
    {
        case ProjectItem::Category::archives:  dir = dir.getChildFile (TRANS("Archived")); break;
        case ProjectItem::Category::exports:   dir = dir.getChildFile (TRANS("Exported")); break;
        case ProjectItem::Category::frozen:    dir = dir.getChildFile (TRANS("Frozen")); break;
        case ProjectItem::Category::imported:  dir = dir.getChildFile (TRANS("Imported")); break;
        case ProjectItem::Category::recorded:  dir = dir.getChildFile (TRANS("Recorded")); break;
        case ProjectItem::Category::rendered:  dir = dir.getChildFile (TRANS("Rendered")); break;
        case ProjectItem::Category::video:     dir = dir.getChildFile (TRANS("Movies")); break;

        case ProjectItem::Category::edit:
        case ProjectItem::Category::none:
            break;
    }

    if (! dir.isDirectory())
        dir.createDirectory();

    return dir;
}

File Project::getDefaultDirectory() const
{
    return file.getParentDirectory();
}

ProjectItem::Ptr Project::createNewEdit()
{
    int maxSuffix = 0;

    for (int i = 0; i < getNumMediaItems(); ++i)
    {
        if (auto p = getProjectItemAt (i))
        {
            if (p->isEdit())
            {
                const String nm (p->getName());

                if (nm.startsWithIgnoreCase (getName() + " Edit "))
                    maxSuffix = jmax (maxSuffix, nm.getTrailingIntValue());
            }
        }
    }

    auto name = getName() + " Edit ";
    name << (maxSuffix + 1);

    auto f = getDefaultDirectory().getNonexistentChildFile (name, editFileSuffix, false);

    if (f.create())
        return createNewItem (f, ProjectItem::editItemType(), name,
                              {}, ProjectItem::Category::edit, true);

    return {};
}

void Project::searchFor (Array<ProjectItemID>& results, SearchOperation& searchOp)
{
    save();

    if (indexOffset > 0)
    {
        ProjectSearchIndex psi (*this);

        {
            const ScopedLock sl (objectLock);

            if (auto in = getInputStream())
            {
                in->setPosition (indexOffset);
                psi.readFromStream (*in);
            }
        }

        psi.findMatches (searchOp, results);
    }
}

void Project::mergeArchiveContents (const File& archiveFile)
{
    TracktionArchiveFile archive (archiveFile);

    if (! archive.isValidArchive())
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
        return;
    }

    bool wasAborted;
    Array<File> newFiles;

    if (archive.extractAllAsTask (getProjectFile().getParentDirectory(), true, newFiles, wasAborted))
    {
        if (! wasAborted)
        {
            for (const auto& f : newFiles)
            {
                if (isTracktionProjectFile (f))
                {
                    mergeOtherProjectIntoThis (f);
                    f.deleteFile();

                    jassert (! f.exists());
                }
            }

            refreshFolderStructure();
        }
    }
    else
    {
        engine.getUIBehaviour().showWarningMessage (TRANS("Errors occurred whilst trying to unpack this archive"));
    }
}

void Project::mergeOtherProjectIntoThis (const File& f)
{
    ProjectManager::TempProject temp (projectManager, f, false);

    if (auto p = temp.project)
    {
        if (p->isValid())
        {
            for (int i = 0; i < p->getNumMediaItems(); ++i)
            {
                if (auto src = p->getProjectItemAt (i))
                {
                    if (auto mo = quickAddProjectItem (src->file,
                                                       src->getType(),
                                                       src->getName(),
                                                       src->description,
                                                       src->getCategory(),
                                                       src->getID()))
                    {
                        mo->copyAllPropertiesFrom (*src);
                        mo->verifyLength();
                        mo->changeProjectId (p->getProjectID(), getProjectID());
                    }
                }
            }
        }
    }
}

Array<ProjectItemID> Project::findOrphanItems()
{
    const ScopedLock sl (objectLock);

    auto unreffed = getAllProjectItemIDs();

    for (int j = 0; j < getNumMediaItems(); ++j)
    {
        if (unreffed.isEmpty())
            break;

        if (auto mo = getProjectItemAt (j))
        {
            if (mo->isEdit())
            {
                Edit ed (engine,
                         loadEditFromProjectManager (projectManager, mo->getID()),
                         Edit::forExamining, nullptr, 1);

                for (int i = unreffed.size(); --i >= 0;)
                    if (referencesProjectItem (ed, unreffed.getReference(i)))
                        unreffed.remove (i);
            }
        }
    }

    return unreffed;
}

String Project::getSelectableDescription()
{
    return isReadOnly() ? TRANS("Read-Only Project")
                        : TRANS("Project");
}

bool Project::askAboutTempoDetect (const File& f, bool& shouldSetAutoTempo) const
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    NagMode im = (NagMode) static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::autoTempoDetect, (int) nagAsk));

    shouldSetAutoTempo = engine.getPropertyStorage().getProperty (SettingID::autoTempoMatch, false);

    if (im == nagAutoYes)
        return true;

    if (im == nagAutoNo)
        return false;

    ToggleButton autoTempo (TRANS("Set tempo to match project"));
    autoTempo.setToggleState (shouldSetAutoTempo, dontSendNotification);
    autoTempo.setSize (200, 20);

    ToggleButton dontAsk (TRANS("Remember my choice"));
    dontAsk.setSize (200, 20);

    const std::unique_ptr<AlertWindow> w (LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Detect Tempo?"),
                                                                                                  TRANS("No tempo information was found in XZZX, would you like to detect it automatically?")
                                                                                                    .replace ("XZZX", f.getFileNameWithoutExtension()),
                                                                                                  {}, {}, {}, AlertWindow::QuestionIcon, 0, nullptr));

    w->addCustomComponent (&autoTempo);
    w->addCustomComponent (&dontAsk);
    w->addButton (TRANS("Yes"), 1, KeyPress (KeyPress::returnKey));
    w->addButton (TRANS("No"), 0, KeyPress (KeyPress::escapeKey));

    const int res = w->runModalLoop();

    shouldSetAutoTempo = autoTempo.getToggleState();
    engine.getPropertyStorage().setProperty (SettingID::autoTempoMatch, shouldSetAutoTempo);

    if (dontAsk.getToggleState())
        engine.getPropertyStorage().setProperty (SettingID::autoTempoDetect, (int) (res == 1 ? nagAutoYes : nagAutoNo));

    return res == 1;
   #else
    juce::ignoreUnused (f, shouldSetAutoTempo);

    return true;
   #endif
}

void Project::ensureFolderCreated (ProjectItem::Category c)
{
    File dir (getDirectoryForMedia (c));

    if (! dir.isDirectory())
        dir.createDirectory();
}

void Project::createDefaultFolders()
{
    ensureFolderCreated (ProjectItem::Category::archives);
    ensureFolderCreated (ProjectItem::Category::exports);
    ensureFolderCreated (ProjectItem::Category::imported);
    ensureFolderCreated (ProjectItem::Category::recorded);
    ensureFolderCreated (ProjectItem::Category::rendered);
    ensureFolderCreated (ProjectItem::Category::edit);
}

void Project::refreshFolderStructure()
{
    auto items = getAllProjectItemIDs();
    auto projDir = getProjectFile().getParentDirectory();

    for (int i = 0; i < items.size(); ++i)
    {
        if (auto mo = getProjectItemForID (items.getReference(i)))
        {
            auto srcFile = mo->getSourceFile();
            auto dstDir = getDirectoryForMedia (mo->getCategory());

            if (! dstDir.isDirectory())
                dstDir.createDirectory();

            if (srcFile.isAChildOf (projDir))
            {
                File dstFile = dstDir.getChildFile (srcFile.getFileName());

                if (srcFile.moveFileTo (dstFile))
                    mo->setSourceFile (dstFile);
            }
        }
        else
        {
            jassertfalse;
        }
    }
}
