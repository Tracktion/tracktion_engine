/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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

    ProjectItemID pid (sourceDescription);

    if (pid.isValid())
    {
        if (auto projectItem = ProjectManager::getInstance()->getProjectItem (pid))
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
    return getSourceProjectItemID().isValid();
}

ProjectItemID SourceFileReference::getSourceProjectItemID() const
{
    jassert (source.get() == state[source.getPropertyID()].toString());
    return ProjectItemID (source.get());
}

ProjectItem::Ptr SourceFileReference::getSourceProjectItem() const
{
    jassert (source.get() == state[source.getPropertyID()].toString());
    ProjectItemID pid (source.get());

    if (pid.isValid())
        return ProjectManager::getInstance()->getProjectItem (pid);

    return {};
}

void SourceFileReference::setToDirectFileReference (const juce::File& newFile, bool useRelativePath)
{
    source = findPathFromFile (edit, newFile, useRelativePath);
}

void SourceFileReference::setToProjectFileReference (const juce::File& file, bool updateProjectItem)
{
    auto oldFile = getFile();
    auto project = ProjectManager::getInstance()->getProject (edit);

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
                    setToProjectFileReference (existingItem->getID());
                }
                else
                {
                    // no such object in the project, so create one..
                    projectItem = project->createNewItem (file, ProjectItem::waveItemType(),
                                                          file.getFileNameWithoutExtension(),
                                                          {}, ProjectItem::Category::imported,
                                                          false);

                    if (projectItem != nullptr)
                        setToProjectFileReference (projectItem->getID());
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
                setToProjectFileReference (projectItem->getID());
        }
    }
    else
    {
        if (project != nullptr)
            if (auto existingProjectItem = project->getProjectItemForFile (file))
                setToProjectFileReference (existingProjectItem->getID());
    }

    if (getFile() != oldFile)
        edit.restartPlayback();
}

void SourceFileReference::setToProjectFileReference (ProjectItemID newID)
{
    auto oldFile = getFile();
    source = newID.toString();

    if (getFile() != oldFile)
        edit.restartPlayback();
}
