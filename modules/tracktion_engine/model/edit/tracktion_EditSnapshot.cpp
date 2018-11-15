/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

//==============================================================================
struct EditSnapshotList
{
    EditSnapshotList() = default;

    EditSnapshot::Ptr getEditSnapshot (ProjectItemID itemID)
    {
        const juce::ScopedLock sl (getLock());

        for (auto s : snapshots)
            if (s->getID() == itemID)
                return s;

        return {};
    }

    void addSnapshot (EditSnapshot& snapshot)
    {
        snapshots.addIfNotAlreadyThere (&snapshot);
    }

    void removeSnapshot (EditSnapshot& snapshot)
    {
        snapshots.removeAllInstancesOf (&snapshot);
    }

    const juce::CriticalSection& getLock()
    {
        return snapshots.getLock();
    }

private:
    juce::Array<EditSnapshot*, juce::CriticalSection> snapshots;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSnapshotList)
};

//==============================================================================
struct EditSnapshot::ListHolder
{
    SharedResourcePointer<EditSnapshotList> list;
};

//==============================================================================
EditSnapshot::Ptr EditSnapshot::getEditSnapshot (ProjectItemID itemID)
{
    SharedResourcePointer<EditSnapshotList> list;
    const ScopedLock sl (list->getLock());

    if (itemID.isInvalid())
        return {};

    if (auto snapshot = list->getEditSnapshot (itemID))
        return snapshot;

    return new EditSnapshot (itemID);
}

//==============================================================================
EditSnapshot::EditSnapshot (ProjectItemID pid)
    : listHolder (std::make_unique<ListHolder>()),
      itemID (pid)
{
    listHolder->list->addSnapshot (*this);
    refreshFromProjectManager();
}

EditSnapshot::~EditSnapshot()
{
    // TODO: Fix this race on destruction as getEditSnapshot may return this at this point
    listHolder->list->removeSnapshot (*this);
}

void EditSnapshot::setState (ValueTree newState, double editLength)
{
    if (state.getReferenceCount() == 1)
        state = std::move (newState);
    else
        state = newState.createCopy();

    length = editLength;
}

bool EditSnapshot::isValid() const
{
    return state.hasType (IDs::EDIT);
}

void EditSnapshot::refreshCacheAndNotifyListeners()
{
    refreshFromState();
    listeners.call (&Listener::editChanged, *this);
}

void EditSnapshot::addSubTracksRecursively (const XmlElement& parent, int& audioTrackNameNumber)
{
    forEachXmlChildElement (parent, track)
    {
        juce::Identifier trackType (track->getTagName());

        if (! TrackList::isTrack (trackType))
            continue;

        auto trackName = track->getStringAttribute (IDs::name);
        trackIDs.add (EditItemID::fromXML (*track, IDs::id));

        mutedTracks.setBit (numTracks, track->getBoolAttribute (IDs::mute, false));
        soloedTracks.setBit (numTracks, track->getBoolAttribute (IDs::solo, false));
        soloIsolatedTracks.setBit (numTracks, track->getBoolAttribute (IDs::soloIsolate, false));

        if (trackType == IDs::TRACK || trackType == IDs::MARKERTRACK)
        {
            if (trackName.isEmpty())
                trackName = TRANS("Track") + " "  + String (audioTrackNameNumber);

            if (trackType == IDs::TRACK)
            {
                audioTracks.setBit (numTracks);
                addEditClips (*track);
                addClipSources (*track);
                ++audioTrackNameNumber;
            }
            else if (trackType == IDs::MARKERTRACK)
            {
                addMarkers (*track);
            }
        }

        trackNames.add (trackName);
        ++numTracks;

        addSubTracksRecursively (*track, audioTrackNameNumber);
    }
}

void EditSnapshot::refreshFromXml (const XmlElement& xml, const juce::String& newName, double newLength)
{
    clear();
    name = newName;
    length = newLength;

    // last significant change
    auto changeHexString = xml.getStringAttribute (IDs::lastSignificantChange);
    lastSaveTime = changeHexString.isEmpty() ? sourceFile.getLastModificationTime()
                                             : Time (changeHexString.getHexValue64());

    // marks
    if (auto viewState = xml.getChildByName (IDs::TRANSPORT))
    {
        auto loopRange = Range<double>::between (viewState->getDoubleAttribute (IDs::loopPoint1),
                                                 viewState->getDoubleAttribute (IDs::loopPoint2));
        markIn = loopRange.getStart();
        markOut = loopRange.getEnd();

        marksActive = markIn != markOut;
    }

    // tempo, time sig & pitch
    if (auto tempoSeq = xml.getChildByName (IDs::TEMPOSEQUENCE))
    {
        if (auto tempoItem = tempoSeq->getChildByName (IDs::TEMPO))
            tempo = tempoItem->getDoubleAttribute (IDs::bpm);

        if (auto timeSigItem = tempoSeq->getChildByName (IDs::TIMESIG))
        {
            timeSigNumerator    = timeSigItem->getIntAttribute (IDs::numerator);
            timeSigDenominator  = timeSigItem->getIntAttribute (IDs::denominator);
        }
    }

    if (auto pitchSeq = xml.getChildByName (IDs::PITCHSEQUENCE))
        if (auto pitchItem = pitchSeq->getChildByName (IDs::PITCH))
            pitch = pitchItem->getIntAttribute (IDs::pitch);

    // tracks
    trackNames.ensureStorageAllocated (xml.getNumChildElements());

    int audioTrackNameNumber = 1;
    addSubTracksRecursively (xml, audioTrackNameNumber);
    numAudioTracks = audioTracks.countNumberOfSetBits();
}

int EditSnapshot::audioToGlobalTrackIndex (int audioIndex) const
{
    int audioTrackIndex = 0;

    for (int i = 0; i < numTracks; ++i)
        if (isAudioTrack (i))
            if (audioTrackIndex++ == audioIndex)
                return i;

    return -1;
}

//==============================================================================
void EditSnapshot::refreshFromProjectManager()
{
    auto pi = ProjectManager::getInstance()->getProjectItem (itemID);
    jassert (pi != nullptr);
    refreshFromProjectItem (pi);
}

void EditSnapshot::refreshFromProjectItem (ProjectItem::Ptr pi)
{
    clear();

    if (pi == nullptr)
        return;

    sourceFile = pi->getSourceFile();
    auto newState = loadValueTree (sourceFile, true);

    if (! newState.hasType (IDs::EDIT))
        return;

    name = pi->getName();
    setState (newState, pi->getLength());
    refreshFromState();
}

void EditSnapshot::refreshFromState()
{
    auto editName = name;
    auto editLength = length;
    clear();

    if (auto xml = std::unique_ptr<XmlElement> (state.createXml()))
        refreshFromXml (*xml, editName, editLength);
}

void EditSnapshot::clear()
{
    name = String();
    numTracks = 0;
    numAudioTracks = 0;
    trackNames.clearQuick();
    audioTracks.clear();
    trackIDs.clear();
    editClipIDs.clear();
    clipSourceIDs.clear();
    length = 0.0;
    markIn = 0.0;
    markOut = 0.0;
    tempo = 0.0;

    marksActive = false;
    timeSigNumerator = 4;
    timeSigDenominator = 4;
    pitch = 60;
    markers.clear();
}

void EditSnapshot::addEditClips (const XmlElement& track)
{
    forEachXmlChildElement (track, clip)
        if (clip->hasTagName (IDs::EDITCLIP))
            editClipIDs.add (ProjectItemID (clip->getStringAttribute ("source")));
}

void EditSnapshot::addClipSources (const XmlElement& track)
{
    forEachXmlChildElement (track, clip)
    {
        auto sourceID = clip->getStringAttribute ("source");

        if (sourceID.isNotEmpty())
            clipSourceIDs.add (ProjectItemID (sourceID));
    }
}

void EditSnapshot::addMarkers (const XmlElement& track)
{
    forEachXmlChildElement (track, clip)
    {
        Marker m;
        m.name      = clip->getStringAttribute ("name", TRANS("unnamed"));
        m.colour    = Colour::fromString (clip->getStringAttribute ("colour", TRANS("unnamed")));
        auto start  = clip->getDoubleAttribute ("start", 0.0);
        auto len    = clip->getDoubleAttribute ("length", 0.0);
        m.time      = { start, start + len };

        if (len > 0.0)
            markers.add (m);
    }
}


static void addNestedEditObjects (EditSnapshot& baseEdit, juce::ReferenceCountedArray<EditSnapshot>& edits)
{
    edits.addIfNotAlreadyThere (&baseEdit);

    for (auto itemID : baseEdit.getEditClips())
        if (auto ptr = EditSnapshot::getEditSnapshot (itemID))
            addNestedEditObjects (*ptr, edits);
}

juce::ReferenceCountedArray<EditSnapshot> EditSnapshot::getNestedEditObjects()
{
    CRASH_TRACER
    juce::ReferenceCountedArray<EditSnapshot> result;
    addNestedEditObjects (*this, result);
    return result;
}
