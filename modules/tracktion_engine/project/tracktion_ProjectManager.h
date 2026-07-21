/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** Selects whether a new project is stored as a single .tracktion file
    or as a folder on disk.
*/
enum class ProjectType
{
    fileBased,    /**< Traditional .tracktion binary project file. */
    folderBased   /**< Folder-based project that discovers items by scanning. */
};

//==============================================================================
/** Manages the master list of projects known to the engine.

    ProjectManager owns a ValueTree folder hierarchy that organises projects
    into "Active Projects" and "Library Projects" folders (plus user-created
    sub-folders). It provides APIs for looking up, creating, adding, removing,
    and persisting projects, as well as archive import/export.

    The folder hierarchy is saved to persistent storage via the Engine's
    PropertyStorage.

    @see Project, ProjectItem
*/
class ProjectManager
{
public:
    //==============================================================================
    /** Constructor — takes the Engine instance this manager belongs to. */
    ProjectManager (Engine&);

    /** Destructor — clears the folder tree and asserts that no projects remain open. */
    ~ProjectManager();

    /** Loads the persisted project list and imports example projects on first run. */
    void initialise();

    //==============================================================================
    /** Adds a project file or folder to the list under the given folder node.

        Deduplicates by file path and (for non-zero IDs) by project ID.
        Returns the Project, or nullptr if the file is not a valid project.

        @param projectFile      The .tracktion file or project folder to add.
        @param shouldSaveList   If true, saves the project list after adding.
        @param folderToAddTo    The ValueTree folder node to insert into.
        @param insertIndex      Position within the folder (-1 to append).
    */
    Project::Ptr addProjectToList (const juce::File& projectFile,
                                   bool shouldSaveList,
                                   juce::ValueTree folderToAddTo,
                                   int insertIndex = -1);

    /** Removes the project for the given file from the list.
        Closes any edits belonging to the project and adds the file to the
        recent-projects list.
    */
    void removeProjectFromList (const juce::File&);

    /** Removes all projects from the list and clears the open-project array. */
    void clearProjects();

    //==============================================================================
    /** Returns the "Active Projects" ValueTree folder. */
    juce::ValueTree getActiveProjectsFolder();

    /** Returns the "Library Projects" ValueTree folder. */
    juce::ValueTree getLibraryProjectsFolder();

    //==============================================================================
    /** Returns the Project object cached in a PROJECT ValueTree node.
        If createIfNotFound is true and the node represents a valid project
        file on disk, the project is opened and cached in the tree.
    */
    Project::Ptr getProjectFrom (const juce::ValueTree&, bool createIfNotFound = true);

    /** Looks up a project by its ID.
        Checks open projects first, then searches the entire folder tree.
    */
    Project::Ptr getProject (ProjectID projectID);

    /** Looks up a project by its file or folder path.
        Checks open projects first, then searches the entire folder tree.
    */
    Project::Ptr getProject (const juce::File&);

    /** Returns every project in the entire folder tree. */
    juce::ReferenceCountedArray<Project> getAllProjects();

    /** Returns every project under the given folder subtree. */
    juce::ReferenceCountedArray<Project> getAllProjects (const juce::ValueTree& folder);

    //==============================================================================
    /** Extracts a .trkx archive into destDir and adds any contained projects to the list. */
    void unpackArchiveAndAddToList (const juce::File& archiveFile, const juce::File& destDir, juce::ValueTree folder);

    //==============================================================================
    /** Finds the project containing the given ref and returns the matching ProjectItem. */
    ProjectItem::Ptr getProjectItem (const ProjectItemRef&);

    /** Finds the project containing the given item ID and returns the matching ProjectItem.
        Prefer the ProjectItemRef overload for new code.
    */
    ProjectItem::Ptr getProjectItem (ProjectItemID);

    /** Returns the ProjectItem for the given Edit's project item ID. */
    ProjectItem::Ptr getProjectItem (const Edit&);

    /** Returns the Project that owns the given Edit (may return nullptr). */
    Project::Ptr getProject (const Edit&);

    /** Returns the source file for the given project item ref. */
    juce::File findSourceFile (const ProjectItemRef&);

    /** Returns the source file for the given project item ID.
        Prefer the ProjectItemRef overload for new code.
    */
    juce::File findSourceFile (ProjectItemID);

    /** Returns the ProjectItem for the a given file. */
    ProjectItem::Ptr getProjectItem (juce::File);

    //==============================================================================
    /** Saves every project in the folder tree. */
    void saveAllProjects();

    //==============================================================================
    /** Returns a list of recently-opened project files not currently in the list.
        @param printableFormat  If true, returns display names instead of full paths.
    */
    juce::StringArray getRecentProjects (bool printableFormat);

    /** Adds a file to the recent-projects list if it isn't already tracked. */
    void addFileToRecentProjectsList (const juce::File&);

    /** Returns the parent ValueTree folder node containing the given project. */
    juce::ValueTree findFolderContaining (const Project&) const;

    /** Returns the parent ValueTree folder node containing the project with the given ID. */
    juce::ValueTree findFolderContaining (ProjectID projectId) const;

    /** Returns the PROJECT ValueTree node for the given project. */
    juce::ValueTree getFolderItemFor (const Project&) const;

    /** Returns the index of the project within its parent folder, or -1 if not found. */
    int getFolderIndexFor (const Project&) const;

    /** Creates a new organisational sub-folder inside the given parent and saves the list. */
    void createNewProjectFolder (juce::ValueTree parent, const juce::String& name);

    /** Removes a folder node from the tree. */
    void deleteProjectFolder (juce::ValueTree folder);

    /** Updates the file path stored in the ValueTree for the given project. */
    void updateProjectFile (Project& p, const juce::File&);

    /** Creates a new Project object for the given file or folder.
        This is a low-level method — the project is not added to the list.
    */
    Project::Ptr createNewProject (const juce::File& projectFile);

    /** Creates a project with a new ID, a default edit, and adds it to the list.
        Also creates default media folders and refreshes the folder structure.
        N.B. the backend is determined by whether projectFile is a directory on disk,
        so for ProjectType::folderBased the caller must create the directory first.
        projectFile is asserted to match the given ProjectType.
    */
    Project::Ptr createNewProject (const juce::File& projectFile,
                                    juce::ValueTree folderToAddTo,
                                    ProjectType projectType);

    /** Shows UI dialogs to let the user create a project in a chosen location.
        Handles directory creation, non-empty directory warnings, and naming.
    */
    void createNewProjectInteractively (const juce::String& suggestedName,
                                        const juce::File& lastPath,
                                        juce::ValueTree folderToAddTo,
                                        ProjectType projectType,
                                        std::function<void (Project::Ptr)> callback);

    /** Extracts a template archive, remaps IDs, and adds the resulting project to the list. */
    Project::Ptr createNewProjectFromTemplate (const juce::String& suggestedName,
                                               const juce::File& lastPath,
                                               const juce::File& templateArchiveFile,
                                               juce::ValueTree folderToAddTo,
                                               ProjectType projectType);

    /** Recursively searches a folder subtree for a project matching the given ID. */
    Project::Ptr findProjectWithId (const juce::ValueTree& folder, ProjectID pid);

    /** Recursively searches a folder subtree for a project matching the given file path. */
    Project::Ptr findProjectWithFile (const juce::ValueTree& folder, const juce::File&);

    //==============================================================================
    /** Loads the project list from persistent storage. */
    void loadList();

    /** Saves the project list to persistent storage. */
    void saveList();

    /** The Engine instance this manager belongs to. */
    Engine& engine;

    /** The root ValueTree holding the entire project folder hierarchy. */
    juce::ValueTree folders;

    //==============================================================================
    /** Helper struct that creates a temporary project (won't be persisted in the list).
        If createNewProjectID is true, assigns a new numeric ID and saves the project
        (needed for file-based projects that require a valid ID).
    */
    struct TempProject
    {
        TempProject (ProjectManager& pm, const juce::File& f, bool createNewProjectID,
                     ProjectType projectType = ProjectType::fileBased)
        {
            const bool created = f.exists() || (projectType == ProjectType::folderBased
                                                    ? f.createDirectory().wasOk()
                                                    : f.create());

            if (created)
            {
                auto p = pm.createNewProject (f);
                p->setTemporary (true);

                if (createNewProjectID)
                {
                    p->createNewProjectId();
                    p->save();
                }

                project = p;
            }
        }

        Project::Ptr project;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempProject)
    };

private:
    friend class Project;

    mutable juce::CriticalSection lock;
    juce::Array<Project*, juce::CriticalSection> openProjects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectManager)
};

} // namespace tracktion::inline engine
