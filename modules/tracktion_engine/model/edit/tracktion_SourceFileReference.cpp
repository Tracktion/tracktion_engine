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
        jassert (edit.editFileRetriever && edit.editFileRetriever().existsAsFile());
        jassert (edit.filePathResolver);

        if (edit.editFileRetriever && edit.filePathResolver)
            return newFile.getRelativePathFrom (edit.editFileRetriever());

        return newFile.getRelativePathFrom (getEditFileFromProjectManager (edit));
    }

    return newFile.getFullPathName();
}

juce::File SourceFileReference::findFileFromString (Edit& edit, const juce::String& sourceDescription)
{
    if (sourceDescription.isEmpty())
        return {};

    ProjectItemRef ref (sourceDescription);

    if (ref.isValid())
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
    return getSourceProjectItemRef().isValid();
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

    if (ref.isValid())
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

}} // namespace tracktion { inline namespace engine
