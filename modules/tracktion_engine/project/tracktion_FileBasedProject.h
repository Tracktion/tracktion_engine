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
class FileBasedProject  : public ProjectBase
{
public:
    FileBasedProject (Project& owner, const juce::File& projectFile);
    ~FileBasedProject() override;

    //==============================================================================
    bool save() override;
    bool isValid() const override;
    bool isReadOnly() const override;
    ProjectID getProjectID() const override;
    juce::String getName() const override;
    juce::String getDescription() const override;
    const juce::File& getProjectFile() const noexcept override          { return file; }
    juce::File getDefaultDirectory() const override;
    juce::File getDirectoryForMedia (ProjectItem::Category) const override;

    void setName (const juce::String&) override;
    void setDescription (const juce::String&) override;
    void createNewProjectId() override;

    juce::String getProjectProperty (const juce::String&) const override;
    void setProjectProperty (const juce::String&, const juce::String&) override;

    void refreshProjectPropertiesFromFile() override;
    bool isLibraryProject() const override;
    bool askAboutTempoDetect (const juce::File&, bool&) const override;

    //==============================================================================
    int getNumProjectItems() override;
    ProjectItemRef getProjectItemRef (int index) override;
    juce::Array<ProjectItemRef> getAllProjectItemRefs() const override;
    juce::Array<int> getAllItemIDs() const override;
    ProjectItem::Ptr getProjectItemAt (int index) override;
    juce::Array<ProjectItem::Ptr> getAllProjectItems() override;
    int getIndexOf (const ProjectItemRef&) const override;

    ProjectItem::Ptr getProjectItemFor (const ProjectItemRef&) override;
    ProjectItem::Ptr getProjectItemForFile (const juce::File&) override;

    //==============================================================================
    ProjectItem::Ptr createNewItem (const juce::File&, const juce::String& type,
                                    const juce::String& name, const juce::String& description,
                                    ProjectItem::Category, bool atTopOfList) override;
    bool removeProjectItem (const ProjectItemRef&, bool deleteSourceMaterial) override;
    void moveProjectItem (int from, int to) override;
    ProjectItem::Ptr createNewEdit() override;
    void redirectIDsFromProject (ProjectID oldProjId, ProjectID newProjId) override;

    //==============================================================================
    void mergeArchiveContents (const juce::File&) override;
    void mergeOtherProjectIntoThis (const juce::File&) override;
    void refreshFolderStructure() override;
    void createDefaultFolders() override;

    //==============================================================================
    void searchFor (juce::Array<ProjectItemRef>&, SearchOperation&) override;

    //==============================================================================
    juce::String getSelectableDescription() const override;

    void load() override;
    void changed() override;
    void lockFile() override;
    void unlockFile() override;

private:
    friend class Project;

    juce::File file;
    ProjectID projectId;

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
    bool readOnly = false, hasChanged = false;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileBasedProject)
};

}} // namespace tracktion { inline namespace engine
