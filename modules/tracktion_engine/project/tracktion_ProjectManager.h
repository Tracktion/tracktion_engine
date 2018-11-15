/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class ProjectManager    : private juce::DeletedAtShutdown
{
public:
    //==============================================================================
    ProjectManager();
    ~ProjectManager();

    JUCE_DECLARE_SINGLETON (ProjectManager, true)

    void initialise();

    //==============================================================================
    Project::Ptr addProjectToList (const juce::File& projectFile,
                                   bool shouldSaveList,
                                   juce::ValueTree folderToAddTo,
                                   int insertIndex = -1);

    void removeProjectFromList (const juce::File&);

    void clearProjects();

    //==============================================================================
    juce::ValueTree getActiveProjectsFolder();
    juce::ValueTree getLibraryProjectsFolder();

    //==============================================================================
    Project::Ptr getProjectFrom (const juce::ValueTree&, bool createIfNotFound = true);

    Project::Ptr getProject (int projectID);
    Project::Ptr getProject (const juce::File&);

    juce::ReferenceCountedArray<Project> getAllProjects (const juce::ValueTree& folder);

    //==============================================================================
    void unpackArchiveAndAddToList (const juce::File& archiveFile, juce::ValueTree folder);

    //==============================================================================
    /** tries to find the project that contains an id, and open it as a ProjectItem. */
    ProjectItem::Ptr getProjectItem (ProjectItemID);
    ProjectItem::Ptr getProjectItem (const Edit&);

    /** Tries to find the project that contains this edit (but may return nullptr!) */
    Project::Ptr getProject (const Edit&);

    /** tries to find the media file used by a particular object. */
    juce::File findSourceFile (ProjectItemID);

    //==============================================================================
    void saveAllProjects();

    //==============================================================================
    juce::StringArray getRecentProjects (bool printableFormat);
    void addFileToRecentProjectsList (const juce::File&);

    juce::ValueTree findFolderContaining (const Project&) const;
    juce::ValueTree findFolderContaining (int projectId) const;
    juce::ValueTree getFolderItemFor (const Project&) const;
    int getFolderIndexFor (const Project&) const;

    void createNewProjectFolder (juce::ValueTree parent, const juce::String& name);
    void deleteProjectFolder (juce::ValueTree folder);
    void updateProjectFile (Project& p, const juce::File&);

    Project::Ptr createNewProject (const juce::File& projectFile);
    Project::Ptr createNewProject (const juce::File& projectFile, juce::ValueTree folderToAddTo);
    Project::Ptr createNewProjectInteractively (const juce::String& suggestedName, const juce::File& lastPath, juce::ValueTree folderToAddTo);
    Project::Ptr createNewProjectFromTemplate (const juce::String& suggestedName, const juce::File& lastPath, const juce::File& templateArchiveFile, juce::ValueTree folderToAddTo);

    Project::Ptr findProjectWithId (const juce::ValueTree& folder, int pid);
    Project::Ptr findProjectWithFile (const juce::ValueTree& folder, const juce::File&);

    //==============================================================================
    void loadList();
    void saveList();

    Engine& engine;
    juce::ValueTree folders;

    //==============================================================================
    struct TempProject
    {
        TempProject (ProjectManager& pm, const juce::File& f, bool createNewProjectID)
        {
            if (f.exists() || f.create())
            {
                auto p = pm.createNewProject (f);

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

    juce::CriticalSection lock;
    juce::Array<Project*, juce::CriticalSection> openProjects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectManager)
};

} // namespace tracktion_engine
