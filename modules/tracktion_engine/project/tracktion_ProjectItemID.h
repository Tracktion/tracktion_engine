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
    An ID representing one of the items in a Project.

    A ProjectItemID consists of two parts: the ID of the project it belongs to, and an
    ID of the item within that project.
*/
class ProjectItemID
{
public:
    ProjectItemID() noexcept;
    ProjectItemID (const ProjectItemID&) noexcept;
    ProjectItemID operator= (const ProjectItemID&) noexcept;
    ~ProjectItemID() noexcept;

    /** takes a string created by toString(). */
    explicit ProjectItemID (const juce::String& stringID) noexcept;

    /** Creates an ID from the . */
    ProjectItemID (int itemID, int projectID) noexcept;

    /** Generates a new ID for a given project. */
    static ProjectItemID createNewID (int projectID) noexcept;
    static ProjectItemID fromProperty (const juce::ValueTree&, const juce::Identifier&);

    ProjectItemID withNewProjectID (int newProjectID) const;

    //==============================================================================
    // TODO: create strong types for ProjectID and the ItemID

    /** Returns the ID of the project this item belongs to. */
    int getProjectID() const;
    /** Returns the ID of the item within the project. */
    int getItemID() const;

    /** Returns a combined ID as an integer, useful for creating hashes */
    juce::int64 getRawID() const noexcept               { return combinedID; }

    //==============================================================================
    bool isValid() const noexcept                       { return combinedID != 0; }
    bool isInvalid() const noexcept                     { return combinedID == 0; }

    //==============================================================================
    juce::String toString() const;
    juce::String toStringSuitableForFilename() const;

    operator juce::var() const noexcept                 { return toString(); }

    //==============================================================================
    bool operator== (ProjectItemID other) const         { return combinedID == other.combinedID; }
    bool operator!= (ProjectItemID other) const         { return combinedID != other.combinedID; }
    bool operator<  (ProjectItemID other) const         { return combinedID <  other.combinedID; }

private:
    juce::int64 combinedID = 0;

    // (NB: this is disallowed because it's ambiguous whether a string or int64 format is intended)
    ProjectItemID (const juce::var&) = delete;
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::ProjectItemID>
    {
        static tracktion_engine::ProjectItemID fromVar (const var& v)   { return tracktion_engine::ProjectItemID (v.toString()); }
        static var toVar (const tracktion_engine::ProjectItemID& v)     { return v; }
    };
}
