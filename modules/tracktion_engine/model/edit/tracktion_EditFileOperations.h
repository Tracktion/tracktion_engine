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
/**
    Contains methods for saving an Edit to a file.
*/
class EditFileOperations
{
public:
    EditFileOperations (Edit&);
    ~EditFileOperations();

    juce::File getEditFile() const;

    bool save (bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges);
    bool saveAs (const juce::File&);
    bool saveAs();

    bool writeToFile (const juce::File&, bool writeQuickBinaryVersion);

    bool saveTempVersion (bool forceSaveEvenIfUnchanged);
    void deleteTempVersion();
    juce::File getTempVersionFile() const;
    static juce::File getTempVersionOfEditFile (const juce::File&);
    static void updateEditFiles();

    juce::Time getTimeOfLastSave() const    { return timeOfLastSave; }

private:
    Edit& edit;
    juce::ValueTree state;

    struct SharedDataPimpl;
    std::unique_ptr<SharedDataPimpl> sharedDataPimpl;

    juce::Time& timeOfLastSave;
    EditSnapshot::Ptr& editSnapshot;
};

//==============================================================================
/** Uses the ProjectManager to find an Edit file and load it as a ValueTree. */
juce::ValueTree loadEditFromProjectManager (ProjectManager&, ProjectItemID);

/** Loads a ValueTree from a file to load an Edit.
    If the file is empty, a new Edit state will be created with the given ProjectItemID.
*/
juce::ValueTree loadEditFromFile (const juce::File&, ProjectItemID);

/** Creates an empty Edit with no project. */
juce::ValueTree createEmptyEdit();

/** Converts old edit formats to the latest structure */
juce::ValueTree updateLegacyEdit (const juce::ValueTree&);

/** Converts old edit formats to the latest structure */
void updateLegacyEdit (juce::XmlElement& editXML);


/** */
void purgeOrphanEditTempFolders (ProjectManager&);


} // namespace tracktion_engine
