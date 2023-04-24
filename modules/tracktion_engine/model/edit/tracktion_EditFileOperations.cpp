/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

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
            if (auto item = edit.engine.getProjectManager().getProjectItem (edit))
                EditFileOperations::getTempVersionOfEditFile (item->getSourceFile()).deleteFile();
        }

        void refresh()
        {
            editSnapshot->refreshFromProjectManager();
        }

        Edit& edit;
        juce::Time timeOfLastSave { juce::Time::getCurrentTime() };
        EditSnapshot::Ptr editSnapshot { EditSnapshot::getEditSnapshot (edit.engine, edit.getProjectItemID()) };
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

static bool editSaveError (Edit& edit, const juce::File& file, bool warnOfFailure)
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

        return edit.engine.getUIBehaviour().showOkCancelAlertBox (TRANS("Save edit"), s,
                                                                  TRANS("Carry on anyway"));
    }

    return false;
}

bool EditFileOperations::save (bool warnOfFailure,
                               bool forceSaveEvenIfNotModified,
                               bool offerToDiscardChanges)
{
    CRASH_TRACER
    auto editFile = getEditFile();

    if (editFile == juce::File())
        return false;

    CustomControlSurface::saveAllSettings (edit.engine);
    auto controllerMappings = state.getOrCreateChildWithName (IDs::CONTROLLERMAPPINGS, nullptr);
    edit.getParameterControlMappings().saveTo (controllerMappings);

    auto tempFile = getTempVersionFile();

    if (! saveTempVersion (true))
        return editSaveError (edit, tempFile, warnOfFailure);

    if (forceSaveEvenIfNotModified || edit.hasChangedSinceSaved())
    {
        // Updates the project list if showing
        if (auto proj = edit.engine.getProjectManager().getProject (edit))
            proj->Selectable::changed();

        if (offerToDiscardChanges)
        {
            const int r = edit.engine.getUIBehaviour().showYesNoCancelAlertBox (TRANS("Closing Edit"),
                                                                                TRANS("Do you want to save your changes to \"XNMX\" before closing it?")
                                                                                  .replace ("XNMX", edit.getName()),
                                                                                TRANS("Save"),
                                                                                TRANS("Discard changes"));

            if (r != 1)
            {
                tempFile.deleteFile();
                return r == 2;
            }
        }

        if (editSnapshot != nullptr)
            editSnapshot->refreshCacheAndNotifyListeners();

        if (! tempFile.moveFileTo (editFile))
            return editSaveError (edit, editFile, warnOfFailure);

        edit.engine.getEngineBehaviour().editHasBeenSaved (edit, editFile);
    }

    tempFile.deleteFile();

    if (auto item = edit.engine.getProjectManager().getProjectItem (edit))
        item->setLength (edit.getLength().inSeconds());

    edit.resetChangedStatus();
    return true;
}

bool EditFileOperations::saveAs()
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    CRASH_TRACER
    auto newEditName = getNonExistentSiblingWithIncrementedNumberSuffix (getEditFile(), false);

    juce::FileChooser chooser (TRANS("Save As") + "...",
                               newEditName,
                               juce::String ("*") + editFileSuffix);

    if (chooser.browseForFileToSave (false))
        return saveAs (chooser.getResult().withFileExtension (editFileSuffix));
   #endif

    return false;
}

bool EditFileOperations::saveAs (const juce::File& f, bool forceOverwriteExisting)
{
    if (f == getEditFile())
        return save (true, false, false);

    if (f.existsAsFile() && ! forceOverwriteExisting)
    {
        if (! edit.engine.getUIBehaviour().showOkCancelAlertBox (TRANS("Save Edit") + "...",
                                                                 TRANS("The file XFNX already exists. Do you want to overwrite it?")
                                                                   .replace ("XFNX", "\n\n" + f.getFullPathName() + "\n\n"),
                                                                 TRANS("Overwrite")))
            return false;
    }

    auto& pm = edit.engine.getProjectManager();

    if (auto project = pm.getProject (edit))
    {
        if (auto item = pm.getProjectItem (edit))
        {
            if (f.create())
            {
                if (auto newItem = project->createNewItem (f, item->getType(),
                                                           f.getFileNameWithoutExtension(),
                                                           item->getDescription(),
                                                           item->getCategory(),
                                                           true))
                {
                    auto oldTempFile = getTempVersionFile();

                    newItem->copyAllPropertiesFrom (*item);
                    newItem->setName (f.getFileNameWithoutExtension(), ProjectItem::SetNameMode::forceNoRename);

                    jassert (edit.getProjectItemID() != newItem->getID());
                    edit.setProjectItemID (newItem->getID());
                    editSnapshot = EditSnapshot::getEditSnapshot (edit.engine, edit.getProjectItemID());

                    const bool ok = save (true, true, false);

                    if (ok)
                        oldTempFile.deleteFile();

                    edit.sendSourceFileUpdate();
                    return ok;
                }
            }
        }
    }
    else
    {
        CRASH_TRACER

        CustomControlSurface::saveAllSettings (edit.engine);
        auto controllerMappings = state.getOrCreateChildWithName (IDs::CONTROLLERMAPPINGS, nullptr);
        edit.getParameterControlMappings().saveTo (controllerMappings);

        auto tempFile = getTempVersionFile();

        if (! saveTempVersion (true))
            return editSaveError (edit, tempFile, true);

        if (editSnapshot != nullptr)
            editSnapshot->refreshCacheAndNotifyListeners();

        if (f.existsAsFile())
            f.deleteFile();

        if (! tempFile.moveFileTo (f))
            return editSaveError (edit, f, true);

        tempFile.deleteFile();

        edit.resetChangedStatus();
        edit.engine.getEngineBehaviour().editHasBeenSaved (edit, f);

        return true;
    }

    jassertfalse;
    return false;
}

bool EditFileOperations::saveTempVersion (bool forceSaveEvenIfUnchanged)
{
    CRASH_TRACER

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
juce::ValueTree loadEditFromProjectManager (ProjectManager& pm, ProjectItemID itemID)
{
    if (auto item = pm.getProjectItem (itemID))
        return loadEditFromFile (pm.engine, item->getSourceFile(), itemID);

    return {};
}

juce::ValueTree loadEditFromFile (Engine& e, const juce::File& f, ProjectItemID itemID)
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

std::unique_ptr<Edit> loadEditFromFile (Engine& engine, const juce::File& editFile, Edit::EditRole role)
{
    auto editState = loadEditFromFile (engine, editFile, ProjectItemID{});
    auto id = ProjectItemID::fromProperty (editState, IDs::projectID);
    
    if (! id.isValid())
        id = ProjectItemID::createNewID (0);

    Edit::Options options =
    {
        engine,
        editState,
        id,

        role,
        nullptr,
        Edit::getDefaultNumUndoLevels(),

        [editFile] { return editFile; },
        {}
    };

    return std::make_unique<Edit> (options);
}

std::unique_ptr<Edit> createEmptyEdit (Engine& engine, const juce::File& editFile)
{
    auto id = ProjectItemID::createNewID (0);
    Edit::Options options =
    {
        engine,
        loadEditFromFile (engine, {}, id),
        id,
        
        Edit::forEditing,
        nullptr,
        Edit::getDefaultNumUndoLevels(),
        
        [editFile] { return editFile; },
        {}
    };
    
    return std::make_unique<Edit> (options);
}

juce::ValueTree createEmptyEdit (Engine& e)
{
    return loadEditFromFile (e, {}, ProjectItemID::createNewID (0));
}

}} // namespace tracktion { inline namespace engine
