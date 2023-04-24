/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
    bool saveAs (const juce::File&, bool forceOverwriteExisting = false);
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
/** Loads an edit from file, ready for playback / editing */
std::unique_ptr<Edit> loadEditFromFile (Engine&, const juce::File&, Edit::EditRole role = Edit::forEditing);

/** Creates a new edit for a file, ready for playback / editing */
std::unique_ptr<Edit> createEmptyEdit (Engine&, const juce::File&);

/** Uses the ProjectManager to find an Edit file and load it as a ValueTree. */
juce::ValueTree loadEditFromProjectManager (ProjectManager&, ProjectItemID);

/** Legacy, will be deprecated soon. Use version that returns an edit.
    Loads a ValueTree from a file to load an Edit.
    If the file is empty, a new Edit state will be created with the given ProjectItemID.
*/
juce::ValueTree loadEditFromFile (Engine&, const juce::File&, ProjectItemID);

/** Legacy, will be deprecated soon. Use version that returns an edit.
    Creates an empty Edit with no project. */
juce::ValueTree createEmptyEdit (Engine&);

/** Converts old edit formats to the latest structure */
juce::ValueTree updateLegacyEdit (const juce::ValueTree&);

/** Converts old edit formats to the latest structure */
void updateLegacyEdit (juce::XmlElement& editXML);


}} // namespace tracktion { inline namespace engine
