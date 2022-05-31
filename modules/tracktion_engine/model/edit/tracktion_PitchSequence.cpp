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

struct PitchSequence::PitchList  : public ValueTreeObjectList<PitchSetting>,
                                   private juce::AsyncUpdater
{
    PitchList (PitchSequence& s, const juce::ValueTree& parentTree)
        : ValueTreeObjectList<PitchSetting> (parentTree), pitchSequence (s)
    {
        rebuildObjects();
    }

    ~PitchList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::PITCH);
    }

    PitchSetting* createNewObject (const juce::ValueTree& v) override
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
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { sendChange(); }

    void sendChange()
    {
        if (! pitchSequence.getEdit().isLoading())
            triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        pitchSequence.getEdit().tempoSequence.updateTempoData();
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

juce::UndoManager* PitchSequence::getUndoManager() const
{
    return &getEdit().getUndoManager();
}

void PitchSequence::clear()
{
    if (auto first = getPitch (0))
    {
        auto pitch = first->getPitch();
        state.removeAllChildren (getUndoManager());
        insertPitch ({}, pitch);
    }
    else
    {
        jassertfalse;
    }
}

void PitchSequence::initialise (Edit& ed, const juce::ValueTree& v)
{
    edit = &ed;
    state = v;

    list = std::make_unique<PitchList> (*this, state);

    if (getNumPitches() == 0)
        insertPitch ({}, 60);

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

const juce::Array<PitchSetting*>& PitchSequence::getPitches() const    { return list->objects; }
int PitchSequence::getNumPitches() const                               { return list->objects.size(); }
PitchSetting* PitchSequence::getPitch (int index) const                { return list->objects[index]; }
PitchSetting& PitchSequence::getPitchAt (TimePosition time) const      { return *list->objects[indexOfPitchAt (time)]; }

PitchSetting& PitchSequence::getPitchAtBeat (BeatPosition beat) const
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

int PitchSequence::indexOfPitchAt (TimePosition t) const
{
    for (int i = list->objects.size(); --i > 0;)
        if (list->objects.getUnchecked (i)->getPosition().getStart() <= t)
            return i;

    jassert (list->size() > 0);
    return 0;
}

int PitchSequence::indexOfNextPitchAt (TimePosition t) const
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

PitchSetting::Ptr PitchSequence::insertPitch (TimePosition time)
{
    return insertPitch (edit->tempoSequence.toBeats (time), -1);
}

PitchSetting::Ptr PitchSequence::insertPitch (BeatPosition beatNum, int pitch)
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

    auto v = createValueTree (IDs::PITCH,
                              IDs::startBeat, beatNum.inBeats(),
                              IDs::pitch, pitch);

    if (newIndex < 0)
        newIndex = list->objects.size();

    state.addChild (v, newIndex, getUndoManager());

    jassert (list->objects[newIndex]->state == v);
    return list->objects[newIndex];
}

void PitchSequence::movePitchStart (PitchSetting& p, BeatDuration deltaBeats, bool snapToBeat)
{
    auto index = indexOfPitch (&p);

    if (index > 0 && deltaBeats != BeatDuration())
    {
        if (auto t = getPitch (index))
        {
            t->startBeat.forceUpdateOfCachedValue();
            auto newBeat = t->getStartBeat() + deltaBeats;
            t->setStartBeat (BeatPosition::fromBeats (juce::jlimit (0.0, 1e10, snapToBeat ? juce::roundToInt (newBeat.inBeats())
                                                                                          : newBeat.inBeats())));
        }
    }
}

void PitchSequence::insertSpaceIntoSequence (TimePosition time, TimeDuration amountOfSpaceInSeconds, bool snapToBeat)
{
    // there may be a tempo change at this time so we need to find the tempo to the left of it hence the nudge
    time = time - TimeDuration::fromSeconds (0.00001);
    auto beatsToInsert = getEdit().tempoSequence.getBeatsPerSecondAt (time) * amountOfSpaceInSeconds.inSeconds();
    auto endIndex = indexOfNextPitchAt (time);

    for (int i = getNumPitches(); --i >= endIndex;)
        movePitchStart (*getPitch (i), BeatDuration::fromBeats (beatsToInsert), snapToBeat);
}

void PitchSequence::sortEvents()
{
    struct PitchSorter
    {
        static int compareElements (const juce::ValueTree& p1, const juce::ValueTree& p2) noexcept
        {
            const double beat1 = p1[IDs::startBeat];
            const double beat2 = p2[IDs::startBeat];

            return beat1 < beat2 ? -1 : (beat1 > beat2 ? 1 : 0);
        }
    };

    PitchSorter sorter;
    state.sort (sorter, getUndoManager(), true);
}

}} // namespace tracktion { inline namespace engine
