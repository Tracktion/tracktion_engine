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
    void set (const juce::String& key, const juce::String& value)
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

    void remove (const juce::String& key)
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

    juce::String get (const juce::String& key) const
    {
        return values [keys.indexOf (key)];
    }

    int size() const
    {
        return keys.size();
    }

    juce::String getKeyAt (int index) const
    {
        return keys[index];
    }

    juce::String getValueAt (int index) const
    {
        return values[index];
    }

    void remove (int index)
    {
        keys.remove (index);
        values.remove (index);
    }

    bool containsKey (const juce::String& key) const
    {
        return keys.contains (key);
    }

    bool containsValue (const juce::String& value) const
    {
        return values.contains (value);
    }

    juce::String getKeyForValue (const juce::String& value) const
    {
        return keys [values.indexOf (value)];
    }

    juce::StringArray getKeysForValue (const juce::String& value) const
    {
        juce::StringArray s;

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
    juce::String toString() const
    {
        juce::String s;

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

    static StringMap recreateFromString (const juce::String& stringVersion)
    {
        StringMap sm;
        auto n = stringVersion.getCharPointer();

        for (;;)
        {
            int len = 0;
            juce::juce_wchar c = 0;

            for (;;)
            {
                c = n[len];

                if (c == '|' || c == 0)
                    break;

                ++len;
            }

            if (c == 0)
                break;

            juce::String key (n, (size_t) len);
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

            juce::String val (n, (size_t) len);

            if (val.contains (slashEscapeSeq))
                val = val.replace (slashEscapeSeq, "|");

            sm.set (key, val);

            if (c == 0)
                break;

            n += len + 1;
        }

        return sm;
    }

    juce::StringArray keys, values;

    JUCE_LEAK_DETECTOR (StringMap)
};


//==============================================================================
ProjectItem::ProjectItem (Engine& e,
                          const juce::String& name_, const juce::String& type_,
                          const juce::String& desc_, const juce::String& file_,
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

static juce::String readStringAutoDetectingUTF (juce::InputStream& in)
{
    juce::MemoryOutputStream mo;

    while (char byte = in.readByte())
        mo << byte;

    return mo.toString();
}

ProjectItem::ProjectItem (Engine& e, ProjectItemID id, juce::InputStream* in)
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

void ProjectItem::writeToStream (juce::OutputStream& out) const
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

juce::String ProjectItem::getSelectableDescription()
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
juce::File ProjectItem::getRelativeFile (const juce::String& name) const
{
    if (auto pp = getProject())
    {
        auto localName = name;

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

juce::File ProjectItem::getSourceFile()
{
    if (sourceFile == juce::File())
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

bool ProjectItem::isForFile (const juce::File& f)
{
    if (sourceFile != juce::File())
        return f == sourceFile;

    return file.endsWithIgnoreCase (f.getFileName()) && getSourceFile() == f;
}

void ProjectItem::setSourceFile (const juce::File& f)
{
    if (auto pp = getProject())
    {
        auto projectDir = pp->getDefaultDirectory();

        if (f.isAChildOf (projectDir))
            file = f.getRelativePathFrom (projectDir);
        else
            file = f.getFullPathName();

        sourceFile = juce::File();

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

juce::String ProjectItem::getFileName() const
{
    if (auto pp = getProject())
        return pp->getDefaultDirectory().getChildFile (file).getFileName();

    return {};
}

juce::File ProjectItem::getEditPreviewFile() const
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

juce::File ProjectItem::getEditThumbnailFile() const
{
    jassert (isEdit());

    if (auto p = getProject())
    {
        auto dir = engine.getTemporaryFileManager().getTempDirectory().getChildFile ("previews");
        dir.createDirectory();

        return dir.getChildFile ("preview_" + getID().toStringSuitableForFilename()).withFileExtension ("png");
    }

    return {};
}

//==============================================================================
Project::Ptr ProjectItem::getProject() const
{
    return engine.getProjectManager().getProject (getID().getProjectID());
}

bool ProjectItem::hasBeenDeleted() const
{
    if (auto p = getProject())
        return p->getProjectItemForID (getID()) == nullptr;

    return true;
}

const juce::String& ProjectItem::getName() const
{
    return objectName;
}

void ProjectItem::setName (const juce::String& n, SetNameMode mode)
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
                newDstFile = src.getParentDirectory().getChildFile (juce::File::createLegalFileName (n))
                                                     .withFileExtension (src.getFileExtension());

                startTimer (1);
            }
            else
            {
                engine.getUIBehaviour().editNamesMayHaveChanged();
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
    afm.releaseFile (AudioFile (engine, src));

    if (! dst.existsAsFile() && src.moveFileTo (dst))
    {
        afm.checkFileForChanges (AudioFile (engine, dst));
        afm.checkFileForChanges (AudioFile (engine, src));

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
    setNamedProperty ("MediaObjectCategory", juce::String ((int) cat));
    sendChange();
}

const juce::String& ProjectItem::getType() const
{
    return type;
}

juce::String ProjectItem::getDescription() const
{
    return description.upToFirstOccurrenceOf ("|", false, false);
}

void ProjectItem::setDescription (const juce::String& newDesc)
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

juce::String ProjectItem::getNamedProperty (const juce::String& name) const
{
    if (description.containsChar ('|'))
    {
        const StringMap map (StringMap::recreateFromString (description.fromFirstOccurrenceOf ("|", false, false)));

        return map.get (name);
    }

    return {};
}

void ProjectItem::setNamedProperty (const juce::String& name, const juce::String& value)
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

juce::Array<TimePosition> ProjectItem::getMarkedPoints() const
{
    juce::Array<TimePosition> marks;
    auto m = getNamedProperty ("marks");

    if (m.isNotEmpty())
    {
        juce::StringArray toks;
        toks.addTokens (m, true);

        for (auto& t : toks)
            marks.add (TimePosition::fromSeconds (t.getDoubleValue()));
    }

    return marks;
}

void ProjectItem::setMarkedPoints (const juce::Array<TimePosition>& points)
{
    if (auto pp = getProject())
    {
        if (! pp->isReadOnly())
        {
            juce::String m;

            for (auto& p : points)
                m << p.inSeconds() << " ";

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
        juce::String m (TRANS("There appears to already be a converted Edit in the project folder."));
        m << juce::newLine << TRANS("Do you want to use this, or create a new conversion?");

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
                                                        + juce::newLine + juce::newLine
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

juce::String ProjectItem::getProjectName() const
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
        len = AudioFile (engine, getSourceFile()).getLength();
    }
    else if (isMidi())
    {
        juce::FileInputStream in (getSourceFile());

        if (in.openedOk())
        {
            juce::MidiFile mf;
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
bool ProjectItem::copySectionToNewFile (const juce::File& destFile,
                                        juce::File& actualFileCreated,
                                        double startTime, double lengthToCopy)
{
    actualFileCreated = destFile;

    if (isWave())
    {
        actualFileCreated = destFile;
        return AudioFileUtils::copySectionToNewFile (engine,
                                                     getSourceFile(), actualFileCreated,
                                                     { TimePosition::fromSeconds (startTime), TimeDuration::fromSeconds (lengthToCopy) }) > 0;
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

        AudioFile af (engine, f);

        for (int attempts = 3; --attempts >= 0;)
        {
            afm.releaseFile (af);
            ok = f.moveToTrash();

            if (ok)
                break;

            juce::Thread::sleep (800);
        }

        afm.checkFileForChangesAsync (af);

        if (! ok)
        {
            auto info = f.getFullPathName() + "  "
                         + juce::File::descriptionOfSizeInBytes (f.getSize());

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
        auto& pm = engine.getProjectManager();

        Edit ed (engine,
                 loadEditFromProjectManager (pm, getID()),
                 Edit::forExamining, nullptr, 1);

        for (auto exp : Exportable::addAllExportables (ed))
        {
            int i = 0;

            for (auto& item : exp->getReferencedItems())
            {
                 if (item.itemID.getProjectID() == oldID)
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
                    && juce::String ("0123456789()").containsChar (newName.getLastCharacter()))
                newName = newName.dropLastCharacters (1);
        }

        if (! newName.endsWithIgnoreCase (TRANS("Copy")))
            newName = newName + " - " + TRANS("Copy");

        auto nameStem = newName;
        int index = 2;

        for (;;)
        {
            bool alreadyThere = false;

            for (int i = pp->getNumProjectItems(); --i >= 0;)
            {
                if (pp->getProjectItemAt (i)->getName().equalsIgnoreCase (newName))
                {
                    alreadyThere = true;
                    break;
                }
            }

            if (alreadyThere)
                newName = nameStem + "(" + juce::String (index++) + ")";
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
static void tokenise (const juce::String& s, juce::StringArray& toks)
{
    auto t = s.getCharPointer();
    auto start = t;

    while (! t.isEmpty())
    {
        if (! (t.isLetterOrDigit()))
        {
            if (t > start + 1)
                toks.add (juce::String (start, t));

            start = t;
        }

        ++t;
    }

    if (t.getAddress() > start.getAddress() + 1)
        toks.add (juce::String (start, t));
}

juce::StringArray ProjectItem::getSearchTokens() const
{
    juce::StringArray toks;
    tokenise (objectName, toks);
    tokenise (getDescription(), toks);
    return toks;
}

}} // namespace tracktion { inline namespace engine
