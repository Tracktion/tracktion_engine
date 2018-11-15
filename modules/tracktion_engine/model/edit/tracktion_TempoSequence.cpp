/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


template<typename ObjectType>
struct TempoAndTimeSigListBase  : public ValueTreeObjectList<ObjectType>,
                                  private AsyncUpdater
{
    TempoAndTimeSigListBase (TempoSequence& ts, const ValueTree& parent)
        : ValueTreeObjectList<ObjectType> (parent), sequence (ts)
    {
    }

    void newObjectAdded (ObjectType*) override                              { sendChange(); }
    void objectRemoved (ObjectType*) override                               { sendChange(); }
    void objectOrderChanged() override                                      { sendChange(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { sendChange(); }

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
    TempoSettingList (TempoSequence& ts, const ValueTree& parent)
        : TempoAndTimeSigListBase<TempoSetting> (ts, parent)
    {
        rebuildObjects();
    }

    ~TempoSettingList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::TEMPO);
    }

    TempoSetting* createNewObject (const ValueTree& v) override
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
    TimeSigList (TempoSequence& ts, const ValueTree& parent)
        : TempoAndTimeSigListBase<TimeSigSetting> (ts, parent)
    {
        rebuildObjects();
    }

    ~TimeSigList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::TIMESIG);
    }

    TimeSigSetting* createNewObject (const ValueTree& v) override
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
int TempoSequence::TempoSections::size() const
{
    return tempos.size();
}

const TempoSequence::SectionDetails& TempoSequence::TempoSections::getReference (int i) const
{
    return tempos.getReference (i);
}

void TempoSequence::TempoSections::swapWith (juce::Array<SectionDetails>& newTempos)
{
    ++changeCounter;
    tempos.swapWith (newTempos);
}

juce::uint32 TempoSequence::TempoSections::getChangeCount() const
{
    return changeCounter;
}

double TempoSequence::TempoSections::timeToBeats (double time) const
{
    for (int i = tempos.size(); --i > 0;)
    {
        auto& it = tempos.getReference (i);

        if (it.startTime <= time)
            return it.startBeatInEdit + (time - it.startTime) * it.beatsPerSecond;
    }

    auto& it = tempos.getReference (0);
    return it.startBeatInEdit + (time - it.startTime) * it.beatsPerSecond;
}

double TempoSequence::TempoSections::beatsToTime (double beats) const
{
    for (int i = tempos.size(); --i > 0;)
    {
        auto& it = tempos.getReference(i);

        if (beats - it.startBeatInEdit >= 0)
            return it.startTime + it.secondsPerBeat * (beats - it.startBeatInEdit);
    }

    auto& it = tempos.getReference(0);
    return it.startTime + it.secondsPerBeat * (beats - it.startBeatInEdit);
}

//==============================================================================
TempoSequence::TempoSequence (Edit& e) : edit (e)
{
}

TempoSequence::~TempoSequence()
{
    cancelPendingUpdate();
    notifyListenersOfDeletion();
}

UndoManager* TempoSequence::getUndoManager() const noexcept
{
    return &edit.getUndoManager();
}

//==============================================================================
void TempoSequence::setState (const ValueTree& v)
{
    freeResources();

    state = v;

    tempos = std::make_unique<TempoSettingList> (*this, state);
    timeSigs = std::make_unique<TimeSigList> (*this, state);

    if (tempos->objects.isEmpty())
        insertTempo (0.0, nullptr);

    if (timeSigs->objects.isEmpty())
        insertTimeSig (0.0, nullptr);

    tempos->objects.getFirst()->startBeatNumber = 0.0;
    timeSigs->objects.getFirst()->startBeatNumber = 0.0;

    for (int i = 1; i < getNumTempos(); ++i)
        getTempo (i)->startBeatNumber = jmax (getTempo (i)->startBeatNumber.get(),
                                              getTempo (i - 1)->startBeatNumber.get());

    for (int i = 1; i < getNumTimeSigs(); ++i)
        getTimeSig (i)->startBeatNumber = jmax (getTimeSig (i)->startBeatNumber.get(),
                                                getTimeSig (i - 1)->startBeatNumber.get() + 1);

    updateTempoData();
}

void TempoSequence::createEmptyState()
{
    setState (ValueTree (IDs::TEMPOSEQUENCE));
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

const Array<TimeSigSetting*>& TempoSequence::getTimeSigs() const   { return timeSigs->objects; }
int TempoSequence::getNumTimeSigs() const                          { return timeSigs->objects.size(); }
TimeSigSetting* TempoSequence::getTimeSig (int index) const        { return timeSigs->objects[index]; }

const Array<TempoSetting*>& TempoSequence::getTempos() const       { return tempos->objects; }
int TempoSequence::getNumTempos() const                            { return tempos->objects.size(); }
TempoSetting* TempoSequence::getTempo (int index) const            { return tempos->objects[index]; }

TempoSetting::Ptr TempoSequence::insertTempo (double time)
{
    return insertTempo (time, getUndoManager());
}

TempoSetting::Ptr TempoSequence::insertTempo (double beatNum, double bpm, float curve)
{
    return insertTempo (beatNum, bpm, curve, getUndoManager());
}

TempoSetting::Ptr TempoSequence::insertTempo (double time, juce::UndoManager* um)
{
    auto bpm = getBpmAt (time);
    float defaultCurve = 1.0f;

    if (getNumTempos() > 0)
        return insertTempo (roundToInt (timeToBeats (time)), bpm, defaultCurve, um);

    return insertTempo (0, bpm, defaultCurve, um);
}

TempoSetting::Ptr TempoSequence::insertTempo (double beatNum, double bpm, float curve, juce::UndoManager* um)
{
    int index = -1;

    if (getNumTempos() > 0)
        index = state.indexOf (getTempoAtBeat (beatNum).state) + 1;

    state.addChild (TempoSetting::create (beatNum, bpm, curve), index, um);

    return getTempoAtBeat (beatNum);
}

TimeSigSetting::Ptr TempoSequence::insertTimeSig (double time)
{
    return insertTimeSig (time, getUndoManager());
}

TimeSigSetting::Ptr TempoSequence::insertTimeSig (double time, juce::UndoManager* um)
{
    double beatNum = 0.0;
    int index = -1;

    ValueTree newTree (IDs::TIMESIG);
    newTree.setProperty (IDs::numerator, 4, nullptr);
    newTree.setProperty (IDs::denominator, 4, nullptr);

    if (getNumTimeSigs() > 0)
    {
        beatNum = roundToInt (timeToBeats (time));
        auto& prev = getTimeSigAtBeat (beatNum);

        index = state.indexOf (prev.state) + 1;
        newTree = prev.state.createCopy();

        // don't add in the same place as another one
        if (prev.startBeatNumber == beatNum)
            return prev;
    }

    newTree.setProperty (IDs::startBeat, beatNum, nullptr);

    state.addChild (newTree, index, um);

    return getTimeSigAtBeat (beatNum);
}

void TempoSequence::removeTempo (int index, bool remapEdit)
{
    if (index == 0)
        return;

    if (auto* ts = getTempo (index))
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

void TempoSequence::removeTemposBetween (EditTimeRange range, bool remapEdit)
{
    EditTimecodeRemapperSnapshot snap;

    if (remapEdit)
        snap.savePreChangeState (edit);

    for (int i = getNumTempos(); --i > 0;)
        if (auto ts = getTempo(i))
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

void TempoSequence::removeTimeSigsBetween (EditTimeRange range)
{
    for (int i = getNumTimeSigs(); --i > 0;)
        if (auto ts = getTimeSig(i))
            if (range.contains (ts->getPosition().getStart()))
                removeTimeSig (i);
}

void TempoSequence::moveTempoStart (int index, double deltaBeats, bool snapToBeat)
{
    if (index > 0 && deltaBeats != 0.0)
    {
        if (auto t = getTempo (index))
        {
            auto prev = getTempo (index - 1);
            auto next = getTempo (index + 1);

            const double prevBeat = (prev != 0) ? prev->startBeatNumber : 0;
            const double nextBeat = (next != 0) ? next->startBeatNumber : 0x7ffffff;

            const double newStart = jlimit (prevBeat, nextBeat, t->startBeatNumber + deltaBeats);
            t->set (snapToBeat ? roundToInt (newStart) : newStart, t->bpm, t->curve, true);
        }
    }
}

void TempoSequence::moveTimeSigStart (int index, double deltaBeats, bool snapToBeat)
{
    if (index > 0 && deltaBeats != 0.0)
    {
        if (auto t = getTimeSig (index))
        {
            auto prev = getTimeSig (index - 1);
            auto next = getTimeSig (index + 1);

            const double prevBeat = (prev != 0) ? prev->startBeatNumber : 0.0;
            const double nextBeat = (next != 0) ? next->startBeatNumber : 0x7ffffff;

            if (nextBeat < prevBeat + 2)
                return;

            t->startBeatNumber.forceUpdateOfCachedValue();
            const double newBeat = t->startBeatNumber + deltaBeats;
            t->startBeatNumber = jlimit (prevBeat + 1, nextBeat - 1, snapToBeat ? roundToInt (newBeat) : newBeat);
        }
    }
}

void TempoSequence::insertSpaceIntoSequence (double time, double amountOfSpaceInSeconds, bool snapToBeat)
{
    const double beatsToInsert = getBeatsPerSecondAt (time) * amountOfSpaceInSeconds;

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

//==============================================================================
TimeSigSetting& TempoSequence::getTimeSigAt (double time) const
{
    return *getTimeSig (indexOfTimeSigAt (time));
}

TimeSigSetting& TempoSequence::getTimeSigAtBeat (double beat) const
{
    for (int i = getNumTimeSigs(); --i >= 0;)
        if (timeSigs->objects.getUnchecked (i)->startBeatNumber <= beat)
            return *timeSigs->objects.getUnchecked (i);

    jassert (timeSigs->size() > 0);
    return *timeSigs->objects.getFirst();
}

int TempoSequence::indexOfTimeSigAt (double t) const
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
TempoSetting& TempoSequence::getTempoAt (double time) const
{
    return *getTempo (indexOfTempoAt (time));
}

TempoSetting& TempoSequence::getTempoAtBeat (double beat) const
{
    for (int i = getNumTempos(); --i >= 0;)
        if (tempos->objects.getUnchecked (i)->startBeatNumber <= beat)
            return *tempos->objects.getUnchecked (i);

    jassert (tempos->size() > 0);
    return *tempos->objects.getFirst();
}

int TempoSequence::indexOfTempoAt (double t) const
{
    for (int i = getNumTempos(); --i >= 0;)
        if (tempos->objects.getUnchecked (i)->getStartTime() <= t)
            return i;

    jassert (tempos->size() > 0);
    return 0;
}

int TempoSequence::indexOfNextTempoAt (double t) const
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

double TempoSequence::getBpmAt (double time) const
{
    for (int i = internalTempos.size(); --i >= 0;)
    {
        auto& it = internalTempos.getReference (i);

        if (it.startTime <= time || i == 0)
            return it.bpm;
    }

    return 120.0;
}

bool TempoSequence::isTripletsAtTime (double time) const
{
    return getTimeSigAt (time).triplets;
}

//==============================================================================
int TempoSequence::BarsAndBeats::getWholeBeats() const          { return (int) std::floor (beats); }
double TempoSequence::BarsAndBeats::getFractionalBeats() const  { return beats - std::floor (beats); }

TempoSequence::BarsAndBeats TempoSequence::timeToBarsBeats (double t) const
{
    for (int i = internalTempos.size(); --i >= 0;)
    {
        auto& it = internalTempos.getReference (i);

        if (it.startTime <= t || i == 0)
        {
            auto beatsSinceFirstBar = (t - it.timeOfFirstBar) * it.beatsPerSecond;

            if (beatsSinceFirstBar < 0)
            {
                if (t < 0)
                    return { (int) std::floor (beatsSinceFirstBar / it.numerator),
                             it.numerator - std::fmod (-beatsSinceFirstBar, it.numerator) };

                return { it.barNumberOfFirstBar - 1,
                         it.prevNumerator + beatsSinceFirstBar };
            }

            return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
                      std::fmod (beatsSinceFirstBar, it.numerator) };
        }
    }

    return { 0, 0.0 };
}

double TempoSequence::barsBeatsToTime (BarsAndBeats barsBeats) const
{
    for (int i = internalTempos.size(); --i >= 0;)
    {
        auto& it = internalTempos.getReference(i);

        if (it.barNumberOfFirstBar == barsBeats.bars + 1
              && barsBeats.beats >= it.prevNumerator - it.beatsUntilFirstBar)
            return it.timeOfFirstBar - it.secondsPerBeat * (it.prevNumerator - barsBeats.beats);

        if (it.barNumberOfFirstBar <= barsBeats.bars || i == 0)
            return it.timeOfFirstBar + it.secondsPerBeat * (((barsBeats.bars - it.barNumberOfFirstBar) * it.numerator) + barsBeats.beats);
    }

    return 0;
}

double TempoSequence::barsBeatsToBeats (BarsAndBeats barsBeats) const
{
    return timeToBeats (barsBeatsToTime (barsBeats));
}

double TempoSequence::timeToBeats (double time) const
{
    return internalTempos.timeToBeats (time);
}

juce::Range<double> TempoSequence::timeToBeats (EditTimeRange range) const
{
    return { timeToBeats (range.getStart()),
             timeToBeats (range.getEnd()) };
}

double TempoSequence::beatsToTime (double beats) const
{
    return internalTempos.beatsToTime (beats);
}

EditTimeRange TempoSequence::beatsToTime (juce::Range<double> range) const
{
    return { beatsToTime (range.getStart()),
             beatsToTime (range.getEnd()) };
}

static CurvePoint getBezierPoint (const TempoSetting& t1, const TempoSetting& t2) noexcept
{
    auto x1 = t1.startBeatNumber.get();
    auto y1 = t1.getBpm();
    auto x2 = t2.startBeatNumber.get();
    auto y2 = t2.getBpm();
    auto c  = t1.getCurve();

    if (y2 > y1)
    {
        auto run  = x2 - x1;
        auto rise = y2 - y1;

        auto xc = x1 + run / 2;
        auto yc = y1 + rise / 2;

        auto x = xc - run  / 2 * -c;
        auto y = yc + rise / 2 * -c;

        return { x, (float) y };
    }

    auto run  = x2 - x1;
    auto rise = y1 - y2;

    auto xc = x1 + run / 2;
    auto yc = y2 + rise / 2;

    auto x = xc - run  / 2 * -c;
    auto y = yc - rise / 2 * -c;

    return { x, (float) y };
}

static void getBezierEnds (const TempoSetting& t1, const TempoSetting& t2,
                           double& x1out, float& y1out, double& x2out, float& y2out) noexcept
{
    auto x1 = t1.startBeatNumber.get();
    auto y1 = t1.getBpm();
    auto x2 = t2.startBeatNumber.get();
    auto y2 = t2.getBpm();
    auto c  = t1.getCurve();

    auto minic = (std::abs (c) - 0.5f) * 2.0f;
    auto run   = minic * (x2 - x1);
    auto rise  = minic * ((y2 > y1) ? (y2 - y1) : (y1 - y2));

    if (c > 0)
    {
        x1out = x1 + run;
        y1out = (float) y1;

        x2out = x2;
        y2out = (float) (y1 < y2 ? (y2 - rise) : (y2 + rise));
    }
    else
    {
        x1out = x1;
        y1out = (float) (y1 < y2 ? (y1 + rise) : (y1 - rise));

        x2out = x2 - run;
        y2out = (float) y2;
    }
}

static float getBezierYFromX (double x, double x1, double y1, double xb, double yb, double x2, double y2) noexcept
{
    // test for straight lines and bail out
    if (x1 == x2 || y1 == y2)
        return (float) y1;

    // test for endpoints
    if (x <= x1)  return (float) y1;
    if (x >= x2)  return (float) y2;

    // ok, we have a bezier curve with one control point,
    // we know x, we need to find y

    // flip the bezier equation around so its an quadratic equation
    auto a = x1 - 2 * xb + x2;
    auto b = -2 * x1 + 2 * xb;
    auto c = x1 - x;

    // solve for t, [0..1]
    double t;

    if (a == 0)
    {
        t = -c / b;
    }
    else
    {
        t = (-b + std::sqrt (b * b - 4 * a * c)) / (2 * a);

        if (t < 0.0f || t > 1.0f)
            t = (-b - std::sqrt (b * b - 4 * a * c)) / (2 * a);
    }

    jassert (t >= 0.0f && t <= 1.0f);

    // find y using the t we just found
    auto y = (pow (1 - t, 2) * y1) + 2 * t * (1 - t) * yb + pow (t, 2) * y2;
    return (float) y;
}

static double calcCurveBpm (double beat, const TempoSetting& t1, const TempoSetting* t2)
{
    if (t2 == nullptr)
        return t1.bpm;

    auto bp = getBezierPoint (t1, *t2);
    auto c = t1.getCurve();

    if (c >= -0.5 && c <= 0.5)
        return getBezierYFromX (beat,
                                t1.startBeatNumber, t1.bpm,
                                bp.time, bp.value,
                                t2->startBeatNumber, t2->bpm);

    double x1end = 0;
    double x2end = 0;
    float y1end = 0;
    float y2end = 0;

    getBezierEnds (t1, *t2, x1end, y1end, x2end, y2end);

    if (beat >= t1.startBeatNumber && beat <= x1end)
        return y1end;

    if (beat >= x2end && beat <= t2->startBeatNumber)
        return y2end;

    return getBezierYFromX (beat, x1end, y1end, bp.time, bp.value, x2end, y2end);
}

void TempoSequence::updateTempoData()
{
    jassert (getNumTempos() > 0 && getNumTimeSigs() > 0);

    SortedSet<double> beatsWithObjects;

    for (auto tempo : tempos->objects)
        beatsWithObjects.add (tempo->startBeatNumber);

    for (auto sig : timeSigs->objects)
        beatsWithObjects.add (sig->startBeatNumber);

    double time    = 0.0;
    double beatNum = 0.0;
    double ppq     = 0.0;
    int timeSigIdx = 0;
    int tempoIdx   = 0;

    auto currTempo   = getTempo (tempoIdx++);
    auto currTimeSig = getTimeSig (timeSigIdx++);

    jassert (currTempo != nullptr);
    jassert (currTempo->startBeatNumber == 0);
    jassert (currTimeSig->startBeatNumber == 0);
    currTempo->startTime = 0;
    currTimeSig->startTime = 0;

    const bool useDenominator = edit.engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();

    Array<SectionDetails> newSections;

    for (int i = 0; i < beatsWithObjects.size(); ++i)
    {
        auto currentBeat = beatsWithObjects.getUnchecked (i);

        jassert (std::abs (currentBeat - beatNum) < 0.001);

        while (i >= 0 && tempoIdx < getNumTempos() && getTempo (tempoIdx)->startBeatNumber == currentBeat)
        {
            currTempo = getTempo (tempoIdx++);
            jassert (currTempo != nullptr);
            currTempo->startTime = time;
        }

        if (i >= 0 && timeSigIdx < getNumTimeSigs() && getTimeSig (timeSigIdx)->startBeatNumber == currentBeat)
        {
            currTimeSig = getTimeSig (timeSigIdx++);
            jassert (currTimeSig != nullptr);
            currTimeSig->startTime = time;
        }

        auto nextTempo = getTempo (tempoIdx);
        double bpm = nextTempo != nullptr ? calcCurveBpm (currTempo->startBeatNumber, *currTempo, nextTempo)
                                          : currTempo->bpm;

        int numSubdivisions = 1;

        if (nextTempo != nullptr && (currTempo->getCurve() != -1.0f && currTempo->getCurve() != 1.0f))
            numSubdivisions = int (jlimit (1.0, 100.0, 4.0 * (nextTempo->startBeatNumber - currentBeat)));

        const double numBeats = (i < beatsWithObjects.size() - 1)
                                    ? ((beatsWithObjects[i + 1] - currentBeat) / (double) numSubdivisions)
                                    : 1.0e6;

        for (int k = 0; k < numSubdivisions; ++k)
        {
            SectionDetails it;

            it.bpm              = bpm;
            it.numerator        = currTimeSig->numerator;
            it.prevNumerator    = it.numerator;
            it.denominator      = currTimeSig->denominator;
            it.triplets         = currTimeSig->triplets;
            it.startTime        = time;
            it.startBeatInEdit  = beatNum;

            it.secondsPerBeat   = useDenominator ? (240.0 / (bpm * it.denominator))
                                                 : (60.0 / bpm);
            it.beatsPerSecond   = 1.0 / it.secondsPerBeat;

            it.ppqAtStart = ppq;
            ppq += 4 * numBeats / it.denominator;

            if (newSections.isEmpty())
            {
                it.barNumberOfFirstBar = 0;
                it.beatsUntilFirstBar = 0;
                it.timeOfFirstBar = 0.0;
            }
            else
            {
                auto& prevSection = newSections.getReference (newSections.size() - 1);

                auto beatsSincePreviousBarUntilStart = (time - prevSection.timeOfFirstBar) * prevSection.beatsPerSecond;
                auto barsSincePrevBar = (int) ceil (beatsSincePreviousBarUntilStart / prevSection.numerator - 1.0e-5);

                it.barNumberOfFirstBar = prevSection.barNumberOfFirstBar + barsSincePrevBar;

                auto beatNumInEditOfNextBar = roundToInt (prevSection.startBeatInEdit + prevSection.beatsUntilFirstBar)
                                                 + (barsSincePrevBar * prevSection.numerator);

                it.beatsUntilFirstBar = beatNumInEditOfNextBar - it.startBeatInEdit;
                it.timeOfFirstBar = time + it.beatsUntilFirstBar * it.secondsPerBeat;

                for (int j = newSections.size(); --j >= 0;)
                {
                    auto& tempo = newSections.getReference(j);

                    if (tempo.barNumberOfFirstBar < it.barNumberOfFirstBar)
                    {
                        it.prevNumerator = tempo.numerator;
                        break;
                    }
                }
            }

            newSections.add (it);

            time += numBeats * it.secondsPerBeat;
            beatNum += numBeats;

            bpm = calcCurveBpm (beatNum, *currTempo, nextTempo);
        }
    }

    jassert (getNumTempos() > 0 && getNumTimeSigs() > 0);
    triggerAsyncUpdate();

    {
        ScopedLock sl (edit.engine.getDeviceManager().deviceManager.getAudioCallbackLock());
        internalTempos.swapWith (newSections);
    }
}

void TempoSequence::handleAsyncUpdate()
{
    edit.sendTempoOrPitchSequenceChangedUpdates();
    changed();
}

String TempoSequence::getSelectableDescription()
{
    return TRANS("Tempo Curve");
}

int TempoSequence::countTemposInRegion (EditTimeRange range) const
{
    int count = 0;

    for (auto t : tempos->objects)
        if (range.contains (t->getStartTime()))
            ++count;

    return count;
}

int64 TempoSequence::createHashForTemposInRange (EditTimeRange range) const
{
    int64 hash = 0;

    for (auto t : tempos->objects)
        if (range.contains (t->getStartTime()))
            hash ^= t->getHash();

    return hash;
}

//==============================================================================
TempoSequencePosition::TempoSequencePosition (const TempoSequence& s)
   : sequence (s)
{
}

TempoSequencePosition::TempoSequencePosition (const TempoSequencePosition& other)
   : sequence (other.sequence),
     time (other.time),
     index (other.index)
{
}

TempoSequencePosition::~TempoSequencePosition()
{
}

TempoSequence::BarsAndBeats TempoSequencePosition::getBarsBeatsTime() const
{
    auto& it = sequence.internalTempos.getReference (index);
    auto beatsSinceFirstBar = (time - it.timeOfFirstBar) * it.beatsPerSecond;

    if (beatsSinceFirstBar < 0)
        return { it.barNumberOfFirstBar - 1,
                 it.prevNumerator + beatsSinceFirstBar };

    return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
             std::fmod (beatsSinceFirstBar, it.numerator) };
}

void TempoSequencePosition::setTime (double t)
{
    const int maxIndex = sequence.internalTempos.size() - 1;

    if (maxIndex >= 0)
    {
        if (index > maxIndex)
        {
            index = maxIndex;
            time = sequence.internalTempos.getReference (index).startTime;
        }

        if (t >= time)
        {
            while (index < maxIndex && sequence.internalTempos.getReference (index + 1).startTime <= t)
                ++index;
        }
        else
        {
            while (index > 0 && sequence.internalTempos.getReference (index).startTime > t)
                --index;
        }

        time = t;
    }
}

void TempoSequencePosition::addBars (int bars)
{
    if (bars > 0)
    {
        while (--bars >= 0)
            addBeats (sequence.internalTempos.getReference (index).numerator);
    }
    else
    {
        while (++bars <= 0)
            addBeats (-sequence.internalTempos.getReference (index).numerator);
    }
}

void TempoSequencePosition::addBeats (double beats)
{
    if (beats > 0)
    {
        for (;;)
        {
            auto maxIndex = sequence.internalTempos.size() - 1;
            auto& it = sequence.internalTempos.getReference (index);
            auto beatTime = it.secondsPerBeat * beats;

            if (index >= maxIndex
                 || sequence.internalTempos.getReference (index + 1).startTime > time + beatTime)
            {
                time += beatTime;
                break;
            }

            ++index;
            auto nextStart = sequence.internalTempos.getReference (index).startTime;
            beats -= (nextStart - time) * it.beatsPerSecond;
            time = nextStart;
        }
    }
    else
    {
        for (;;)
        {
            auto& it = sequence.internalTempos.getReference (index);
            auto beatTime = it.secondsPerBeat * beats;

            if (index <= 0
                 || sequence.internalTempos.getReference (index).startTime <= time + beatTime)
            {
                time += beatTime;
                break;
            }

            beats += (time - it.startTime) * it.beatsPerSecond;
            time = sequence.internalTempos.getReference (index).startTime;
            --index;
        }
    }
}

void TempoSequencePosition::setBarsBeats (TempoSequence::BarsAndBeats newTime)
{
    setTime (sequence.barsBeatsToTime (newTime));
}

void TempoSequencePosition::addSeconds (double seconds)
{
    setTime (time + seconds);
}

const TempoSequence::SectionDetails& TempoSequencePosition::getCurrentTempo() const
{
    // this index might go off the end when tempos are deleted..
    return sequence.internalTempos.getReference (jlimit (0, sequence.internalTempos.size() - 1, index));
}

double TempoSequencePosition::getPPQTime() const noexcept
{
    auto& it = sequence.internalTempos.getReference (index);
    auto beatsSinceStart = (time - it.startTime) * it.beatsPerSecond;

    return it.ppqAtStart + 4.0 * beatsSinceStart / it.denominator;
}

void TempoSequencePosition::setPPQTime (double ppq)
{
    for (int i = sequence.internalTempos.size(); --i >= 0;)
    {
        index = i;

        if (sequence.internalTempos.getReference (i).ppqAtStart <= ppq)
            break;
    }

    auto& it = sequence.internalTempos.getReference (index);
    auto beatsSinceStart = ((ppq - it.ppqAtStart) * it.denominator) / 4.0;
    time = (beatsSinceStart * it.secondsPerBeat) + it.startTime;
}

double TempoSequencePosition::getPPQTimeOfBarStart() const noexcept
{
    for (int i = index + 1; --i >= 0;)
    {
        auto& it = sequence.internalTempos.getReference (i);
        const double beatsSinceFirstBar = (time - it.timeOfFirstBar) * it.beatsPerSecond;

        if (beatsSinceFirstBar >= -it.beatsUntilFirstBar || i == 0)
        {
            const double beatNumberOfLastBarSinceFirstBar = it.numerator * std::floor (beatsSinceFirstBar / it.numerator);

            return it.ppqAtStart + 4.0 * (it.beatsUntilFirstBar + beatNumberOfLastBarSinceFirstBar) / it.denominator;
        }
    }

    return 0.0;
}

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
            cp.startBeat        = tempoSequence.timeToBeats (pos.getStart());
            cp.endBeat          = tempoSequence.timeToBeats (pos.getEnd());
            cp.contentStartBeat = tempoSequence.timeToBeats (pos.getStartOfSource());
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

            juce::Array<double> beats;
            beats.ensureStorageAllocated (numPoints);

            for (int p = 0; p < numPoints; ++p)
                beats.add (tempoSequence.timeToBeats (curve.getPoint (p).time));

            automation.add ({ curve, std::move (beats) });
        }
    }

    const auto loopRange = ed.getTransport().getLoopRange();
    loopPositionBeats = { tempoSequence.timeToBeats (loopRange.getStart()),
                          tempoSequence.timeToBeats (loopRange.getEnd()) };
}

void EditTimecodeRemapperSnapshot::remapEdit (Edit& ed)
{
    auto& transport = ed.getTransport();
    auto& tempoSequence = ed.tempoSequence;
    tempoSequence.updateTempoData();

    transport.setLoopRange (tempoSequence.beatsToTime (loopPositionBeats));

    for (auto& cp : clips)
    {
        if (Selectable::isSelectableValid (cp.clip))
        {
            auto& c = *cp.clip;

            auto newStart  = tempoSequence.beatsToTime (cp.startBeat);
            auto newEnd    = tempoSequence.beatsToTime (cp.endBeat);
            auto newOffset = newStart - tempoSequence.beatsToTime (cp.contentStartBeat);

            auto pos = c.getPosition();

            if (std::abs (pos.getStart() - newStart)
                 + std::abs (pos.getEnd() - newEnd)
                 + std::abs (pos.getOffset() - newOffset) > 0.001)
            {
                if (c.getSyncType() == Clip::syncAbsolute)
                    continue;

                if (c.type == TrackItem::Type::wave)
                {
                    auto ac = dynamic_cast<AudioClipBase*> (cp.clip);

                    if (ac != nullptr && ac->getAutoTempo())
                        c.setPosition ({ { newStart, newEnd }, newOffset });
                    else
                        c.setStart (newStart, false, true);
                }
                else
                {
                    c.setPosition ({ { newStart, newEnd }, newOffset });
                }
            }
        }
    }

    const ParameterChangeHandler::Disabler disabler (ed.getParameterChangeHandler());

    for (auto& a : automation)
        for (int i = a.beats.size(); --i >= 0;)
            a.curve.setPointTime (i, tempoSequence.beatsToTime (a.beats.getUnchecked (i)));
}
