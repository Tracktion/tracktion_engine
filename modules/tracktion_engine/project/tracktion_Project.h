/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/** A tracktion project.

    The projects contain a set of ProjectItems to represent each item in it, which
    may be edits, wave files, etc.

    Originally these were designed to hold huge numbers of items (so I could use them
    for holding sound-libraries and searching them), so some of the implementation
    might seem like overkill, such as the reverse-index text search stuff.

    @see ProjectManager
*/
class Project  : public juce::ReferenceCountedObject,
                 public Selectable,
                 private juce::AsyncUpdater
{
public:
    //==============================================================================
    ~Project();

    //==============================================================================
    /** only saves if a change has been made since last time  */
    bool save();
    void handleAsyncUpdate() override;

    //==============================================================================
    /** true if it's got a proper project ID. */
    bool isValid() const;
    bool isReadOnly() const;

    int getProjectID() const;
    juce::String getName() const;
    juce::String getDescription() const;
    const juce::File& getProjectFile() const noexcept         { return file; }
    juce::File getDefaultDirectory() const;
    juce::File getDirectoryForMedia (ProjectItem::Category category) const;

    void setName (const juce::String& newName);
    void setDescription (const juce::String& newDesc);

    void createNewProjectId();

    juce::String getProjectProperty (const juce::String& name) const;
    void setProjectProperty (const juce::String& name, const juce::String& value);

    void refreshProjectPropertiesFromFile();

    bool isLibraryProject() const;
    bool askAboutTempoDetect (const juce::File&, bool& shouldSetAutoTempo) const;

    juce::Array<ProjectItemID> findOrphanItems();

    //==============================================================================
    int getNumMediaItems();
    ProjectItemID getProjectItemID (int index);
    juce::Array<ProjectItemID> getAllProjectItemIDs() const;
    juce::Array<int> getAllItemIDs() const;
    ProjectItem::Ptr getProjectItemAt (int index);
    int getIndexOf (ProjectItemID) const;

    ProjectItem::Ptr getProjectItemForID (ProjectItemID);
    ProjectItem::Ptr getProjectItemForFile (const juce::File& file);

    //==============================================================================
    /** Returns an existing object if there is one. If not, it will use the given name
        and description to create a new one
    */
    ProjectItem::Ptr createNewItem (const juce::File& fileToReference,
                                    const juce::String& type,
                                    const juce::String& name,
                                    const juce::String& description,
                                    const ProjectItem::Category cat,
                                    bool atTopOfList);

    bool removeProjectItem (ProjectItemID, bool deleteSourceMaterial);

    void moveProjectItem (int indexToMoveFrom, int indexToMoveTo);

    ProjectItem::Ptr createNewEdit();

    /** Tells all exportables in all edits to change this project ID. */
    void redirectIDsFromProject (int oldProjId, int newProjId);

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

    void mergeArchiveContents (const juce::File& archiveFile);

    void mergeOtherProjectIntoThis (const juce::File& otherProject);

    /** makes sure all media is in the correct sub folder **/
    void refreshFolderStructure();
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
    /** this will load the keyword table, and do a search */
    void searchFor (juce::Array<ProjectItemID>& results, SearchOperation&);

    //==============================================================================
    juce::String getSelectableDescription() override;

    /** Stops anyone writing/moving the project file */
    void lockFile();
    void unlockFile();

    using Ptr = juce::ReferenceCountedObjectPtr<Project>;

    Engine& engine;
    ProjectManager& projectManager;

private:
    friend class ProjectItem;
    friend class ProjectManager;

    juce::File file;
    int projectId = 0;

    juce::NamedValueSet properties;
    juce::CriticalSection objectLock, propertyLock;

    juce::ScopedPointer<juce::BufferedInputStream> stream;
    juce::ScopedPointer<juce::FileInputStream> fileLockingStream;

    struct ObjectInfo
    {
        int itemID, fileOffset;
        ProjectItem::Ptr item;
    };

    juce::Array<ObjectInfo> objects;
    int objectOffset = 0, indexOffset = 0;
    bool readOnly = false, hasChanged = false;

    Project (Engine&, ProjectManager&, const juce::File&);

    juce::BufferedInputStream* getInputStream();

    void changed() override;
    void load();
    void saveTo (juce::FileOutputStream&);
    bool readProjectHeader (juce::InputStream&, bool clearObjectInfo = true);
    void loadAllProjectItems();
    bool loadProjectItem (ObjectInfo&);
    void ensureFolderCreated (ProjectItem::Category);

    /** adds an item without checking */
    ProjectItem::Ptr quickAddProjectItem (const juce::String& relPathName,
                                          const juce::String& type,
                                          const juce::String& name,
                                          const juce::String& description,
                                          const ProjectItem::Category cat,
                                          ProjectItemID newID);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};

} // namespace tracktion_engine
