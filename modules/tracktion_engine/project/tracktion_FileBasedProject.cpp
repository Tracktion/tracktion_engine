/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

// a combined version number and file identifier for the project file
static const char* fileBasedProjectMagicNumberV1 = "TP01";

//==============================================================================
FileBasedProject::FileBasedProject (Project& o, const juce::File& projectFile)
   : ProjectBase (o), file (projectFile)
{
    jassert (isTracktionProjectFile (file));
}

void FileBasedProject::lockFile()
{
    if (fileLockingStream == nullptr)
        fileLockingStream = file.createInputStream();
}

void FileBasedProject::unlockFile()
{
    fileLockingStream.reset();
    stream.reset();
}

void FileBasedProject::load()
{
    CRASH_TRACER
    readOnly = ! (file.hasWriteAccess() && ! file.isDirectory());
    projectId = {};

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
        stream.reset();
        projectId = {};
    }

    hasChanged = false;
}

void FileBasedProject::refreshProjectPropertiesFromFile()
{
    const juce::ScopedLock sl (objectLock);
    stream.reset();

    if (auto in = getInputStream())
        readProjectHeader (*in, false);
}

bool FileBasedProject::readProjectHeader (juce::InputStream& in, bool clearObjectInfo)
{
    CRASH_TRACER

    if (clearObjectInfo)
        objects.clear();

    char n[4] = { 0 };
    in.read (n, 4);

    if (strncmp (n, fileBasedProjectMagicNumberV1, 4) == 0)
    {
        projectId = ProjectID (in.readInt());
        objectOffset = in.readInt();
        indexOffset = in.readInt();

        int numProps = in.readInt();
        properties.clear();

        if (numProps < 0 || numProps > 10000)
            return false;

        while (--numProps >= 0)
        {
            auto propName = in.readString();
            auto size = in.readInt();

            if (size <= 0 || size > 1024 * 1024)
                return false;

            juce::MemoryBlock mem ((size_t) size);
            in.read (mem.getData(), size);

            properties.set (propName, mem.toString());
        }

        return objectOffset > 0 && indexOffset > 0;
    }

    return false;
}

bool FileBasedProject::loadProjectItem (ObjectInfo& o)
{
    if (o.fileOffset > 0)
    {
        if (auto in = getInputStream())
        {
            in->setPosition (o.fileOffset);
            o.item = new ProjectItem (owner.engine, ProjectItemID (o.itemID, projectId), in);
            o.item->ownerProject = &owner;
            return true;
        }
    }

    jassertfalse;
    return false;
}

void FileBasedProject::loadAllProjectItems()
{
    CRASH_TRACER
    const juce::ScopedLock sl (objectLock);

    for (auto& o : objects)
        if (o.item == nullptr)
            if (! loadProjectItem (o))
                break;
}

juce::BufferedInputStream* FileBasedProject::getInputStream()
{
    if (stream == nullptr && file.getSize() > 0)
        if (auto in = file.createInputStream())
            stream = std::make_unique<juce::BufferedInputStream> (in.release(), 16384, true);

    return stream.get();
}

bool FileBasedProject::save()
{
    CRASH_TRACER

    if (isValid() && ! isReadOnly())
    {
        if (! hasChanged)
            return true;

        const juce::ScopedLock sl (objectLock);

        loadAllProjectItems();

        auto tempFile = file.getParentDirectory().getNonexistentChildFile ("temp", ".tmp");

        if (auto out = tempFile.createOutputStream())
        {
            saveTo (*out);
            out.reset();

            stream.reset();
            unlockFile();

            // try this twice
            if (tempFile.moveFileTo (file) || tempFile.moveFileTo (file))
            {
                hasChanged = false;

                DBG (juce::Time::getCurrentTime().toString (false, true)
                        + " Saved: " + file.getFullPathName());
            }
            else
            {
                jassertfalse;
                hasChanged = true;

                bool b = tempFile.deleteFile();
                jassert (b); juce::ignoreUnused (b);

                DBG ("!!couldn't save " + file.getFullPathName());
            }

            lockFile();
        }

        return ! hasChanged;
    }

    return false;
}

//==============================================================================
void FileBasedProject::saveTo (juce::FileOutputStream& out)
{
    if (! isValid())
        return;

    out.write (fileBasedProjectMagicNumberV1, 4);
    out.writeInt (getProjectID().toInt());
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
    out.writeInt (0); // empty search index for backward compat

    out.setPosition (8);
    out.writeInt (objectOffset);
    out.writeInt (indexOffset);
}

//==============================================================================
bool FileBasedProject::isValid() const
{
    return projectId.isValid();
}

bool FileBasedProject::isReadOnly() const
{
    return readOnly;
}

ProjectID FileBasedProject::getProjectID() const
{
    return projectId;
}

juce::String FileBasedProject::getProjectProperty (const juce::String& name) const
{
    const juce::ScopedLock sl (propertyLock);
    return properties [name];
}

void FileBasedProject::setProjectProperty (const juce::String& name, const juce::String& value)
{
    const juce::ScopedLock sl (propertyLock);
    properties.set (name, value);
    changed();
}

juce::String FileBasedProject::getName() const
{
    return getProjectProperty ("name");
}

void FileBasedProject::setName (const juce::String& newName)
{
    if (getName() != newName)
    {
        setProjectProperty ("name", newName.substring (0, 64));
        owner.engine.getUIBehaviour().updateAllProjectItemLists();

        auto dst = file.getParentDirectory().getChildFile (juce::File::createLegalFileName (newName)
                                                             + file.getFileExtension());
        stream.reset();

        unlockFile();

        if (file.moveFileTo (dst) || file.moveFileTo (dst))
            file = dst;

        owner.projectManager.updateProjectFile (owner, file);
        lockFile();

        owner.projectManager.saveList();
    }
}

void FileBasedProject::createNewProjectId()
{
    auto newID = ProjectID (juce::Random::getSystemRandom().nextInt (9999999));

    while (owner.projectManager.getProject (newID))
    {
        jassertfalse;
        newID = ProjectID (juce::Random::getSystemRandom().nextInt (9999999));
    }

    projectId = newID;
    hasChanged = true;
}

void FileBasedProject::redirectIDsFromProject (ProjectID oldProjId, ProjectID newProjId)
{
    for (int k = 0; k < getNumProjectItems(); ++k)
    {
        if (auto mo = getProjectItemAt (k))
        {
            if (mo->isEdit())
            {
                auto ed = loadEditForExamining (owner.projectManager, mo->getProjectItemRef());

                for (auto exportable : Exportable::addAllExportables (*ed))
                {
                    for (auto& item : exportable->getReferencedItems())
                    {
                        if (auto pid = item.itemRef.getProjectItemID(); pid && pid->getProjectID() == oldProjId)
                            exportable->reassignReferencedItem (item, pid->withNewProjectID (newProjId), 0.0);
                    }
                }

                EditFileOperations (*ed).save (false, true, false);
            }
        }
    }

    changed();
}

bool FileBasedProject::isLibraryProject() const
{
    return owner.projectManager.findFolderContaining (owner) == owner.projectManager.getLibraryProjectsFolder();
}

void FileBasedProject::changed()
{
    hasChanged = true;
    owner.projectChanged();
}

int FileBasedProject::getNumProjectItems()
{
    return objects.size();
}

ProjectItemRef FileBasedProject::getProjectItemRef (int i)
{
    const juce::ScopedLock sl (objectLock);

    if (juce::isPositiveAndBelow (i, objects.size()))
        return ProjectItemRef (ProjectItemID (objects.getReference(i).itemID, projectId));

    return {};
}

juce::Array<ProjectItemRef> FileBasedProject::getAllProjectItemRefs() const
{
    juce::Array<ProjectItemRef> dest;

    const juce::ScopedLock sl (objectLock);

    for (auto& o : objects)
        dest.add (ProjectItemRef (ProjectItemID (o.itemID, projectId)));

    return dest;
}

ProjectItem::Ptr FileBasedProject::getProjectItemAt (int i)
{
    const juce::ScopedLock sl (objectLock);

    if (juce::isPositiveAndBelow (i, objects.size()))
    {
        auto& o = objects.getReference(i);

        if (o.item == nullptr)
            loadProjectItem (o);

        return o.item;
    }

    return {};
}

juce::Array<ProjectItem::Ptr> FileBasedProject::getAllProjectItems()
{
    juce::Array<ProjectItem::Ptr> dest;

    const juce::ScopedLock sl (objectLock);

    for (auto& o : objects)
    {
        if (o.item == nullptr)
            loadProjectItem (o);

        dest.add (o.item);
    }

    return dest;
}

ProjectItem::Ptr FileBasedProject::getProjectItemFor (const ProjectItemRef& ref)
{
    if (ref.isProjectItemID())
    {
        const juce::ScopedLock sl (objectLock);
        return getProjectItemAt (getIndexOf (ref));
    }

    auto resolved = ref.resolve (owner.engine, getDefaultDirectory());
    return resolved != juce::File() ? getProjectItemForFile (resolved) : nullptr;
}

ProjectItem::Ptr FileBasedProject::getProjectItemForFile (const juce::File& fileToFind)
{
    const juce::ScopedLock sl (objectLock);

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

int FileBasedProject::getIndexOf (const ProjectItemRef& ref) const
{
    auto mo = ref.getProjectItemID();

    const juce::ScopedLock sl (objectLock);

    if (mo && mo->getProjectID() == getProjectID())
    {
        auto itemID = mo->getItemID();

        for (int i = objects.size(); --i >= 0;)
            if (objects.getReference(i).itemID == itemID)
                return i;
    }

    return -1;
}

void FileBasedProject::moveProjectItem (int indexToMoveFrom, int indexToMoveTo)
{
    if (indexToMoveTo != indexToMoveFrom)
    {
        const juce::ScopedLock sl (objectLock);

        if (indexToMoveFrom >= 0 && indexToMoveFrom < objects.size())
        {
            objects.move (indexToMoveFrom, juce::jlimit (0, objects.size(), indexToMoveTo));
            changed();
        }
    }
}

ProjectItem::Ptr FileBasedProject::createNewItem (const juce::File& fileToReference,
                                                  const juce::String& type,
                                                  const juce::String& name,
                                                  const juce::String& description,
                                                  const ProjectItem::Category cat,
                                                  bool atTopOfList)
{
    jassert (type.isNotEmpty());

    if (isValid() && ! isReadOnly())
    {
        if (auto mo = getProjectItemForFile (fileToReference))
            if (mo->getProjectItemRef().getProjectItemID().has_value() && mo->getType() == type)
                return mo;

        ObjectInfo o;
        o.item = new ProjectItem (owner.engine, name, type, description, {}, cat, 0,
                                  ProjectItemID::createNewID (getProjectID()));
        o.itemID = o.item->getProjectItemRef().getProjectItemID()->getItemID();
        o.fileOffset = 0;

        {
            const juce::ScopedLock sl (objectLock);

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

ProjectItem::Ptr FileBasedProject::quickAddProjectItem (const juce::String& relPathName,
                                                        const juce::String& type,
                                                        const juce::String& name,
                                                        const juce::String& description,
                                                        const ProjectItem::Category cat,
                                                        ProjectItemID newID)
{
    ObjectInfo o;
    o.item = new ProjectItem (owner.engine, name, type, description, {}, cat, 0, newID);
    o.itemID = o.item->getProjectItemRef().getProjectItemID()->getItemID();
    o.fileOffset = 0;
    o.item->file = relPathName;

    {
        const juce::ScopedLock sl (objectLock);
        objects.add (o);
    }

    changed();
    return o.item;
}

bool FileBasedProject::removeProjectItem (const ProjectItemRef& ref, bool deleteSourceMaterial)
{
    if (isValid() && ! isReadOnly())
    {
        {
            const juce::ScopedLock sl (objectLock);

            const int index = getIndexOf (ref);
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

juce::File FileBasedProject::getDirectoryForMedia (ProjectItem::Category category) const
{
    auto dir = getDefaultDirectory();

    switch (category)
    {
        case ProjectItem::Category::archives:  dir = dir.getChildFile ("Archived"); break;
        case ProjectItem::Category::exports:   dir = dir.getChildFile ("Exported"); break;
        case ProjectItem::Category::frozen:    dir = dir.getChildFile ("Frozen"); break;
        case ProjectItem::Category::imported:  dir = dir.getChildFile ("Imported"); break;
        case ProjectItem::Category::recorded:  dir = dir.getChildFile ("Recorded"); break;
        case ProjectItem::Category::rendered:  dir = dir.getChildFile ("Rendered"); break;
        case ProjectItem::Category::video:     dir = dir.getChildFile ("Movies"); break;

        case ProjectItem::Category::edit:
        case ProjectItem::Category::none:
            break;
    }

    if (! dir.isDirectory())
        dir.createDirectory();

    return dir;
}

juce::File FileBasedProject::getDefaultDirectory() const
{
    return file.getParentDirectory();
}

ProjectItem::Ptr FileBasedProject::createNewEdit()
{
    int maxSuffix = 0;

    for (int i = 0; i < getNumProjectItems(); ++i)
    {
        if (auto p = getProjectItemAt (i))
        {
            if (p->isEdit())
            {
                auto nm = p->getName();

                if (nm.startsWithIgnoreCase (getName() + " Edit "))
                    maxSuffix = std::max (maxSuffix, nm.getTrailingIntValue());
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

void FileBasedProject::mergeArchiveContents (const juce::File& archiveFile)
{
    legacy::TracktionArchiveFile archive (owner.engine, archiveFile);

    if (! archive.isValidArchive())
    {
        owner.engine.getUIBehaviour().showWarningMessage (TRANS("This file wasn't a valid tracktion archive file"));
        return;
    }

    bool wasAborted;
    juce::Array<juce::File> newFiles;

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
        owner.engine.getUIBehaviour().showWarningMessage (TRANS("Errors occurred whilst trying to unpack this archive"));
    }
}

void FileBasedProject::mergeOtherProjectIntoThis (const juce::File& f)
{
    ProjectManager::TempProject temp (owner.projectManager, f, false);

    if (auto p = temp.project)
    {
        if (p->isValid())
        {
            for (int i = 0; i < p->getNumProjectItems(); ++i)
            {
                if (auto src = p->getProjectItemAt (i))
                {
                    if (auto mo = quickAddProjectItem (src->file,
                                                       src->getType(),
                                                       src->getName(),
                                                       src->description,
                                                       src->getCategory(),
                                                       *src->getProjectItemRef().getProjectItemID()))
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

juce::String FileBasedProject::getSelectableDescription() const
{
    return isReadOnly() ? TRANS("Read-Only Project")
                        : TRANS("Project");
}

void FileBasedProject::ensureFolderCreated (ProjectItem::Category c)
{
    auto dir = getDirectoryForMedia (c);

    if (! dir.isDirectory())
        dir.createDirectory();
}

void FileBasedProject::createDefaultFolders()
{
    ensureFolderCreated (ProjectItem::Category::archives);
    ensureFolderCreated (ProjectItem::Category::exports);
    ensureFolderCreated (ProjectItem::Category::imported);
    ensureFolderCreated (ProjectItem::Category::recorded);
    ensureFolderCreated (ProjectItem::Category::rendered);
    ensureFolderCreated (ProjectItem::Category::edit);
}

void FileBasedProject::refreshFolderStructure()
{
    auto projDir = getProjectFile().getParentDirectory();

    for (auto& ref : getAllProjectItemRefs())
    {
        if (auto mo = getProjectItemFor (ref))
        {
            auto srcFile = mo->getSourceFile();
            auto dstDir = getDirectoryForMedia (mo->getCategory());

            if (! dstDir.isDirectory())
                dstDir.createDirectory();

            if (srcFile.isAChildOf (projDir))
            {
                auto dstFile = dstDir.getChildFile (srcFile.getFileName());

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

} // namespace tracktion::inline engine
