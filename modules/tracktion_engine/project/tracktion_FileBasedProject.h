/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
    Contains the implementation logic for a file-based (.tracktion) project.
    Project delegates to this class via the pimpl pattern.
*/
class FileBasedProject
{
public:
    FileBasedProject (Project& owner, const juce::File& projectFile);
    ~FileBasedProject();

    //==============================================================================
    bool save();
    bool isValid() const;
    bool isReadOnly() const;
    bool isTemporary() const                                    { return temporary; }
    int getProjectID() const;
    juce::String getName() const;
    juce::String getDescription() const;
    const juce::File& getProjectFile() const noexcept           { return file; }
    juce::File getDefaultDirectory() const;
    juce::File getDirectoryForMedia (ProjectItem::Category) const;

    void setName (const juce::String&);
    void setDescription (const juce::String&);
    void createNewProjectId();

    juce::String getProjectProperty (const juce::String&) const;
    void setProjectProperty (const juce::String&, const juce::String&);

    void refreshProjectPropertiesFromFile();
    bool isLibraryProject() const;
    bool askAboutTempoDetect (const juce::File&, bool&) const;

    juce::Array<ProjectItemID> findOrphanItems();

    //==============================================================================
    int getNumProjectItems();
    ProjectItemID getProjectItemID (int index);
    juce::Array<ProjectItemID> getAllProjectItemIDs() const;
    juce::Array<int> getAllItemIDs() const;
    ProjectItem::Ptr getProjectItemAt (int index);
    juce::Array<ProjectItem::Ptr> getAllProjectItems();
    int getIndexOf (ProjectItemID) const;

    ProjectItem::Ptr getProjectItemForID (ProjectItemID);
    ProjectItem::Ptr getProjectItemForFile (const juce::File&);

    //==============================================================================
    ProjectItem::Ptr createNewItem (const juce::File&, const juce::String& type,
                                    const juce::String& name, const juce::String& description,
                                    ProjectItem::Category, bool atTopOfList);
    bool removeProjectItem (ProjectItemID, bool deleteSourceMaterial);
    void moveProjectItem (int from, int to);
    ProjectItem::Ptr createNewEdit();
    void redirectIDsFromProject (int oldProjId, int newProjId);

    //==============================================================================
    void mergeArchiveContents (const juce::File&);
    void mergeOtherProjectIntoThis (const juce::File&);
    void refreshFolderStructure();
    void createDefaultFolders();

    //==============================================================================
    void searchFor (juce::Array<ProjectItemID>&, SearchOperation&);

    //==============================================================================
    juce::String getSelectableDescription() const;

    void lockFile();
    void unlockFile();

private:
    friend class Project;

    Project& owner;

    juce::File file;
    int projectId = 0;

    juce::NamedValueSet properties;
    juce::CriticalSection objectLock, propertyLock;

    std::unique_ptr<juce::BufferedInputStream> stream;
    std::unique_ptr<juce::FileInputStream> fileLockingStream;

    struct ObjectInfo
    {
        int itemID = 0, fileOffset = 0;
        ProjectItem::Ptr item;
    };

    juce::Array<ObjectInfo> objects;
    int objectOffset = 0, indexOffset = 0;
    bool readOnly = false, hasChanged = false, temporary = false;

    void load();
    void saveTo (juce::FileOutputStream&);
    bool readProjectHeader (juce::InputStream&, bool clearObjectInfo = true);
    juce::BufferedInputStream* getInputStream();
    void loadAllProjectItems();
    bool loadProjectItem (ObjectInfo&);
    void ensureFolderCreated (ProjectItem::Category);

    ProjectItem::Ptr quickAddProjectItem (const juce::String& relPathName,
                                          const juce::String& type,
                                          const juce::String& name,
                                          const juce::String& description,
                                          ProjectItem::Category,
                                          ProjectItemID);

    void changed();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileBasedProject)
};

}} // namespace tracktion { inline namespace engine
