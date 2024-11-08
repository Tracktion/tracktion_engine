/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

Project::Ptr getProjectForEdit (const Edit& e)
{
    return e.engine.getProjectManager().getProject (e);
}

ProjectItem::Ptr getProjectItemForEdit (const Edit& e)
{
    return e.engine.getProjectManager().getProjectItem (e);
}

juce::File getEditFileFromProjectManager (Edit& edit)
{
    if (auto item = getProjectItemForEdit (edit))
        return item->getSourceFile();

    return {};
}

bool referencesProjectItem (Edit& edit, ProjectItemID itemID)
{
    for (auto e : Exportable::addAllExportables (edit))
        for (auto& i : e->getReferencedItems())
            if (i.itemID == itemID)
                return true;

    return false;
}

//==============================================================================
//==============================================================================
void insertSpaceIntoEdit (Edit& edit, TimeRange timeRange)
{
    const bool doTempoTrackFirst = ! edit.getTimecodeFormat().isBarsBeats();
    const auto time = timeRange.getStart();
    const auto length = timeRange.getLength();

    if (doTempoTrackFirst)
        edit.getTempoTrack()->insertSpaceIntoTrack (time, length);

    for (auto ct : getTracksOfType<ClipTrack> (edit, true))
    {
        ct->splitAt (time);
        ct->insertSpaceIntoTrack (time, length);
    }

    if (! doTempoTrackFirst)
        edit.getTempoTrack()->insertSpaceIntoTrack (time, length);
}

void insertSpaceIntoEditFromBeatRange (Edit& edit, BeatRange beatRange)
{
    auto& ts = edit.tempoSequence;
    const auto timeToInsertAt = ts.toTime (beatRange.getStart());
    auto& tempoAtInsertionPoint = ts.getTempoAt (timeToInsertAt - TimeDuration::fromSeconds (0.0001));

    const auto lengthInTimeToInsert = TimeDuration::fromSeconds (beatRange.getLength().inBeats() * tempoAtInsertionPoint.getApproxBeatLength().inSeconds());
    insertSpaceIntoEdit (edit, TimeRange (timeToInsertAt, lengthInTimeToInsert));
}

EditItem* findEditItemForID (Edit& edit, EditItemID itemID)
{
    if (auto item = findTrackForID (edit, itemID))
        return item;

    if (auto item = findClipForID (edit, itemID))
        return item;

    if (auto item = findPluginForID (edit, itemID))
        return item.get();

    if (auto item = findModifierForID (edit, itemID))
        return item.get();

    if (auto item = findClipSlotForID (edit, itemID))
        return item;

    if (auto item = edit.getRackList().getRackTypeForID (itemID))
        return item.get();

    return {};
}

//==============================================================================
juce::Array<Track*> getAllTracks (const Edit& edit)
{
    juce::Array<Track*> tracks;
    edit.visitAllTracks ([&] (Track& t) { tracks.add (&t); return true; }, true);
    return tracks;
}

juce::Array<Track*> getTopLevelTracks (const Edit& edit)
{
    juce::Array<Track*> tracks;
    edit.visitAllTracks ([&] (Track& t) { tracks.add (&t); return true; }, false);
    return tracks;
}

juce::Array<AudioTrack*> getAudioTracks (const Edit& edit)
{
    return getTracksOfType<AudioTrack> (edit, true);
}

juce::Array<ClipTrack*> getClipTracks (const Edit& edit)
{
    return getTracksOfType<ClipTrack> (edit, true);
}

int getTotalNumTracks (const Edit& edit)
{
    int count = 0;
    edit.visitAllTracksRecursive ([&] (Track&) { ++count; return true; });
    return count;
}

Track* findTrackForID (const Edit& edit, EditItemID id)
{
    return findTrackForPredicate (edit, [id] (Track& t) { return t.itemID == id; });
}

AudioTrack* findAudioTrackForID (const Edit& edit, EditItemID id)
{
    return dynamic_cast<AudioTrack*> (findTrackForID (edit, id));
}

juce::Array<Track*> findTracksForIDs (const Edit& edit, const juce::Array<EditItemID>& ids)
{
    juce::Array<Track*> tracks;

    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      if (ids.contains (t.itemID))
                                          tracks.add (&t);

                                      return true;
                                  });

    return tracks;
}

Track* findTrackForState (const Edit& edit, const juce::ValueTree& v)
{
    return findTrackForPredicate (edit, [&] (Track& t) { return t.state == v; });
}

AudioTrack* getFirstAudioTrack (const Edit& edit)
{
    return getAudioTracks (edit).getFirst();
}

bool containsTrack (const Edit& edit, const Track& track)
{
    return findTrackForPredicate (edit, [&track] (Track& t) { return &track == &t; }) != nullptr;
}

TrackOutput* getTrackOutput (Track& track)
{
    if (auto t = dynamic_cast<AudioTrack*> (&track))
        return &t->getOutput();

    if (auto t = dynamic_cast<FolderTrack*> (&track))
        return t->getOutput();

    return {};
}

juce::BigInteger toBitSet (const juce::Array<Track*>& tracks)
{
    juce::BigInteger bitset;

    if (auto first = tracks[0])
    {
        auto allTracks = getAllTracks (first->edit);

        for (auto t : allTracks)
            if (int index = allTracks.indexOf (t); index >= 0)
                bitset.setBit (index);
    }

    return bitset;
}

juce::Array<Track*> toTrackArray (Edit& edit, const juce::BigInteger& tracksToAdd)
{
    juce::Array<Track*> tracks;

    auto allTracks = getAllTracks (edit);

    for (auto bit = tracksToAdd.findNextSetBit (0); bit != -1; bit = tracksToAdd.findNextSetBit (bit + 1))
        tracks.add (allTracks[bit]);

    return tracks;
}

juce::Array<Track*> findAllTracksContainingSelectedItems (const SelectableList& items)
{
    auto tracks = items.getItemsOfType<Track>();

    if (tracks.isEmpty())
        for (auto& i : items.getItemsOfType<TrackItem>())
            if (auto t = i->getTrack())
                tracks.addIfNotAlreadyThere (t);

    if (tracks.isEmpty())
        for (auto& cs : items.getItemsOfType<ClipSlot>())
            tracks.addIfNotAlreadyThere (&cs->track);

    std::sort (tracks.begin(), tracks.end(),
               [] (Track* t1, Track* t2)
               {
                   return t1->getIndexInEditTrackList() < t2->getIndexInEditTrackList();
               });

    return tracks;
}

ClipTrack* findFirstClipTrackFromSelection (const SelectableList& items)
{
    auto clipTracks = items.getItemsOfType<ClipTrack>();

    if (clipTracks.isEmpty())
        for (auto& clip : items.getItemsOfType<Clip>())
            clipTracks.add (clip->getClipTrack());

    ClipTrack* firstTrack = nullptr;
    auto firstIndex = std::numeric_limits<int>::max();

    for (auto& t : clipTracks)
    {
        auto index = t->getIndexInEditTrackList();

        if (index < firstIndex)
        {
            firstIndex = index;
            firstTrack = t;
        }
    }

    return firstTrack;
}

Clip::Ptr duplicateClip (const Clip& c)
{
    auto n = c.state.createCopy();
    EditItemID::remapIDs (n, nullptr, c.edit);
    auto newClipID = EditItemID::fromID (n);
    jassert (newClipID != EditItemID::fromID (c.state));
    jassert (c.edit.clipCache.findItem (newClipID) == nullptr);

    if (auto t = c.getClipTrack())
    {
        jassert (! t->state.getChildWithProperty (IDs::id, newClipID).isValid());
        t->state.appendChild (n, c.getUndoManager());

        if (auto newClip = t->findClipForID (newClipID))
            return newClip;
    }
    else
    {
        jassertfalse;
    }

    return {};
}

SelectableList splitClips (const SelectableList& clips, TimePosition time)
{
    SelectableList newClips;

    for (auto c : getClipSelectionWithCollectionClipContents (clips).getItemsOfType<Clip>())
        if (c->getEditTimeRange().contains (time))
            if (auto ct = c->getClipTrack())
                if (auto newClip = ct->splitClip (*c, time))
                    newClips.add (newClip);

    return newClips;
}

void deleteRegionOfClip (Clip& c, TimeRange timeRangeToDelete)
{
    CRASH_TRACER

    if (auto track = c.getClipTrack())
    {
        track->setFrozen (false, Track::groupFreeze);
        track->setFrozen (false, Track::individualFreeze);

        const auto clipTimeRange = c.getEditTimeRange();

        if (timeRangeToDelete.contains (clipTimeRange))
        {
            c.removeFromParent();
        }
        else if (clipTimeRange.getStart() < timeRangeToDelete.getStart() && clipTimeRange.getEnd() > timeRangeToDelete.getEnd())
        {
            if (auto newClip = track->splitClip (c, timeRangeToDelete.getStart()))
            {
                newClip->setStart (timeRangeToDelete.getEnd(), true, false);
                c.setEnd (timeRangeToDelete.getStart(), true);
            }
        }
        else
        {
            c.trimAwayOverlap (timeRangeToDelete);
        }
    }
}

void deleteRegionOfSelectedClips (SelectionManager& selectionManager, TimeRange rangeToDelete,
                                  CloseGap closeGap, bool moveAllSubsequentClipsOnTrack)
{
    Clip::Array selectedClips;

    for (auto c : selectionManager.getItemsOfType<Clip>())
        selectedClips.add (c);

    for (auto cc : selectionManager.getItemsOfType<CollectionClip>())
        for (int j = 0; j < cc->getNumClips(); ++j)
            selectedClips.add (cc->getClip(j));

    if (selectedClips.isEmpty())
        return;

    auto& edit = selectedClips[0]->edit;

    for (auto c : edit.engine.getUIBehaviour().getAssociatedClipsToEdit (selectedClips).getItemsOfType<Clip>())
        selectedClips.addIfNotAlreadyThere (c);

    juce::Array<ClipTrack*> tracks;

   #if JUCE_DEBUG
    // A clip should always have a track. Crash reports are
    // showing this is sometimes not the case
    for (auto c : selectedClips)
        jassert (c->getClipTrack() != nullptr);
   #endif

    for (auto c : selectedClips)
        if (c->getPosition().time.overlaps (rangeToDelete))
            if (auto t = c->getClipTrack())
                tracks.addIfNotAlreadyThere (t);

    if (tracks.isEmpty())
    {
        selectionManager.engine.getUIBehaviour()
            .showWarningMessage (TRANS("None of the selected clips lay between the in/out marks"));

        return;
    }

    for (auto c : selectedClips)
        if (c->getPosition().time.overlaps (rangeToDelete))
            if (auto t = c->getClipTrack())
                t->deleteRegionOfClip (c, rangeToDelete, &selectionManager);

    if (closeGap == CloseGap::yes)
    {
        const auto centreTime = rangeToDelete.getCentre();

        if (moveAllSubsequentClipsOnTrack)
        {
            for (auto& t : tracks)
                for (auto c : t->getClips())
                    if (c->getPosition().getStart() > centreTime)
                        c->setStart (c->getPosition().getStart() - rangeToDelete.getLength(), false, true);
        }
        else
        {
            for (auto c : selectionManager.getItemsOfType<Clip>())
                if (c->getPosition().getStart() > centreTime)
                    c->setStart (c->getPosition().getStart() - rangeToDelete.getLength(), false, true);
        }
    }
}

void deleteRegionOfTracks (Edit& edit, TimeRange rangeToDelete, bool onlySelected, CloseGap closeGap, SelectionManager* selectionManager)
{
    juce::Array<Track*> tracks;

    if (onlySelected)
    {
        jassert (selectionManager != nullptr);

        if (selectionManager != nullptr)
        {
            for (auto track : selectionManager->getItemsOfType<Track>())
            {
                tracks.addIfNotAlreadyThere (track);
                for (auto t : track->getAllSubTracks (true))
                    tracks.addIfNotAlreadyThere (t);
            }
        }
    }
    else
    {
        tracks = getAllTracks (edit);
    }

    if (tracks.size() == 0 || rangeToDelete.getLength() <= TimeDuration::fromSeconds (0.0001))
        return;

    std::unique_ptr<SelectionManager::ScopedSelectionState> selectionState;

    if (selectionManager != nullptr)
    {
        selectionState = std::make_unique<SelectionManager::ScopedSelectionState> (*selectionManager);
        selectionManager->deselectAll();
    }

    Plugin::Array pluginsInRacks;

    auto addPluginsInRack = [&] (RackInstance& r)
    {
        if (r.type != nullptr)
            for (auto p : r.type->getPlugins())
                pluginsInRacks.addIfNotAlreadyThere (p);
    };

    auto removeAutomationRangeOfPlugin = [&] (Plugin& p)
    {
        for (auto param : p.getAutomatableParameters())
            param->getCurve().removeRegionAndCloseGap (rangeToDelete);
    };

    auto removeAutomationRangeFindingRackPlugins = [&] (Track& t)
    {
        for (auto p : t.pluginList)
        {
            removeAutomationRangeOfPlugin (*p);

            // find all the plugins in racks
            if (auto rf = dynamic_cast<RackInstance*> (p))
                addPluginsInRack (*rf);
        }
    };

    for (int i = tracks.size(); --i >= 0;)
    {
        if (auto t = dynamic_cast<ClipTrack*> (tracks[i]))
        {
            t->deleteRegion (rangeToDelete, selectionManager);

            // Remove any tiny clips that might be left over
            juce::Array<Clip*> clipsToRemove;

            for (auto& c : t->getClips())
                if (c->getPosition().getLength() < TimeDuration::fromSeconds (0.0001))
                    clipsToRemove.add (c);

            for (auto c : clipsToRemove)
                c->removeFromParent();

            if (closeGap == CloseGap::yes)
            {
                for (auto& c : t->getClips())
                    if (c->getPosition().getStart() > rangeToDelete.getCentre())
                        c->setStart (c->getPosition().getStart() - rangeToDelete.getLength(), false, true);

                removeAutomationRangeFindingRackPlugins (*t);
            }
        }
        else if (auto ft = dynamic_cast<FolderTrack*> (tracks[i]))
        {
            removeAutomationRangeFindingRackPlugins (*ft);
        }
    }

    for (auto p : pluginsInRacks)
        removeAutomationRangeOfPlugin (*p);

    // N.B. Delete tempo last
    if (! onlySelected || tracks.contains (edit.getTempoTrack()))
        edit.tempoSequence.deleteRegion (rangeToDelete);
}

void moveSelectedClips (const SelectableList& selectedObjectsIn, Edit& edit, MoveClipAction mode, bool automationLocked)
{
    auto selectedObjects = edit.engine.getUIBehaviour().getAssociatedClipsToEdit (selectedObjectsIn);
    auto expandedList = getClipSelectionWithCollectionClipContents (selectedObjects);

    auto firstClip  = selectedObjects.getFirstOfType<Clip>();
    auto firstCClip = selectedObjects.getFirstOfType<CollectionClip>();

    if (firstClip == nullptr && firstCClip == nullptr)
        return;

    auto& ed = firstClip ? firstClip->edit
                         : firstCClip->edit;

    auto moveClipsAndAutomation = [automationLocked] (const juce::Array<Clip*>& clips, TimeDuration delta)
    {
        if (delta == TimeDuration())
            return;

        juce::Array<TrackAutomationSection> sections;

        for (auto c : clips)
        {
            if (automationLocked)
            {
                if (auto cc = c->getGroupClip())
                {
                    TrackAutomationSection ts;
                    ts.position = cc->getPosition().time;
                    ts.src = cc->getTrack();
                    ts.dst = cc->getTrack();
                    sections.add (ts);
                }
                else
                {
                    sections.add (TrackAutomationSection (*c));
                }
            }

            c->setStart (c->getPosition().getStart() + delta, false, true);
        }

        if (sections.size() > 0 && delta != TimeDuration())
            moveAutomation (sections, delta, false);
    };

    auto moveCollectionClipAutomation = [automationLocked] (const juce::Array<CollectionClip*>& clips, TimeDuration delta)
    {
        if (delta == TimeDuration())
            return;

        juce::Array<TrackAutomationSection> sections;

        if (automationLocked)
            for (auto c : clips)
                sections.add (TrackAutomationSection (*c));

        if (sections.size() > 0 && delta != TimeDuration())
            moveAutomation (sections, delta, false);
    };

    if (mode == MoveClipAction::moveStartToCursor || mode == MoveClipAction::moveEndToCursor)
    {
        auto selectedRange = getTimeRangeForSelectedItems (selectedObjects);

        auto delta = edit.getTransport().getPosition() - (mode == MoveClipAction::moveEndToCursor ? selectedRange.getEnd()
                                                                                                  : selectedRange.getStart());

        moveClipsAndAutomation (expandedList.getItemsOfType<Clip>(), delta);
        moveCollectionClipAutomation (selectedObjects.getItemsOfType<CollectionClip>(), delta);
    }
    else
    {
        for (auto track : getClipTracks (ed))
        {
            juce::Array<Clip*> clipsInTrack;

            for (auto s : expandedList)
                if (auto c = dynamic_cast<Clip*> (s))
                    if (c->getTrack() == track)
                        clipsInTrack.add (c);

            if (! clipsInTrack.isEmpty())
            {
                TrackItem::sortByTime (clipsInTrack);

                if (mode == MoveClipAction::moveToEndOfLast)
                {
                    if (auto previousClip = track->getClips()[track->getClips().indexOf (clipsInTrack.getFirst()) - 1])
                        moveClipsAndAutomation (clipsInTrack, previousClip->getPosition().getEnd() - clipsInTrack.getFirst()->getPosition().getStart());
                }
                else if (mode == MoveClipAction::moveToStartOfNext)
                {
                    if (auto nextClip = track->getClips()[track->getClips().indexOf (clipsInTrack.getLast()) + 1])
                        moveClipsAndAutomation (clipsInTrack, nextClip->getPosition().getStart() - clipsInTrack.getLast()->getPosition().getEnd());
                }
            }
        }
    }
}

SelectableList getClipSelectionWithCollectionClipContents (const SelectableList& in)
{
    SelectableList result;

    if (in.size() > 1000)
    {
        // For large sizes it's much quicker to add all the items and then remove duplicates
        std::vector<Selectable*> selectables;
        selectables.reserve (static_cast<size_t> (in.size()));

        for (auto ti : in.getItemsOfType<TrackItem>())
        {
            if (auto clip = dynamic_cast<Clip*> (ti))
                selectables.push_back (clip);

            if (auto cc = dynamic_cast<CollectionClip*> (ti))
                for (auto c : cc->getClips())
                    selectables.push_back (c);
        }

        return stable_remove_duplicates (selectables);
    }
    else
    {
        for (auto ti : in.getItemsOfType<TrackItem>())
        {
            if (auto clip = dynamic_cast<Clip*> (ti))
                result.addIfNotAlreadyThere (clip);

            if (auto cc = dynamic_cast<CollectionClip*> (ti))
                for (auto c : cc->getClips())
                    result.addIfNotAlreadyThere (c);
        }
    }

    return result;
}

juce::Array<ClipEffect*> getAllClipEffects (Edit& edit)
{
    juce::Array<ClipEffect*> res;

    for (auto audioTrack : getAudioTracks (edit))
        for (auto clip : audioTrack->getClips())
            if (auto waveClip = dynamic_cast<AudioClipBase*> (clip))
                if (auto effects = waveClip->getClipEffects())
                    for (auto effect : *effects)
                        res.add (effect);

    return res;
}


//==============================================================================
ClipOwner* findClipOwnerForID (const Edit& edit, EditItemID id)
{
    if (auto clipTrack = dynamic_cast<ClipTrack*> (findTrackForID (edit, id)))
        return clipTrack;

    if (auto clipSlot = findClipSlotForID (edit, id))
        return clipSlot;

    if (auto containerClip = dynamic_cast<ContainerClip*> (findClipForID (edit, id)))
        return containerClip;

    return nullptr;
}

//==============================================================================
ClipSlot* findClipSlotForID (const Edit& edit, EditItemID id)
{
    ClipSlot* result = nullptr;

    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      if (auto at = dynamic_cast<AudioTrack*> (&t))
                                      {
                                          for (auto cs : at->getClipSlotList().getClipSlots())
                                          {
                                              if (cs->itemID == id)
                                              {
                                                  result = cs;
                                                  return false;
                                              }
                                          }
                                      }

                                      return true;
                                  });

    return result;
}

int findClipSlotIndex (ClipSlot& slot)
{
    if (auto at = dynamic_cast<AudioTrack*> (&slot.track))
        return at->getClipSlotList().getClipSlots().indexOf (&slot);

    jassertfalse; // This should never happen
    return -1;
}


//==============================================================================
Clip* findClipForID (const Edit& edit, EditItemID clipID)
{
    Clip* result = nullptr;

    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      if (auto c = t.findClipForID (clipID))
                                      {
                                          result = c;
                                          return false;
                                      }

                                      for (auto cc : getTrackItemsOfType<ContainerClip> (t))
                                      {
                                          if (auto c = findClipForID (*cc, clipID))
                                          {
                                              result = c;
                                              return false;
                                          }
                                      }

                                      return true;
                                  });

    return result;
}

Clip* findClipForState (const Edit& edit, const juce::ValueTree& v)
{
    Clip* result = nullptr;

    visitAllTrackItems (edit, [&] (TrackItem& t)
                        {
                            if (auto c = dynamic_cast<Clip*> (&t))
                            {
                                if (c->state == v)
                                {
                                    result = c;
                                    return false;
                                }
                            }

                            return true;
                        });

    return result;
}

bool containsClip (const Edit& edit, Clip* clip)
{
    return findTrackForPredicate (edit,
                                  [clip] (Track& t)
                                  {
                                      if (t.indexOfTrackItem (clip) >= 0)
                                          return true;

                                      if (auto at = dynamic_cast<AudioTrack*> (&t))
                                          for (auto slot : at->getClipSlotList().getClipSlots())
                                              if (clip == slot->getClip())
                                                  return true;

                                      for (auto cc : getTrackItemsOfType<ContainerClip> (t))
                                          if (cc->getClips().contains (clip))
                                              return true;

                                      return false;
                                  }) != nullptr;
}

void visitAllTrackItems (const Edit& edit, std::function<bool (TrackItem&)> f)
{
    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      auto numItems = t.getNumTrackItems();

                                      for (int i = 0; i < numItems; ++i)
                                      {
                                          if (auto ti = t.getTrackItem (i))
                                              if (! f (*ti))
                                                  return false;
                                      }

                                      if (auto at = dynamic_cast<AudioTrack*> (&t))
                                          for (auto slot : at->getClipSlotList().getClipSlots())
                                              if (auto c = slot->getClip())
                                                  if (! f (*c))
                                                      return false;

                                      for (auto cc : getTrackItemsOfType<ContainerClip> (t))
                                          for (auto c : cc->getClips())
                                              if (! f (*c))
                                                  return false;

                                      return true;
                                  });
}

TimeRange getTimeRangeForSelectedItems (const SelectableList& selected)
{
    auto items = selected.getItemsOfType<TrackItem>();

    if (items.isEmpty())
        return {};

    auto range = items.getFirst()->getEditTimeRange();

    for (auto& i : items)
        range = range.getUnionWith (i->getEditTimeRange());

    return range;
}

//==============================================================================
MidiNote* findNoteForState (const Edit& edit, const juce::ValueTree& v)
{
    MidiNote* result = nullptr;

    visitAllTrackItems (edit, [&] (TrackItem& t)
                        {
                            if (auto c = dynamic_cast<MidiClip*> (&t))
                            {
                                if (auto n = c->getSequence().getNoteFor (v))
                                {
                                    result = n;
                                    return false;
                                }
                            }

                            return true;
                        });

    return result;
}

juce::Result mergeMidiClips (juce::Array<MidiClip*> clips, SelectionManager* sm)
{
    for (auto c : clips)
        if (c->getClipTrack() == nullptr || c->getClipTrack()->isFrozen (Track::anyFreeze))
            return juce::Result::fail (TRANS("Unable to merge clips on frozen tracks"));

    TrackItem::sortByTime (clips);

    if (auto first = clips.getFirst())
    {
        if (auto track = first->getClipTrack())
        {
            if (auto newClip = track->insertMIDIClip (first->getName(), first->getPosition().time, sm))
            {
                newClip->setQuantisation (first->getQuantisation());
                newClip->setGrooveTemplate (first->getGrooveTemplate());
                newClip->setMidiChannel (first->getMidiChannel());

                auto startBeat = BeatPosition::fromBeats (1.0e10);
                auto startTime = TimePosition::fromSeconds (1.0e10);
                auto endTime = TimePosition();

                for (auto c : clips)
                {
                    startBeat = juce::jmin (startBeat, c->getStartBeat());
                    startTime = juce::jmin (startTime, c->getPosition().getStart());
                    endTime   = juce::jmax (endTime,   c->getPosition().getEnd());
                }

                MidiList destinationList;
                destinationList.setMidiChannel (newClip->getMidiChannel());

                for (auto c : clips)
                {
                    MidiList sourceList;
                    sourceList.copyFrom (c->getSequenceLooped(), nullptr);

                    const auto offset = BeatDuration::fromBeats (c->getPosition().getOffset().inSeconds() * c->edit.tempoSequence.getBeatsPerSecondAt (c->getPosition().getStart(), true));

                    sourceList.trimOutside (toPosition (offset), toPosition (offset + c->getLengthInBeats()), nullptr);
                    sourceList.moveAllBeatPositions (c->getStartBeat() - startBeat - offset, nullptr);

                    destinationList.addFrom (sourceList, nullptr);
                }

                newClip->setPosition ({ { startTime, endTime }, TimeDuration() });
                newClip->getSequence().addFrom (destinationList, &track->edit.getUndoManager());

                for (int i = clips.size(); --i >= 0;)
                    clips.getUnchecked (i)->removeFromParent();
            }

            return juce::Result::ok();
        }
    }

    return juce::Result::fail (TRANS("No clips to merge"));
}

juce::OwnedArray<MidiList> readFileToMidiList (juce::File midiFile, bool importAsNoteExpression)
{
    CRASH_TRACER
    juce::OwnedArray<MidiList> lists;
    juce::Array<BeatPosition> tempoChangeBeatNumbers;
    juce::Array<double> bpms;
    juce::Array<int> numerators, denominators;
    BeatDuration len;

    if (MidiList::readSeparateTracksFromFile (midiFile, lists,
                                              tempoChangeBeatNumbers, bpms,
                                              numerators, denominators, len,
                                              importAsNoteExpression))
    {
        return lists;
    }

    return {};
}

MidiClip::Ptr createClipFromFile (juce::File midiFile, ClipOwner& owner, bool importAsNoteExpression)
{
    auto lists = readFileToMidiList (std::move (midiFile), importAsNoteExpression);

    if (auto l = lists.getFirst())
    {
        const auto& ts = owner.getClipOwnerEdit().tempoSequence;
        const auto endTime = ts.toTime (l->getLastBeatNumber());
        const auto barsBeats = ts.toBarsAndBeats (endTime);
        const auto totalBars = barsBeats.getTotalBars();
        const auto nextBar = static_cast<int> (std::ceil (totalBars));
        const auto clipEndTime = ts.toTime ({ nextBar });

        if (auto c = insertMIDIClip (owner, { 0_tp, clipEndTime }))
        {
            c->setName (l->getImportedFileName ());
            c->getSequence ().copyFrom (*l, &owner.getClipOwnerEdit().getUndoManager());

            return c;
        }
    }

    return {};
}


//==============================================================================
Plugin::Array getAllPlugins (const Edit& edit, bool includeMasterVolume)
{
    CRASH_TRACER
    Plugin::Array list;
    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      list.addArray (t.getAllPlugins());

                                      if (auto at = dynamic_cast<AudioTrack*> (&t))
                                      {
                                          for (auto clip : at->getClips())
                                          {
                                              if (auto abc = dynamic_cast<AudioClipBase*> (clip))
                                              {
                                                  if (auto pluginList = abc->getPluginList())
                                                      list.addArray (pluginList->getPlugins());

                                                  if (auto clipEffects = abc->getClipEffects())
                                                      for (auto effect : *clipEffects)
                                                          if (auto pluginEffect = dynamic_cast<PluginEffect*> (effect))
                                                              if (pluginEffect->plugin != nullptr)
                                                                  list.add (pluginEffect->plugin);
                                              }
                                          }
                                      }

                                      return true;
                                  });

    // Master plugins may have been found by the MasterTrack plugnList
    for (auto p : edit.getMasterPluginList().getPlugins())
        list.addIfNotAlreadyThere (p);

    for (auto r : edit.getRackList().getTypes())
        for (auto p : r->getPlugins())
            list.add (p);

    if (includeMasterVolume)
        list.add (edit.getMasterVolumePlugin().get());

    return list;
}

Plugin::Ptr findPluginForState (const Edit& edit, const juce::ValueTree& v)
{
    for (auto p : getAllPlugins (edit, true))
        if (p->state == v)
            return p;

    return {};
}

Plugin::Ptr findPluginForID (const Edit& edit, EditItemID id)
{
    for (auto p : getAllPlugins (edit, true))
        if (p->itemID == id)
            return p;

    return {};
}

Track* getTrackContainingPlugin (const Edit& edit, const Plugin* p)
{
    return findTrackForPredicate (edit, [p] (Track& t) { return t.containsPlugin (p); });
}

bool areAnyPluginsMissing (const Edit& edit)
{
    for (auto p : getAllPlugins (edit, false))
        if (p->isMissing())
            return true;

    return false;
}

juce::Array<RackInstance*> getRackInstancesInEditForType (const RackType& rt)
{
    juce::Array<RackInstance*> instances;

    for (auto p : getAllPlugins (rt.edit, false))
        if (auto ri = dynamic_cast<RackInstance*> (p))
            if (ri->type == &rt)
                instances.add (ri);

    return instances;
}

void muteOrUnmuteAllPlugins (const Edit& edit)
{
    auto allPlugins = getAllPlugins (edit, true);

    int numEnabled = 0;

    for (auto p : allPlugins)
        if (p->isEnabled())
            ++numEnabled;

    for (auto p : allPlugins)
        p->setEnabled (numEnabled == 0);
}

void midiPanic (Edit& edit, bool resetPlugins)
{
    for (auto p : getAllPlugins (edit, false))
    {
        p->midiPanic();

        if (resetPlugins)
            p->reset();
    }
}

//==============================================================================
juce::Array<AutomatableEditItem*> getAllAutomatableEditItems (const Edit& edit)
{
    CRASH_TRACER
    juce::Array<AutomatableEditItem*> destArray;

    if (auto mpl = edit.getGlobalMacros().getMacroParameterList())
        destArray.add (mpl);

    edit.visitAllTracksRecursive ([&] (Track& t)
    {
        if (auto m = dynamic_cast<MacroParameterElement*> (&t))
            if (auto mpl = m->getMacroParameterList())
                destArray.add (mpl);

        destArray.addArray (t.getAllAutomatableEditItems());
        return true;
    });

    for (auto p : edit.getMasterPluginList())
    {
        if (auto mpl = p->getMacroParameterList())
            destArray.add (mpl);

        destArray.add (p);
    }

    for (auto r : edit.getRackList().getTypes())
    {
        for (auto p : r->getPlugins())
        {
            destArray.add (p);

            if (auto mpl = p->getMacroParameterList())
                destArray.add (mpl);
        }

        if (auto mpl = r->getMacroParameterList())
            destArray.add (mpl);

        destArray.addArray (r->getModifierList().getModifiers());
    }

    destArray.add (edit.getMasterVolumePlugin().get());

    return destArray;
}

void deleteAutomation (const SelectableList& selectedClips)
{
    if (selectedClips.containsType<Clip>())
    {
        for (auto& section : TrackSection::findSections (selectedClips.getItemsOfType<TrackItem>()))
        {
            for (auto plugin : section.track->pluginList)
            {
                for (auto& param : plugin->getAutomatableParameters())
                {
                    auto& curve = param->getCurve();

                    if (curve.countPointsInRegion (section.range.expanded (TimeDuration::fromSeconds (0.0001))))
                    {
                        auto start = curve.getValueAt (section.range.getStart());
                        auto end   = curve.getValueAt (section.range.getEnd());

                        curve.removePointsInRegion (section.range.expanded (TimeDuration::fromSeconds (0.0001)));
                        curve.addPoint (section.range.getStart(), start, 1.0f);
                        curve.addPoint (section.range.getEnd(),   end,   0.0f);
                    }
                }
            }
        }
    }
}

//==============================================================================
juce::Array<AutomatableParameter::ModifierSource*> getAllModifierSources (const Edit& edit)
{
    juce::Array<AutomatableParameter::ModifierSource*> sources;

    sources.addArray (getAllModifiers (edit));

    for (auto mpe : getAllMacroParameterElements (edit))
        sources.addArray (mpe->getMacroParameters());

    return sources;
}

juce::ReferenceCountedArray<Modifier> getAllModifiers (const Edit& edit)
{
    juce::ReferenceCountedArray<Modifier> modifiers;

    for (auto r : edit.getRackList().getTypes())
        modifiers.addArray (r->getModifierList().getModifiers());

    edit.visitAllTracksRecursive ([&] (Track& t)
    {
        if (auto modifierList = t.getModifierList())
            modifiers.addArray (modifierList->getModifiers());

        return true;
    });

    return modifiers;
}

Modifier::Ptr findModifierForID (const Edit& edit, EditItemID id)
{
    for (auto modifier : getAllModifiers (edit))
        if (modifier->itemID == id)
            return modifier;

    return {};
}

Modifier::Ptr findModifierForID (const RackType& rack, EditItemID id)
{
    for (auto modifier : rack.getModifierList().getModifiers())
        if (modifier->itemID == id)
            return modifier;

    return {};
}

Track* getTrackContainingModifier (const Edit& edit, const Modifier::Ptr& m)
{
    if (m == nullptr)
        return nullptr;

    return findTrackForPredicate (edit, [m] (Track& t)
    {
        if (auto list = t.getModifierList())
            return list->getModifiers().contains (m);

        return false;
    });
}

//==============================================================================
juce::Array<MacroParameterList*> getAllMacroParameterLists (const Edit& edit)
{
    CRASH_TRACER
    juce::Array<MacroParameterList*> destArray;

    for (auto ae : getAllAutomatableEditItems (edit))
        if (auto m = dynamic_cast<MacroParameterList*> (ae))
            destArray.add (m);

    return destArray;
}

juce::Array<MacroParameterElement*> getAllMacroParameterElements (const Edit& edit)
{
    juce::Array<MacroParameterElement*> elements;

    elements.add (&edit.getGlobalMacros());
    elements.addArray (edit.getRackList().getTypes());
    elements.addArray (getAudioTracks (edit));
    elements.addArray (getAllPlugins (edit, false));

    return elements;
}

InputDeviceInstance::RecordingParameters getDefaultRecordingParameters (const EditPlaybackContext& context,
                                                                        TimePosition playStart,
                                                                        TimePosition punchIn)
{
    InputDeviceInstance::RecordingParameters params;

    const auto& edit = context.edit;

    params.punchRange   = { punchIn, Edit::getMaximumEditTimeRange().getEnd() };

    if (edit.recordingPunchInOut)
    {
        const auto loopRange = context.transport.getLoopRange();
        params.punchRange = params.punchRange.withStart (std::max (params.punchRange.getStart(), loopRange.getStart() - 0.5s));

        if (edit.getNumCountInBeats() > 0 && context.getLoopTimes().getStart() > loopRange.getStart())
            params.punchRange = params.punchRange.withStart (context.getLoopTimes().getStart());

        // Ignore punch out if we start recording very close to the end of the punch markers
        if (playStart < loopRange.getEnd() - 0.5s)
            params.punchRange = params.punchRange.withEnd (loopRange.getEnd());
    }
    else if (context.isLooping())
    {
        params.punchRange = params.punchRange.withStart (context.getLoopTimes().getStart());
    }

    return params;
}

juce::Result prepareAndPunchRecord (InputDeviceInstance& instance, EditItemID targetID)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD
    InputDeviceInstance::Destination* dest = nullptr;

    for (auto inputDest : instance.destinations)
    {
        if (inputDest->targetID == targetID)
        {
            dest = inputDest;
            break;
        }
    }

    if (! dest)
        return juce::Result::fail (TRANS("Input not assigned to this destination"));

    if (! instance.edit.getTransport().isRecording())
        return juce::Result::fail (TRANS("Transport must be recording to punch record"));

    if (! dest->recordEnabled)
        return juce::Result::fail (TRANS("Input must be armed to punch record"));

    // Punch in at the current play time don't punch out until recording is stopped
    InputDeviceInstance::RecordingParameters params;
    params.punchRange   = { instance.context.getPosition(), Edit::getMaximumEditTimeRange().getEnd() };

    if (auto [recContexts, errors] = extract (instance.prepareToRecord (params)); errors.isEmpty())
        instance.startRecording (std::move (recContexts));
    else
        return juce::Result::fail (errors[0]);

    return juce::Result::ok();
}

tl::expected<Clip::Array, juce::String> punchOutRecording (InputDeviceInstance& instance)
{
    auto& transport = instance.edit.getTransport();
    jassert (transport.isPlaying()); // If this isn't playing the getUnloopedPosition call below will be incorrect

    InputDeviceInstance::StopRecordingParameters params;

    for (auto targetID : instance.getTargets())
        params.targetsToStop.push_back (targetID);

    if (auto epc = transport.getCurrentPlaybackContext())
        params.unloopedTimeToEndRecording = epc->getUnloopedPosition();

    params.isLooping = transport.looping;
    params.markedRange = transport.getLoopRange();
    params.discardRecordings = false;

    instance.prepareToStopRecording (params.targetsToStop);

    return instance.stopRecording (params);
}

bool isRecording (EditPlaybackContext& epc)
{
    for (auto input : epc.getAllInputs())
        if (input->isRecording())
            return true;

    return false;
}

InputDeviceInstance::Destination* assignTrackAsInput (AudioTrack& destTrack, const AudioTrack& sourceTrack, InputDevice::DeviceType type)
{
    assert (&sourceTrack.edit == &destTrack.edit);
    assert (type == InputDevice::trackMidiDevice || type == InputDevice::trackWaveDevice);

    auto& edit = destTrack.edit;
    auto sourceDevice = type == InputDevice::trackMidiDevice
                            ? static_cast<InputDevice*> (&sourceTrack.getMidiInputDevice())
                            : static_cast<InputDevice*> (&sourceTrack.getWaveInputDevice());
    edit.getTransport().ensureContextAllocated();
    edit.getEditInputDevices().getInstanceStateForInputDevice (*sourceDevice);
    auto res = edit.getCurrentPlaybackContext()->getInputFor (sourceDevice)->setTarget (destTrack.itemID, false, nullptr);

    return res ? *res : nullptr;
}

}} // namespace tracktion { inline namespace engine
