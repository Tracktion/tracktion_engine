/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

class ProjectBase;
class FileBasedProject;
class FolderBasedProject;

//==============================================================================
/** A tracktion project.

    A Project wraps either a .tracktion project file (FileBasedProject) or a
    project folder (FolderBasedProject). The backend is chosen automatically
    based on whether the path passed to ProjectManager is a file or directory.

    **File-based projects** persist their state in a binary .tracktion file.
    Items have valid ProjectItemIDs and support full ID-based lookup, search,
    reordering, and merging.

    **Folder-based projects** discover
    *items by lazily scanning the folder on
    disk. Items have invalid (zero) ProjectItemIDs, so ID-based lookup always
    returns nullptr. Operations that rely on IDs or binary state (merge, reorder)
    are no-ops for folder-based projects; properties and descriptions are stored
    in a project_info.json file inside the folder.

    The per-method docs below note where behaviour differs between backends.

    **Source references in edits:**
    Clips in an Edit reference their audio source via a string stored in the
    @c source property. For file-based projects this is a ProjectItemID string
    (e.g. @c "1234_5678"). For folder-based projects this is a file path --
    relative to the project folder if the file lives inside it, or absolute
    otherwise. Use getSourcePathForFile() to obtain the correct string for
    a given file.

    @see ProjectManager, ProjectItem
*/
class Project  : public juce::ReferenceCountedObject,
                 public Selectable,
                 private juce::AsyncUpdater
{
public:
    //==============================================================================
    ~Project() override;

    //==============================================================================
    /** Saves the project if changes have been made since the last save.
        Folder-based projects have no persistent state file, so this always returns true.
    */
    bool save();
    void handleAsyncUpdate() override;

    //==============================================================================
    /** Returns true if the project is valid.
        File-based: checks that the project ID is non-zero.
        Folder-based: checks that the folder exists on disk.
    */
    bool isValid() const;

    /** Returns true if the project file or folder is read-only. */
    bool isReadOnly() const;

    /** Returns true if this is a temporary project (won't appear in recent projects). */
    bool isTemporary() const;

    /** Marks this project as temporary (won't appear in recent projects). */
    void setTemporary (bool);

    /** Returns the project ID.
        File-based: the persisted random ID.
        Folder-based: a hash of the folder path.
    */
    ProjectID getProjectID() const;

    /** Returns the project name.
        Folder-based projects return the folder name.
    */
    juce::String getName() const;

    /** Returns the project description string.
        Stored as a project property and works for both file-based and folder-based projects.
    */
    juce::String getDescription() const;

    /** Returns true if this is a folder-based project (as opposed to file-based). */
    bool isFolderBased() const;

    /** Returns the project file (.tracktion) or, for folder-based projects, the folder itself. */
    const juce::File& getProjectFile() const noexcept;

    /** Returns the default directory for storing project media.
        For folder-based projects this is the project folder itself.
    */
    juce::File getDefaultDirectory() const;

    /** Returns the subdirectory for media of the given category (e.g. "recorded", "rendered"). */
    juce::File getDirectoryForMedia (ProjectItem::Category category) const;

    /** Returns the appropriate source path string for referencing the given file.

        For folder-based projects, this returns a path relative to the project
        folder if the file is a child of it, or an absolute path otherwise.
        File-based projects should not use this method — they use ProjectItemIDs.
    */
    juce::String getSourcePathForFile (const juce::File& file) const;

    /** Renames the project. This renames the underlying file or folder on disk. */
    void setName (const juce::String& newName);

    /** Sets the project description. Works for both file-based and folder-based projects. */
    void setDescription (const juce::String& newDesc);

    /** Generates and assigns a new random project ID. No-op for folder-based projects. */
    void createNewProjectId();

    /** Returns a named project property.
        Folder-based projects store properties in project_info.json.
    */
    juce::String getProjectProperty (const juce::String& name) const;

    /** Sets a named project property.
        Folder-based projects store properties in project_info.json.
    */
    void setProjectProperty (const juce::String& name, const juce::String& value);

    /** Reloads project properties from the project file (or project_info.json). */
    void refreshProjectPropertiesFromFile();

    /** Returns true if this project is flagged as a library project (a shared sound/media library). */
    bool isLibraryProject() const;

    /** Returns refs to items whose source files are not referenced by any edit.
        Works for both file-based and folder-based projects.
    */
    juce::Array<ProjectItemRef> findOrphanItemRefs();

    /** @deprecated Use findOrphanItemRefs() instead. */
    juce::Array<ProjectItem::Ptr> findOrphanItems();

    //==============================================================================
    /** Returns the number of items in this project.
        Folder-based projects lazily scan the folder on the first call.
    */
    int getNumProjectItems();

    /** Returns a ProjectItemRef at the given index.
        File-based projects return an ID-based ref, folder-based return a path-based ref.
    */
    ProjectItemRef getProjectItemRef (int index);

    /** Returns all ProjectItemRefs in this project.
        Folder-based projects trigger the lazy folder scan and return path-based refs.
    */
    juce::Array<ProjectItemRef> getAllProjectItemRefs() const;

    /** Returns the ProjectItem at the given index, or nullptr if out of range. */
    ProjectItem::Ptr getProjectItemAt (int index);

    /** Returns all ProjectItems in this project. */
    juce::Array<ProjectItem::Ptr> getAllProjectItems();

    /** Returns the index of the item with the given ref, or -1 if not found. */
    int getIndexOf (const ProjectItemRef&) const;

    /** Returns the ProjectItem for the given ref, or nullptr if not found.
        For file-based projects, looks up by ID. For folder-based, resolves
        the path and looks up by file.
    */
    ProjectItem::Ptr getProjectItemFor (const ProjectItemRef&);

    /** Returns the ProjectItem that references the given file, or nullptr if not found.
        Works for both file-based and folder-based projects.
    */
    ProjectItem::Ptr getProjectItemForFile (const juce::File& file);

    //==============================================================================
    /** Returns an existing ProjectItem for the file if one exists, otherwise creates
        a new one with the given name, type, description, and category.
        Folder-based items are created with invalid ProjectItemIDs.
    */
    ProjectItem::Ptr createNewItem (const juce::File& fileToReference,
                                    const juce::String& type,
                                    const juce::String& name,
                                    const juce::String& description,
                                    const ProjectItem::Category cat,
                                    bool atTopOfList);

    /** Removes the item matching the given ref. If deleteSourceMaterial is true, deletes
        the source file on disk. Works for both file-based and folder-based projects.
    */
    bool removeProjectItem (const ProjectItemRef&, bool deleteSourceMaterial);

    /** Moves a project item from one index to another. No-op for folder-based projects. */
    void moveProjectItem (int indexToMoveFrom, int indexToMoveTo);

    /** Creates a new empty .tracktionedit file and adds it as a ProjectItem.
        Folder-based items are created with invalid ProjectItemIDs.
    */
    ProjectItem::Ptr createNewEdit();

    /** Tells all exportables in all edits to remap references from oldProjId to newProjId.
        No-op for folder-based projects.
    */
    void redirectIDsFromProject (ProjectID oldProjId, ProjectID newProjId);

    //==============================================================================
    /** General enum for requests that have a project setting and so can return
        true without asking the user.
    */
    enum NagMode
    {
        nagAsk,     /**< Should ask user. */
        nagAutoYes, /**< Should do task automatically. */
        nagAutoNo   /**< Should not do task automatically. */
    };

    /** Imports items from a .tracktion archive file into this project.
        No-op for folder-based projects.
    */
    void mergeArchiveContents (const juce::File& archiveFile);

    /** Merges all items from another project file into this project.
        No-op for folder-based projects.
    */
    void mergeOtherProjectIntoThis (const juce::File& otherProject);

    /** Makes sure all media files are in the correct category subfolders.
        No-op for folder-based projects.
    */
    void refreshFolderStructure();

    /** Creates the default media subdirectories (e.g. "recorded", "rendered") if they don't exist. */
    void createDefaultFolders();

    //==============================================================================
    enum ProjectSortType
    {
        SortByName,
        SortByDesc,
        SortByType,
        SortByLength,
        SortBySize,
        SortByModified,
        SortByCreated,
    };

    //==============================================================================
    juce::String getSelectableDescription() override;

    /** Locks the project file to prevent writes or moves. No-op for folder-based projects. */
    void lockFile();

    /** Unlocks the project file. No-op for folder-based projects. */
    void unlockFile();

    /** Updates all edit source references after a file has been moved/renamed.
        For folder-based projects, this walks all edits and calls reassignReferencedItem
        to update path-based source references. File-based projects are a no-op (IDs are stable).
    */
    void sourceFileMoved (const juce::File& oldFile, const juce::File& newFile);

    /** Controls when reload() actually re-reads the backing store.
        @see ProjectBase::ReloadMode */
    using ReloadMode = ProjectBase::ReloadMode;

    /** Invalidates any cached project item list so the next call to getAllProjectItems()
        re-reads the backing store. For folder-based projects this clears the directory
        scan cache; for file-based projects this is a no-op. */
    void reload (ReloadMode = ReloadMode::lazy);

    using Ptr = juce::ReferenceCountedObjectPtr<Project>;

    /** The Engine instance this project belongs to. */
    Engine& engine;

    /** The ProjectManager that owns this project. */
    ProjectManager& projectManager;

    /** @internal */
    void setNewProjectId (ProjectID);

private:
    friend class ProjectBase;
    friend class FileBasedProject;
    friend class FolderBasedProject;
    friend class ProjectItem;
    friend class ProjectManager;

    std::unique_ptr<ProjectBase> impl;

    Project (Engine&, ProjectManager&, const juce::File&);

    void changed() override;
    void projectChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};

/** Converts a file-based project to a folder-based project.
    Opens each edit and rewrites ProjectItemID source references as relative
    file paths. Writes project metadata to project_info.json and deletes the
    .tracktion file. Returns a new folder-based Project opened from the
    directory. The original Project object should be released promptly.
*/
Project::Ptr convertToFolderBasedProject (Project&);

} // namespace tracktion::inline engine
