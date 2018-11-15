/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static const char* slashEscapeSeq = "[[slash]]";

struct StringMap
{
    StringMap() {}

    StringMap (const StringMap& other)
        : keys (other.keys),
          values (other.values)
    {
    }

    StringMap& operator= (const StringMap& other)
    {
        keys = other.keys;
        values = other.values;
        return *this;
    }

    //==============================================================================
    void set (const String& key, const String& value)
    {
        if (value.isEmpty())
        {
            remove (key);
        }
        else
        {
            const int index = keys.indexOf (key);

            if (index >= 0)
            {
                values.set (index, value);
            }
            else
            {
                keys.add (key);
                values.add (value);
            }
        }
    }

    void remove (const String& key)
    {
        const int index = keys.indexOf (key);

        if (index >= 0)
        {
            keys.remove (index);
            values.remove (index);
        }
    }

    void clear()
    {
        keys.clear();
        values.clear();
    }

    String get (const String& key) const
    {
        return values [keys.indexOf (key)];
    }

    int size() const
    {
        return keys.size();
    }

    String getKeyAt (int index) const
    {
        return keys[index];
    }

    String getValueAt (int index) const
    {
        return values[index];
    }

    void remove (int index)
    {
        keys.remove (index);
        values.remove (index);
    }

    bool containsKey (const String& key) const
    {
        return keys.contains (key);
    }

    bool containsValue (const String& value) const
    {
        return values.contains (value);
    }

    String getKeyForValue (const String& value) const
    {
        return keys [values.indexOf (value)];
    }

    StringArray getKeysForValue (const String& value) const
    {
        StringArray s;

        for (int i = values.size(); --i >= 0;)
            if (values[i] == value)
                s.add (keys[i]);

        return s;
    }

    bool operator== (const StringMap& other) const
    {
        return keys == other.keys && values == other.values;
    }

    //==============================================================================
    String toString() const
    {
        String s;

        for (int i = 0; i < keys.size(); ++i)
        {
            s << keys[i].replace ("|", slashEscapeSeq)
              << '|'
              << values[i].replace ("|", slashEscapeSeq);

            if (i < keys.size() - 1)
                s << '|';
        }

        return s;
    }

    static StringMap recreateFromString (const String& stringVersion)
    {
        StringMap sm;
        auto n = stringVersion.getCharPointer();

        for (;;)
        {
            int len = 0;
            juce_wchar c = 0;

            for (;;)
            {
                c = n[len];

                if (c == '|' || c == 0)
                    break;

                ++len;
            }

            if (c == 0)
                break;

            String key (n, (size_t) len);
            n += len + 1;
            len = 0;

            for (;;)
            {
                c = n[len];
                if (c == '|' || c == 0)
                    break;

                ++len;
            }

            if (key.contains (slashEscapeSeq))
                key = key.replace (slashEscapeSeq, "|");

            String val (n, (size_t) len);

            if (val.contains (slashEscapeSeq))
                val = val.replace (slashEscapeSeq, "|");

            sm.set (key, val);

            if (c == 0)
                break;

            n += len + 1;
        }

        return sm;
    }

    StringArray keys, values;

    JUCE_LEAK_DETECTOR (StringMap)
};


//==============================================================================
ProjectItem::ProjectItem (Engine& e,
                          const String& name_, const String& type_,
                          const String& desc_, const String& file_,
                          ProjectItem::Category category_,
                          double length_, ProjectItemID id)
   : Selectable(),
     ReferenceCountedObject(),
     engine (e),
     itemID (id),
     type (type_),
     objectName (name_),
     description (desc_),
     file (file_),
     length (length_)
{
    setCategory (category_);
}

static String readStringAutoDetectingUTF (InputStream& in)
{
    MemoryOutputStream mo;

    while (char byte = in.readByte())
        mo << byte;

    return mo.toString();
}

ProjectItem::ProjectItem (Engine& e, ProjectItemID id, InputStream* in)
   : engine (e), itemID (id)
{
    objectName  = readStringAutoDetectingUTF (*in);
    type        = readStringAutoDetectingUTF (*in);
    description = readStringAutoDetectingUTF (*in);
    file        = readStringAutoDetectingUTF (*in);
    length      = in->readDouble();
}

ProjectItem::~ProjectItem()
{
    notifyListenersOfDeletion();
}

void ProjectItem::writeToStream (OutputStream& out) const
{
    out.writeString (objectName);
    out.writeString (type);
    out.writeString (description);
    out.writeString (file);
    out.writeDouble (length);
}

void ProjectItem::sendChange()
{
    changed();

    if (auto pp = getProject())
        pp->changed();
}

String ProjectItem::getSelectableDescription()
{
    if (isEdit())               return TRANS ("Edit");
    if (isWave() || isMidi())   return TRANS ("Audio File");
    if (isVideo())              return TRANS ("Video File");

    return TRANS("Project item of type 'XXX'").replace ("XXX", type);
}

void ProjectItem::selectionStatusChanged (bool isNowSelected)
{
    if (isNowSelected && getLength() == 0)
        verifyLength();
}

//==============================================================================
File ProjectItem::getRelativeFile (const String& name) const
{
    if (auto pp = getProject())
    {
        String localName = name;

       #if JUCE_WINDOWS
        if (localName.containsChar ('/'))
        {
            if (localName.startsWithChar ('/'))
                localName = "C:" + localName.replaceCharacter ('/', '\\');
            else
                localName = localName.replaceCharacter ('/', '\\');
        }
       #else
        if (localName.containsChar ('\\'))
            localName = localName.replaceCharacter ('\\', '/');
       #endif

        auto projectDir = pp->getDefaultDirectory();
        return projectDir.getChildFile (localName);
    }

    jassertfalse;
    return {};
}

File ProjectItem::getSourceFile()
{
    if (sourceFile == File())
    {
        auto f = getRelativeFile (file);

        if (f.existsAsFile())
        {
            sourceFile = f;
        }
        else if (engine.getAudioFileFormatManager().canOpen (f))
        {
            // if not found, look for a compressed version to use..
            auto substitute = f.withFileExtension ("flac");

            if (! substitute.existsAsFile())
                substitute = f.withFileExtension ("ogg");

            if (substitute.existsAsFile())
                sourceFile = substitute;
        }
    }

    return sourceFile;
}

bool ProjectItem::isForFile (const File& f)
{
    if (sourceFile != File())
        return f == sourceFile;

    return file.endsWithIgnoreCase (f.getFileName()) && getSourceFile() == f;
}

void ProjectItem::setSourceFile (const File& f)
{
    if (auto pp = getProject())
    {
        auto projectDir = pp->getDefaultDirectory();

        if (f.isAChildOf (projectDir))
            file = f.getRelativePathFrom (projectDir);
        else
            file = f.getFullPathName();

        sourceFile = File();

        changed();
        pp->changed();

        triggerAsyncUpdate();
    }
}

void ProjectItem::handleAsyncUpdate()
{
    if (isEdit())
        EditFileOperations::updateEditFiles();

    for (auto edit : engine.getActiveEdits().getEdits())
        if (edit != nullptr)
            edit->sendSourceFileUpdate();
}

String ProjectItem::getFileName() const
{
    if (auto pp = getProject())
        return pp->getDefaultDirectory().getChildFile (file).getFileName();

    return {};
}

File ProjectItem::getEditPreviewFile() const
{
    jassert (isEdit());

    if (auto p = getProject())
    {
        auto dir = engine.getTemporaryFileManager().getTempDirectory().getChildFile ("previews");
        dir.createDirectory();

        return dir.getChildFile ("preview_" + getID().toStringSuitableForFilename()).withFileExtension ("ogg");
    }

    return {};
}

//==============================================================================
Project::Ptr ProjectItem::getProject() const
{
    return ProjectManager::getInstance()->getProject (getID().getProjectID());
}

bool ProjectItem::hasBeenDeleted() const
{
    if (auto p = getProject())
        return p->getProjectItemForID (getID()) == nullptr;

    return true;
}

const String& ProjectItem::getName() const
{
    return objectName;
}

void ProjectItem::setName (const String& n, SetNameMode mode)
{
    if (objectName != n)
    {
        if (auto pp = getProject())
        {
            objectName = n.substring (0, 256);

            // also rename the file if it is in the project folder
            auto src = getSourceFile();

            bool shouldRename = false;

            auto mrm = (ProjectItem::RenameMode) static_cast<int> (engine.getPropertyStorage().getProperty (SettingID::renameMode, (int) RenameMode::local));

            if (mode == SetNameMode::forceNoRename)     shouldRename = false;
            else if (mode == SetNameMode::forceRename)  shouldRename = true;
            else if (mrm == RenameMode::always)         shouldRename = true;
            else if (mrm == RenameMode::never)          shouldRename = false;
            else if (mrm == RenameMode::local)          shouldRename = src.isAChildOf (pp->getDefaultDirectory());

            if (shouldRename)
            {
                newDstFile = src.getParentDirectory().getChildFile (File::createLegalFileName (n))
                                                     .withFileExtension (src.getFileExtension());

                startTimer (1);
            }

            sendChange();
        }
    }
}

void ProjectItem::timerCallback()
{
    stopTimer();

    auto src = getSourceFile();
    auto dst = getNonExistentSiblingWithIncrementedNumberSuffix (newDstFile, false);

    TransportControl::stopAllTransports (engine, false, true);

    auto& afm = engine.getAudioFileManager();
    afm.releaseFile (AudioFile (src));

    if (! dst.existsAsFile() && src.moveFileTo (dst))
    {
        afm.checkFileForChanges (AudioFile (dst));
        afm.checkFileForChanges (AudioFile (src));

        setSourceFile (dst);

        SelectionManager::refreshAllPropertyPanels();
        engine.getUIBehaviour().editNamesMayHaveChanged();
        sendChange();
    }
}

ProjectItem::Category ProjectItem::getCategory() const
{
    auto cat = (Category) getNamedProperty ("MediaObjectCategory").getIntValue();

    if (cat == Category::none && isEdit())
        return Category::edit;

    return cat;
}

void ProjectItem::setCategory (ProjectItem::Category cat)
{
    setNamedProperty ("MediaObjectCategory", String ((int) cat));
    sendChange();
}

const String& ProjectItem::getType() const
{
    return type;
}

String ProjectItem::getDescription() const
{
    return description.upToFirstOccurrenceOf ("|", false, false);
}

void ProjectItem::setDescription (const String& newDesc)
{
    if (newDesc != getDescription())
    {
        if (description.containsChar ('|'))
            description = newDesc.removeCharacters ("|")
                            + description.fromFirstOccurrenceOf ("|", true, false);
        else
            description = newDesc.removeCharacters ("|");

        sendChange();
    }
}

void ProjectItem::copyAllPropertiesFrom (const ProjectItem& other)
{
    objectName = other.objectName;
    description = other.description;
}

String ProjectItem::getNamedProperty (const String& name) const
{
    if (description.containsChar ('|'))
    {
        const StringMap map (StringMap::recreateFromString (description.fromFirstOccurrenceOf ("|", false, false)));

        return map.get (name);
    }

    return {};
}

void ProjectItem::setNamedProperty (const String& name, const String& value)
{
    if (auto pp = getProject())
    {
        if (! pp->isReadOnly())
        {
            StringMap map;

            if (description.containsChar ('|'))
                map = StringMap::recreateFromString (description.fromFirstOccurrenceOf ("|", false, false));

            if (map.get (name) != value)
            {
                map.set (name, value);
                description = getDescription() + '|' + map.toString();
                sendChange();
            }
        }
    }
}

Array<double> ProjectItem::getMarkedPoints() const
{
    juce::Array<double> marks;
    const String m (getNamedProperty ("marks"));

    if (m.isNotEmpty())
    {
        StringArray toks;
        toks.addTokens (m, true);

        for (auto& t : toks)
            marks.add (t.getDoubleValue());
    }

    return marks;
}

void ProjectItem::setMarkedPoints (const juce::Array<double>& points)
{
    if (auto pp = getProject())
    {
        if (! pp->isReadOnly())
        {
            String m;

            for (auto& p : points)
                m << p << " ";

            setNamedProperty ("marks", m.trim());

            // stop the ui getting chuggy when moving points
            pp->cancelAnyPendingUpdates();
        }
    }
}

bool ProjectItem::convertEditFile()
{
    auto f = getSourceFile();

    if (f.hasFileExtension (editFileSuffix))
        return true;

    auto newFile = f.withFileExtension (editFileSuffix);

    if (newFile.existsAsFile())
    {
        String m (TRANS("There appears to already be a converted Edit in the project folder."));
        m << newLine << TRANS("Do you want to use this, or create a new conversion?");

        if (engine.getUIBehaviour().showOkCancelAlertBox (TRANS("Converted Edit Already Exists"), m,
                                                          TRANS("Use Existing"),
                                                          TRANS("Create New")))
        {
            setSourceFile (newFile);
            return true;
        }

        newFile.copyFileTo (newFile.getNonexistentSibling());
    }

    if (f.existsAsFile() && f != newFile)
    {
        if (! f.copyFileTo (newFile))
        {
            engine.getUIBehaviour().showWarningAlert (TRANS("Unable to Open Edit"),
                                                      TRANS("The selected Edit file could not be converted to the current project format.")
                                                        + newLine + newLine
                                                        + TRANS("Please ensure you can write to the Edit directory and try again."));
            return false;
        }

        setSourceFile (newFile);
    }

    return true;
}

double ProjectItem::getLength() const
{
    return length;
}

void ProjectItem::setLength (double l)
{
    if (std::abs (length - l) > 0.001)
    {
        length = l;
        sendChange();
    }
}

String ProjectItem::getProjectName() const
{
    if (auto p = getProject())
        return p->getName();

    return {};
}

//==============================================================================
void ProjectItem::verifyLength()
{
    double len = 0.0;

    if (isWave())
    {
        len = AudioFile (getSourceFile()).getLength();
    }
    else if (isMidi())
    {
        FileInputStream in (getSourceFile());

        if (in.openedOk())
        {
            MidiFile mf;
            mf.readFrom (in);
            mf.convertTimestampTicksToSeconds();

            len = mf.getLastTimestamp();
        }
    }
    else if (isEdit())
    {
        // don't do this - too slow..
        //Edit tempEdit (this);
        //len = tempEdit.getLength();
    }

    if (len > 0.001 && len != getLength())
        setLength (len);
}

//==============================================================================
bool ProjectItem::copySectionToNewFile (const File& destFile,
                                        File& actualFileCreated,
                                        double startTime, double lengthToCopy)
{
    actualFileCreated = destFile;

    if (isWave())
    {
        actualFileCreated = destFile;
        return AudioFileUtils::copySectionToNewFile (getSourceFile(), actualFileCreated,
                                                     EditTimeRange (startTime, startTime + lengthToCopy)) > 0;
    }

    if (isEdit())
        return getSourceFile().copyFileTo (destFile);

    return false;
}

bool ProjectItem::deleteSourceFile()
{
    bool ok = true;
    auto f = getSourceFile();

    if (f.existsAsFile())
    {
        // extra steps in case it's in use by a strip or something..
        TransportControl::stopAllTransports (engine, false, true);
        auto& afm = engine.getAudioFileManager();

        AudioFile af (f);

        for (int attempts = 3; --attempts >= 0;)
        {
            afm.releaseFile (af);
            ok = f.moveToTrash();

            if (ok)
                break;

            Thread::sleep (800);
        }

        afm.checkFileForChangesAsync (af);

        if (! ok)
        {
            auto info = f.getFullPathName() + "  " + File::descriptionOfSizeInBytes (f.getSize());

            if (! f.hasWriteAccess())
                info << "  (read only)";

            if (f.isDirectory())
                info << "  (directory)";

            info << "  modified: " << f.getLastModificationTime().toString (true, true);

            TRACKTION_LOG_ERROR (info);
        }
    }

    return ok;
}

//==============================================================================
void ProjectItem::changeProjectId (int oldID, int newID)
{
    if (getID().getProjectID() == oldID)
        itemID = ProjectItemID (getID().getItemID(), newID);

    if (isEdit())
    {
        auto& pm = *ProjectManager::getInstance();

        Edit ed (pm.engine,
                 loadEditFromProjectManager (pm, getID()),
                 Edit::forExamining, nullptr, 1);

        for (auto exp : Exportable::addAllExportables (ed))
        {
            int i = 0;

            for (auto& item : exp->getReferencedItems())
            {
                 if (item.itemID == oldID)
                     exp->reassignReferencedItem (item, item.itemID.withNewProjectID (newID), 0.0);

                ++i;
            }
        }

        EditFileOperations (ed).save (false, true, false);
    }
}


//==============================================================================
ProjectItem::Ptr ProjectItem::createCopy()
{
    if (auto pp = getProject())
    {
        // create a new unique name
        auto newName = getName().trim();

        if (newName.fromLastOccurrenceOf ("(", false, false).containsOnly ("0123456789()")
             && newName.retainCharacters ("0123456789()").length() > 0)
        {
            while (newName.length() > 1
                    && String ("0123456789()").containsChar (newName.getLastCharacter()))
                newName = newName.dropLastCharacters (1);
        }

        if (! newName.startsWithIgnoreCase (TRANS("Copy of ")))
            newName = TRANS("Copy of ") + newName;

        auto nameStem = newName;
        int index = 2;

        for (;;)
        {
            bool alreadyThere = false;

            for (int i = pp->getNumMediaItems(); --i >= 0;)
            {
                if (pp->getProjectItemAt (i)->getName().equalsIgnoreCase (newName))
                {
                    alreadyThere = true;
                    break;
                }
            }

            if (alreadyThere)
                newName = nameStem + "(" + String (index++) + ")";
            else
                break;
        }

        auto source = getSourceFile();
        auto fileExtension = source.getFileExtension();

        if (fileExtension.isEmpty())
            if (auto af = engine.getAudioFileFormatManager().getFormatFromFileName (source))
                fileExtension = af->getFileExtensions()[0];

        auto newFile = source.getParentDirectory()
                             .getNonexistentChildFile (source.getFileNameWithoutExtension(),
                                                       fileExtension, true);

        if (pp->isReadOnly())
        {
            engine.getUIBehaviour().showWarningMessage (TRANS("Can't create a new item because this project is read-only"));
        }
        else if (source.copyFileTo (newFile))
        {
            return pp->createNewItem (newFile, getType(), newName, description, getCategory(), true);
        }
    }

    return {};
}

//==============================================================================
static void tokenise (const String& s, StringArray& toks)
{
    auto t = s.getCharPointer();
    auto start = t;

    while (! t.isEmpty())
    {
        if (! (t.isLetterOrDigit()))
        {
            if (t > start + 1)
                toks.add (String (start, t));

            start = t;
        }

        ++t;
    }

    if (t.getAddress() > start.getAddress() + 1)
        toks.add (String (start, t));
}

StringArray ProjectItem::getSearchTokens() const
{
    StringArray toks;
    tokenise (objectName, toks);
    tokenise (getDescription(), toks);
    return toks;
}
