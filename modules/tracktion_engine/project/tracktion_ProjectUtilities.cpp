/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

namespace ProjectUtilities
{

EditReferences getEditsInProject (Project& project)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    EditReferences edits;

    auto& engine = project.engine;
    const auto activeEdits = engine.getActiveEdits().getEdits();
    const auto projectID = project.getProjectID();

    for (auto projectItem : project.getAllProjectItems())
    {
        const auto projectItemRef = projectItem->getProjectItemRef();

        if (projectItemRef.getProjectID() != projectID)
            continue;

        if (! projectItem->isEdit())
            continue;

        bool foundActiveEdit = false;

        for (auto activeEdit : activeEdits)
        {
            if (activeEdit->getProjectItemRef() == projectItem->getProjectItemRef())
            {
                edits.add (activeEdit);
                foundActiveEdit = true;
                break;
            }
        }

        if (foundActiveEdit)
            continue;

        edits.add (loadEditForExamining (engine.getProjectManager(),
                                        projectItem->getProjectItemRef()));
    }

    return edits;
}

int importExternalFiles (Project& proj, juce::Array<ProjectItemRef> refsToImport)
{
    auto& pm = proj.projectManager;
    const auto projectDir = proj.getDefaultDirectory();
    const auto mediaDir = proj.getDirectoryForMedia (ProjectItem::Category::imported);
    int numImported = 0;

    for (auto ref : refsToImport)
    {
        if (auto item = pm.getProjectItem (ref))
        {
            // Only mutate items that actually belong to `proj`. Path-based refs
            // without an owner can match items in any open project, so guard
            // against accidentally rewriting another project's source paths.
            if (item->getProject().get() != &proj)
                continue;

            const auto src = item->getSourceFile();

            if (src.isAChildOf (projectDir))
                continue;

            const auto dst = mediaDir.getNonexistentChildFile (src.getFileNameWithoutExtension(), src.getFileExtension(), false);

            if (src.copyFileTo (dst))
            {
                item->setSourceFile (dst);
                numImported++;
            }
        }
    }

    return numImported;
}

int importExternalReferences (Project& proj, juce::Array<ProjectItemRef> refsToImport)
{
    auto& pm = proj.projectManager;
    juce::Array<ProjectItemRef> newRefs;
    int numImported = 0;

    for (auto ref : refsToImport)
    {
        if (ref.getProjectID() == proj.getProjectID())
            continue;

        if (auto item = pm.getProjectItem (ref))
        {
            if (auto newItem = proj.createNewItem (item->getSourceFile(), item->getType(), item->getName(),
                                                   item->getDescription(), item->getCategory(), true))
            {
                ++numImported;
                newItem->copyAllPropertiesFrom (*item);
                newRefs.add (newItem->getProjectItemRef());
            }
            else
            {
                newRefs.add ({});
            }
        }
        else
        {
            // Path-based ref without owner - resolve file directly
            auto resolvedFile = ref.resolve (pm.engine, proj.getDefaultDirectory());

            if (resolvedFile.existsAsFile())
            {
                if (auto newItem = proj.createNewItem (resolvedFile, ProjectItem::waveItemType(),
                                                       resolvedFile.getFileNameWithoutExtension(),
                                                       {}, ProjectItem::Category::imported, true))
                {
                    ++numImported;
                    newRefs.add (newItem->getProjectItemRef());
                }
                else
                {
                    newRefs.add ({});
                }
            }
            else
            {
                newRefs.add ({});
            }
        }
    }

    jassert (newRefs.size() == refsToImport.size());
    auto edits = getEditsInProject (proj);
    std::set<Edit*> modifiedEdits;

    for (auto edit : edits.getEdits())
    {
        for (auto e : Exportable::addAllExportables (*edit))
        {
            for (auto& refItem : e->getReferencedItems())
            {
                for (int i = refsToImport.size(); --i >= 0;)
                {
                    if (refItem.itemRef == refsToImport[i] && newRefs[i].isValid())
                    {
                        e->reassignReferencedItem (refItem, newRefs[i], refItem.firstTimeUsed);
                        modifiedEdits.insert (edit);
                        break;
                    }
                }
            }
        }
    }

    saveEdits (modifiedEdits);

    return numImported;
}

std::pair<int, juce::String> consolidateEdit (Edit& edit)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto proj = getProjectForEdit (edit);

    if (proj == nullptr)
    {
        jassertfalse;
        return { 0, {} };
    }

    const auto projectDir = proj->getDefaultDirectory();

    juce::Array<ProjectItemRef> allRefs, externalProjectItemRefs;

    for (auto e : Exportable::addAllExportables (edit))
    {
        for (const auto& refItem : e->getReferencedItems())
        {
            auto refProjectID = refItem.itemRef.getProjectID();
            bool isInternal = (refProjectID == proj->getProjectID());

            if (! isInternal && refItem.itemRef.isRelativePath())
            {
                auto resolved = refItem.itemRef.resolve (edit.engine, projectDir);
                isInternal = resolved != juce::File() && resolved.isAChildOf (projectDir);
            }

            if (isInternal)
                allRefs.addIfNotAlreadyThere (refItem.itemRef);
            else
                externalProjectItemRefs.addIfNotAlreadyThere (refItem.itemRef);
        }
    }

    const auto mediaDir = proj->getDirectoryForMedia (ProjectItem::Category::imported);

    const int numProjectItemsImported = importExternalReferences (*proj, externalProjectItemRefs);
    const auto error = [&]
    {
        return (numProjectItemsImported < externalProjectItemRefs.size())
                ? TRANS("This edit referenced some external clips that didn't exist, so couldn't be imported")
                : juce::String();
    }();

    const int numImported = importExternalFiles (*proj, allRefs);

    return { numImported, error };
}

bool canConsolidateEdit (Edit& edit)
{
    auto proj = getProjectForEdit (edit);

    if (proj == nullptr)
    {
        jassertfalse;
        return false;
    }

    juce::Array<ProjectItemRef> allRefs;
    const auto projectDir = proj->getDefaultDirectory();

    for (auto e : Exportable::addAllExportables (edit))
    {
        for (const auto& ref : e->getReferencedItems())
        {
            auto refProjectID = ref.itemRef.getProjectID();
            bool isInternal = (refProjectID == proj->getProjectID());

            if (! isInternal && ref.itemRef.isRelativePath())
            {
                auto resolved = ref.itemRef.resolve (edit.engine, projectDir);
                isInternal = resolved != juce::File() && resolved.isAChildOf (projectDir);
            }

            if (isInternal)
                allRefs.addIfNotAlreadyThere (ref.itemRef);
        }
    }

    auto& pm = edit.engine.getProjectManager();

    for (auto ref : allRefs)
        if (auto item = pm.getProjectItem (ref))
            if (! item->getSourceFile().isAChildOf (projectDir))
                return true;

    return false;
}

std::pair<int, juce::String> consolidateProject (Project& project)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    int numImported = 0;
    auto edits = getEditsInProject (project);

    for (auto edit : edits.getEdits())
    {
        const auto [imported, error] = consolidateEdit (*edit);
        numImported += imported;

        if (error.isNotEmpty())
            return { numImported, error };
    }

    if (edits.getNumFailedLoads() > 0)
        return { numImported, TRANS("Some of the edits in this project couldn't be opened, so their file references couldn't be updated") };

    return { numImported, {} };
}

bool canConsolidateProject (Project& project)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    juce::Array<ProjectItemRef> allRefs;
    auto& pm = project.projectManager;
    const auto projectDir = project.getDefaultDirectory();
    auto edits = getEditsInProject (project);

    for (auto edit : edits.getEdits())
        for (auto e : Exportable::addAllExportables (*edit))
            for (const auto& ref : e->getReferencedItems())
                allRefs.addIfNotAlreadyThere (ref.itemRef);

    for (auto ref : allRefs)
        if (auto item = pm.getProjectItem (ref))
            if (ref.getProjectID() != project.getProjectID()
                || ! item->getSourceFile().isAChildOf (projectDir))
               return true;

    return false;
}

bool isConsolidated (Edit& edit)
{
    return ! canConsolidateEdit (edit);
}

bool isConsolidated (Project& project)
{
    return ! canConsolidateProject (project);
}

void consolidateEditInteractive (Edit& edit, std::function<void()> completionCallback)
{
    edit.engine.getUIBehaviour().showOkCancelAlertBoxAsync (
        TRANS("Consolidate Edit?"),
        TRANS("This will copy any files outside of the Project folder in to it and update the Project items.") + "\n\n"
            + TRANS("N.B. If you have other Edits referencing these project items, they will also be updated to refer to the new copies.") + "\n\n"
            + TRANS("Do you want to proceed?"),
        {}, {},
        [editRef = makeSafeRef (edit), completionCallback] (bool okPressed)
        {
            if (! okPressed)
                return;

            if (auto e = editRef.get())
            {
                const auto [num, error] = consolidateEdit (*e);

                e->engine.getUIBehaviour().showWarningAlert (TRANS("Finished Consolidating"),
                                                              TRANS("XXX external files copied into project.")
                                                                  .replace ("XXX", juce::String (num)));

                if (completionCallback)
                    completionCallback();
            }
        });
}

void consolidateProjectInteractive (Project& project, std::function<void()> completionCallback)
{
    project.engine.getUIBehaviour().showOkCancelAlertBoxAsync (
        TRANS("Consolidate Project?"),
        TRANS("This will copy any files outside of the Project folder in to it and update the Project items.") + "\n\n"
            + TRANS("N.B. If you have other Edits referencing these project items, they will also be updated to refer to the new copies.") + "\n\n"
            + TRANS("Do you want to proceed?"),
        {}, {},
        [projectRef = makeSafeRef (project), completionCallback] (bool okPressed)
        {
            if (! okPressed)
                return;

            if (auto p = projectRef.get())
            {
                const auto [num, error] = consolidateProject (*p);

                if (error.isNotEmpty())
                {
                    p->engine.getUIBehaviour().showWarningAlert (TRANS("Unable to Consolidate"),
                                                                  TRANS("XXX external files copied into project.")
                                                                      .replace ("XXX", juce::String (num))
                                                                      + "\n\n" + error);
                }
                else
                {
                    p->engine.getUIBehaviour().showWarningAlert (TRANS("Finished Consolidating"),
                                                                  TRANS("XXX external files copied into project.")
                                                                      .replace ("XXX", juce::String (num)));
                }

                if (completionCallback)
                    completionCallback();
            }
        });
}

} // namespace ProjectUtilities

} // namespace tracktion::inline engine