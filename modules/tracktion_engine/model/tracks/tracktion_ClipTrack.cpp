/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct ClipTrack::ClipList  : public ValueTreeObjectList<Clip>,
                              private AsyncUpdater
{
    ClipList (ClipTrack& ct, const ValueTree& parent)
        : ValueTreeObjectList<Clip> (parent),
          clipTrack (ct)
    {
        rebuildObjects();

        editLoadedCallback = new Edit::LoadFinishedCallback<ClipList> (*this, ct.edit);
        clipTrack.trackItemsDirty = true;
    }

    ~ClipList()
    {
        for (auto c : objects)
            c->setTrack (nullptr);

        freeObjects();
    }

    Clip::Ptr getClipForTree (const ValueTree& v) const
    {
        for (auto c : objects)
            if (c->state == v)
                return c;

        return {};
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return Clip::isClipState (v);
    }

    Clip* createNewObject (const ValueTree& v) override
    {
        if (auto newClip = Clip::createClipForState (v, clipTrack))
        {
            if (newClip->isGrouped())
                clipTrack.refreshCollectionClips (*newClip);

            clipTrack.trackItemsDirty = true;
            newClip->incReferenceCount();

            return newClip.get();
        }

        jassertfalse;
        return {};
    }

    void deleteObject (Clip* c) override
    {
        jassert (c != nullptr);
        if (c == nullptr)
            return;

        clipTrack.trackItemsDirty = true;
        c->decReferenceCount();
    }

    void newObjectAdded (Clip* c) override      { objectAddedOrRemoved (c); }
    void objectRemoved (Clip* c) override       { objectAddedOrRemoved (c); }
    void objectOrderChanged() override          { objectAddedOrRemoved (nullptr); }

    void objectAddedOrRemoved (Clip* c)
    {
        if (c == nullptr || c->type != TrackItem::Type::unknown)
        {
            clipTrack.changed();
            clipTrack.setFrozen (false, Track::groupFreeze);

            if (! clipTrack.edit.isLoading())
                triggerAsyncUpdate();

            clipTrack.trackItemsDirty = true;
        }
    }

    ClipTrack& clipTrack;
    ScopedPointer<Edit::LoadFinishedCallback<ClipList>> editLoadedCallback;

    void valueTreePropertyChanged (ValueTree& v, const Identifier& id) override
    {
        if (Clip::isClipState (v))
        {
            if (id == IDs::start || id == IDs::length)
            {
                triggerAsyncUpdate();
                clipTrack.trackItemsDirty = true;
            }
        }
    }

    void handleAsyncUpdate() override
    {
        Sorter clipSorter;
        clipTrack.state.sort (clipSorter, &clipTrack.edit.getUndoManager(), true);
    }

    void editFinishedLoading()
    {
        editLoadedCallback = nullptr;

        for (auto c : objects)
            if (auto acb = dynamic_cast<AudioClipBase*> (c))
                acb->updateAutoCrossfadesAsync (false);

        clipTrack.trackItemsDirty = true;
    }

    struct Sorter
    {
        static int getPriority (const juce::Identifier& i)
        {
            if (i == IDs::AUTOMATIONTRACK)  return 0;
            if (Clip::isClipState (i))      return 1;
            if (i == IDs::PLUGIN)           return 2;
            if (i == IDs::OUTPUTDEVICES)    return 3;
            if (i == IDs::LFOS)             return 4;

            return -1;
        }

        int compareElements (const juce::ValueTree& first, const juce::ValueTree& second) const noexcept
        {
            auto priority = getPriority (first.getType()) - getPriority (second.getType());

            if (priority != 0)
                return priority;

            if (Clip::isClipState (first) && Clip::isClipState (second))
            {
                double t1 = first[IDs::start];
                double t2 = second[IDs::start];
                return t1 < t2 ? -1 : (t2 < t1 ? 1 : 0);
            }

            return 0;
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipList)
};

//==============================================================================
struct ClipTrack::CollectionClipList  : public ValueTree::Listener
{
    CollectionClipList (ClipTrack& t, ValueTree& v) : ct (t), state (v)
    {
        state.addListener (this);
    }

    ~CollectionClipList()
    {
        state.removeListener (this);
    }

    void valueTreePropertyChanged (ValueTree& v, const Identifier& id) override
    {
        if (id == IDs::groupID)
        {
            if (auto c = ct.findClipForID (EditItemID::fromID (v)))
            {
                for (auto cc : collectionClips)
                {
                    if (cc->containsClip (c))
                    {
                        cc->removeClip (c);

                        if (cc->getNumClips() == 0)
                        {
                            collectionClips.removeObject (cc);
                            ct.trackItemsDirty = true;
                            break;
                        }

                        cc->updateStartAndEnd();
                        ct.trackItemsDirty = true;
                    }
                }

                if (c->isGrouped())
                {
                    auto cc = findOrCreateCollectionClip (c->getGroupID());
                    cc->addClip (c);
                    cc->updateStartAndEnd();
                    ct.trackItemsDirty = true;

                    c->deselect();
                }
            }
        }
        else if (id == IDs::start || id == IDs::length)
        {
            if (auto c = ct.findClipForID (EditItemID::fromID (v)))
            {
                if (c->isGrouped())
                {
                    if (auto cc = findCollectionClip (c->getGroupID()))
                    {
                        cc->updateStartAndEnd();
                        ct.trackItemsDirty = true;
                    }
                }
            }
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree& child) override
    {
        if (Clip::isClipState (child))
        {
            if (auto c = ct.findClipForID (EditItemID::fromID (child)))
            {
                if (c->isGrouped())
                {
                    auto cc = findOrCreateCollectionClip (c->getGroupID());
                    cc->addClip (c);
                    cc->updateStartAndEnd();
                    ct.trackItemsDirty = true;
                }
            }
        }
    }

    void valueTreeChildRemoved (ValueTree&, ValueTree& child, int) override
    {
        if (Clip::isClipState (child))
        {
            for (auto cc : collectionClips)
            {
                if (cc->removeClip (EditItemID::fromID (child)))
                {
                    if (cc->getNumClips() == 0)
                    {
                        collectionClips.removeObject (cc);
                        ct.trackItemsDirty = true;
                        break;
                    }

                    cc->updateStartAndEnd();
                    ct.trackItemsDirty = true;
                }
            }
        }
    }

    CollectionClip* findOrCreateCollectionClip (EditItemID groupID)
    {
        for (auto cc : collectionClips)
            if (cc->getGroupID() == groupID)
                return cc;

        auto cc = new CollectionClip (ct);
        cc->setGroupID (groupID);
        collectionClips.add (cc);
        ct.trackItemsDirty = true;

        return cc;
    }

    CollectionClip* findCollectionClip (EditItemID groupID)
    {
        for (auto cc : collectionClips)
            if (cc->getGroupID() == groupID)
                return cc;

        return {};
    }

    void clipCreated (Clip& c)
    {
        auto cc = findOrCreateCollectionClip (c.getGroupID());
        cc->addClip (&c);
        cc->updateStartAndEnd();
        ct.trackItemsDirty = true;
    }

    void valueTreeChildOrderChanged (ValueTree&, int, int) override {}
    void valueTreeParentChanged (ValueTree&) override {}

    ClipTrack& ct;
    ValueTree& state;

    ReferenceCountedArray<CollectionClip> collectionClips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollectionClipList)
};

//==============================================================================
ClipTrack::ClipTrack (Edit& edit, const ValueTree& v, double defaultHeight, double minHeight, double maxHeight)
    : Track (edit, v, defaultHeight, minHeight, maxHeight)
{
    ClipList::Sorter sorter;
    state.sort (sorter, &edit.getUndoManager(), true);

    collectionClipList = new CollectionClipList (*this, state);
    clipList = new ClipList (*this, state);
}

ClipTrack::~ClipTrack()
{
}

void ClipTrack::flushStateToValueTree()
{
    Track::flushStateToValueTree();

    for (auto c : clipList->objects)
        c->flushStateToValueTree();
}

//==============================================================================
void ClipTrack::refreshTrackItems() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (trackItemsDirty)
    {
        trackItemsDirty = false;

        trackItems.clear();
        trackItems.ensureStorageAllocated (clipList->objects.size());

        for (auto clip : clipList->objects)
            trackItems.add (clip);

        for (auto cc : collectionClipList->collectionClips)
            trackItems.add (cc);

        TrackItemStartTimeSorter sorter;
        trackItems.sort (sorter);
    }
}

int ClipTrack::getNumTrackItems() const
{
    refreshTrackItems();
    return trackItems.size();
}

TrackItem* ClipTrack::getTrackItem (int idx) const
{
    refreshTrackItems();
    return trackItems[idx];
}

int ClipTrack::indexOfTrackItem (TrackItem* ti) const
{
    refreshTrackItems();
    return trackItems.indexOf (ti);
}

int ClipTrack::getIndexOfNextTrackItemAt (double time)
{
    refreshTrackItems();
    return findIndexOfNextItemAt (trackItems, time);
}

TrackItem* ClipTrack::getNextTrackItemAt (double time)
{
    refreshTrackItems();
    return trackItems[getIndexOfNextTrackItemAt (time)];
}

//==============================================================================
void ClipTrack::refreshCollectionClips (Clip& newClip)
{
    collectionClipList->clipCreated (newClip);
}

CollectionClip* ClipTrack::getCollectionClip (int index)  const noexcept
{
    return collectionClipList->collectionClips[index].get();
}

CollectionClip* ClipTrack::getCollectionClip (Clip* clip) const
{
    if (clip->isGrouped())
        for (auto cc : collectionClipList->collectionClips)
            if (cc->containsClip (clip))
                return cc;

    return {};
}

int ClipTrack::getNumCollectionClips() const noexcept
{
    return collectionClipList->collectionClips.size();
}

int ClipTrack::indexOfCollectionClip (CollectionClip* cc) const
{
    return collectionClipList->collectionClips.indexOf (cc);
}

int ClipTrack::getIndexOfNextCollectionClipAt (double time)
{
    return findIndexOfNextItemAt (collectionClipList->collectionClips, time);
}

CollectionClip* ClipTrack::getNextCollectionClipAt (double time)
{
    return collectionClipList->collectionClips [getIndexOfNextCollectionClipAt (time)].get();
}

bool ClipTrack::contains (CollectionClip* cc) const
{
    return collectionClipList->collectionClips.contains (cc);
}

//==============================================================================
const juce::Array<Clip*>& ClipTrack::getClips() const noexcept
{
    return clipList->objects;
}

Clip* ClipTrack::findClipForID (EditItemID id) const
{
    for (auto* c : clipList->objects)
        if (c->itemID == id)
            return c;

    return {};
}

double ClipTrack::getLength() const
{
    return getTotalRange().getEnd();
}

double ClipTrack::getLengthIncludingInputTracks() const
{
    auto l = getLength();

    for (auto t : getAudioTracks (edit))
        if (t != this && t->getOutput().getDestinationTrack() == this)
            l = jmax (l, t->getLengthIncludingInputTracks());

    return l;
}

EditTimeRange ClipTrack::getTotalRange() const
{
    return findUnionOfEditTimeRanges (clipList->objects);
}

void ClipTrack::addClip (const Clip::Ptr& clip)
{
    CRASH_TRACER

    if (clip != nullptr)
    {
        if (clipList->objects.size() < Edit::maxClipsInTrack)
        {
            jassert (findClipForID (clip->itemID) == nullptr);

            auto um = clip->getUndoManager();
            clip->state.getParent().removeChild (clip->state, um);
            state.addChild (clip->state, -1, um);

            changed();
        }
        else
        {
            clip->edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't add any more clips to a single track!"));
        }
    }
}

void ClipTrack::addCollectionClip (CollectionClip* cc)
{
    CollectionClip::Ptr refHolder (cc);

    // if this collection clip has already been automatically created, remove it
    for (int i = collectionClipList->collectionClips.size(); --i >= 0;)
        if (collectionClipList->collectionClips[i]->containsClip (cc->getClip (0).get()))
            collectionClipList->collectionClips.remove (i);

    collectionClipList->collectionClips.add (cc);
}

void ClipTrack::removeCollectionClip (CollectionClip* cc)
{
    collectionClipList->collectionClips.removeObject (cc);
}

//==============================================================================
static void updateClipState (ValueTree& state, const String& name, EditItemID itemID, ClipPosition position)
{
    state.setProperty (IDs::name, name, nullptr);
    state.setProperty (IDs::start, position.getStart(), nullptr);
    state.setProperty (IDs::length, position.getLength(), nullptr);
    state.setProperty (IDs::offset, position.getOffset(), nullptr);
    itemID.writeID (state, nullptr);
}

static ValueTree createNewClipState (const String& name, TrackItem::Type type, EditItemID itemID, ClipPosition position)
{
    ValueTree state (TrackItem::clipTypeToXMLType (type));
    updateClipState (state, name, itemID, position);
    return state;
}

//==============================================================================
Clip* ClipTrack::insertClipWithState (juce::ValueTree clipState)
{
    CRASH_TRACER
    jassert (clipState.isValid());
    jassert (! clipState.getParent().isValid());

    if (clipState.hasType (IDs::MIDICLIP))
    {
        setPropertyIfMissing (clipState, IDs::sync, edit.engine.getEngineBehaviour().areMidiClipsRemappedWhenTempoChanges()
                                                       ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
    }
    else if (clipState.hasType (IDs::AUDIOCLIP) || clipState.hasType (IDs::EDITCLIP))
    {
        if (! clipState.getChildWithName (IDs::LOOPINFO).isValid())
        {
            auto sourceFile = SourceFileReference::findFileFromString (edit, clipState[IDs::source]);

            if (sourceFile.exists())
            {
                auto loopInfo = AudioFile (sourceFile).getInfo().loopInfo;

                if (loopInfo.getRootNote() != -1)
                    clipState.setProperty (IDs::autoPitch, true, nullptr);

                if (loopInfo.isLoopable())
                {
                    clipState.setProperty (IDs::autoTempo, true, nullptr);
                    clipState.setProperty (IDs::stretchMode, true, nullptr);
                    clipState.setProperty (IDs::elastiqueMode, (int) TimeStretcher::elastiquePro, nullptr);

                    auto& ts = edit.tempoSequence;

                    auto startBeat = ts.timeToBeats (clipState[IDs::start]);
                    auto endBeat   = startBeat + loopInfo.getNumBeats();
                    auto newLength = ts.beatsToTime (endBeat) - ts.beatsToTime (startBeat);

                    clipState.setProperty (IDs::length, newLength, nullptr);
                }

                auto loopSate = loopInfo.state;

                if (loopSate.getNumProperties() > 0 || loopSate.getNumChildren() > 0)
                    clipState.addChild (loopSate.createCopy(), -1, nullptr);
            }
        }

        if (! clipState.hasProperty (IDs::sync))
        {
            if (clipState.getProperty (IDs::autoTempo))
                clipState.setProperty (IDs::sync, (int) edit.engine.getEngineBehaviour().areAutoTempoClipsRemappedWhenTempoChanges() ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
            else
                clipState.setProperty (IDs::sync, (int) edit.engine.getEngineBehaviour().areAudioClipsRemappedWhenTempoChanges() ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
        }

        if (! clipState.hasProperty (IDs::autoCrossfade))
            if (edit.engine.getPropertyStorage().getProperty (SettingID::xFade, 0))
                clipState.setProperty (IDs::autoCrossfade, true, nullptr);
    }

    state.addChild (clipState, -1, &edit.getUndoManager());

    if (auto newClip = clipList->getClipForTree (clipState))
    {
        if (auto at = dynamic_cast<AudioTrack*> (this))
        {
            if (newClip->getColour() == newClip->getDefaultColour())
            {
                float hue = ((at->getAudioTrackNumber() - 1) % 9) / 9.0f;
                newClip->setColour (newClip->getDefaultColour().withHue (hue));
            }
        }

        return newClip.get();
    }

    jassertfalse;
    return {};
}

Clip* ClipTrack::insertClipWithState (const ValueTree& stateToUse, const juce::String& name, TrackItem::Type type,
                                      ClipPosition position, bool deleteExistingClips, bool allowSpottingAdjustment)
{
    CRASH_TRACER

    if (position.getStart() >= Edit::maximumLength)
        return {};

    if (position.time.end > Edit::maximumLength)
        position.time.end = Edit::maximumLength;

    setFrozen (false, groupFreeze);

    if (deleteExistingClips)
        deleteRegion (position.time, nullptr);

    auto newClipID = edit.createNewItemID();

    ValueTree newState;

    if (stateToUse.isValid())
    {
        jassert (stateToUse.hasType (TrackItem::clipTypeToXMLType (type)));
        newState = stateToUse;
        updateClipState (newState, name, newClipID, position);
    }
    else
    {
        newState = createNewClipState (name, type, newClipID, position);
    }

    if (auto newClip = insertClipWithState (newState))
    {
        if (allowSpottingAdjustment)
            newClip->setStart (jmax (0.0, newClip->getPosition().getStart() - newClip->getSpottingPoint()), false, false);

        return newClip;
    }

    return {};
}

WaveAudioClip::Ptr ClipTrack::insertWaveClip (const juce::String& name, const juce::File& sourceFile,
                                              ClipPosition position, bool deleteExistingClips)
{
    CRASH_TRACER

    if (auto proj = ProjectManager::getInstance()->getProject (edit))
    {
        if (auto source = proj->createNewItem (sourceFile, ProjectItem::waveItemType(),
                                               name, {}, ProjectItem::Category::imported, true))
            return insertWaveClip (name, source->getID(), position, deleteExistingClips);

        jassertfalse;
        return {};
    }

    // Insert with a relative path if possible, otherwise an absolute
    {
        auto newState = createNewClipState (name, TrackItem::Type::wave, edit.createNewItemID(), position);
        const bool useRelativePath = edit.filePathResolver && edit.editFileRetriever && edit.editFileRetriever().existsAsFile();
        newState.setProperty (IDs::source, SourceFileReference::findPathFromFile (edit, sourceFile, useRelativePath), nullptr);

        if (auto c = insertClipWithState (newState, name, TrackItem::Type::wave, position, deleteExistingClips, false))
        {
            if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                return *wc;

            jassertfalse;
        }
    }

    jassertfalse;
    return {};
}

WaveAudioClip::Ptr ClipTrack::insertWaveClip (const juce::String& name, ProjectItemID sourceID,
                                              ClipPosition position, bool deleteExistingClips)
{
    CRASH_TRACER

    auto newState = createNewClipState (name, TrackItem::Type::wave, edit.createNewItemID(), position);
    newState.setProperty (IDs::source, sourceID.toString(), nullptr);

    if (auto c = insertClipWithState (newState, name, TrackItem::Type::wave, position, deleteExistingClips, false))
    {
        if (auto wc = dynamic_cast<WaveAudioClip*> (c))
            return *wc;

        jassertfalse;
    }

    return {};
}

MidiClip::Ptr ClipTrack::insertMIDIClip (const juce::String& name, EditTimeRange position, SelectionManager* sm)
{
    if (auto t = dynamic_cast<AudioTrack*> (this))
        if (! containsAnyMIDIClips())
            t->setVerticalScaleToDefault();

    if (auto c = insertNewClip (TrackItem::Type::midi, name, position, sm))
    {
        if (auto mc = dynamic_cast<MidiClip*> (c))
            return *mc;

        jassertfalse;
    }

    return {};
}

MidiClip::Ptr ClipTrack::insertMIDIClip (EditTimeRange position, SelectionManager* sm)
{
    return insertMIDIClip (TrackItem::getSuggestedNameForNewItem (TrackItem::Type::midi), position, sm);
}

EditClip::Ptr ClipTrack::insertEditClip (EditTimeRange position, ProjectItemID sourceID)
{
    CRASH_TRACER

    auto name = TrackItem::getSuggestedNameForNewItem (TrackItem::Type::edit);
    auto newState = createNewClipState (name, TrackItem::Type::edit, edit.createNewItemID(), { position, 0.0 });
    newState.setProperty (IDs::source, sourceID.toString(), nullptr);

    if (auto c = insertClipWithState (newState, name, TrackItem::Type::wave, { position, 0.0 }, false, false))
    {
        if (auto ec = dynamic_cast<EditClip*> (c))
            return *ec;

        jassertfalse;
    }

    return {};
}

void ClipTrack::deleteRegion (EditTimeRange range, SelectionManager* sm)
{
    CRASH_TRACER
    setFrozen (false, groupFreeze);
    setFrozen (false, individualFreeze);

    // make a copied list first, as they'll get moved out-of-order..
    Clip::Array clipsToDo;

    for (auto c : getClips())
        if (c->getPosition().time.overlaps (range))
            clipsToDo.add (c);

    for (int i = clipsToDo.size(); --i >= 0;)
        deleteRegionOfClip (clipsToDo.getUnchecked (i), range, sm);
}

void ClipTrack::deleteRegionOfClip (Clip::Ptr c, EditTimeRange range, SelectionManager* sm)
{
    jassert (c != nullptr);

    CRASH_TRACER
    setFrozen (false, groupFreeze);
    setFrozen (false, individualFreeze);

    auto pos = c->getPosition();

    if (range.contains (pos.time))
    {
        c->removeFromParentTrack();
    }
    else if (pos.getStart() < range.getStart() && pos.getEnd() > range.getEnd())
    {
        if (auto newClip = splitClip (*c, range.getStart()))
        {
            newClip->setStart (range.getEnd(), true, false);
            c->setEnd (range.getStart(), true);
            c->deselect();

            if (sm != nullptr)
                sm->addToSelection (newClip);
        }
    }
    else
    {
        c->trimAwayOverlap (range);
    }
}

Clip* ClipTrack::insertNewClip (TrackItem::Type type, const juce::String& name, EditTimeRange pos, SelectionManager* sm)
{
    return insertNewClip (type, name, { pos, 0.0 }, sm);
}

Clip* ClipTrack::insertNewClip (TrackItem::Type type, const juce::String& name, ClipPosition position, SelectionManager* sm)
{
    CRASH_TRACER

    if (auto newClip = insertClipWithState ({}, name, type, position, false, false))
    {
        if (sm != nullptr)
        {
            sm->selectOnly (newClip);
            sm->keepSelectedObjectsOnScreen();
        }

        return newClip;
    }

    return {};
}

Clip* ClipTrack::insertNewClip (TrackItem::Type type, EditTimeRange pos, SelectionManager* sm)
{
    return insertNewClip (type, TrackItem::getSuggestedNameForNewItem (type), pos, sm);
}

bool ClipTrack::containsAnyMIDIClips() const
{
    for (auto& c : clipList->objects)
        if (c->isMidi())
            return true;

    return false;
}

inline String incrementLastDigit (const String& in)
{
    int digitCount = 0;

    for (int i = in.length(); --i >= 0;)
    {
        if (CharacterFunctions::isDigit (in[i]))
            digitCount++;
        else
            break;
    }

    if (digitCount == 0)
        return in + " 2";

    return in.dropLastCharacters (digitCount) + String (in.getTrailingIntValue() + 1);
}

Clip* ClipTrack::splitClip (Clip& clip, const double time)
{
    CRASH_TRACER
    setFrozen (false, groupFreeze);

    if (clipList->objects.contains (&clip)
         && clip.getPosition().time.reduced (0.001).contains (time)
         && ! clip.isGrouped())
    {
        auto newClipState = clip.state.createCopy();
        edit.createNewItemID().writeID (newClipState, nullptr);

        if (auto newClip = insertClipWithState (newClipState))
        {
            // special case for waveaudio clips that may have fade in/out
            if (auto acb = dynamic_cast<AudioClipBase*> (newClip))
                acb->setFadeIn (0.0);

            if (auto acb = dynamic_cast<AudioClipBase*> (&clip))
                acb->setFadeOut (0.0);

            // need to do this after setting the fades, so the fades don't
            // get mucked around with..
            newClip->setStart (time, true, false);
            clip.setEnd (time, true);

            // special case for marker clips, set the marker number
            if (auto mc1 = dynamic_cast<MarkerClip*> (&clip))
            {
                if (auto mc2 = dynamic_cast<MarkerClip*> (newClip))
                {
                    auto id = edit.getMarkerManager().getNextUniqueID (mc1->getMarkerID());
                    mc2->setMarkerID (id);

                    if (mc1->getName() == (TRANS("Marker") + " " + String (mc1->getMarkerID())))
                        mc2->setName (TRANS("Marker") + " " + String (id));
                    else
                        mc2->setName (incrementLastDigit (mc1->getName()));
                }
            }

            // fiddle with offsets for looped clips
            if (newClip->getLoopLengthBeats() > 0)
            {
                auto extra = roundToInt (std::floor (newClip->getOffsetInBeats() / newClip->getLoopLengthBeats() + 0.00001));

                if (extra != 0)
                {
                    auto newOffsetBeats = newClip->getOffsetInBeats() - (newClip->getLoopLengthBeats() * extra);
                    auto offset = newOffsetBeats / edit.tempoSequence.getBeatsPerSecondAt (newClip->getPosition().getStart());

                    newClip->setOffset (offset);
                }
            }

            return newClip;
        }
    }

    return {};
}

void ClipTrack::splitAt (double time)
{
    CRASH_TRACER
    // make a copied list first, as they'll get moved out-of-order..
    Clip::Array clipsToDo;

    for (auto c : *clipList)
        if (c->getPosition().time.contains (time))
            clipsToDo.add (c);

    for (auto c : clipsToDo)
        splitClip (*c, time);
}

void ClipTrack::insertSpaceIntoTrack (double time, double amountOfSpace)
{
    CRASH_TRACER
    Track::insertSpaceIntoTrack (time, amountOfSpace);

    // make a copied list first, as they'll get moved out-of-order..
    Clip::Array clipsToDo;
    const auto& clips = getClips();

    for (int i = clips.size(); --i >= 0;)
    {
        auto c = clips.getUnchecked (i);

        if (c->getPosition().time.getCentre() >= time)
            clipsToDo.add (c);
        else
            break;
    }

    for (int i = clipsToDo.size(); --i >= 0;)
        if (auto c = clipsToDo.getUnchecked (i).get())
            c->setStart (c->getPosition().getStart() + amountOfSpace, false, true);
}

juce::Array<double> ClipTrack::findAllTimesOfInterest()
{
    juce::Array<double> cuts;

    for (auto& o : clipList->objects)
        cuts.addArray (o->getInterestingTimes());

    cuts.sort();
    return cuts;
}

double ClipTrack::getNextTimeOfInterest (double t)
{
    if (t < 0)
        return 0;

    for (auto c : findAllTimesOfInterest())
        if (c > t + 0.0001)
            return c;

    return getLength();
}

double ClipTrack::getPreviousTimeOfInterest (double t)
{
    if (t < 0.0)
        return {};

    auto cuts = findAllTimesOfInterest();

    for (int i = cuts.size(); --i >= 0;)
        if (cuts.getUnchecked (i) < t - 0.0001)
            return cuts.getUnchecked (i);

    return {};
}

bool ClipTrack::containsPlugin (const Plugin* plugin) const
{
    if (pluginList.contains (plugin))
        return true;

    for (auto c : clipList->objects)
        if (auto plugins = c->getPluginList())
            if (plugins->contains (plugin))
                return true;

    return false;
}

Plugin::Array ClipTrack::getAllPlugins() const
{
    auto destArray = Track::getAllPlugins();

    for (auto c : clipList->objects)
        destArray.addArray (c->getAllPlugins());

    return destArray;
}

void ClipTrack::sendMirrorUpdateToAllPlugins (Plugin& p) const
{
    pluginList.sendMirrorUpdateToAllPlugins (p);

    for (auto c : clipList->objects)
        c->sendMirrorUpdateToAllPlugins (p);
}

bool ClipTrack::areAnyClipsUsingFile (const AudioFile& af)
{
    for (auto c : clipList->objects)
        if (auto acb = dynamic_cast<AudioClipBase*> (c))
            if (acb->isUsingFile (af))
                return true;

    return false;
}
