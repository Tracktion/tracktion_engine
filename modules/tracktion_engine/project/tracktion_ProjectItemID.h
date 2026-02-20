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
/** A strongly-typed wrapper for a project's numeric ID.

    Project IDs are opaque identity tokens used only for equality comparisons
    and lookup — they are never used in arithmetic or as array indices.
*/
class ProjectID
{
public:
    ProjectID() noexcept = default;
    explicit ProjectID (int rawID) noexcept : id (rawID) {}

    int toInt() const noexcept              { return id; }

    bool isValid() const noexcept           { return id != 0; }

    bool operator== (ProjectID o) const noexcept    { return id == o.id; }
    bool operator!= (ProjectID o) const noexcept    { return id != o.id; }

private:
    int id = 0;
};

//==============================================================================
/**
    An ID representing one of the items in a Project.

    A ProjectItemID consists of two parts: the ID of the project it belongs to, and an
    ID of the item within that project.
*/
class ProjectItemID
{
public:
    ProjectItemID() noexcept = default;
    ProjectItemID (const ProjectItemID&) noexcept = default;
    ProjectItemID& operator= (const ProjectItemID&) noexcept = default;
    ~ProjectItemID() noexcept = default;

    /** takes a string created by toString(). */
    explicit ProjectItemID (const juce::String& stringID) noexcept;

    /** Creates an ID from the item ID and project ID. */
    ProjectItemID (int itemID, ProjectID projectID) noexcept;

    /** Generates a new ID for a given project. */
    static ProjectItemID createNewID (ProjectID projectID) noexcept;
    static ProjectItemID fromProperty (const juce::ValueTree&, const juce::Identifier&);

    ProjectItemID withNewProjectID (ProjectID newProjectID) const;

    //==============================================================================
    /** Returns the ID of the project this item belongs to. */
    ProjectID getProjectID() const;
    /** Returns the ID of the item within the project. */
    int getItemID() const;

    /** Returns a combined ID as an integer, useful for creating hashes */
    int64_t getRawID() const noexcept                   { return combinedID; }

    //==============================================================================
    bool isValid() const noexcept                       { return combinedID != 0; }
    bool isInvalid() const noexcept                     { return combinedID == 0; }

    //==============================================================================
    juce::String toString() const;
    juce::String toStringSuitableForFilename() const;

    //==============================================================================
    bool operator== (ProjectItemID other) const         { return combinedID == other.combinedID; }
    bool operator!= (ProjectItemID other) const         { return combinedID != other.combinedID; }
    bool operator<  (ProjectItemID other) const         { return combinedID <  other.combinedID; }

private:
    int64_t combinedID = 0;

    // (NB: this is disallowed because it's ambiguous whether a string or int64 format is intended)
    ProjectItemID (const juce::var&) = delete;
};

} // namespace tracktion::inline engine

namespace juce
{
template <>
struct VariantConverter<tracktion::engine::ProjectItemID>
{
    static tracktion::engine::ProjectItemID fromVar (const var& v)   { return tracktion::engine::ProjectItemID (v.toString()); }
    static var toVar (const tracktion::engine::ProjectItemID& v)     { return v.toString(); }
};
}
