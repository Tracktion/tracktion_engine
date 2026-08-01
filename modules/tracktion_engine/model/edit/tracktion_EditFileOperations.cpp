/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

struct ThreadedEditFileWriter   : private juce::Thread
{
    ThreadedEditFileWriter()
        : Thread ("TemporyFileWriter") {}

    ~ThreadedEditFileWriter() override
    {
        flushAllFiles();
        stopThread (10000);
        jassert (pending.isEmpty());
    }

    void writeTreeToFile (juce::ValueTree&& v, const juce::File& f)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        pending.add (std::pair<juce::ValueTree, juce::File> (v, f));
        waiter.signal();
        startThread();
    }

    void flushAllFiles()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        waiter.signal();
        startThread();

        while (! pending.isEmpty())
            Thread::sleep (50);
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            while (! pending.isEmpty())
                writeToFile (pending.removeAndReturn (0));

            waiter.wait (1000);
        }
    }

    void writeToFile (std::pair<juce::ValueTree, juce::File> item)
    {
        item.second.deleteFile();
        juce::FileOutputStream os (item.second);
        item.first.writeToStream (os);
    }

    juce::Array<std::pair<juce::ValueTree, juce::File>, juce::CriticalSection> pending;
    juce::WaitableEvent waiter;
};

//==============================================================================
struct SharedEditFileDataCache
{
    struct Data
    {
        Data (Edit& e)
            : edit (e)
        {
            jassert (Selectable::isSelectableValid (&edit));
        }

        ~Data()
        {
            jassert (Selectable::isSelectableValid (&edit));

            // If we managed to shutdown cleanly (i.e. without crashing) then delete the temp file
            if (auto item = getProjectItemForEdit (edit))
                EditFileOperations::getTempVersionOfEditFile (item->getSourceFile()).deleteFile();
        }

        void refresh()
        {
            editSnapshot->refresh();
        }

        Edit& edit;
        juce::Time timeOfLastSave { juce::Time::getCurrentTime() };
        EditSnapshot::Ptr editSnapshot { EditSnapshot::getEditSnapshot (edit.engine, edit.editFileRetriever()) };
    };

    SharedEditFileDataCache() = default;

    std::shared_ptr<Data> get (Edit& edit)
    {
        for (auto& ptr : sharedData)
            if (&ptr->edit == &edit)
                return ptr;

        auto newData = std::make_shared<Data> (edit);
        sharedData.push_back (newData);
        return newData;
    }

    void refresh()
    {
        for (auto& ptr : sharedData)
            ptr->refresh();
    }

    void cleanUp()
    {
        sharedData.erase (std::remove_if (sharedData.begin(), sharedData.end(),
                                          [] (auto& ptr) { return ptr.use_count() == 1; }),
                          sharedData.end());
    }

private:
    std::vector<std::shared_ptr<Data>> sharedData;
};


//==============================================================================
struct EditFileOperations::SharedDataPimpl
{
    SharedDataPimpl (Edit& e)
        : data (cache->get (e))
    {
        jassert (data);
    }

    ~SharedDataPimpl()
    {
        data.reset(); // Make sure this is reset before calling cleanUp
        cache->cleanUp();
    }

    void writeValueTreeToDisk (juce::ValueTree&& v, const juce::File& f)
    {
        editFileWriter->writeTreeToFile (std::move (v), f);
    }

    juce::SharedResourcePointer<SharedEditFileDataCache> cache;
    std::shared_ptr<SharedEditFileDataCache::Data> data;
    juce::SharedResourcePointer<ThreadedEditFileWriter> editFileWriter;
};


//==============================================================================
EditFileOperations::EditFileOperations (Edit& e)
    : edit (e), state (edit.state),
      sharedDataPimpl (new SharedDataPimpl (e)),
      timeOfLastSave (sharedDataPimpl->data->timeOfLastSave),
      editSnapshot (sharedDataPimpl->data->editSnapshot)
{
}

EditFileOperations::~EditFileOperations()
{
}

juce::File EditFileOperations::getEditFile() const
{
    return edit.editFileRetriever();
}

bool EditFileOperations::writeToFile (const juce::File& file, bool writeQuickBinaryVersion)
{
    CRASH_TRACER
    bool ok = false;
    std::unique_ptr<ScopedWaitCursor> waitCursor;

    if (! writeQuickBinaryVersion)
    {
        EditPlaybackContext::RealtimePriorityDisabler realtimeDisabler (edit.engine);
        waitCursor = std::make_unique<ScopedWaitCursor>();
        sharedDataPimpl->editFileWriter->flushAllFiles();
    }

    if (file.hasWriteAccess() && ! file.isDirectory())
    {
        if (writeQuickBinaryVersion)
        {
            sharedDataPimpl->writeValueTreeToDisk (edit.state.createCopy(), file);
        }
        else
        {
            edit.flushState();

            if (editSnapshot != nullptr)
                editSnapshot->setState (edit.state, edit.getLength());

            if (auto xml = edit.state.createXml())
                ok = xml->writeTo (file);

            jassert (ok);
        }
    }

    if (ok)
        timeOfLastSave = juce::Time::getCurrentTime();

    return ok;
}

static void editSaveError (Edit& edit, const juce::File& file, bool warnOfFailure,
                           std::function<void (bool)> callback)
{
    // failed..
    TRACKTION_LOG_ERROR ("Can't write to edit file: " + file.getFullPathName());

    if (warnOfFailure)
    {
        juce::String s (TRANS("Unable to save edit \"XEDTX\" to file: XFNX")
                         .replace ("XEDTX", edit.getName())
                         .replace ("XFNX", file.getFullPathName()));

        if (! file.hasWriteAccess())
            s << "\n\n(" << TRANS("File or directory is read-only") << ")";

        edit.engine.getUIBehaviour().showOkCancelAlertBoxAsync (TRANS("Save edit"), s,
                                                                TRANS("Carry on anyway"),
                                                                TRANS("Cancel"),
                                                                [callback] (bool okPressed)
                                                                {
                                                                    if (callback)
                                                                        callback (okPressed);
                                                                });
        return;
    }

    if (callback)
        callback (false);
}

void EditFileOperations::save (bool warnOfFailure,
                               bool forceSaveEvenIfNotModified,
                               bool offerToDiscardChanges,
                               std::function<void (bool)> callback)
{
    CRASH_TRACER
    auto editFile = getEditFile();

    if (editFile == juce::File())
    {
        if (callback)
            callback (false);

        return;
    }

    // Something is part-way through an operation that has temporarily changed the
    // Edit and will restore it (see Edit::SaveInhibitor), so writing now would
    // persist state the user never asked for
    if (edit.isSaveInhibited())
    {
        if (callback)
            callback (false);

        return;
    }

    CustomControlSurface::saveAllSettings (edit.engine);
    edit.getParameterControlMappings().saveToEdit();

    auto tempFile = getTempVersionFile();

    if (! saveTempVersion (true))
    {
        editSaveError (edit, tempFile, warnOfFailure, callback);
        return;
    }

    if (forceSaveEvenIfNotModified || edit.hasChangedSinceSaved())
    {
        // Updates the project list if showing
        if (auto proj = getProjectForEdit (edit))
            proj->Selectable::changed();

        if (offerToDiscardChanges)
        {
            auto editRef = makeSafeRef (edit);

            edit.engine.getUIBehaviour().showYesNoCancelAlertBoxAsync (TRANS("Closing Edit"),
                TRANS("Do you want to save your changes to \"XNMX\" before closing it?")
                  .replace ("XNMX", edit.getName()),
                TRANS("Save"),
                TRANS("Discard changes"),
                TRANS("Cancel"),
                [editRef, editFile, tempFile, warnOfFailure, callback] (int r)
                {
                    if (r != 1)
                    {
                        tempFile.deleteFile();

                        if (callback)
                            callback (r == 2);

                        return;
                    }

                    if (auto ed = editRef.get())
                    {
                        EditFileOperations ops (*ed);

                        if (ops.editSnapshot != nullptr)
                            ops.editSnapshot->refreshCacheAndNotifyListeners();

                        if (! tempFile.moveFileTo (editFile))
                        {
                            editSaveError (*ed, editFile, warnOfFailure, callback);
                            return;
                        }

                        ed->engine.getEngineBehaviour().editHasBeenSaved (*ed, editFile);

                        tempFile.deleteFile();

                        if (auto item = getProjectItemForEdit (*ed))
                            item->setLength (ed->getLength().inSeconds());

                        ed->resetChangedStatus();

                        if (callback)
                            callback (true);
                    }
                    else
                    {
                        if (callback)
                            callback (false);
                    }
                });

            return;
        }

        if (editSnapshot != nullptr)
            editSnapshot->refreshCacheAndNotifyListeners();

        if (! tempFile.moveFileTo (editFile))
        {
            editSaveError (edit, editFile, warnOfFailure, callback);
            return;
        }

        edit.engine.getEngineBehaviour().editHasBeenSaved (edit, editFile);
    }

    tempFile.deleteFile();

    if (auto item = getProjectItemForEdit (edit))
        item->setLength (edit.getLength().inSeconds());

    edit.resetChangedStatus();

    if (callback)
        callback (true);
}

void EditFileOperations::saveAs()
{
    CRASH_TRACER
    auto newEditName = getNonExistentSiblingWithIncrementedNumberSuffix (getEditFile(), false);

    auto fc = std::make_shared<juce::FileChooser> (TRANS("Save As") + "...",
                                                    newEditName,
                                                    juce::String ("*") + editFileSuffix);

    fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                     [fc, editRef = makeSafeRef (edit)] (const juce::FileChooser&)
                     {
                         if (fc->getResult() != juce::File())
                             if (auto ed = editRef.get())
                                 EditFileOperations (*ed).saveAs (fc->getResult().withFileExtension (editFileSuffix));
                     });
}

void EditFileOperations::saveAs (const juce::File& f, bool forceOverwriteExisting,
                                  std::function<void (bool)> callback)
{
    if (f == getEditFile())
    {
        save (true, false, false, callback);
        return;
    }

    auto doSaveAs = [f, callback, editRef = makeSafeRef (edit)]
    {
        if (auto ed = editRef.get())
        {
            if (auto project = getProjectForEdit (*ed))
            {
                if (auto item = getProjectItemForEdit (*ed))
                {
                    CRASH_TRACER

                    if (f.create())
                    {
                        if (auto newItem = project->createNewItem (f, item->getType(),
                                                                f.getFileNameWithoutExtension(),
                                                                item->getDescription(),
                                                                item->getCategory(),
                                                                true))
                        {
                            EditFileOperations ops (*ed);
                            auto oldTempFile = ops.getTempVersionFile();

                            newItem->copyAllPropertiesFrom (*item);
                            newItem->setName (f.getFileNameWithoutExtension(), ProjectItem::SetNameMode::forceNoRename);

                            jassert (ed->getProjectItemRef() != newItem->getProjectItemRef());
                            ed->setProjectItemRef (newItem->getProjectItemRef());
                            ops.editSnapshot = EditSnapshot::getEditSnapshot (ed->engine, newItem->getSourceFile());

                            ops.save (true, true, false,
                                    [ed, oldTempFile, callback] (bool ok)
                                    {
                                        if (ok)
                                            oldTempFile.deleteFile();

                                        ed->sendSourceFileUpdate();

                                        if (callback)
                                            callback (ok);
                                    });

                            return;
                        }
                    }
                }
            }
            else
            {
                CRASH_TRACER

                CustomControlSurface::saveAllSettings (ed->engine);
                ed->getParameterControlMappings().saveToEdit();

                EditFileOperations ops (*ed);
                auto tempFile = ops.getTempVersionFile();

                if (tempFile == juce::File())
                {
                    tempFile = getTempVersionOfEditFile (f);

                    if (! ops.writeToFile (tempFile, false))
                    {
                        editSaveError (*ed, tempFile, true, callback);
                        return;
                    }
                }
                else
                {
                    if (! ops.saveTempVersion (true))
                    {
                        editSaveError (*ed, tempFile, true, callback);
                        return;
                    }
                }

                if (ops.editSnapshot != nullptr)
                    ops.editSnapshot->refreshCacheAndNotifyListeners();

                if (f.existsAsFile())
                    f.deleteFile();

                if (! tempFile.moveFileTo (f))
                {
                    editSaveError (*ed, f, true, callback);
                    return;
                }

                tempFile.deleteFile();

                ed->resetChangedStatus();
                ed->engine.getEngineBehaviour().editHasBeenSaved (*ed, f);

                if (callback)
                    callback (true);

                return;
            }
        }

        if (callback)
            callback (false);
    };

    if (f.existsAsFile() && ! forceOverwriteExisting)
    {
        edit.engine.getUIBehaviour().showOkCancelAlertBoxAsync (TRANS("Save Edit") + "...",
                                                                TRANS("The file XFNX already exists. Do you want to overwrite it?")
                                                                  .replace ("XFNX", "\n\n" + f.getFullPathName() + "\n\n"),
                                                                TRANS("Overwrite"),
                                                                TRANS("Cancel"),
                                                                [doSaveAs, callback] (bool okPressed)
                                                                {
                                                                    if (okPressed)
                                                                        doSaveAs();
                                                                    else if (callback)
                                                                        callback (false);
                                                                });
        return;
    }

    doSaveAs();
}

bool EditFileOperations::saveTempVersion (bool forceSaveEvenIfUnchanged)
{
    CRASH_TRACER

    // As in save(): nothing should be written whilst the Edit is temporarily
    // modified. Not an error - there just isn't anything safe to write yet.
    if (edit.isSaveInhibited())
        return true;

    if (! (forceSaveEvenIfUnchanged || edit.hasChangedSinceSaved()))
        return true;

    return writeToFile (getTempVersionFile(), ! forceSaveEvenIfUnchanged);
}

juce::File EditFileOperations::getTempVersionOfEditFile (const juce::File& f)
{
    return f != juce::File() ? f.getSiblingFile (".tmp_" + f.getFileNameWithoutExtension())
                             : juce::File();
}

juce::File EditFileOperations::getTempVersionFile() const
{
    return getTempVersionOfEditFile (getEditFile());
}

void EditFileOperations::deleteTempVersion()
{
    getTempVersionFile().deleteFile();
}

//==============================================================================
void EditFileOperations::updateEditFiles()
{
    juce::SharedResourcePointer<SharedEditFileDataCache>()->refresh();
}

//==============================================================================
juce::ValueTree loadEditFromProjectManager (ProjectManager& pm, ProjectItemRef itemID)
{
    if (auto item = pm.getProjectItem (itemID))
        return loadEditFromFile (pm.engine, item->getSourceFile(), itemID);

    return {};
}

std::unique_ptr<Edit> loadEditForExamining (ProjectManager& pm, ProjectItemRef ref, Edit::EditRole role, Edit::LoadContext* loadContext)
{
    if (auto pid = ref.getProjectItemID())
    {
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
        JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4996)
        return Edit::createEditForExamining (pm.engine, loadEditFromProjectManager (pm, ref), role, loadContext);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        JUCE_END_IGNORE_WARNINGS_MSVC
    }

    if (auto p = ref.getProject())
    {
        assert (p->isFolderBased());
        auto editFile = ref.resolve (p->engine);
        auto edit = loadEditFromFile (p->engine, editFile, role);

        if (edit)
            edit->setProjectItemRef (ref);

        return edit;
    }

    // Finally try and get the Edit from the project manager
    if (auto item = pm.getProjectItem (ref))
    {
        if (auto p = item->getProject())
        {
            auto editFile = ref.resolve (p->engine, p->getDefaultDirectory());
            auto edit = loadEditFromFile (p->engine, editFile, role, loadContext);

            if (edit)
                edit->setProjectItemRef (ref);

            return edit;
        }
    }


    return {};
}

juce::ValueTree loadEditFromFile (Engine& e, const juce::File& f, ProjectItemRef itemID)
{
    CRASH_TRACER
    juce::ValueTree state;

    if (auto xml = juce::parseXML (f))
    {
        updateLegacyEdit (*xml);
        state = juce::ValueTree::fromXml (*xml);
    }

    if (! state.isValid())
    {
        if (juce::FileInputStream is (f); is.openedOk())
        {
            if (state = juce::ValueTree::readFromStream (is); state.hasType (IDs::EDIT))
                state = updateLegacyEdit (state);
            else
                state = {};
        }
    }

    if (! state.isValid())
    {
        // If the file already exists and is not empty, don't write over it as it could have been corrupted and be recoverable
        if (f.existsAsFile() && f.getSize() > 0)
            return {};

        state = juce::ValueTree (IDs::EDIT);
        state.setProperty (IDs::appVersion, e.getPropertyStorage().getApplicationVersion(), nullptr);
    }

    state.setProperty (IDs::projectID, itemID.toString(), nullptr);

    return state;
}

juce::ValueTree createEmptyEdit (Engine& e)
{
    return loadEditFromFile (e, {}, ProjectItemID::createNewID (ProjectID{}));
}

static std::unique_ptr<Edit> createEdit (Engine& engine, const juce::ValueTree& editState,
                                         Edit::EditFileRetriever editFileRetriever, Edit::EditRole role,
                                         Edit::LoadContext* loadContext)
{
    if (! editState.isValid())
        return {};

    auto id = ProjectItemID::fromProperty (editState, IDs::projectID);

    if (! id.isValid())
        id = ProjectItemID::createNewID (ProjectID{});

    return Edit::createEdit (Edit::Options
                             {
                                 engine,
                                 editState,
                                 id,
                                 role,
                                 loadContext,
                                 Edit::getDefaultNumUndoLevels(),
                                 std::move (editFileRetriever),
                                 {}
                             });
}

std::unique_ptr<Edit> loadEditFromFile (Engine& engine, const juce::File& editFile, Edit::EditRole role,
                                        Edit::LoadContext* loadContext)
{
    auto editState = loadEditFromFile (engine, editFile, ProjectItemID{});
    return createEdit (engine, editState, [editFile] { return editFile; }, role, loadContext);
}

std::unique_ptr<Edit> loadEditFromState (Engine& engine, const juce::ValueTree& editState, Edit::EditRole role,
                                         Edit::LoadContext* loadContext)
{
    return createEdit (engine, editState, {}, role, loadContext);
}

std::unique_ptr<Edit> createEmptyEdit (Engine& engine, const juce::File& editFile)
{
    auto id = ProjectItemID::createNewID (ProjectID{});

    return Edit::createEdit (Edit::Options
                             {
                                 engine,
                                 loadEditFromFile (engine, {}, id),
                                 id,
                                 Edit::forEditing,
                                 nullptr,
                                 Edit::getDefaultNumUndoLevels(),
                                 [editFile] { return editFile; },
                                 {}
                             });
}

} // namespace tracktion::inline engine
