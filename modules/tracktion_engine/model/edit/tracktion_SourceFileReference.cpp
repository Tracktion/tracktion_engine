/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

SourceFileReference::SourceFileReference (Edit& e, juce::ValueTree& v, const juce::Identifier& prop)
    : edit (e), source (v, prop, &e.getUndoManager()), state (v)
{
    ignoreUnused (state);
}

SourceFileReference::~SourceFileReference()
{
}

juce::String SourceFileReference::findPathFromFile (Edit& edit, const juce::File& newFile, bool useRelativePath)
{
    if (useRelativePath)
    {
        auto editFile = edit.editFileRetriever ? edit.editFileRetriever()
                                               : getEditFileFromProjectManager (edit);

        if (editFile != juce::File())
        {
            // Files inside the project folder are always relative
            if (auto proj = getProjectForEdit (edit))
            {
                auto projectDir = proj->getProjectFile().getParentDirectory();

                if (newFile.isAChildOf (projectDir))
                    return newFile.getRelativePathFrom (editFile);
            }

            // For external files, use relative only if the path isn't too deep
            auto relativePath = newFile.getRelativePathFrom (editFile);

            int dotDotCount = 0;

            for (auto part : juce::StringArray::fromTokens (relativePath, "/\\", ""))
            {
                if (part == "..")
                    dotDotCount++;
                else
                    break;
            }

            if (dotDotCount <= 2)
                return relativePath;
        }
    }

    return newFile.getFullPathName();
}

juce::File SourceFileReference::findFileFromString (Edit& edit, const juce::String& sourceDescription)
{
    if (sourceDescription.isEmpty())
        return {};

    ProjectItemRef ref (sourceDescription);

    if (ref.isProjectItemID())
    {
        if (auto projectItem = edit.engine.getProjectManager().getProjectItem (ref))
            return projectItem->getSourceFile();

        return {};
    }

    if (edit.filePathResolver)
        return edit.filePathResolver (sourceDescription);

    return getEditFileFromProjectManager (edit).getChildFile (sourceDescription);
}

juce::File SourceFileReference::getFile() const
{
    jassert (source.get() == state[source.getPropertyID()].toString());
    return findFileFromString (edit, source.get());
}

bool SourceFileReference::isUsingProjectReference() const
{
    return getSourceProjectItemRef().isProjectItemID();
}

ProjectItemRef SourceFileReference::getSourceProjectItemRef() const
{
    jassert (source.get() == state[source.getPropertyID()].toString());
    return ProjectItemRef (source.get());
}

ProjectItem::Ptr SourceFileReference::getSourceProjectItem() const
{
    jassert (source.get() == state[source.getPropertyID()].toString());
    ProjectItemRef ref (source.get());

    if (ref.isProjectItemID())
        return edit.engine.getProjectManager().getProjectItem (ref);

    return {};
}

void SourceFileReference::setToDirectFileReference (const juce::File& newFile, bool useRelativePath)
{
    source = findPathFromFile (edit, newFile, useRelativePath);
}

void SourceFileReference::setToProjectFileReference (const juce::File& file, bool updateProjectItem)
{
    auto oldFile = getFile();
    auto project = getProjectForEdit (edit);

    if (updateProjectItem)
    {
        if (auto projectItem = getSourceProjectItem())
        {
            // if we've got a proper source ProjectItem but its file is missing, reassign the ProjectItem..
            if (! projectItem->getSourceFile().existsAsFile())
            {
                projectItem->setSourceFile (file);
            }
            else if (project != nullptr)
            {
                // see if there's another one that has this new file..
                if (auto existingItem = project->getProjectItemForFile (file))
                {
                    // point at the existing ProjectItem for this file
                    setToProjectFileReference (existingItem->getProjectItemRef());
                }
                else
                {
                    // no such object in the project, so create one..
                    projectItem = project->createNewItem (file, ProjectItem::waveItemType(),
                                                          file.getFileNameWithoutExtension(),
                                                          {}, ProjectItem::Category::imported,
                                                          false);

                    if (projectItem != nullptr)
                        setToProjectFileReference (projectItem->getProjectItemRef());
                }
            }
        }
        else if (project != nullptr)
        {
            // if we haven't got a legit ProjectItem, create one..
            projectItem = project->createNewItem (file, ProjectItem::waveItemType(),
                                                  file.getFileNameWithoutExtension(),
                                                  {},
                                                  ProjectItem::Category::imported,
                                                  false);

            if (projectItem != nullptr)
                setToProjectFileReference (projectItem->getProjectItemRef());
        }
    }
    else
    {
        if (project != nullptr)
            if (auto existingProjectItem = project->getProjectItemForFile (file))
                setToProjectFileReference (existingProjectItem->getProjectItemRef());
    }

    if (getFile() != oldFile)
        edit.restartPlayback();
}

void SourceFileReference::setToProjectFileReference (ProjectItemRef newID)
{
    auto oldFile = getFile();
    source = newID.toString();

    if (getFile() != oldFile)
        edit.restartPlayback();
}

} // namespace tracktion::inline engine
