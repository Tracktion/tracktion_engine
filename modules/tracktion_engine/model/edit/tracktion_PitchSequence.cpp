/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct PitchSequence::PitchList  : public ValueTreeObjectList<PitchSetting>,
                                   private AsyncUpdater
{
    PitchList (PitchSequence& s, const ValueTree& parent)
        : ValueTreeObjectList<PitchSetting> (parent), pitchSequence (s)
    {
        rebuildObjects();
    }

    ~PitchList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::PITCH);
    }

    PitchSetting* createNewObject (const ValueTree& v) override
    {
        auto t = new PitchSetting (pitchSequence.getEdit(), v);
        t->incReferenceCount();
        return t;
    }

    void deleteObject (PitchSetting* t) override
    {
        jassert (t != nullptr);
        t->decReferenceCount();
    }

    void newObjectAdded (PitchSetting*) override    { sendChange(); }
    void objectRemoved (PitchSetting*) override     { sendChange(); }
    void objectOrderChanged() override              { sendChange(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { sendChange(); }

    void sendChange()
    {
        if (! pitchSequence.getEdit().isLoading())
            triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        pitchSequence.getEdit().sendTempoOrPitchSequenceChangedUpdates();
    }

    PitchSequence& pitchSequence;
};

//==============================================================================
PitchSequence::PitchSequence() {}
PitchSequence::~PitchSequence() {}

Edit& PitchSequence::getEdit() const
{
    jassert (edit != nullptr);
    return *edit;
}

UndoManager* PitchSequence::getUndoManager() const
{
    return &getEdit().getUndoManager();
}

void PitchSequence::clear()
{
    const int pitch = getPitch (0)->getPitch();
    state.removeAllChildren (getUndoManager());
    insertPitch (0, pitch);
}

void PitchSequence::initialise (Edit& ed, const ValueTree& v)
{
    edit = &ed;
    state = v;

    list = std::make_unique<PitchList> (*this, state);

    if (getNumPitches() == 0)
        insertPitch (0, 60);

    sortEvents();
}

void PitchSequence::freeResources()
{
    list.reset();
}

void PitchSequence::copyFrom (const PitchSequence& other)
{
    copyValueTree (state, other.state, nullptr);
}

int PitchSequence::getNumPitches() const                        { return list->objects.size(); }
PitchSetting* PitchSequence::getPitch (int index) const         { return list->objects[index]; }
PitchSetting& PitchSequence::getPitchAt (double time) const     { return *list->objects[indexOfPitchAt (time)]; }

PitchSetting& PitchSequence::getPitchAtBeat (double beat) const
{
    for (int i = list->objects.size(); --i > 0;)
    {
        auto p = list->objects.getUnchecked (i);

        if (p->getStartBeatNumber() <= beat)
            return *p;
    }

    jassert (list->size() > 0);
    return *list->objects.getFirst();
}

int PitchSequence::indexOfPitchAt (double t) const
{
    for (int i = list->objects.size(); --i > 0;)
        if (list->objects.getUnchecked (i)->getPosition().getStart() <= t)
            return i;

    jassert (list->size() > 0);
    return 0;
}

int PitchSequence::indexOfNextPitchAt (double t) const
{
    for (int i = 0; i < list->objects.size(); ++i)
        if (list->objects.getUnchecked (i)->getPosition().getStart() >= t)
            return i;

    jassert (list->size() > 0);
    return list->objects.size();
}

int PitchSequence::indexOfPitch (const PitchSetting* pitchSetting) const
{
    return list->objects.indexOf ((PitchSetting*) pitchSetting);
}

PitchSetting::Ptr PitchSequence::insertPitch (double time)
{
    return insertPitch (edit->tempoSequence.timeToBeats (time), -1);
}

PitchSetting::Ptr PitchSequence::insertPitch (double beatNum, int pitch)
{
    int newIndex = -1;

    if (list->size() > 0)
    {
        auto& prev = getPitchAtBeat (beatNum);

        // don't add in the same place as another one
        if (prev.getStartBeatNumber() == beatNum)
        {
            if (pitch >= 0)
                prev.setPitch (pitch);

            return prev;
        }

        if (pitch < 0)
            pitch = prev.getPitch();

        newIndex = indexOfPitch (&prev) + 1;
    }

    ValueTree v (IDs::PITCH);
    v.setProperty (IDs::startBeat, beatNum, nullptr);
    v.setProperty (IDs::pitch, pitch, nullptr);

    if (newIndex < 0)
        newIndex = list->objects.size();

    state.addChild (v, newIndex, getUndoManager());

    jassert (list->objects[newIndex]->state == v);
    return list->objects[newIndex];
}

void PitchSequence::movePitchStart (PitchSetting& p, double deltaBeats, bool snapToBeat)
{
    auto index = indexOfPitch (&p);

    if (index > 0 && deltaBeats != 0)
    {
        if (auto t = getPitch (index))
        {
            t->startBeat.forceUpdateOfCachedValue();
            auto newBeat = t->getStartBeat() + deltaBeats;
            t->setStartBeat (jlimit (0.0, 1e10, snapToBeat ? roundToInt (newBeat) : newBeat));
        }
    }
}

void PitchSequence::insertSpaceIntoSequence (double time, double amountOfSpaceInSeconds, bool snapToBeat)
{
    auto beatsToInsert = getEdit().tempoSequence.getBeatsPerSecondAt (time) * amountOfSpaceInSeconds;
    auto endIndex = indexOfNextPitchAt (time);

    for (int i = getNumPitches(); --i >= endIndex;)
        movePitchStart (*getPitch (i), beatsToInsert, snapToBeat);
}

struct PitchSorter
{
    static int compareElements (const ValueTree& p1, const ValueTree& p2) noexcept
    {
        const double beat1 = p1[IDs::startBeat];
        const double beat2 = p2[IDs::startBeat];

        return beat1 < beat2 ? -1 : (beat1 > beat2 ? 1 : 0);
    }
};

void PitchSequence::sortEvents()
{
    PitchSorter sorter;
    state.sort (sorter, getUndoManager(), true);
}
