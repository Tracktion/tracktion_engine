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
/**
    Abstract base class for project backends.
    Both FileBasedProject and FolderBasedProject inherit from this.
    Project delegates to this class via the pimpl pattern.
*/
class ProjectBase
{
public:
    ProjectBase (Project& o) : owner (o) {}
    virtual ~ProjectBase() = default;

    //==============================================================================
    virtual bool save() = 0;
    virtual bool isValid() const = 0;
    virtual bool isReadOnly() const = 0;
    virtual bool isTemporary() const                                    { return temporary; }
    virtual ProjectID getProjectID() const = 0;
    virtual juce::String getName() const = 0;
    virtual const juce::File& getProjectFile() const noexcept = 0;
    virtual juce::File getDefaultDirectory() const = 0;
    virtual juce::File getDirectoryForMedia (ProjectItem::Category) const = 0;

    virtual void setName (const juce::String&) = 0;
    virtual void createNewProjectId() = 0;

    virtual juce::String getProjectProperty (const juce::String&) const = 0;
    virtual void setProjectProperty (const juce::String&, const juce::String&) = 0;

    virtual void refreshProjectPropertiesFromFile() = 0;
    virtual bool isLibraryProject() const = 0;
    virtual bool askAboutTempoDetect (const juce::File&, bool&) const = 0;

    //==============================================================================
    virtual int getNumProjectItems() = 0;
    virtual ProjectItemRef getProjectItemRef (int index) = 0;
    virtual juce::Array<ProjectItemRef> getAllProjectItemRefs() const = 0;
    virtual ProjectItem::Ptr getProjectItemAt (int index) = 0;
    virtual juce::Array<ProjectItem::Ptr> getAllProjectItems() = 0;
    virtual int getIndexOf (const ProjectItemRef&) const = 0;

    virtual ProjectItem::Ptr getProjectItemFor (const ProjectItemRef&) = 0;
    virtual ProjectItem::Ptr getProjectItemForFile (const juce::File&) = 0;

    //==============================================================================
    virtual ProjectItem::Ptr createNewItem (const juce::File&, const juce::String& type,
                                            const juce::String& name, const juce::String& description,
                                            ProjectItem::Category, bool atTopOfList) = 0;
    virtual bool removeProjectItem (const ProjectItemRef&, bool deleteSourceMaterial) = 0;
    virtual void moveProjectItem (int from, int to) = 0;
    virtual ProjectItem::Ptr createNewEdit() = 0;
    virtual void redirectIDsFromProject (ProjectID oldProjId, ProjectID newProjId) = 0;

    //==============================================================================
    virtual void mergeArchiveContents (const juce::File&) = 0;
    virtual void mergeOtherProjectIntoThis (const juce::File&) = 0;
    virtual void refreshFolderStructure() = 0;
    virtual void createDefaultFolders() = 0;

    //==============================================================================
    virtual juce::String getSelectableDescription() const = 0;

    virtual void load() {}
    virtual void changed() = 0;
    virtual void lockFile() {}
    virtual void unlockFile() {}

    /** Updates all edit source references after a file has been moved/renamed. */
    virtual void sourceFileMoved (const juce::File& oldFile, const juce::File& newFile) = 0;

    Project& owner;
    bool temporary = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectBase)
};

} // namespace tracktion::inline engine
