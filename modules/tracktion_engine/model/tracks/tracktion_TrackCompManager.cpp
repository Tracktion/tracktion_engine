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

TrackCompManager::CompSection::CompSection (const juce::ValueTree& v) : state (v)
{
    updateTrack();
    updateEnd();
}

TrackCompManager::CompSection::~CompSection()
{
}

TrackCompManager::CompSection* TrackCompManager::CompSection::createAndIncRefCount (const juce::ValueTree& v)
{
    auto cs = new CompSection (v);
    cs->incReferenceCount();
    return cs;
}

void TrackCompManager::CompSection::updateFrom (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::track)        updateTrack();
        else if (i == IDs::end)     updateEnd();
    }
    else
    {
        jassertfalse;
    }
}

void TrackCompManager::CompSection::updateTrack()      { track = EditItemID::fromProperty (state, IDs::track); }
void TrackCompManager::CompSection::updateEnd()        { end = static_cast<double> (state[IDs::end]); }

//==============================================================================
TrackCompManager::TrackComp* TrackCompManager::TrackComp::createAndIncRefCount (Edit& edit, const juce::ValueTree& v)
{
    auto tc = new TrackComp (edit, v);
    tc->incReferenceCount();
    return tc;
}

TrackCompManager::TrackComp::TrackComp (Edit& e, const juce::ValueTree& v)
   : ValueTreeObjectList<CompSection> (v), state (v), edit (e)
{
    auto um = &edit.getUndoManager();
    name.referTo (state, IDs::name, um);
    timeFormat.referTo (state, IDs::timecodeFormat, um);
    includedColour.referTo (state, IDs::includedColour, um, juce::Colours::green);
    excludedColour.referTo (state, IDs::excludedColour, um, juce::Colours::red);

    rebuildObjects();
}

TrackCompManager::TrackComp::~TrackComp()
{
    notifyListenersOfDeletion();
    freeObjects();
}


juce::String TrackCompManager::TrackComp::getSelectableDescription()   { return TRANS("Track Comp Group"); }

void TrackCompManager::TrackComp::setTimeFormat (TimeFormat t)
{
    const TimeFormat current = timeFormat.get();

    if (current == t)
        return;

    timeFormat = t;
    convertTimes (current, t);
}


juce::Array<TimeRange> TrackCompManager::TrackComp::getMuteTimes (const juce::Array<TimeRange>& nonMuteTimes)
{
    juce::Array<TimeRange> muteTimes;
    muteTimes.ensureStorageAllocated (nonMuteTimes.size() + 1);

    auto lastTime = 0_tp;

    if (! nonMuteTimes.isEmpty())
    {
        lastTime = std::min (lastTime, nonMuteTimes.getFirst().getStart());

        for (auto r : nonMuteTimes)
        {
            muteTimes.add ({ lastTime, r.getStart() });
            lastTime = r.getEnd();
        }
    }

    muteTimes.add ({ lastTime, TimePosition::fromSeconds (std::numeric_limits<double>::max()) });

    return muteTimes;
}

juce::Array<TimeRange> TrackCompManager::TrackComp::getNonMuteTimes (Track& t, TimeDuration crossfadeTime) const
{
    auto halfCrossfade = crossfadeTime / 2.0;
    juce::Array<TimeRange> nonMuteTimes;

    auto& ts = edit.tempoSequence;
    const bool convertFromBeats = timeFormat == beats;

    for (const auto& sec : getSectionsForTrack (&t))
    {
        auto s = sec.timeRange.getStart();
        auto e = sec.timeRange.getEnd();

        if (convertFromBeats)
        {
            s = ts.toTime (BeatPosition::fromBeats (s)).inSeconds();
            e = ts.toTime (BeatPosition::fromBeats (e)).inSeconds();
        }

        nonMuteTimes.add ({ TimePosition::fromSeconds (s) - halfCrossfade,
                            TimePosition::fromSeconds (e) + halfCrossfade });
    }

    // remove any overlaps
    for (int i = 0; i < nonMuteTimes.size() - 1; ++i)
    {
        auto& r1 = nonMuteTimes.getReference (i);
        auto& r2 = nonMuteTimes.getReference (i + 1);

        if (r1.getEnd() > r2.getStart())
        {
            auto diff = (r1.getEnd() - r2.getStart()) / 2.0;
            r1 = r1.withEnd (r1.getEnd() - diff);
            jassert (r1.getEnd() > r1.getStart());
            r2 = r2.withStart (r2.getStart() + diff);
            jassert (r2.getEnd() > r2.getStart());
        }
    }

    return nonMuteTimes;
}

TimeRange TrackCompManager::TrackComp::getTimeRange() const
{
    TimeRange time;

    const auto crossfadeTimeMs = edit.engine.getPropertyStorage().getProperty (SettingID::compCrossfadeMs, 20.0);
    const auto crossfadeTime = TimeDuration::fromSeconds (static_cast<double> (crossfadeTimeMs) / 1000.0);
    const auto halfCrossfade = crossfadeTime / 2.0;

    auto& ts = edit.tempoSequence;
    bool convertFromBeats = timeFormat == beats;

    for (const auto& sec : getSectionsForTrack ({}))
    {
        if (! sec.compSection->getTrack().isValid())
            continue;

        auto s = sec.timeRange.getStart();
        auto e = sec.timeRange.getEnd();

        if (convertFromBeats)
        {
            s = ts.toTime (BeatPosition::fromBeats (s)).inSeconds();
            e = ts.toTime (BeatPosition::fromBeats (e)).inSeconds();
        }

        TimeRange secTime (TimePosition::fromSeconds (s) - halfCrossfade,
                           TimePosition::fromSeconds (e) + halfCrossfade);
        time = time.isEmpty() ? secTime : time.getUnionWith (secTime);
    }

    return time;
}

void TrackCompManager::TrackComp::setName (const juce::String& n)
{
    name = n;
}

EditItemID TrackCompManager::TrackComp::getSourceTrackID (const juce::ValueTree& v)
{
    return EditItemID::fromProperty (v, IDs::track);
}

void TrackCompManager::TrackComp::setSourceTrackID (juce::ValueTree& v, EditItemID newID, juce::UndoManager* um)
{
    v.setProperty (IDs::track, newID, um);
}

void TrackCompManager::TrackComp::setSectionTrack (CompSection& cs, const Track::Ptr& t)
{
    jassert (cs.state.getParent() == state);
    auto um = &edit.getUndoManager();
    setSourceTrackID (cs.state, t != nullptr ? t->itemID : EditItemID(), um);
}

void TrackCompManager::TrackComp::removeSection (CompSection& cs)
{
    auto um = &edit.getUndoManager();
    auto previous = cs.state.getSibling (-1);
    auto next = cs.state.getSibling (1);
    bool remove = false;

    if (next.isValid())
    {
        if (getSourceTrackID (next).isValid())
            setSourceTrackID (cs.state, {}, um);
        else
            remove = true;
    }
    else if (previous.isValid())
    {
        if (! getSourceTrackID (previous).isValid())
            state.removeChild (previous, um);

        remove = true;
    }

    if (remove)
        state.removeChild (cs.state, um);
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::moveSectionToEndAt (CompSection* cs, double newEndTime)
{
    if (cs == nullptr)
        return {};

    return moveSection (cs, newEndTime - cs->getEnd());
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::moveSection (CompSection* cs, double timeDelta)
{
    if (timeDelta == 0.0 || cs == nullptr)
        return cs;

    bool needToAddSectionAtStart = false;
    const int sectionIndex = objects.indexOf (cs);

    // first section
    if (sectionIndex == 0)
    {
        if (timeDelta > 0.0)
            needToAddSectionAtStart = true;
        else
            return cs;
    }

    // move section
    auto um = &edit.getUndoManager();
    juce::WeakReference<CompSection> prevCs = objects[sectionIndex - 1];
    juce::Range<double> oldSectionTimes (prevCs == nullptr ? 0.0 : prevCs->getEnd(), cs->getEnd());
    auto newSectionTimes = oldSectionTimes + timeDelta;

    if (newSectionTimes.getStart() < 0.0)
        newSectionTimes = newSectionTimes.movedToStartAt (0.0);

    if (oldSectionTimes == newSectionTimes)
        return cs;

    removeSectionsWithinRange (oldSectionTimes.getUnionWith (newSectionTimes), cs);
    prevCs = objects[objects.indexOf (cs) - 1];

    if (prevCs != nullptr)
        prevCs->state.setProperty (IDs::end, newSectionTimes.getStart(), um);

    cs->state.setProperty (IDs::end, newSectionTimes.getEnd(), um);

    if (needToAddSectionAtStart)
        addSection (nullptr, timeDelta);

    return cs;
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::moveSectionEndTime (CompSection* cs, double newTime)
{
    const auto minSectionLength = 0.01;

    juce::WeakReference<CompSection> prevCs = objects[objects.indexOf (cs) - 1];
    juce::Range<double> oldSectionTimes (prevCs == nullptr ? 0.0 : prevCs->getEnd(), cs->getEnd());
    auto newSectionTimes = oldSectionTimes.withEnd (newTime);
    auto um = &edit.getUndoManager();

    if (newSectionTimes.getEnd() >= oldSectionTimes.getEnd())
    {
        // If expanding remove all sections within the new range
        removeSectionsWithinRange (newSectionTimes, cs);
        cs->state.setProperty (IDs::end, newSectionTimes.getEnd(), um);
        return cs;
    }

    if (newSectionTimes.getLength() >= minSectionLength)
    {
        cs->state.setProperty (IDs::end, newSectionTimes.getEnd(), um);
        return cs;
    }

    if (newSectionTimes.getLength() < minSectionLength)
    {
        newSectionTimes = oldSectionTimes.withLength (minSectionLength);
        cs->state.setProperty (IDs::end, newSectionTimes.getEnd(), um);
        return cs;
    }

    return cs;
}

int TrackCompManager::TrackComp::removeSectionsWithinRange (juce::Range<double> timeRange, CompSection* sectionToKeep)
{
    int numRemoved = 0;
    auto sections = getSectionsForTrack ({});

    for (int i = sections.size(); --i >= 0;)
    {
        auto& section = sections.getReference (i);
        auto sectionTime = section.timeRange;

        if (section.compSection != sectionToKeep
             && (timeRange.contains (sectionTime) || sectionTime.getLength() < 0.0))
        {
            state.removeChild (section.compSection->state, &edit.getUndoManager());
            ++numRemoved;
        }

        if (sectionTime.getStart() < timeRange.getStart())
            break;
    }

    return numRemoved;
}

struct SectionSorter
{
    static int compareElements (const juce::ValueTree& f, const juce::ValueTree& s) noexcept
    {
        return double (f[IDs::end]) < double (s[IDs::end]) ? -1 : 1;
    }
};

juce::ValueTree TrackCompManager::TrackComp::addSection (EditItemID trackID, double endTime,
                                                         juce::UndoManager* um)
{
    auto newSection = createValueTree (IDs::COMPSECTION,
                                       IDs::track, trackID,
                                       IDs::end, endTime);

    int insertIndex = 0;
    const int numSections = state.getNumChildren();

    for (int i = numSections; --i >= 0;)
    {
        auto v = state.getChild (i);

        if (double (v[IDs::end]) < endTime)
            break;

        insertIndex = i;
    }

    state.addChild (newSection, insertIndex, um);

    SectionSorter sorter;
    state.sort (sorter, &edit.getUndoManager(), false);

    return newSection;
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::addSection (Track::Ptr t, double endTime)
{
    auto trackID = t == nullptr ? EditItemID() : t->itemID;
    auto cs = getCompSectionFor (addSection (trackID, endTime, &edit.getUndoManager()));
    jassert (cs != nullptr);
    return cs;
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::splitSectionAtTime (double time)
{
    EditItemID trackID;

    if (auto cs = findSectionAtTime (nullptr, time))
        trackID = cs->getTrack();

    return getCompSectionFor (addSection (trackID, time, &edit.getUndoManager()));
}

juce::Array<TrackCompManager::TrackComp::Section> TrackCompManager::TrackComp::getSectionsForTrack (const Track::Ptr& track) const
{
    juce::Array<Section> sections;
    double lastTime = 0.0;
    auto trackId = track == nullptr ? EditItemID() : track->itemID;

    for (auto cs : objects)
    {
        const auto t = cs->getEnd();

        if (! trackId.isValid() || cs->getTrack() == trackId)
            sections.add (Section { cs, { lastTime, t } });

        lastTime = t;
    }

    return sections;
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::findSectionWithEdgeTimeWithin (const Track::Ptr& track,
                                                                                           juce::Range<double> timeRange,
                                                                                           bool& startEdge) const
{
    for (const auto& section : getSectionsForTrack (track))
    {
        if (timeRange.contains (section.timeRange.getStart()))
        {
            startEdge = true;
            return objects[objects.indexOf (section.compSection) - 1];
        }

        if (timeRange.contains (section.timeRange.getEnd()))
        {
            startEdge = false;
            return section.compSection;
        }
    }

    return {};
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::findSectionAtTime (const Track::Ptr& track, double time) const
{
    auto trackID = track != nullptr ? track->itemID : EditItemID();

    for (const auto& section : getSectionsForTrack (track))
    {
        auto* cs = section.compSection;

        if ((track == nullptr || cs->getTrack() == trackID)
             && section.timeRange.contains (time))
            return cs;
    }

    return {};
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::createNewObject (const juce::ValueTree& v)
{
    return CompSection::createAndIncRefCount (v);
}

bool TrackCompManager::TrackComp::isSuitableType (const juce::ValueTree& v) const   { return v.hasType (IDs::COMPSECTION); }
void TrackCompManager::TrackComp::deleteObject (CompSection* cs)                    { cs->decReferenceCount(); }
void TrackCompManager::TrackComp::newObjectAdded (CompSection*)                     {}
void TrackCompManager::TrackComp::objectRemoved (CompSection*)                      {}
void TrackCompManager::TrackComp::objectOrderChanged()                              {}

void TrackCompManager::TrackComp::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v.hasType (IDs::COMPSECTION))
        if (CompSection* cs = getCompSectionFor (v))
            cs->updateFrom (v, i);
}

TrackCompManager::CompSection* TrackCompManager::TrackComp::getCompSectionFor (const juce::ValueTree& v)
{
    jassert (v.hasType (IDs::COMPSECTION));

    for (auto cs : objects)
        if (cs->state == v)
            return cs;

    return {};
}

void TrackCompManager::TrackComp::convertFromSecondsToBeats()
{
    auto& ts = edit.tempoSequence;
    auto um = &edit.getUndoManager();

    for (auto cs : objects)
    {
        const auto s = cs->getEnd();
        const auto b = ts.toBeats (TimePosition::fromSeconds (s)).inBeats();
        cs->state.setProperty (IDs::end, b, um);
        jassert (cs->getEnd() == b);
    }
}

void TrackCompManager::TrackComp::convertFromBeatsToSeconds()
{
    auto& ts = edit.tempoSequence;
    auto um = &edit.getUndoManager();

    for (auto cs : objects)
    {
        const auto b = cs->getEnd();
        const auto s = ts.toTime (BeatPosition::fromBeats (b)).inSeconds();
        cs->state.setProperty (IDs::end, s, um);
        jassert (cs->getEnd() == s);
    }
}

void TrackCompManager::TrackComp::convertTimes (TimeFormat o, TimeFormat n)
{
    if (o == n)
        return;

    if (o == seconds)
    {
        convertFromSecondsToBeats();
        return;
    }

    convertFromBeatsToSeconds();
}

//==============================================================================
struct TrackCompManager::TrackCompList   : public ValueTreeObjectList<TrackComp>
{
    TrackCompList (Edit& e, const juce::ValueTree& v) : ValueTreeObjectList<TrackComp> (v), edit (e)
    {
        rebuildObjects();
    }

    ~TrackCompList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override     { return v.hasType (IDs::TRACKCOMP); }
    TrackComp* createNewObject (const juce::ValueTree& v) override    { return TrackComp::createAndIncRefCount (edit, v); }
    void deleteObject (TrackComp* tc) override                        { tc->decReferenceCount(); }
    void newObjectAdded (TrackComp*) override                         {}
    void objectRemoved (TrackComp*) override                          {}
    void objectOrderChanged() override                                {}

private:
    Edit& edit;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackCompList)
};


//==============================================================================
TrackCompManager::TrackCompManager (Edit& e) : edit (e) {}
TrackCompManager::~TrackCompManager() {}

void TrackCompManager::initialise (const juce::ValueTree& v)
{
    jassert (v.hasType (IDs::TRACKCOMPS));
    state = v;
    trackCompList.reset (new TrackCompList (edit, v));
}

int TrackCompManager::getNumGroups() const
{
    return state.getNumChildren();
}

juce::StringArray TrackCompManager::getCompNames() const
{
    juce::StringArray names;
    auto numComps = state.getNumChildren();

    for (int i = 0; i < numComps; ++i)
    {
        auto v = state.getChild (i);
        jassert (v.hasType (IDs::TRACKCOMP));

        auto name = v.getProperty (IDs::name).toString();

        if (name.isEmpty())
            name = juce::String (TRANS("Comp Group")) + " " + juce::String (i);

        names.add (name);
    }

    return names;
}

juce::String TrackCompManager::getCompName (int index)
{
    if (index == -1)
        return "<" + TRANS("None") + ">";

    auto name = getCompNames()[index];

    if (name.isNotEmpty())
        return name;

    jassert (juce::isPositiveAndBelow (index, getNumGroups()));
    return TRANS("Comp Group") + " " + juce::String (index);
}

void TrackCompManager::setCompName (int index, const juce::String& name)
{
    jassert (juce::isPositiveAndBelow (index, getNumGroups()));

    auto v = state.getChild (index);

    if (v.isValid())
        v.setProperty (IDs::name, name, &edit.getUndoManager());
}

int TrackCompManager::addGroup (const juce::String& name)
{
    auto v = createValueTree (IDs::TRACKCOMP,
                              IDs::name, name);

    state.addChild (v, -1, &edit.getUndoManager());

    return getNumGroups() - 1;
}

void TrackCompManager::removeGroup (int index)
{
    for (auto at : getAudioTracks (edit))
        if (at->getCompGroup() == index)
            at->setCompGroup (-1);

    state.removeChild (index, &edit.getUndoManager());
}

juce::Array<Track*> TrackCompManager::getTracksInComp (int index)
{
    juce::Array<Track*> tracks;

    for (auto at : getAudioTracks (edit))
        if (at->getCompGroup() == index)
            tracks.add (at);

    return tracks;
}

TrackCompManager::TrackComp::Ptr TrackCompManager::getTrackComp (AudioTrack* at)
{
    return at == nullptr ? nullptr : trackCompList->objects[at->getCompGroup()];
}

}} // namespace tracktion { inline namespace engine
