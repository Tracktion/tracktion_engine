/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

/** Utilities for consolidating Projects and Edits. */
namespace ProjectUtilities
{
    /** Holds a mixed list of Edit pointers and owned Edits. */
    struct EditReferences
    {
        EditReferences() = default;

        /** Returns the Edits. */
        const std::vector<Edit*>& getEdits() const
        {
            return edits;
        }

        /** Adds a non-owned Edit. */
        void add (Edit* edit)
        {
            edits.push_back (edit);
        }

        /** Adds an owned Edit. */
        void add (std::unique_ptr<Edit> edit)
        {
            add (edit.get());
            ownedEdits.push_back (std::move (edit));
        }

    private:
        std::vector<Edit*> edits;
        std::vector<std::unique_ptr<Edit>> ownedEdits;
    };

    /** Force saves a container of Edits. */
    template<typename EditContainer>
    void saveEdits (const EditContainer& edits)
    {
        for (auto edit : edits)
            EditFileOperations (*edit).save (false, true, false);
    }

    /** Returns a list of all Edits in a Project.
        If some Edits are already open, this will return those, closed Edits will be opened for
        examining so they can be edited.
    */
    EditReferences getEditsInProject (Project& project);

    /** Looks through the given list of ProjectItemRefs for ProjectItems that have source files
        outside the project folder and copies these in, updating the ProjectItem to reference
        the new source files.
    */
    int importExternalFiles (Project& proj, juce::Array<ProjectItemRef> refsToImport);

    /** Looks through the given list of ProjectItemRefs for ProjectItems that are in a Project
        other than this Project, and creates new ProjectItems for them in this Project.

        Additionally this updates all the Edits in the Project to point at these new ProjectItems.

        N.B. Running this and then importExternalFiles should fully consolidate an Edit.
        Running this for all Edits in a Project should fully consolidate the Project.

        @returns the number of ProjectItems imported.
    */
    int importExternalReferences (Project& proj, juce::Array<ProjectItemRef> refsToImport);

    /** Looks for clips that have source files outside of the project folder, and copies them in,
        updating the ProjectItem references.

        If items have references to other Projects, this will create new Project items for them.
        Additionally, this will update references in other Edits belonging to this project with
        the new Project refs.

        @returns The number of files copied imported and an error message if one occured
    */
    std::pair<int, juce::String> consolidateEdit (Edit& edit);

    /** Returns true if the Edit has any ProjectItem references to files outside the project directory. */
    bool canConsolidateEdit (Edit& edit);

    /** Looks for clips that have source files outside of the project folder, and copies them in,
        updating the ProjectItem references.

        If items have references to other Projects, this will create new Project items for them.
        Additionally, this will update references in all Edits belonging to this project with
        the new Project refs.

        @returns The number of files copied imported and an error message if one occured
    */
    std::pair<int, juce::String> consolidateProject (Project& project);

    /** Returns true if the Project has any ProjectItem references to files outside the project directory. */
    bool canConsolidateProject (Project& project);

    /** Consolidates an Edit showing a confirmation dialog box first and a completion dialog afterwards. */
    void consolidateEditInteractive (Edit& edit, std::function<void()> completionCallback = nullptr);

    /** Consolidates a Project showing a confirmation dialog box first and a completion dialog afterwards. */
    void consolidateProjectInteractive (Project& project, std::function<void()> completionCallback = nullptr);
}

} // namespace tracktion::inline engine