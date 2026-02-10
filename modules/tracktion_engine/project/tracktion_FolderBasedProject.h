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
    A project backend that represents a folder on disk.
    Files in the folder are discovered as ProjectItems.
*/
class FolderBasedProject  : public ProjectBase
{
public:
    FolderBasedProject (Project& owner, const juce::File& folder);
    ~FolderBasedProject() override;

    //==============================================================================
    bool save() override;
    bool isValid() const override;
    bool isReadOnly() const override;
    int getProjectID() const override;
    juce::String getName() const override;
    juce::String getDescription() const override;
    const juce::File& getProjectFile() const noexcept override;
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

    juce::Array<ProjectItemID> findOrphanItems() override;

    //==============================================================================
    int getNumProjectItems() override;
    ProjectItemID getProjectItemID (int index) override;
    juce::Array<ProjectItemID> getAllProjectItemIDs() const override;
    juce::Array<int> getAllItemIDs() const override;
    ProjectItem::Ptr getProjectItemAt (int index) override;
    juce::Array<ProjectItem::Ptr> getAllProjectItems() override;
    int getIndexOf (ProjectItemID) const override;

    ProjectItem::Ptr getProjectItemForID (ProjectItemID) override;
    ProjectItem::Ptr getProjectItemForFile (const juce::File&) override;

    //==============================================================================
    ProjectItem::Ptr createNewItem (const juce::File&, const juce::String& type,
                                    const juce::String& name, const juce::String& description,
                                    ProjectItem::Category, bool atTopOfList) override;
    bool removeProjectItem (ProjectItemID, bool deleteSourceMaterial) override;
    void moveProjectItem (int from, int to) override;
    ProjectItem::Ptr createNewEdit() override;
    void redirectIDsFromProject (int oldProjId, int newProjId) override;

    //==============================================================================
    void mergeArchiveContents (const juce::File&) override;
    void mergeOtherProjectIntoThis (const juce::File&) override;
    void refreshFolderStructure() override;
    void createDefaultFolders() override;

    //==============================================================================
    void searchFor (juce::Array<ProjectItemID>&, SearchOperation&) override;

    //==============================================================================
    juce::String getSelectableDescription() const override;

    void changed() override;

    /** Clears the cached items so the folder is rescanned on next access.
        If lazy is false, immediately rescans. */
    void reload (bool lazy = true);

private:
    juce::File folder;
    juce::Array<ProjectItem::Ptr> cachedItems;
    juce::CriticalSection itemLock;
    bool itemsScanned = false;

    juce::NamedValueSet properties;
    juce::CriticalSection propertyLock;

    juce::File getInfoFile() const;
    void loadPropertiesFromFile();
    bool savePropertiesToFile();

    void ensureScanned();
    void scanFolder();
    static juce::String inferType (const juce::File&);
    static ProjectItem::Category inferCategory (const juce::File&, const juce::File& root);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderBasedProject)
};

}} // namespace tracktion { inline namespace engine
