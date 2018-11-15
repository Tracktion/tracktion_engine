/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Represents a file-based resource that is used in a project.
    This might be an edit, audio file, midi file, etc.
*/
class ProjectItem   : public Selectable,
                      public juce::ReferenceCountedObject,
                      private juce::Timer,
                      private juce::AsyncUpdater
{
public:
    //==============================================================================
    enum class Category
    {
        none,
        edit,
        recorded,
        exports,
        archives,
        imported,
        rendered,
        frozen,
        video
    };

    enum class RenameMode
    {
        always,
        local,
        never
    };

    enum class SetNameMode
    {
        doDefault,
        forceRename,
        forceNoRename
    };

    using Ptr = juce::ReferenceCountedObjectPtr<ProjectItem>;
    using Array = juce::ReferenceCountedArray<ProjectItem>;

    //==============================================================================
    /** Loads a ProjectItem from a stream that was saved using writeToStream(). */
    ProjectItem (Engine&, ProjectItemID, juce::InputStream*);

    //==============================================================================
    /** Creates a ProjectItem with some settings. */
    ProjectItem (Engine&,
                 const juce::String& name,
                 const juce::String& type,
                 const juce::String& desc,
                 const juce::String& file,
                 Category,
                 double length,
                 ProjectItemID);

    ~ProjectItem();

    //==============================================================================
    juce::String getSelectableDescription() override;
    void selectionStatusChanged (bool isNowSelected) override;

    //==============================================================================
    ProjectItemID getID() const noexcept            { return itemID; }

    juce::ReferenceCountedObjectPtr<Project> getProject() const;

    // checks whether it's been removed from its project
    bool hasBeenDeleted() const;

    static const char* waveItemType() noexcept      { return "wave"; }
    static const char* editItemType() noexcept      { return "edit"; }
    static const char* midiItemType() noexcept      { return "midi"; }
    static const char* videoItemType() noexcept     { return "video"; }

    const juce::String& getType() const;

    bool isWave() const noexcept                    { return getType() == waveItemType(); }
    bool isMidi() const noexcept                    { return getType() == midiItemType(); }
    bool isEdit() const noexcept                    { return getType() == editItemType(); }
    bool isVideo() const noexcept                   { return getType() == videoItemType(); }

    const juce::String& getName() const;
    void setName (const juce::String& name, SetNameMode mode);

    Category getCategory() const;
    void setCategory (Category cat);

    // get/set a persistent string property with a name. Setting to empty removes it
    juce::String getNamedProperty (const juce::String& name) const;
    void setNamedProperty (const juce::String& name, const juce::String& value);

    juce::String getDescription() const;
    void setDescription (const juce::String& newDesc);

    /** optional set of interesting time markers, for wave files */
    juce::Array<double> getMarkedPoints() const;
    void setMarkedPoints (const juce::Array<double>& points);

    /** copies the full description, categories, properties, etc. */
    void copyAllPropertiesFrom (const ProjectItem& other);

    bool convertEditFile();

    double getLength() const;
    void setLength (double length);

    /** name of the project it's inside. */
    juce::String getProjectName() const;

    /** Returns the file that should be used as a preview for this Edit. */
    juce::File getEditPreviewFile() const;

    //==============================================================================
    juce::File getSourceFile();
    juce::String getRawFileName() const       { return file; }
    juce::String getFileName() const;
    void setSourceFile (const juce::File&);
    bool isForFile (const juce::File&);

    /** lets the user rename the file. */
    void renameSourceFile();

    /** Creates a copy of this object and returns the copy. */
    Ptr createCopy();

    void sendChange();

    //==============================================================================
    /** the actual file created may differ, e.g. if an ogg file is chopped up and
        has to become a wav.
    */
    bool copySectionToNewFile (const juce::File& destFile,
                               juce::File& actualFileCreated,
                               double startTime,
                               double length);

    bool deleteSourceFile();

    /** Updates the stored length value in this object. */
    void verifyLength();

    /** used when moving to another project. */
    void changeProjectId (int oldID, int newID);

    //==============================================================================
    void writeToStream (juce::OutputStream&) const;

    /** Returns a list of search strings for this object, by chopping up the
        name and description into words.
    */
    juce::StringArray getSearchTokens() const;

private:
    friend class ProjectManager;
    friend class Project;

    Engine& engine;
    ProjectItemID itemID;
    juce::String type, objectName, description, file;
    double length = 0;
    juce::File sourceFile, newDstFile;

    void timerCallback() override;
    void handleAsyncUpdate() override;

    juce::File getRelativeFile (const juce::String&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectItem)
};

} // namespace tracktion_engine
