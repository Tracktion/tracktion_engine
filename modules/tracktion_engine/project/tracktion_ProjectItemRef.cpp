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

ProjectItemRef::ProjectItemRef (const juce::String& pathOrID)
    : value (pathOrID)
{
}

ProjectItemRef::ProjectItemRef (const juce::String& pathOrID, Project& owner)
    : value (pathOrID), ownerProject (&owner)
{
}

ProjectItemRef::ProjectItemRef (ProjectItemID pid)
    : value (pid.toString())
{
}

ProjectItemRef ProjectItemRef::fromPath (const juce::String& relativePath)
{
    return ProjectItemRef (relativePath);
}

ProjectItemRef ProjectItemRef::fromPath (const juce::String& relativePath, Project& owner)
{
    return ProjectItemRef (relativePath, owner);
}

ProjectItemRef ProjectItemRef::fromAbsolutePath (const juce::File& file)
{
    return ProjectItemRef (file.getFullPathName());
}

//==============================================================================
bool ProjectItemRef::isValid() const noexcept
{
    return value.isNotEmpty();
}

bool ProjectItemRef::isProjectItemID() const noexcept
{
    return ProjectItemID (value).isValid();
}

bool ProjectItemRef::isRelativePath() const noexcept
{
    return value.isNotEmpty()
        && ! ProjectItemID (value).isValid()
        && ! juce::File::isAbsolutePath (value);
}

bool ProjectItemRef::isAbsolutePath() const noexcept
{
    return value.isNotEmpty()
        && ! ProjectItemID (value).isValid()
        && juce::File::isAbsolutePath (value);
}

ProjectID ProjectItemRef::getProjectID() const noexcept
{
    ProjectItemID pid (value);

    if (pid.isValid())
        return pid.getProjectID();

    if (ownerProject)
        return ownerProject->getProjectID();

    return {};
}

std::optional<ProjectItemID> ProjectItemRef::getProjectItemID() const noexcept
{
    ProjectItemID pid (value);
    return pid.isValid() ? std::optional<ProjectItemID> (pid) : std::nullopt;
}

juce::String ProjectItemRef::toIDForFilename() const noexcept
{
    ProjectItemID pid (value);
    return pid.isValid() ? pid.toStringSuitableForFilename()
                         : juce::String::toHexString (value.hash());
}

juce::String ProjectItemRef::toString() const noexcept
{
    return value;
}

Project* ProjectItemRef::getProject() const noexcept
{
    return ownerProject.get();
}

juce::File ProjectItemRef::resolve (Engine& engine, const juce::File& projectFolder) const
{
    if (value.isEmpty())
        return {};

    ProjectItemID pid (value);

    if (pid.isValid())
    {
        if (auto item = engine.getProjectManager().getProjectItem (pid))
            return item->getSourceFile();

        return {};
    }

    if (juce::File::isAbsolutePath (value))
        return { value };

    // Relative path — resolve against provided folder or owner project
    if (projectFolder != juce::File())
        return projectFolder.getChildFile (value);

    if (auto p = ownerProject.get())
        return p->getDefaultDirectory().getChildFile (value);

    jassertfalse;
    return {};
}

//==============================================================================
bool ProjectItemRef::operator== (const ProjectItemRef& other) const    { return value == other.value; }
bool ProjectItemRef::operator!= (const ProjectItemRef& other) const    { return value != other.value; }
bool ProjectItemRef::operator<  (const ProjectItemRef& other) const    { return value <  other.value; }

bool ProjectItemRef::operator== (ProjectItemID pid) const              { return value == pid.toString(); }
bool ProjectItemRef::operator!= (ProjectItemID pid) const              { return value != pid.toString(); }

}} // namespace tracktion { inline namespace engine
