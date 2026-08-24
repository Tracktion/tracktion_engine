/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
struct EditSnapshotList
{
    EditSnapshotList() = default;

    EditSnapshot::Ptr getEditSnapshot (const juce::File& f)
    {
        const juce::ScopedLock sl (getLock());

        for (auto s : snapshots)
            if (s->getFile() == f)
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
    juce::SharedResourcePointer<EditSnapshotList> list;
};

//==============================================================================
EditSnapshot::Ptr EditSnapshot::getEditSnapshot (Engine& engine, juce::File editFile)
{
    if (editFile == juce::File())
        return {};

    juce::SharedResourcePointer<EditSnapshotList> list;
    const juce::ScopedLock sl (list->getLock());

    if (auto snapshot = list->getEditSnapshot (editFile))
        return snapshot;

    return new EditSnapshot (engine, editFile);
}

//==============================================================================
EditSnapshot::EditSnapshot (Engine& e, juce::File f)
    : engine (e),
      listHolder (std::make_unique<ListHolder>()),
      sourceFile (std::move (f))
{
    listHolder->list->addSnapshot (*this);
    refresh();
}

EditSnapshot::~EditSnapshot()
{
    // TODO: Fix this race on destruction as getEditSnapshot may return this at this point
    listHolder->list->removeSnapshot (*this);
}

void EditSnapshot::setState (juce::ValueTree newState, TimeDuration editLength)
{
    if (state.getReferenceCount() == 1)
        state = std::move (newState);
    else
        state = newState.createCopy();

    length = editLength.inSeconds();
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

void EditSnapshot::addSubTracksRecursively (const juce::XmlElement& parent, int& audioTrackNameNumber)
{
    for (auto track : parent.getChildIterator())
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
                trackName = TRANS("Track") + " "  + juce::String (audioTrackNameNumber);

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

void EditSnapshot::refreshFromXml (const juce::XmlElement& xml,
                                   const juce::String& newName, double newLength)
{
    clear();
    name = newName;
    length = newLength;

    // last significant change
    auto changeHexString = xml.getStringAttribute (IDs::lastSignificantChange);
    lastSaveTime = changeHexString.isEmpty() ? sourceFile.getLastModificationTime()
                                             : juce::Time (changeHexString.getHexValue64());

    // marks
    if (auto viewState = xml.getChildByName (IDs::TRANSPORT))
    {
        auto loopRange = juce::Range<double>::between (viewState->getDoubleAttribute (IDs::loopPoint1),
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

    // The length isn't stored in the Edit XML so if no cached length was
    // provided (e.g. there's no ProjectItem), fall back to the last clip end
    if (length == 0.0)
        length = maxClipEnd;
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
void EditSnapshot::refresh()
{
    clear();

    auto pi = engine.getProjectManager().getProjectItem (sourceFile);

    if (pi)
    {
        project = pi->getProject();
        sourceFile = pi->getSourceFile();
    }

    auto newState = loadValueTree (sourceFile, true);

    if (! newState.hasType (IDs::EDIT))
        return;

    name = pi ? pi->getName() : sourceFile.getFileNameWithoutExtension();
    setState (newState, TimeDuration::fromSeconds (pi != nullptr ? pi->getLength() : 0.0));
    refreshFromState();
}

void EditSnapshot::refreshFromState()
{
    auto editName = name;
    auto editLength = length;
    clear();

    if (auto xml = state.createXml())
        refreshFromXml (*xml, editName, editLength);
}

void EditSnapshot::clear()
{
    name = {};
    numTracks = 0;
    numAudioTracks = 0;
    trackNames.clearQuick();
    audioTracks.clear();
    trackIDs.clear();
    editClipRefs.clear();
    clipSourceRefs.clear();
    length = 0.0;
    maxClipEnd = 0.0;
    markIn = 0.0;
    markOut = 0.0;
    tempo = 0.0;

    marksActive = false;
    timeSigNumerator = 4;
    timeSigDenominator = 4;
    pitch = 60;
    markers.clear();
}

void EditSnapshot::addEditClips (const juce::XmlElement& track)
{
    for (auto clip : track.getChildIterator())
    {
        if (clip->hasTagName (IDs::EDITCLIP))
        {
            auto source = clip->getStringAttribute ("source");
            auto file = ProjectItemRef (source).resolve (engine, project ? project->getDefaultDirectory()
                                                                         : sourceFile.getParentDirectory());
            editClipRefs.add (file);
        }
    }
}

void EditSnapshot::addClipSources (const juce::XmlElement& track)
{
    for (auto clip : track.getChildIterator())
    {
        auto source = clip->getStringAttribute ("source");
        auto file = ProjectItemRef (source).resolve (engine, project ? project->getDefaultDirectory()
                                                                     : sourceFile.getParentDirectory());
        clipSourceRefs.add (file);

        maxClipEnd = std::max (maxClipEnd, clip->getDoubleAttribute (IDs::start)
                                            + clip->getDoubleAttribute (IDs::length));
    }
}

void EditSnapshot::addMarkers (const juce::XmlElement& track)
{
    for (auto clip : track.getChildIterator())
    {
        Marker m;
        m.name      = clip->getStringAttribute ("name", TRANS("unnamed"));
        m.colour    = juce::Colour::fromString (clip->getStringAttribute ("colour", TRANS("unnamed")));
        auto start  = TimePosition::fromSeconds (clip->getDoubleAttribute ("start", 0.0));
        auto len    = TimeDuration::fromSeconds (clip->getDoubleAttribute ("length", 0.0));
        m.time      = { start, start + len };

        if (len > 0s)
            markers.add (m);
    }
}


static void addNestedEditObjects (EditSnapshot& baseEdit, juce::ReferenceCountedArray<EditSnapshot>& edits)
{
    if (edits.contains (&baseEdit))
        return;

    edits.add (&baseEdit);

    for (auto editFile : baseEdit.getEditClips())
        if (auto ptr = EditSnapshot::getEditSnapshot (baseEdit.engine, editFile))
            addNestedEditObjects (*ptr, edits);
}

juce::ReferenceCountedArray<EditSnapshot> EditSnapshot::getNestedEditObjects()
{
    CRASH_TRACER
    juce::ReferenceCountedArray<EditSnapshot> result;
    addNestedEditObjects (*this, result);
    return result;
}

} // namespace tracktion::inline engine
