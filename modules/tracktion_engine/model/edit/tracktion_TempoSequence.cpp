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

template<typename ObjectType>
struct TempoAndTimeSigListBase  : public ValueTreeObjectList<ObjectType>,
                                  public juce::AsyncUpdater
{
    TempoAndTimeSigListBase (TempoSequence& ts, const juce::ValueTree& parentTree)
        : ValueTreeObjectList<ObjectType> (parentTree), sequence (ts)
    {
    }

    void newObjectAdded (ObjectType*) override                                          { sendChange(); }
    void objectRemoved (ObjectType*) override                                           { sendChange(); }
    void objectOrderChanged() override                                                  { sendChange(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { sendChange(); }

    void sendChange()
    {
        if (! sequence.getEdit().isLoading())
            triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        sequence.updateTempoData();
    }

    TempoSequence& sequence;
};

//==============================================================================
struct TempoSequence::TempoSettingList  : public TempoAndTimeSigListBase<TempoSetting>
{
    TempoSettingList (TempoSequence& ts, const juce::ValueTree& parentTree)
        : TempoAndTimeSigListBase<TempoSetting> (ts, parentTree)
    {
        rebuildObjects();
    }

    ~TempoSettingList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::TEMPO);
    }

    TempoSetting* createNewObject (const juce::ValueTree& v) override
    {
        auto t = new TempoSetting (sequence, v);
        t->incReferenceCount();
        return t;
    }

    void deleteObject (TempoSetting* t) override
    {
        jassert (t != nullptr);
        t->decReferenceCount();
    }
};

//==============================================================================
struct TempoSequence::TimeSigList  : public TempoAndTimeSigListBase<TimeSigSetting>
{
    TimeSigList (TempoSequence& ts, const juce::ValueTree& parentTree)
        : TempoAndTimeSigListBase<TimeSigSetting> (ts, parentTree)
    {
        rebuildObjects();
    }

    ~TimeSigList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::TIMESIG);
    }

    TimeSigSetting* createNewObject (const juce::ValueTree& v) override
    {
        auto t = new TimeSigSetting (sequence, v);
        t->incReferenceCount();
        return t;
    }

    void deleteObject (TimeSigSetting* t) override
    {
        jassert (t != nullptr);
        t->decReferenceCount();
    }
};


//==============================================================================
TempoSequence::TempoSequence (Edit& e) : edit (e)
{
}

TempoSequence::~TempoSequence()
{
    cancelPendingUpdate();
    notifyListenersOfDeletion();
}

juce::UndoManager* TempoSequence::getUndoManager() const noexcept
{
    return &edit.getUndoManager();
}

//==============================================================================
void TempoSequence::setState (const juce::ValueTree& v, bool remapEdit)
{
    EditTimecodeRemapperSnapshot snap;

    if (remapEdit)
        snap.savePreChangeState (edit);

    freeResources();

    state = v;

    tempos = std::make_unique<TempoSettingList> (*this, state);
    timeSigs = std::make_unique<TimeSigList> (*this, state);

    if (tempos->objects.isEmpty())
        insertTempo ({}, nullptr);

    if (timeSigs->objects.isEmpty())
        insertTimeSig ({}, nullptr);

    tempos->objects.getFirst()->startBeatNumber = BeatPosition();
    timeSigs->objects.getFirst()->startBeatNumber = BeatPosition();

    for (int i = 1; i < getNumTempos(); ++i)
        getTempo (i)->startBeatNumber = std::max (getTempo (i)->startBeatNumber.get(),
                                                  getTempo (i - 1)->startBeatNumber.get());

    for (int i = 1; i < getNumTimeSigs(); ++i)
        getTimeSig (i)->startBeatNumber = std::max (getTimeSig (i)->startBeatNumber.get(),
                                                    getTimeSig (i - 1)->startBeatNumber.get() + BeatDuration::fromBeats (1));

    updateTempoData();

    if (remapEdit)
        snap.remapEdit (edit);
}

void TempoSequence::createEmptyState()
{
    setState (juce::ValueTree (IDs::TEMPOSEQUENCE), false);
}

void TempoSequence::copyFrom (const TempoSequence& other)
{
    copyValueTree (state, other.state, nullptr);
}

void TempoSequence::freeResources()
{
    tempos.reset();
    timeSigs.reset();
}

const juce::Array<TimeSigSetting*>& TempoSequence::getTimeSigs() const   { return timeSigs->objects; }
int TempoSequence::getNumTimeSigs() const                                { return timeSigs->objects.size(); }
TimeSigSetting* TempoSequence::getTimeSig (int index) const              { return timeSigs->objects[index]; }

const juce::Array<TempoSetting*>& TempoSequence::getTempos() const  { return tempos->objects; }
int TempoSequence::getNumTempos() const                             { return tempos->objects.size(); }
TempoSetting* TempoSequence::getTempo (int index) const             { return tempos->objects[index]; }

TempoSetting::Ptr TempoSequence::insertTempo (TimePosition time)
{
    return insertTempo (time, getUndoManager());
}

TempoSetting::Ptr TempoSequence::insertTempo (BeatPosition beatNum, double bpm, float curve)
{
    return insertTempo (beatNum, bpm, curve, getUndoManager());
}

TempoSetting::Ptr TempoSequence::insertTempo (TimePosition time, juce::UndoManager* um)
{
    auto bpm = getBpmAt (time);
    float defaultCurve = 1.0f;

    if (getNumTempos() > 0)
        return insertTempo (tracktion::roundToNearestBeat (toBeats (time)), bpm, defaultCurve, um);

    return insertTempo ({}, bpm, defaultCurve, um);
}

TempoSetting::Ptr TempoSequence::insertTempo (BeatPosition beatNum, double bpm, float curve, juce::UndoManager* um)
{
    int index = -1;

    if (getNumTempos() > 0)
        index = state.indexOf (getTempoAt (beatNum).state) + 1;

    state.addChild (TempoSetting::create (beatNum, bpm, curve), index, um);

    return getTempoAt (beatNum);
}

TimeSigSetting::Ptr TempoSequence::insertTimeSig (TimePosition time)
{
    return insertTimeSig (time, getUndoManager());
}

TimeSigSetting::Ptr TempoSequence::insertTimeSig (BeatPosition beats)
{
    return insertTimeSig (toTime (beats));
}

TimeSigSetting::Ptr TempoSequence::insertTimeSig (TimePosition time, juce::UndoManager* um)
{
    BeatPosition beatNum;
    int index = -1;

    auto newTree = createValueTree (IDs::TIMESIG,
                                    IDs::numerator, 4,
                                    IDs::denominator, 4);

    if (getNumTimeSigs() > 0)
    {
        beatNum = tracktion::roundToNearestBeat (toBeats (time));
        auto& prev = getTimeSigAt (beatNum);

        index = state.indexOf (prev.state) + 1;
        newTree = prev.state.createCopy();

        // don't add in the same place as another one
        if (prev.startBeatNumber == beatNum)
            return prev;
    }

    newTree.setProperty (IDs::startBeat, beatNum.inBeats(), nullptr);

    state.addChild (newTree, index, um);

    return getTimeSigAt (beatNum);
}

void TempoSequence::removeTempo (int index, bool remapEdit)
{
    if (index == 0)
        return;

    if (auto ts = getTempo (index))
    {
        if (getNumTempos() > 1)
        {
            EditTimecodeRemapperSnapshot snap;

            if (remapEdit)
                snap.savePreChangeState (edit);

            jassert (ts->state.isAChildOf (state));
            state.removeChild (ts->state, getUndoManager());

            if (remapEdit)
                snap.remapEdit (edit);
        }
    }
}

void TempoSequence::removeTemposBetween (TimeRange range, bool remapEdit)
{
    EditTimecodeRemapperSnapshot snap;

    if (remapEdit)
        snap.savePreChangeState (edit);

    for (int i = getNumTempos(); --i > 0;)
        if (auto ts = getTempo (i))
            if (range.contains (ts->getStartTime()))
                removeTempo (i, false);

    if (remapEdit)
        snap.remapEdit (edit);
}

void TempoSequence::removeTimeSig (int index)
{
    if (index > 0)
    {
        if (auto ts = getTimeSig (index))
        {
            jassert (ts->state.isAChildOf (state));
            state.removeChild (ts->state, getUndoManager());
        }
    }
}

void TempoSequence::removeTimeSigsBetween (TimeRange range)
{
    for (int i = getNumTimeSigs(); --i > 0;)
        if (auto ts = getTimeSig (i))
            if (range.contains (ts->getPosition().getStart()))
                removeTimeSig (i);
}

void TempoSequence::moveTempoStart (int index, BeatDuration deltaBeats, bool snapToBeat)
{
    if (index > 0 && deltaBeats != BeatDuration())
    {
        if (auto t = getTempo (index))
        {
            auto prev = getTempo (index - 1);
            auto next = getTempo (index + 1);

            const auto prevBeat = prev != nullptr ? prev->startBeatNumber : BeatPosition();
            const auto nextBeat = next != nullptr ? next->startBeatNumber : BeatPosition::fromBeats (0x7ffffff);

            const auto newStart = juce::jlimit (prevBeat, nextBeat, t->startBeatNumber.get() + deltaBeats);
            t->set (snapToBeat ? tracktion::roundToNearestBeat (newStart) : newStart,
                    t->bpm, t->curve, false);
        }
    }
}

void TempoSequence::moveTimeSigStart (int index, BeatDuration deltaBeats, bool snapToBeat)
{
    if (index > 0 && deltaBeats != BeatDuration())
    {
        if (auto t = getTimeSig (index))
        {
            auto prev = getTimeSig (index - 1);
            auto next = getTimeSig (index + 1);

            const auto prevBeat = prev != nullptr ? prev->startBeatNumber : BeatPosition();
            const auto nextBeat = next != nullptr ? next->startBeatNumber : BeatPosition::fromBeats (0x7ffffff);

            if (nextBeat < prevBeat + BeatDuration::fromBeats (2))
                return;

            t->startBeatNumber.forceUpdateOfCachedValue();
            const auto newBeat = t->startBeatNumber.get() + deltaBeats;
            t->startBeatNumber = juce::jlimit (prevBeat + BeatDuration::fromBeats (1), nextBeat - BeatDuration::fromBeats (1),
                                               snapToBeat ? tracktion::roundToNearestBeat (newBeat) : newBeat);
        }
    }
}

void TempoSequence::insertSpaceIntoSequence (TimePosition time, TimeDuration amountOfSpaceInSeconds, bool snapToBeat)
{
    // there may be a temp change at this time so we need to find the tempo to the left of it hence the nudge
    time = time - TimeDuration::fromSeconds (0.00001);
    const auto beatsToInsert = BeatDuration::fromBeats (getBeatsPerSecondAt (time) * amountOfSpaceInSeconds.inSeconds());

    // Move timesig settings
    {
        const int endIndex = indexOfTimeSigAt (time) + 1;

        for (int i = getNumTimeSigs(); --i >= endIndex;)
            moveTimeSigStart (i, beatsToInsert, snapToBeat);
    }

    // Move tempo settings
    {
        const int endIndex = indexOfNextTempoAt (time);

        for (int i = getNumTempos(); --i >= endIndex;)
            moveTempoStart (i, beatsToInsert, snapToBeat);
    }
}

void TempoSequence::deleteRegion (TimeRange range)
{
    const auto beatRange = toBeats (range);
    
    removeTemposBetween (range, false);
    removeTimeSigsBetween (range);

    const bool snapToBeat = false;
    const auto startTime = toTime (beatRange.getStart());
    const auto deltaBeats = -beatRange.getLength();

    // Move timesig settings
    {
        const int startIndex = indexOfTimeSigAt (startTime) + 1;

        for (int i = startIndex; i < getNumTimeSigs(); ++i)
            moveTimeSigStart (i, deltaBeats, snapToBeat);
    }

    // Move tempo settings
    {
        const int startIndex = indexOfNextTempoAt (startTime);

        for (int i = startIndex; i < getNumTempos(); ++i)
            moveTempoStart (i, deltaBeats, snapToBeat);
    }
}

//==============================================================================
TimeSigSetting& TempoSequence::getTimeSigAt (TimePosition time) const
{
    return *getTimeSig (indexOfTimeSigAt (time));
}

TimeSigSetting& TempoSequence::getTimeSigAt (BeatPosition beat) const
{
    for (int i = getNumTimeSigs(); --i >= 0;)
        if (timeSigs->objects.getUnchecked (i)->startBeatNumber <= beat)
            return *timeSigs->objects.getUnchecked (i);

    jassert (timeSigs->size() > 0);
    return *timeSigs->objects.getFirst();
}

int TempoSequence::indexOfTimeSigAt (TimePosition t) const
{
    for (int i = getNumTimeSigs(); --i >= 0;)
        if (timeSigs->objects.getUnchecked (i)->startTime <= t)
            return i;

    jassert (timeSigs->size() > 0);
    return 0;
}

int TempoSequence::indexOfTimeSig (const TimeSigSetting* timeSigSetting) const
{
    return timeSigs->objects.indexOf (const_cast<TimeSigSetting*> (timeSigSetting));
}

//==============================================================================
TempoSetting& TempoSequence::getTempoAt (TimePosition time) const
{
    return *getTempo (indexOfTempoAt (time));
}

TempoSetting& TempoSequence::getTempoAt (BeatPosition beat) const
{
    for (int i = getNumTempos(); --i >= 0;)
        if (tempos->objects.getUnchecked (i)->startBeatNumber <= beat)
            return *tempos->objects.getUnchecked (i);

    jassert (tempos->size() > 0);
    return *tempos->objects.getFirst();
}

int TempoSequence::indexOfTempoAt (TimePosition t) const
{
    for (int i = getNumTempos(); --i >= 0;)
        if (tempos->objects.getUnchecked (i)->getStartTime() <= t)
            return i;

    jassert (tempos->size() > 0);
    return 0;
}

int TempoSequence::indexOfNextTempoAt (TimePosition t) const
{
    for (int i = 0; i < getNumTempos(); ++i)
        if (tempos->objects.getUnchecked (i)->getStartTime() >= t)
            return i;

    return getNumTempos();
}

int TempoSequence::indexOfTempo (const TempoSetting* const t) const
{
    return tempos->objects.indexOf (const_cast<TempoSetting*> (t));
}

double TempoSequence::getBpmAt (TimePosition time) const
{
    updateTempoDataIfNeeded();
    return internalSequence.getBpmAt (time);
}

double TempoSequence::getBeatsPerSecondAt (TimePosition time, bool lengthOfOneBeatDependsOnTimeSignature) const
{
    if (lengthOfOneBeatDependsOnTimeSignature)
    {
        updateTempoDataIfNeeded();
        return internalSequence.getBeatsPerSecondAt (time).v;
    }

    return getBpmAt (time) / 60.0;
}

bool TempoSequence::isTripletsAtTime (TimePosition time) const
{
    return getTimeSigAt (time).triplets;
}

//==============================================================================
BeatPosition TempoSequence::toBeats (TimePosition time) const
{
    updateTempoDataIfNeeded();
    return internalSequence.toBeats (time);
}

BeatRange TempoSequence::toBeats (TimeRange range) const
{
    return { toBeats (range.getStart()),
             toBeats (range.getEnd()) };
}

BeatPosition TempoSequence::toBeats (tempo::BarsAndBeats barsBeats) const
{
    return toBeats (toTime (barsBeats));
}

TimePosition TempoSequence::toTime (BeatPosition beats) const
{
    updateTempoDataIfNeeded();
    return internalSequence.toTime (beats);
}

TimeRange TempoSequence::toTime (BeatRange range) const
{
    return { toTime (range.getStart()),
             toTime (range.getEnd()) };
}

TimePosition TempoSequence::toTime (tempo::BarsAndBeats barsBeats) const
{
    updateTempoDataIfNeeded();
    return internalSequence.toTime (barsBeats);
}

tempo::BarsAndBeats TempoSequence::toBarsAndBeats (TimePosition t) const
{
    updateTempoDataIfNeeded();
    return internalSequence.toBarsAndBeats (t);
}

//==============================================================================
const tempo::Sequence& TempoSequence::getInternalSequence() const
{
    return internalSequence;
}

void TempoSequence::updateTempoData()
{
    tempos->cancelPendingUpdate();
    jassert (getNumTempos() > 0 && getNumTimeSigs() > 0);

    // Build the new sequence
    std::vector<tempo::TempoChange> tempoChanges;
    std::vector<tempo::TimeSigChange> timeSigChanges;
    std::vector<tempo::KeyChange> keyChanges;

    // Copy change events
    {
        for (auto ts : getTempos())
            tempoChanges.push_back ({ ts->startBeatNumber.get(), ts->bpm.get(), ts->curve.get() });

        for (auto ts : getTimeSigs())
            timeSigChanges.push_back ({ ts->startBeatNumber.get(), ts->numerator.get(), ts->denominator.get(), ts->triplets.get() });

        for (auto pc : edit.pitchSequence.getPitches())
            keyChanges.push_back ({ pc->startBeat.get(), { pc->pitch.get(), static_cast<int> (pc->scale.get()) } });
    }

    const bool useDenominator = edit.engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();
    tempo::Sequence newSeq (std::move (tempoChanges), std::move (timeSigChanges), std::move (keyChanges),
                            useDenominator ? tempo::LengthOfOneBeat::dependsOnTimeSignature
                                           : tempo::LengthOfOneBeat::isAlwaysACrotchet);

    // Update startTime properties of the model objects
    {
        for (auto ts : getTempos())
            ts->startTime = newSeq.toTime (ts->startBeatNumber.get());

        for (auto ts : getTimeSigs())
            ts->startTime = newSeq.toTime (ts->startBeatNumber.get());
    }

    jassert (getNumTempos() > 0 && getNumTimeSigs() > 0);
    triggerAsyncUpdate();

    {
        //TODO: This lock should be removed when all playback classes are using the new tempo::Sequence class
        juce::ScopedLock sl (edit.engine.getDeviceManager().deviceManager.getAudioCallbackLock());
        internalSequence = std::move (newSeq);
    }
}

void TempoSequence::updateTempoDataIfNeeded() const
{
    if (tempos->isUpdatePending())
        tempos->handleAsyncUpdate();

    if (timeSigs->isUpdatePending())
        timeSigs->handleAsyncUpdate();
}

void TempoSequence::handleAsyncUpdate()
{
    edit.sendTempoOrPitchSequenceChangedUpdates();
    changed();
}

juce::String TempoSequence::getSelectableDescription()
{
    return TRANS("Tempo Curve");
}

int TempoSequence::countTemposInRegion (TimeRange range) const
{
    int count = 0;

    for (auto t : tempos->objects)
        if (range.contains (t->getStartTime()))
            ++count;

    return count;
}

HashCode TempoSequence::createHashForTemposInRange (TimeRange range) const
{
    HashCode hash = 0;

    for (auto t : tempos->objects)
        if (range.contains (t->getStartTime()))
            hash ^= t->getHash();

    return hash;
}

//==============================================================================
tempo::BarsAndBeats TempoSequence::timeToBarsBeats (TimePosition t) const
{
    return toBarsAndBeats (t);
}

TimePosition TempoSequence::barsBeatsToTime (tempo::BarsAndBeats barsBeats) const
{
    return toTime (barsBeats);
}

BeatPosition TempoSequence::barsBeatsToBeats (tempo::BarsAndBeats barsBeats) const
{
    return toBeats (barsBeats);
}

BeatPosition TempoSequence::timeToBeats (TimePosition time) const
{
    return toBeats (time);
}

BeatRange TempoSequence::timeToBeats (TimeRange range) const
{
    return toBeats (range);
}

TimePosition TempoSequence::beatsToTime (BeatPosition beats) const
{
    return toTime (beats);
}

TimeRange TempoSequence::beatsToTime (BeatRange range) const
{
    return toTime (range);
}

TimeSigSetting& TempoSequence::getTimeSigAtBeat (BeatPosition beat) const
{
    return getTimeSigAt (beat);
}

TempoSetting& TempoSequence::getTempoAtBeat (BeatPosition beat) const
{
    return getTempoAt (beat);
}

//==============================================================================
//==============================================================================
tempo::Sequence::Position createPosition (const TempoSequence& s)
{
    return tempo::Sequence::Position (s.getInternalSequence());
}


//==============================================================================
//==============================================================================
void EditTimecodeRemapperSnapshot::savePreChangeState (Edit& ed)
{
    auto& tempoSequence = ed.tempoSequence;

    clips.clear();

    for (auto t : getClipTracks (ed))
    {
        for (auto& c : t->getClips())
        {
            auto pos = c->getPosition();

            ClipPos cp;
            cp.clip = c;
            cp.startBeat        = tempoSequence.toBeats (pos.getStart());
            cp.endBeat          = tempoSequence.toBeats (pos.getEnd());
            cp.contentStartBeat = toDuration (tempoSequence.toBeats (pos.getStartOfSource()));
            clips.add (cp);
        }
    }

    automation.clear();

    for (auto aei : getAllAutomatableEditItems (ed))
    {
        if (aei == nullptr)
            continue;

        for (auto ap : aei->getAutomatableParameters())
        {
            if (! ap->automatableEditElement.remapOnTempoChange)
                continue;

            auto& curve = ap->getCurve();
            auto numPoints = curve.getNumPoints();

            if (numPoints == 0)
                continue;

            juce::Array<BeatPosition> beats;
            beats.ensureStorageAllocated (numPoints);

            for (int p = 0; p < numPoints; ++p)
                beats.add (toBeats (curve.getPoint (p).time, tempoSequence));

            automation.add ({ curve, std::move (beats) });
        }
    }

    const auto loopRange = ed.getTransport().getLoopRange();
    loopPositionBeats = { tempoSequence.toBeats (loopRange.getStart()),
                          tempoSequence.toBeats (loopRange.getEnd()) };
}

void EditTimecodeRemapperSnapshot::remapEdit (Edit& ed)
{
    auto& transport = ed.getTransport();
    auto& tempoSequence = ed.tempoSequence;
    tempoSequence.updateTempoData();

    transport.setLoopRange (tempoSequence.toTime (loopPositionBeats));

    for (auto& cp : clips)
    {
        if (auto c = dynamic_cast<Clip*> (cp.clip.get()))
        {
            auto newStart  = tempoSequence.toTime (cp.startBeat);
            auto newEnd    = tempoSequence.toTime (cp.endBeat);
            auto newOffset = newStart - tempoSequence.toTime (toPosition (cp.contentStartBeat));

            auto pos = c->getPosition();

            if (std::abs ((pos.getStart() - newStart).inSeconds())
                 + std::abs ((pos.getEnd() - newEnd).inSeconds())
                 + std::abs ((pos.getOffset() - newOffset).inSeconds()) > 0.001)
            {
                if (c->getSyncType() == Clip::syncAbsolute)
                    continue;

                if (c->type == TrackItem::Type::wave)
                {
                    auto ac = dynamic_cast<AudioClipBase*> (cp.clip.get());

                    if (ac != nullptr && ac->getAutoTempo())
                        c->setPosition ({ { newStart, newEnd }, newOffset });
                    else
                        c->setStart (newStart, false, true);
                }
                else
                {
                    c->setPosition ({ { newStart, newEnd }, newOffset });
                }
            }
        }
    }

    const ParameterChangeHandler::Disabler disabler (ed.getParameterChangeHandler());

    for (auto& a : automation)
        for (int i = a.beats.size(); --i >= 0;)
            a.curve.setPointTime (i, tempoSequence.toTime (a.beats.getUnchecked (i)));
}

//==============================================================================
//==============================================================================
class TempoSequenceTests : public juce::UnitTest
{
public:
    TempoSequenceTests() : juce::UnitTest ("TempoSequence", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        runPositionTests();
        runModificationTests();
    }

private:
    void expectBarsAndBeats (tempo::Sequence::Position& pos, int bars, int beats)
    {
        auto barsBeats = pos.getBarsBeats();
        expectEquals (barsBeats.bars, bars);
        expectEquals (barsBeats.getWholeBeats(), beats);
    }

    void expectTempoSetting (TempoSetting& tempo, double startTime, double bpm, float curve)
    {
        expectWithinAbsoluteError (tempo.getStartTime().inSeconds(), startTime, 0.001);
        expectWithinAbsoluteError (tempo.getBpm(), bpm, 0.001);
        expectWithinAbsoluteError (tempo.getCurve(), curve, 0.001f);
    }

    void runPositionTests()
    {
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines().getFirst());

        beginTest ("Defaults");
        {
            auto pos = createPosition (edit->tempoSequence);

            expectEquals (pos.getTempo(), 120.0);
            expectEquals (pos.getTimeSignature().numerator, 4);
            expectEquals (pos.getTimeSignature().denominator, 4);
            expectEquals (pos.getPPQTimeOfBarStart(), 0.0);
            expectEquals (pos.getBarsBeats().numerator, 4);
        }

        beginTest ("Positive sequences");
        {
            auto pos = createPosition (edit->tempoSequence);
            pos.set (0_tp);

            expectBarsAndBeats (pos, 0, 0);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 1);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 2);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 3);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 1, 0);

            pos.addBars (1);
            expectBarsAndBeats (pos, 2, 0);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 2, 1);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 2, 2);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 2, 3);
        }

        beginTest ("Negative sequences");
        {
            auto pos = createPosition (edit->tempoSequence);
            pos.set (0_tp);
            pos.addBars (-2);

            expectBarsAndBeats (pos, -2, 0);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -2, 1);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -2, 2);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -2, 3);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -1, 0);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -1, 1);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -1, 2);
            pos.add (1_bd);
            expectBarsAndBeats (pos, -1, 3);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 0);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 1);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 2);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 0, 3);
            pos.add (1_bd);
            expectBarsAndBeats (pos, 1, 0);
            pos.add (1_bd);
        }
    }

    void runModificationTests()
    {
        using namespace std::literals;
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);
        auto& ts = edit->tempoSequence;

        beginTest ("Insertions");
        {
            expectWithinAbsoluteError (ts.getTempoAt (0_tp).getBpm(), 120.0, 0.001);
            expectWithinAbsoluteError (ts.getTempoAt (0_tp).getCurve(), 1.0f, 0.001f);

            ts.insertTempo (4_bp, 120, 0.0);
            ts.insertTempo (4_bp, 300, 0.0);
            ts.insertTempo (8_bp, 300, 0.0);

            expectTempoSetting (*ts.getTempo (0), 0.0, 120.0, 1.0f);
            expectTempoSetting (*ts.getTempo (1), 2.0, 120.0, 0.0f);
            expectTempoSetting (*ts.getTempo (2), 2.0, 300.0, 0.0f);
            expectTempoSetting (*ts.getTempo (3), 2.8, 300.0, 0.0f);

            expectTempoSetting (ts.getTempoAt (0_tp), 0.0, 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (2.0s), 2.0, 300.0, 0.0f);
            expectTempoSetting (ts.getTempoAt (3.0s), 2.8, 300.0, 0.0f);
        }
    }
};

static TempoSequenceTests tempoSequenceTests;

}} // namespace tracktion { inline namespace engine
