/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


juce::File getEditFileFromProjectManager (Edit& edit)
{
    if (auto item = ProjectManager::getInstance()->getProjectItem (edit))
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
Array<Track*> getAllTracks (const Edit& edit)
{
    Array<Track*> tracks;
    edit.visitAllTracks ([&] (Track& t) { tracks.add (&t); return true; }, true);
    return tracks;
}

Array<Track*> getTopLevelTracks (const Edit& edit)
{
    Array<Track*> tracks;
    edit.visitAllTracks ([&] (Track& t) { tracks.add (&t); return true; }, false);
    return tracks;
}

Array<AudioTrack*> getAudioTracks (const Edit& edit)
{
    return getTracksOfType<AudioTrack> (edit, true);
}

Array<ClipTrack*> getClipTracks (const Edit& edit)
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

Track* findTrackForState (const Edit& edit, const ValueTree& v)
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

juce::Array<Track*> findAllTracksContainingSelectedItems (const SelectableList& items)
{
    auto tracks = items.getItemsOfType<Track>();

    if (tracks.isEmpty())
        for (auto& i : items.getItemsOfType<TrackItem>())
            if (auto t = i->getTrack())
                tracks.addIfNotAlreadyThere (t);

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
        jassert (! t->state.getChildWithProperty (IDs::id, newClipID.toVar()).isValid());
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

SelectableList splitClips (const SelectableList& clips, double time)
{
    SelectableList newClips;

    for (auto c : getClipSelectionWithCollectionClipContents (clips).getItemsOfType<Clip>())
        if (c->getEditTimeRange().contains (time))
            if (auto ct = c->getClipTrack())
                if (auto newClip = ct->splitClip (*c, time))
                    newClips.add (newClip);

    return newClips;
}

void deleteRegionOfClip (Clip& c, EditTimeRange timeRangeToDelete)
{
    CRASH_TRACER

    if (auto track = c.getClipTrack())
    {
        track->setFrozen (false, Track::groupFreeze);
        track->setFrozen (false, Track::individualFreeze);

        const auto clipTimeRange = c.getEditTimeRange();

        if (timeRangeToDelete.contains (clipTimeRange))
        {
            c.removeFromParentTrack();
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

void deleteRegionOfSelectedClips (SelectionManager& selectionManager, EditTimeRange rangeToDelete,
                                  bool closeGap, bool moveAllSubsequentClipsOnTrack)
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
            if (auto* t = c->getClipTrack())
                tracks.addIfNotAlreadyThere (t);

    if (tracks.isEmpty())
    {
        selectionManager.engine.getUIBehaviour()
            .showWarningMessage (TRANS("None of the selected clips lay between the in/out marks"));

        return;
    }

    for (auto c : selectedClips)
        if (c->getPosition().time.overlaps (rangeToDelete))
            if (auto* t = c->getClipTrack())
                t->deleteRegionOfClip (c, rangeToDelete, &selectionManager);

    if (closeGap)
    {
        auto centreTime = (rangeToDelete.getStart() + rangeToDelete.getEnd()) * 0.5;

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

void deleteRegionOfTracks (Edit& edit, EditTimeRange rangeToDelete, bool onlySelected, bool closeGap, SelectionManager* selectionManager)
{
    juce::Array<Track*> tracks;

    if (onlySelected)
    {
        jassert (selectionManager != nullptr);

        if (selectionManager != nullptr)
            for (auto track : selectionManager->getItemsOfType<Track>())
                for (auto t : track->getAllSubTracks (true))
                    tracks.addIfNotAlreadyThere (t);
    }
    else
    {
        tracks = getAllTracks (edit);
    }

    if (tracks.size() > 0)
    {
        if (rangeToDelete.getLength() > 0.0001)
        {
            ScopedPointer<SelectionManager::ScopedSelectionState> selectionState;

            if (selectionManager != nullptr)
            {
                selectionState = new SelectionManager::ScopedSelectionState (*selectionManager);
                selectionManager->deselectAll();
            }

            Plugin::Array pluginsInRacks;

            for (int i = tracks.size(); --i >= 0;)
            {
                if (auto t = dynamic_cast<ClipTrack*> (tracks[i]))
                {
                    t->deleteRegion (rangeToDelete, selectionManager);

                    if (closeGap)
                    {
                        for (auto& c : t->getClips())
                            if (c->getPosition().getStart() > rangeToDelete.getCentre())
                                c->setStart (c->getPosition().getStart() - rangeToDelete.getLength(), false, true);

                        if (auto at = dynamic_cast<AudioTrack*> (t))
                        {
                            for (auto p : at->pluginList)
                            {
                                for (int j = p->getNumAutomatableParameters(); --j >= 0;)
                                    if (auto param = p->getAutomatableParameter(j))
                                        param->getCurve().removeRegionAndCloseGap (rangeToDelete);

                                // find all the plugins in racks
                                if (auto rf = dynamic_cast<RackInstance*> (p))
                                    if (rf->type != nullptr)
                                        for (auto p2 : rf->type->getPlugins())
                                            pluginsInRacks.addIfNotAlreadyThere (p2);
                            }
                        }
                    }
                }
            }

            for (auto p : pluginsInRacks)
                for (int j = 0; j < p->getNumAutomatableParameters(); j++)
                    if (auto param = p->getAutomatableParameter(j))
                        param->getCurve().removeRegionAndCloseGap (rangeToDelete);
        }
    }
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

    auto moveClipsAndAutomation = [automationLocked] (const juce::Array<Clip*>& clips, double delta)
    {
        if (delta == 0.0)
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

        if (sections.size() > 0 && delta != 0.0)
            moveAutomation (sections, delta, false);
    };

    auto moveCollectionClipAutomation = [automationLocked] (const juce::Array<CollectionClip*>& clips, double delta)
    {
        if (delta == 0.0)
            return;

        juce::Array<TrackAutomationSection> sections;

        if (automationLocked)
            for (auto c : clips)
                sections.add (TrackAutomationSection (*c));

        if (sections.size() > 0 && delta != 0.0)
            moveAutomation (sections, delta, false);
    };

    if (mode == MoveClipAction::moveStartToCursor || mode == MoveClipAction::moveEndToCursor)
    {
        auto selectedRange = getTimeRangeForSelectedItems (selectedObjects);

        auto delta = edit.getTransport().position - (mode == MoveClipAction::moveEndToCursor ? selectedRange.getEnd()
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
                TrackItemStartTimeSorter sorter;
                clipsInTrack.sort (sorter);

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

    for (auto ti : in.getItemsOfType<TrackItem>())
    {
        if (auto clip = dynamic_cast<Clip*> (ti))
            result.addIfNotAlreadyThere (clip);

        if (auto cc = dynamic_cast<CollectionClip*> (ti))
            for (auto c : cc->getClips())
                result.addIfNotAlreadyThere (c);
    }

    return result;
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

                                      return true;
                                  });

    return result;
}

Clip* findClipForState (const Edit& edit, const ValueTree& v)
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
    return findTrackForPredicate (edit, [clip] (Track& t) { return t.indexOfTrackItem (clip) >= 0; }) != nullptr;
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

                                      return true;
                                  });
}

EditTimeRange getTimeRangeForSelectedItems (const SelectableList& selected)
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

void mergeMidiClips (juce::Array<MidiClip*> clips)
{
    for (auto c : clips)
        if (c->getClipTrack() == nullptr || c->getClipTrack()->isFrozen (Track::anyFreeze))
            return;

    TrackItemStartTimeSorter sorter;
    clips.sort (sorter);

    if (auto first = clips.getFirst())
    {
        if (auto track = first->getClipTrack())
        {
            if (auto newClip = track->insertMIDIClip (first->getName(), first->getPosition().time, nullptr))
            {
                newClip->setQuantisation (first->getQuantisation());
                newClip->setGrooveTemplate (first->getGrooveTemplate());
                newClip->setMidiChannel (first->getMidiChannel());

                double startBeat = 1.0e10;
                double startTime = 1.0e10;
                double endTime = 0.0;

                for (auto c : clips)
                {
                    startBeat = jmin (startBeat, c->getStartBeat());
                    startTime = jmin (startTime, c->getPosition().getStart());
                    endTime   = jmax (endTime,   c->getPosition().getEnd());
                }

                MidiList destinationList;
                destinationList.setMidiChannel (newClip->getMidiChannel());

                for (auto c : clips)
                {
                    MidiList sourceList;
                    sourceList.copyFrom (c->getSequenceLooped(), nullptr);

                    sourceList.trimOutside (c->getOffsetInBeats(), c->getOffsetInBeats() + c->getLengthInBeats(), nullptr);
                    sourceList.moveAllBeatPositions (c->getStartBeat() - startBeat - c->getOffsetInBeats(), nullptr);

                    destinationList.addFrom (sourceList, nullptr);
                }

                newClip->setPosition ({ { startTime, endTime }, 0.0 });
                newClip->getSequence().addFrom (destinationList, &track->edit.getUndoManager());

                for (int i = clips.size(); --i >= 0;)
                    clips.getUnchecked(i)->removeFromParentTrack();
            }
        }
    }
}

//==============================================================================
Plugin::Array getAllPlugins (const Edit& edit, bool includeMasterVolume)
{
    CRASH_TRACER
    Plugin::Array list;
    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      list.addArray (t.getAllPlugins());
                                      return true;
                                  });

    list.addArray (edit.getMasterPluginList().getPlugins());

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

//==============================================================================
Array<AutomatableEditItem*> getAllAutomatableEditItems (const Edit& edit)
{
    CRASH_TRACER
    Array<AutomatableEditItem*> destArray;

    destArray.add (&edit.getGlobalMacros().macroParameterList);

    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      destArray.add (&t.macroParameterList);
                                      destArray.addArray (t.getAllAutomatableEditItems());
                                      return true;
                                  });

    for (auto p : edit.getMasterPluginList())
    {
        destArray.add (&p->macroParameterList);
        destArray.add (p);
    }

    for (auto r : edit.getRackList().getTypes())
    {
        for (auto p : r->getPlugins())
        {
            destArray.add (p);
            destArray.add (&p->macroParameterList);
        }

        destArray.add (&r->macroParameterList);
        destArray.addArray (r->getModifierList().getModifiers());
    }

    destArray.add (edit.getMasterVolumePlugin().get());

    return destArray;
}

void deleteAutomation (const SelectableList& selectedClips)
{
    if (auto clip = selectedClips.getFirstOfType<Clip>())
    {
        for (auto& section : TrackSection::findSections (selectedClips.getItemsOfType<TrackItem>()))
        {
            for (auto plugin : section.track->pluginList)
            {
                for (auto& param : plugin->getAutomatableParameters())
                {
                    auto& curve = param->getCurve();

                    if (curve.countPointsInRegion (section.range.expanded (0.0001)))
                    {
                        auto start = curve.getValueAt (section.range.getStart());
                        auto end   = curve.getValueAt (section.range.getEnd());

                        curve.removePointsInRegion (section.range.expanded (0.0001));
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
        sources.addArray (mpe->macroParameterList.getMacroParameters());

    return sources;
}

ReferenceCountedArray<Modifier> getAllModifiers (const Edit& edit)
{
    juce::ReferenceCountedArray<Modifier> modifiers;

    for (auto r : edit.getRackList().getTypes())
        modifiers.addArray (r->getModifierList().getModifiers());

    edit.visitAllTracksRecursive ([&] (Track& t)
                                  {
                                      modifiers.addArray (t.getModifierList().getModifiers());
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

Track* getTrackContainingModifier (const Edit& edit, const Modifier::Ptr& m)
{
    if (m == nullptr)
        return nullptr;

    return findTrackForPredicate (edit, [m] (Track& t) { return t.getModifierList().getModifiers().contains (m); });
}

//==============================================================================
juce::Array<MacroParameterList*> getAllMacroParameterLists (const Edit& edit)
{
    CRASH_TRACER
    Array<MacroParameterList*> destArray;

    for (auto ae : getAllAutomatableEditItems (edit))
        if (auto m = dynamic_cast<MacroParameterList*> (ae))
            destArray.add (m);

    return destArray;
}

Array<MacroParameterElement*> getAllMacroParameterElements (const Edit& edit)
{
    Array<MacroParameterElement*> elements;

    elements.add (&edit.getGlobalMacros());
    elements.addArray (edit.getRackList().getTypes());
    elements.addArray (getAllTracks (edit));
    elements.addArray (getAllPlugins (edit, false));

    return elements;
}
