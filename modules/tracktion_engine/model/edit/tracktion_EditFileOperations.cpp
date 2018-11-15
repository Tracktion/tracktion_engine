/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct ThreadedEditFileWriter   : private Thread
{
    ThreadedEditFileWriter()
        : Thread ("TemporyFileWriter") {}

    ~ThreadedEditFileWriter()
    {
        flushAllFiles();
        stopThread (10000);
        jassert (pending.isEmpty());
    }

    void writeTreeToFile (ValueTree&& v, const File& f)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        pending.add (std::pair<ValueTree, File> (v, f));
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

    void writeToFile (std::pair<ValueTree, File> item)
    {
        item.second.deleteFile();
        FileOutputStream os (item.second);
        item.first.writeToStream (os);
    }

    juce::Array<std::pair<ValueTree, File>, CriticalSection> pending;
    WaitableEvent waiter;
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
            if (auto item = ProjectManager::getInstance()->getProjectItem (edit))
                EditFileOperations::getTempVersionOfEditFile (item->getSourceFile()).deleteFile();
        }

        void refresh()
        {
            editSnapshot->refreshFromProjectManager();
        }

        Edit& edit;
        Time timeOfLastSave { Time::getCurrentTime() };
        EditSnapshot::Ptr editSnapshot { EditSnapshot::getEditSnapshot (edit.getProjectItemID()) };
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

    void writeValueTreeToDisk (ValueTree&& v, const File& f)
    {
        editFileWriter->writeTreeToFile (std::move (v), f);
    }

    SharedResourcePointer<SharedEditFileDataCache> cache;
    std::shared_ptr<SharedEditFileDataCache::Data> data;
    SharedResourcePointer<ThreadedEditFileWriter> editFileWriter;
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

File EditFileOperations::getEditFile() const
{
    return getEditFileFromProjectManager (edit);
}

bool EditFileOperations::writeToFile (const File& file, bool writeQuickBinaryVersion)
{
    CRASH_TRACER
    bool ok = false;

    if (! writeQuickBinaryVersion)
    {
        EditPlaybackContext::RealtimePriorityDisabler realtimeDisabler;
        MouseCursor::showWaitCursor();
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

            if (auto xml = std::unique_ptr<XmlElement> (edit.state.createXml()))
                ok = xml->writeToFile (file, {});

            jassert (ok);
        }
    }

    if (! writeQuickBinaryVersion)
        MouseCursor::hideWaitCursor();

    if (ok)
        timeOfLastSave = Time::getCurrentTime();

    return ok;
}

static bool editSaveError (Edit& edit, const File& file, bool warnOfFailure)
{
    // failed..
    TRACKTION_LOG_ERROR ("Can't write to edit file: " + file.getFullPathName());

    if (warnOfFailure)
    {
        String s (TRANS("Unable to save edit \"XEDTX\" to file: XFNX")
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
    const File editFile (getEditFile());

    if (editFile == File())
        return false;

    CustomControlSurface::saveAllSettings();
    auto controllerMappings = state.getOrCreateChildWithName (IDs::CONTROLLERMAPPINGS, nullptr);
    edit.getParameterControlMappings().saveTo (controllerMappings);

    const File tempFile (getTempVersionFile());

    if (! saveTempVersion (true))
        return editSaveError (edit, tempFile, warnOfFailure);

    if (forceSaveEvenIfNotModified || edit.hasChangedSinceSaved())
    {
        // Updates the project list if showing
        if (auto proj = ProjectManager::getInstance()->getProject (edit))
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
    }

    tempFile.deleteFile();

    if (auto item = ProjectManager::getInstance()->getProjectItem (edit))
        item->setLength (edit.getLength());

    edit.resetChangedStatus();

    return true;
}

bool EditFileOperations::saveAs()
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    CRASH_TRACER
    File newEditName = getNonExistentSiblingWithIncrementedNumberSuffix (getEditFile(), false);

    FileChooser chooser (TRANS("Save As") + "...",
                         newEditName,
                         String ("*") + editFileSuffix);

    if (chooser.browseForFileToSave (false))
        return saveAs (chooser.getResult().withFileExtension (editFileSuffix));
   #endif

    return false;
}

bool EditFileOperations::saveAs (const File& f)
{
    if (f == getEditFile())
        return save (true, false, false);

    if (f.existsAsFile())
    {
        if (! edit.engine.getUIBehaviour().showOkCancelAlertBox (TRANS("Save Edit") + "...",
                                                                 TRANS("The file XFNX already exists. Do you want to overwrite it?")
                                                                   .replace ("XFNX", "\n\n" + f.getFullPathName() + "\n\n"),
                                                                 TRANS("Overwrite")))
            return false;
    }

    auto& pm = *ProjectManager::getInstance();

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
                    editSnapshot = EditSnapshot::getEditSnapshot (edit.getProjectItemID());

                    const bool ok = save (true, true, false);

                    if (ok)
                        oldTempFile.deleteFile();

                    edit.sendSourceFileUpdate();
                    return ok;
                }
            }
        }
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

File EditFileOperations::getTempVersionOfEditFile (const File& f)
{
    return f.exists() ? f.getSiblingFile (".tmp_" + f.getFileNameWithoutExtension())
                      : File();
}

File EditFileOperations::getTempVersionFile() const
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
    SharedResourcePointer<SharedEditFileDataCache>()->refresh();
}

//==============================================================================
ValueTree loadEditFromProjectManager (ProjectManager& pm, ProjectItemID itemID)
{
    if (auto item = pm.getProjectItem (itemID))
        return loadEditFromFile (item->getSourceFile(), itemID);

    return {};
}

ValueTree loadEditFromFile (const File& f, ProjectItemID itemID)
{
    CRASH_TRACER
    ValueTree state;

    if (auto xml = std::unique_ptr<XmlElement> (XmlDocument::parse (f)))
    {
        updateLegacyEdit (*xml);
        state = ValueTree::fromXml (*xml);
    }

    if (! state.isValid())
    {
        FileInputStream is (f);

        if (is.openedOk())
            state = updateLegacyEdit (ValueTree::readFromStream (is));
    }

    if (! state.isValid())
    {
        state = ValueTree (IDs::EDIT);
        state.setProperty (IDs::appVersion, Engine::getInstance().getPropertyStorage().getApplicationVersion(), nullptr);
    }

    state.setProperty (IDs::projectID, itemID, nullptr);

    return state;
}

ValueTree createEmptyEdit()
{
    return loadEditFromFile ({}, ProjectItemID::createNewID (0));
}

// TODO: move logic for creating and parsing these filenames into one place - see
// also AudioClipBase::getProxyFileToCreate
static ProjectItemID getProjectItemIDFromFilename (const String& name)
{
    StringArray tokens;
    tokens.addTokens (name.fromFirstOccurrenceOf ("_", false, false), "_", {});

    if (tokens.size() <= 1)
        return {};

    return ProjectItemID (tokens[0] + "_" + tokens[1]);
}

void purgeOrphanEditTempFolders (ProjectManager& pm)
{
    CRASH_TRACER
    Array<File> filesToDelete;
    StringArray resonsForDeletion;

    for (DirectoryIterator i (pm.engine.getTemporaryFileManager().getTempDirectory(), false,
                              "edit_*", File::findDirectories); i.next();)
    {
        auto itemID = getProjectItemIDFromFilename (i.getFile().getFileName());

        if (itemID.isValid())
        {
            if (pm.getProjectItem (itemID) == nullptr)
            {
                filesToDelete.add (i.getFile());

                auto pp = pm.getProject (itemID.getProjectID());

                if (itemID.getProjectID() == 0)
                    resonsForDeletion.add ("Invalid project ID");
                else if (pp == nullptr)
                    resonsForDeletion.add ("Can't find project");
                else if (pp->getProjectItemForID (itemID) == nullptr)
                    resonsForDeletion.add ("Can't find source media");
            }
        }
    }

    for (int i = filesToDelete.size(); --i >= 0;)
    {
        TRACKTION_LOG ("Purging edit folder: " + filesToDelete.getReference (i).getFileName() + " - " + resonsForDeletion[i]);
        filesToDelete.getReference (i).deleteRecursively();
    }
}
