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

//==============================================================================
Project::Project (Engine& e, ProjectManager& pm, const juce::File& fileOrFolder)
   : engine (e), projectManager (pm)
{
    if (fileOrFolder.isDirectory())
        impl = std::make_unique<FolderBasedProject> (*this, fileOrFolder);
    else
        impl = std::make_unique<FileBasedProject> (*this, fileOrFolder);

    for (auto* p : pm.openProjects)
    {
        juce::ignoreUnused (p);
        jassert (p->getProjectFile() != fileOrFolder);
    }

    pm.openProjects.add (this);

    impl->lockFile();
    impl->load();
}

Project::~Project()
{
    projectManager.openProjects.removeFirstMatchingValue (this);
    save();
    notifyListenersOfDeletion();
}

//==============================================================================
void Project::projectChanged()
{
    triggerAsyncUpdate();
    Selectable::changed();
}

void Project::changed()
{
    impl->changed();
}

void Project::handleAsyncUpdate()
{
    save();
}

void Project::setTemporary (bool t)
{
    impl->temporary = t;
}

//==============================================================================
bool Project::save()
{
    auto result = impl->save();
    cancelPendingUpdate();
    return result;
}

bool Project::isValid() const                                               { return impl->isValid(); }
bool Project::isReadOnly() const                                            { return impl->isReadOnly(); }
bool Project::isTemporary() const                                           { return impl->isTemporary(); }
int Project::getProjectID() const                                           { return impl->getProjectID(); }
juce::String Project::getName() const                                       { return impl->getName(); }
juce::String Project::getDescription() const                                { return impl->getDescription(); }
const juce::File& Project::getProjectFile() const noexcept                  { return impl->getProjectFile(); }
juce::File Project::getDefaultDirectory() const                             { return impl->getDefaultDirectory(); }
juce::File Project::getDirectoryForMedia (ProjectItem::Category c) const    { return impl->getDirectoryForMedia (c); }

void Project::setName (const juce::String& n)                               { impl->setName (n); }
void Project::setDescription (const juce::String& d)                        { impl->setDescription (d); }
void Project::createNewProjectId()                                          { impl->createNewProjectId(); }

juce::String Project::getProjectProperty (const juce::String& n) const      { return impl->getProjectProperty (n); }
void Project::setProjectProperty (const juce::String& n, const juce::String& v) { impl->setProjectProperty (n, v); }

void Project::refreshProjectPropertiesFromFile()                            { impl->refreshProjectPropertiesFromFile(); }
bool Project::isLibraryProject() const                                      { return impl->isLibraryProject(); }

bool Project::askAboutTempoDetect (const juce::File& f, bool& s) const      { return impl->askAboutTempoDetect (f, s); }

juce::Array<ProjectItemID> Project::findOrphanItems()                       { return impl->findOrphanItems(); }

//==============================================================================
int Project::getNumProjectItems()                                           { return impl->getNumProjectItems(); }
ProjectItemID Project::getProjectItemID (int i)                             { return impl->getProjectItemID (i); }
juce::Array<ProjectItemID> Project::getAllProjectItemIDs() const            { return impl->getAllProjectItemIDs(); }
juce::Array<int> Project::getAllItemIDs() const                             { return impl->getAllItemIDs(); }
ProjectItem::Ptr Project::getProjectItemAt (int i)                          { return impl->getProjectItemAt (i); }
juce::Array<ProjectItem::Ptr> Project::getAllProjectItems()                 { return impl->getAllProjectItems(); }
int Project::getIndexOf (ProjectItemID id) const                            { return impl->getIndexOf (id); }

ProjectItem::Ptr Project::getProjectItemForID (ProjectItemID id)            { return impl->getProjectItemForID (id); }
ProjectItem::Ptr Project::getProjectItemForFile (const juce::File& f)       { return impl->getProjectItemForFile (f); }

//==============================================================================
ProjectItem::Ptr Project::createNewItem (const juce::File& f, const juce::String& type,
                                         const juce::String& name, const juce::String& desc,
                                         const ProjectItem::Category cat, bool atTop)
{
    return impl->createNewItem (f, type, name, desc, cat, atTop);
}

bool Project::removeProjectItem (ProjectItemID id, bool del)                { return impl->removeProjectItem (id, del); }
void Project::moveProjectItem (int from, int to)                            { impl->moveProjectItem (from, to); }
ProjectItem::Ptr Project::createNewEdit()                                   { return impl->createNewEdit(); }
void Project::redirectIDsFromProject (int oldId, int newId)                 { impl->redirectIDsFromProject (oldId, newId); }

//==============================================================================
void Project::mergeArchiveContents (const juce::File& f)                    { impl->mergeArchiveContents (f); }
void Project::mergeOtherProjectIntoThis (const juce::File& f)               { impl->mergeOtherProjectIntoThis (f); }
void Project::refreshFolderStructure()                                      { impl->refreshFolderStructure(); }
void Project::createDefaultFolders()                                        { impl->createDefaultFolders(); }

//==============================================================================
void Project::searchFor (juce::Array<ProjectItemID>& r, SearchOperation& s) { impl->searchFor (r, s); }

//==============================================================================
juce::String Project::getSelectableDescription()                            { return impl->getSelectableDescription(); }

void Project::lockFile()                                                    { impl->lockFile(); }
void Project::unlockFile()                                                  { impl->unlockFile(); }

}} // namespace tracktion { inline namespace engine
