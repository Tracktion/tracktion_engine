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

//==============================================================================
struct ClipTrack::CollectionClipList  : public juce::ValueTree::Listener
{
    CollectionClipList (ClipTrack& t, juce::ValueTree& v) : ct (t), state (v)
    {
        state.addListener (this);
    }

    ~CollectionClipList() override
    {
        state.removeListener (this);
    }

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
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

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child) override
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

    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int) override
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

    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    ClipTrack& ct;
    juce::ValueTree& state;

    juce::ReferenceCountedArray<CollectionClip> collectionClips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollectionClipList)
};

//==============================================================================
ClipTrack::ClipTrack (Edit& ed, const juce::ValueTree& v, double defaultHeight, double minHeight, double maxHeight)
    : Track (ed, v, defaultHeight, minHeight, maxHeight)
{
    collectionClipList.reset (new CollectionClipList (*this, state));
}

ClipTrack::~ClipTrack()
{
}

void ClipTrack::initialise()
{
    initialiseClipOwner (edit, state);
    Track::initialise();
}

void ClipTrack::flushStateToValueTree()
{
    Track::flushStateToValueTree();

    for (auto c : getClips())
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
        trackItems.ensureStorageAllocated (getClips().size());

        for (auto clip : getClips())
            trackItems.add (clip);

        for (auto cc : collectionClipList->collectionClips)
            trackItems.add (cc);

        TrackItem::sortByTime (trackItems);
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

int ClipTrack::getIndexOfNextTrackItemAt (TimePosition time)
{
    refreshTrackItems();
    return findIndexOfNextItemAt (trackItems, time);
}

TrackItem* ClipTrack::getNextTrackItemAt (TimePosition time)
{
    refreshTrackItems();
    return trackItems[getIndexOfNextTrackItemAt (time)];
}

//==============================================================================
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

int ClipTrack::getIndexOfNextCollectionClipAt (TimePosition time)
{
    return findIndexOfNextItemAt (collectionClipList->collectionClips, time);
}

CollectionClip* ClipTrack::getNextCollectionClipAt (TimePosition time)
{
    return collectionClipList->collectionClips [getIndexOfNextCollectionClipAt (time)].get();
}

bool ClipTrack::contains (CollectionClip* cc) const
{
    return collectionClipList->collectionClips.contains (cc);
}

//==============================================================================
Clip* ClipTrack::findClipForID (EditItemID id) const
{
    for (auto c : getClips())
        if (c->itemID == id)
            return c;

    return {};
}

TimeDuration ClipTrack::getLength() const
{
    return toDuration (getTotalRange().getEnd());
}

TimeDuration ClipTrack::getLengthIncludingInputTracks() const
{
    auto l = getLength();

    for (auto t : getAudioTracks (edit))
        if (t != this && t->getOutput().getDestinationTrack() == this)
            l = std::max (l, t->getLengthIncludingInputTracks());

    return l;
}

TimeRange ClipTrack::getTotalRange() const
{
    return findUnionOfEditTimeRanges (getClips());
}

bool ClipTrack::addClip (const Clip::Ptr& clip)
{
    CRASH_TRACER

    if (clip != nullptr)
    {
        if (getClips().size() < edit.engine.getEngineBehaviour().getEditLimits().maxClipsInTrack)
        {
            jassert (findClipForID (clip->itemID) == nullptr);

            auto um = clip->getUndoManager();
            clip->state.getParent().removeChild (clip->state, um);
            state.addChild (clip->state, -1, um);

            changed();
            return true;
        }
        else
        {
            clip->edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't add any more clips to this track!"));
        }
    }
    return false;
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
Clip* ClipTrack::insertClipWithState (juce::ValueTree clipState)
{
    return engine::insertClipWithState (*this, clipState);
}

Clip* ClipTrack::insertClipWithState (const juce::ValueTree& stateToUse, const juce::String& name, TrackItem::Type type,
                                      ClipPosition position, bool deleteExistingClips, bool allowSpottingAdjustment)
{
    return engine::insertClipWithState (*this, stateToUse, name, type,
                                        position, deleteExistingClips ? DeleteExistingClips::yes : DeleteExistingClips::no, allowSpottingAdjustment);
}

WaveAudioClip::Ptr ClipTrack::insertWaveClip (const juce::String& name, const juce::File& sourceFile,
                                              ClipPosition position, bool deleteExistingClips)
{
    return engine::insertWaveClip (*this, name, sourceFile, position, deleteExistingClips ? DeleteExistingClips::yes : DeleteExistingClips::no);
}

WaveAudioClip::Ptr ClipTrack::insertWaveClip (const juce::String& name, ProjectItemID sourceID,
                                              ClipPosition position, bool deleteExistingClips)
{
    return engine::insertWaveClip (*this, name, sourceID, position, deleteExistingClips ? DeleteExistingClips::yes : DeleteExistingClips::no);
}

MidiClip::Ptr ClipTrack::insertMIDIClip (const juce::String& name, TimeRange position, SelectionManager* sm)
{
    if (auto newClip = engine::insertMIDIClip (*this, name, position))
    {
        if (sm != nullptr)
        {
            sm->selectOnly (newClip.get());
            sm->keepSelectedObjectsOnScreen();
        }

        return newClip;
    }

    return {};
}

MidiClip::Ptr ClipTrack::insertMIDIClip (TimeRange position, SelectionManager* sm)
{
    return insertMIDIClip (TrackItem::getSuggestedNameForNewItem (TrackItem::Type::midi), position, sm);
}

EditClip::Ptr ClipTrack::insertEditClip (TimeRange position, ProjectItemID sourceID)
{
    return engine::insertEditClip (*this, position, sourceID);
}

void ClipTrack::deleteRegion (TimeRange range, SelectionManager* sm)
{
    auto newClips = engine::deleteRegion (*this, range);

    if (sm != nullptr)
        for (auto newClip : newClips)
            sm->addToSelection (newClip);
}

void ClipTrack::deleteRegionOfClip (Clip::Ptr c, TimeRange range, SelectionManager* sm)
{
    jassert (c != nullptr);
    auto newClips = engine::deleteRegion (*c, range);

    if (sm != nullptr)
        for (auto newClip : newClips)
            sm->addToSelection (newClip);
}

Clip* ClipTrack::insertNewClip (TrackItem::Type type, const juce::String& name, TimeRange pos, SelectionManager* sm)
{
    return insertNewClip (type, name, { pos, 0_td }, sm);
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

Clip* ClipTrack::insertNewClip (TrackItem::Type type, TimeRange pos, SelectionManager* sm)
{
    return insertNewClip (type, TrackItem::getSuggestedNameForNewItem (type), pos, sm);
}

bool ClipTrack::containsAnyMIDIClips() const
{
    return engine::containsAnyMIDIClips (*this);
}

juce::ValueTree& ClipTrack::getClipOwnerState()
{
    return state;
}

Selectable* ClipTrack::getClipOwnerSelectable()
{
    return this;
}

Edit& ClipTrack::getClipOwnerEdit()
{
    return edit;
}

void ClipTrack::clipCreated (Clip& c)
{
    if (c.isGrouped())
        collectionClipList->clipCreated (c);

    trackItemsDirty = true;
}

void ClipTrack::clipAddedOrRemoved()
{
    changed();
    setFrozen (false, Track::groupFreeze);
    trackItemsDirty = true;
}

void ClipTrack::clipOrderChanged()
{
    changed();
    setFrozen (false, Track::groupFreeze);
    trackItemsDirty = true;
}

void ClipTrack::clipPositionChanged()
{
    trackItemsDirty = true;
}

inline juce::String incrementLastDigit (const juce::String& in)
{
    int digitCount = 0;

    for (int i = in.length(); --i >= 0;)
    {
        if (juce::CharacterFunctions::isDigit (in[i]))
            digitCount++;
        else
            break;
    }

    if (digitCount == 0)
        return in + " 2";

    return in.dropLastCharacters (digitCount)
            + juce::String (in.getTrailingIntValue() + 1);
}

Clip* ClipTrack::splitClip (Clip& clip, const TimePosition time)
{
    return split (clip, time);
}

void ClipTrack::splitAt (TimePosition time)
{
    engine::split (*this, time);
}

void ClipTrack::insertSpaceIntoTrack (TimePosition time, TimeDuration amountOfSpace)
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

juce::Array<TimePosition> ClipTrack::findAllTimesOfInterest()
{
    juce::Array<TimePosition> cuts;

    for (auto& o : getClips())
        cuts.addArray (o->getInterestingTimes());

    cuts.sort();
    return cuts;
}

TimePosition ClipTrack::getNextTimeOfInterest (TimePosition t)
{
    if (t < TimePosition())
        return TimePosition();

    for (auto c : findAllTimesOfInterest())
        if (c > t + TimeDuration::fromSeconds (0.0001))
            return c;

    return toPosition (getLength());
}

TimePosition ClipTrack::getPreviousTimeOfInterest (TimePosition t)
{
    if (t < TimePosition())
        return {};

    auto cuts = findAllTimesOfInterest();

    for (int i = cuts.size(); --i >= 0;)
        if (cuts.getUnchecked (i) < t - TimeDuration::fromSeconds (0.0001))
            return cuts.getUnchecked (i);

    return {};
}

bool ClipTrack::containsPlugin (const Plugin* plugin) const
{
    if (pluginList.contains (plugin))
        return true;

    for (auto c : getClips())
        if (auto plugins = c->getPluginList())
            if (plugins->contains (plugin))
                return true;

    return false;
}

Plugin::Array ClipTrack::getAllPlugins() const
{
    auto destArray = Track::getAllPlugins();

    for (auto c : getClips())
    {
        destArray.addArray (c->getAllPlugins());

        if (auto containerClip = dynamic_cast<ContainerClip*> (c))
            for (auto childClip : containerClip->getClips())
                destArray.addArray (childClip->getAllPlugins());
    }

    return destArray;
}

void ClipTrack::sendMirrorUpdateToAllPlugins (Plugin& p) const
{
    pluginList.sendMirrorUpdateToAllPlugins (p);

    for (auto c : getClips())
        c->sendMirrorUpdateToAllPlugins (p);
}

bool ClipTrack::areAnyClipsUsingFile (const AudioFile& af)
{
    for (auto c : getClips())
        if (auto acb = dynamic_cast<AudioClipBase*> (c))
            if (acb->isUsingFile (af))
                return true;

    return false;
}

}} // namespace tracktion { inline namespace engine
