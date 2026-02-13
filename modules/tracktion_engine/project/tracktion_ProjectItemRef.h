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

class Project;

//==============================================================================
/**
    A lightweight reference that can hold either a ProjectItemID or a file path.

    The string value is auto-detected:
    - If it parses as a valid ProjectItemID (hex/hex format), it's treated as an ID
    - If it's an absolute file path, it's treated as such
    - Otherwise it's treated as a relative path (resolved against a project folder)
*/
class ProjectItemRef
{
public:
    ProjectItemRef() noexcept = default;

    /** Creates a ref from a string that is either a ProjectItemID or a file path. */
    explicit ProjectItemRef (const juce::String& pathOrID);

    /** Creates a ref from a path string with a known owner project. */
    ProjectItemRef (const juce::String& pathOrID, Project& owner);

    /** Creates a ref from an existing ProjectItemID. */
    ProjectItemRef (ProjectItemID);

    /** Creates a ref from a relative path string. */
    static ProjectItemRef fromPath (const juce::String& relativePath);

    /** Creates a ref from a relative path with a known owner project. */
    static ProjectItemRef fromPath (const juce::String& relativePath, Project& owner);

    /** Creates a ref from an absolute file. */
    static ProjectItemRef fromAbsolutePath (const juce::File&);

    //==============================================================================
    bool isValid() const noexcept;
    bool isProjectItemID() const noexcept;
    bool isRelativePath() const noexcept;
    bool isAbsolutePath() const noexcept;

    /** Returns the ProjectItemID if this ref is ID-based, otherwise invalid. */
    ProjectItemID getProjectItemID() const noexcept;

    /** Returns a string which can be used for filenames.
        This isn't meant for resolution, it will be the ProjectItemID or a hash of path.
    */
    juce::String toIDForFilename() const noexcept;

    /** Returns the raw string value. */
    juce::String toString() const noexcept;

    /** Returns the owner project, if one was provided at construction. */
    Project* getProject() const noexcept;

    /** Resolves to a file.
        - If ProjectItemID: looks up via engine's ProjectManager
        - If relative path: resolves against the given folder, or the owner project's
          default directory if no folder is provided
        - If absolute path: returns directly
    */
    juce::File resolve (Engine&, const juce::File& projectFolder = {}) const;

    //==============================================================================
    bool operator== (const ProjectItemRef&) const;
    bool operator!= (const ProjectItemRef&) const;
    bool operator<  (const ProjectItemRef&) const;

private:
    juce::String value;
    SafeSelectable<Project> ownerProject;
};

}} // namespace tracktion { inline namespace engine
