/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
FolderBasedProject::FolderBasedProject (Project& o, const juce::File& f)
   : ProjectBase (o), folder (f)
{
    jassert (folder.isDirectory());
    loadPropertiesFromFile();
}

//==============================================================================
juce::File FolderBasedProject::getInfoFile() const
{
    return folder.getChildFile ("project_info.json");
}

void FolderBasedProject::loadPropertiesFromFile()
{
    auto infoFile = getInfoFile();

    if (! infoFile.existsAsFile())
        return;

    auto parsed = juce::JSON::parse (infoFile);

    if (auto* obj = parsed.getDynamicObject())
    {
        const juce::ScopedLock sl (propertyLock);
        properties.clear();

        for (const auto& prop : obj->getProperties())
            properties.set (prop.name, prop.value.toString());
    }
}

bool FolderBasedProject::savePropertiesToFile()
{
    auto infoFile = getInfoFile();
    auto info = std::make_unique<juce::DynamicObject>();

    {
        const juce::ScopedLock sl (propertyLock);

        if (properties.isEmpty())
        {
            if (infoFile.existsAsFile())
                return infoFile.deleteFile();

            return true;
        }

        for (int i = 0; i < properties.size(); ++i)
            info->setProperty (properties.getName (i), properties.getValueAt (i));
    }

    return infoFile.replaceWithText (juce::JSON::toString (juce::var (info.release())));
}

//==============================================================================
void FolderBasedProject::ensureScanned() const
{
    const juce::ScopedLock sl (itemLock);

    if (! itemsScanned)
        scanFolder();
}

void FolderBasedProject::scanFolder() const
{
    cachedItems.clear();

    auto files = folder.findChildFiles (juce::File::findFiles, true,
                                        "*.tracktionedit;*.trkedit;"
                                        "*.wav;*.aiff;*.aif;*.mp3;*.ogg;*.flac;"
                                        "*.mid;*.midi;"
                                        "*.mp4;*.mov");

    for (auto& f : files)
    {
        auto type = inferType (f);

        if (type.isEmpty())
            continue;

        auto category = inferCategory (f, folder);
        auto name = f.getFileNameWithoutExtension();

        cachedItems.add (new ProjectItem (owner.engine, f, type, name, category, owner));
    }

    itemsScanned = true;
}

juce::String FolderBasedProject::inferType (const juce::File& f)
{
    if (f.hasFileExtension (".tracktionedit;.trkedit"))
        return ProjectItem::editItemType();

    if (f.hasFileExtension (".wav;.aiff;.aif;.mp3;.ogg;.flac"))
        return ProjectItem::waveItemType();

    if (f.hasFileExtension (".mid;.midi"))
        return ProjectItem::midiItemType();

    if (f.hasFileExtension (".mp4;.mov"))
        return ProjectItem::videoItemType();

    return {};
}

ProjectItem::Category FolderBasedProject::inferCategory (const juce::File& f, const juce::File& root)
{
    auto parent = f.getParentDirectory();

    if (parent == root)
    {
        if (f.hasFileExtension (".tracktionedit;.trkedit"))
            return ProjectItem::Category::edit;

        if (f.hasFileExtension (".mp4;.mov"))
            return ProjectItem::Category::video;

        return ProjectItem::Category::recorded;
    }

    auto dirName = parent.getFileName().toLowerCase();

    if (dirName == "recorded")   return ProjectItem::Category::recorded;
    if (dirName == "exported")   return ProjectItem::Category::exports;
    if (dirName == "imported")   return ProjectItem::Category::imported;
    if (dirName == "rendered")   return ProjectItem::Category::rendered;
    if (dirName == "frozen")     return ProjectItem::Category::frozen;
    if (dirName == "archived")   return ProjectItem::Category::archives;
    if (dirName == "movies")     return ProjectItem::Category::video;

    return ProjectItem::Category::none;
}

//==============================================================================
bool FolderBasedProject::save()
{
    return savePropertiesToFile();
}

bool FolderBasedProject::isValid() const
{
    return folder.isDirectory();
}

bool FolderBasedProject::isReadOnly() const
{
    return ! folder.hasWriteAccess();
}

ProjectID FolderBasedProject::getProjectID() const
{
    return ProjectID (folder.hashCode());
}

juce::String FolderBasedProject::getName() const
{
    return folder.getFileNameWithoutExtension();
}

const juce::File& FolderBasedProject::getProjectFile() const noexcept
{
    return folder;
}

juce::File FolderBasedProject::getDefaultDirectory() const
{
    return folder;
}

juce::File FolderBasedProject::getDirectoryForMedia (ProjectItem::Category category) const
{
    auto dir = folder;

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

void FolderBasedProject::setName (const juce::String& newName)
{
    if (getName() != newName)
    {
        auto dst = folder.getParentDirectory().getChildFile (juce::File::createLegalFileName (newName));

        if (folder.moveFileTo (dst) || folder.moveFileTo (dst))
            folder = dst;

        owner.projectManager.updateProjectFile (owner, folder);
        owner.projectManager.saveList();
        owner.changed();
    }
}

void FolderBasedProject::createNewProjectId()
{
    // No-op: folder-based projects don't use project IDs
}

void FolderBasedProject::setNewProjectId (ProjectID)
{
}

juce::String FolderBasedProject::getProjectProperty (const juce::String& name) const
{
    const juce::ScopedLock sl (propertyLock);
    return properties [name];
}

void FolderBasedProject::setProjectProperty (const juce::String& name, const juce::String& value)
{
    {
        const juce::ScopedLock sl (propertyLock);
        properties.set (name, value);
    }

    savePropertiesToFile();
    changed();
}

void FolderBasedProject::refreshProjectPropertiesFromFile()
{
    loadPropertiesFromFile();
}

bool FolderBasedProject::isLibraryProject() const
{
    return owner.projectManager.findFolderContaining (owner) == owner.projectManager.getLibraryProjectsFolder();
}

//==============================================================================
int FolderBasedProject::getNumProjectItems()
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);
    return cachedItems.size();
}

ProjectItemRef FolderBasedProject::getProjectItemRef (int index)
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);

    if (juce::isPositiveAndBelow (index, cachedItems.size()))
    {
        auto f = cachedItems[index]->getSourceFile();

        if (f.isAChildOf (folder))
            return ProjectItemRef::fromPath (f.getRelativePathFrom (folder), owner);

        return ProjectItemRef::fromAbsolutePath (f);
    }

    return {};
}

juce::Array<ProjectItemRef> FolderBasedProject::getAllProjectItemRefs() const
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);

    juce::Array<ProjectItemRef> result;

    for (auto& item : cachedItems)
    {
        auto f = item->getSourceFile();

        if (f.isAChildOf (folder))
            result.add (ProjectItemRef::fromPath (f.getRelativePathFrom (folder), owner));
        else
            result.add (ProjectItemRef::fromAbsolutePath (f));
    }

    return result;
}

ProjectItem::Ptr FolderBasedProject::getProjectItemAt (int index)
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);

    if (juce::isPositiveAndBelow (index, cachedItems.size()))
        return cachedItems[index];

    return {};
}

juce::Array<ProjectItem::Ptr> FolderBasedProject::getAllProjectItems()
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);

    juce::Array<ProjectItem::Ptr> result;
    result.addArray (cachedItems);
    return result;
}

int FolderBasedProject::getIndexOf (const ProjectItemRef& ref) const
{
    ensureScanned();
    auto file = ref.resolve (owner.engine, folder);

    if (file == juce::File())
        return -1;

    const juce::ScopedLock sl (itemLock);

    for (int i = 0; i < cachedItems.size(); ++i)
        if (cachedItems[i]->isForFile (file))
            return i;

    return -1;
}

ProjectItem::Ptr FolderBasedProject::getProjectItemFor (const ProjectItemRef& ref)
{
    auto file = ref.resolve (owner.engine, folder);
    return file != juce::File() ? getProjectItemForFile (file) : nullptr;
}

ProjectItem::Ptr FolderBasedProject::getProjectItemForFile (const juce::File& fileToFind)
{
    ensureScanned();
    const juce::ScopedLock sl (itemLock);

    for (auto& item : cachedItems)
        if (item->isForFile (fileToFind))
            return item;

    return {};
}

//==============================================================================
ProjectItem::Ptr FolderBasedProject::createNewItem (const juce::File& fileToReference,
                                                    const juce::String& type,
                                                    const juce::String& name,
                                                    const juce::String&,
                                                    ProjectItem::Category cat,
                                                    bool)
{
    ensureScanned();

    if (auto existing = getProjectItemForFile (fileToReference))
        if (existing->getType() == type)
            return existing;

    // Hold a strong reference to the new item before calling changed(). A
    // change-listener may synchronously call reload(), which clears cachedItems
    // and would drop the only reference to the new item — leaving 'item' as a
    // dangling pointer and causing the implicit conversion to Ptr at 'return'
    // to operate on freed memory.
    ProjectItem::Ptr item (new ProjectItem (owner.engine, fileToReference, type, name, cat, owner));

    {
        const juce::ScopedLock sl (itemLock);
        cachedItems.add (item.get());
    }

    changed();
    return item;
}

bool FolderBasedProject::removeProjectItem (const ProjectItemRef& ref, bool deleteSourceMaterial)
{
    if (isValid() && ! isReadOnly())
    {
        {
            const juce::ScopedLock sl (itemLock);

            const int index = getIndexOf (ref);
            jassert (index >= 0);

            if (index >= 0)
            {
                auto item = cachedItems[index];

                if (item != nullptr)
                {
                    item->deselect();

                    if (deleteSourceMaterial)
                        if (! item->deleteSourceFile())
                            return false;
                }

                cachedItems.remove (index);
            }
        }

        changed();
        return true;
    }

    return false;
}

void FolderBasedProject::moveProjectItem (int, int)
{
    // No-op: no ordering concept
}

ProjectItem::Ptr FolderBasedProject::createNewEdit()
{
    ensureScanned();

    int maxSuffix = 0;
    auto items = getAllProjectItems();

    for (auto& item : items)
    {
        if (item->isEdit())
        {
            auto nm = item->getName();

            if (nm.startsWithIgnoreCase (getName() + " Edit "))
                maxSuffix = std::max (maxSuffix, nm.getTrailingIntValue());
        }
    }

    auto name = getName() + " Edit ";
    name << (maxSuffix + 1);

    auto f = folder.getNonexistentChildFile (name, editFileSuffix, false);

    if (f.create())
        return createNewItem (f, ProjectItem::editItemType(), name,
                              {}, ProjectItem::Category::edit, true);

    return {};
}

void FolderBasedProject::redirectIDsFromProject (ProjectID, ProjectID)
{
    // No-op: no IDs to redirect
}

//==============================================================================
void FolderBasedProject::mergeArchiveContents (const juce::File&)
{
    // No-op: doesn't apply to folder-based projects
}

void FolderBasedProject::mergeOtherProjectIntoThis (const juce::File&)
{
    // No-op: doesn't apply to folder-based projects
}

void FolderBasedProject::refreshFolderStructure()
{
    // No-op: folder-based projects don't move files to category subfolders.
    // Use reload() instead to rescan.
}

void FolderBasedProject::createDefaultFolders()
{
    getDirectoryForMedia (ProjectItem::Category::archives);
    getDirectoryForMedia (ProjectItem::Category::exports);
    getDirectoryForMedia (ProjectItem::Category::imported);
    getDirectoryForMedia (ProjectItem::Category::recorded);
    getDirectoryForMedia (ProjectItem::Category::rendered);
    getDirectoryForMedia (ProjectItem::Category::edit);
}

//==============================================================================
juce::String FolderBasedProject::getSelectableDescription() const
{
    return TRANS("Folder Project");
}

void FolderBasedProject::changed()
{
    owner.projectChanged();
}

void FolderBasedProject::sourceFileMoved (const juce::File& oldFile, const juce::File& newFile)
{
    auto projectDir = getDefaultDirectory();
    auto oldRef = ProjectItemRef::fromPath (oldFile.getRelativePathFrom (projectDir));
    auto newRef = ProjectItemRef::fromPath (newFile.getRelativePathFrom (projectDir), owner);

    // Helper lambda to reassign refs in a single edit
    auto reassignInEdit = [&] (Edit& edit)
    {
        for (auto exportable : Exportable::addAllExportables (edit))
            for (auto& item : exportable->getReferencedItems())
                if (item.itemRef == oldRef)
                    exportable->reassignReferencedItem (item, newRef, 0.0);
    };

    // 1. Update all currently open edits belonging to this project
    for (auto edit : owner.engine.getActiveEdits().getEdits())
    {
        if (edit != nullptr && owner.projectManager.getProject (*edit).get() == &owner)
        {
            reassignInEdit (*edit);
            EditFileOperations (*edit).save (false, true, false);
        }
    }

    // 2. Update closed edit files on disk
    auto allItems = getAllProjectItems();

    for (auto item : allItems)
    {
        if (item != nullptr)
        {
            if (item->isEdit())
            {
                auto editFile = item->getSourceFile();

                // Skip if this edit is already open (handled above)
                bool isOpen = false;

                for (auto edit : owner.engine.getActiveEdits().getEdits())
                    if (edit != nullptr && edit->getProjectItemRef() == item->getProjectItemRef())
                        isOpen = true;

                if (! isOpen && editFile.existsAsFile())
                {
                    auto ed = loadEditForExamining (owner.projectManager, item->getProjectItemRef());

                    if (ed != nullptr)
                    {
                        reassignInEdit (*ed);
                        EditFileOperations saveOps (*ed);
                        jassert (saveOps.getEditFile() == editFile);
                        saveOps.save (false, true, false);
                    }
                }
            }
        }
    }

    // 3. Refresh cached items so the moved file is correctly indexed
    reload (ReloadMode::lazy);
}

void FolderBasedProject::reload (ReloadMode mode)
{
    const juce::ScopedLock sl (itemLock);
    cachedItems.clear();
    itemsScanned = false;

    if (mode == ReloadMode::immediate)
        scanFolder();
}

} // namespace tracktion::inline engine
