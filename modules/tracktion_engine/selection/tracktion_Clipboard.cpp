/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


Clipboard::Clipboard() {}
Clipboard::~Clipboard() { clearSingletonInstance(); }

JUCE_IMPLEMENT_SINGLETON (Clipboard)

void Clipboard::clear()
{
    if (! isEmpty())
    {
        content.reset();
        broadcaster.sendChangeMessage();
    }
}

void Clipboard::setContent (std::unique_ptr<ContentType> newContent)
{
    if (content != newContent)
    {
        content = std::move (newContent);
        broadcaster.sendChangeMessage();
    }
}

const Clipboard::ContentType* Clipboard::getContent() const     { return content.get(); }
bool Clipboard::isEmpty() const                                 { return content == nullptr; }

void Clipboard::addListener (ChangeListener* l)         { broadcaster.addChangeListener (l); }
void Clipboard::removeListener (ChangeListener* l)      { broadcaster.removeChangeListener (l); }

Clipboard::ContentType::EditPastingOptions::EditPastingOptions (Edit& e, EditInsertPoint& ip, SelectionManager* sm)
    : edit (e), insertPoint (ip), selectionManager (sm)
{}

Clipboard::ContentType::EditPastingOptions::EditPastingOptions (Edit& e, EditInsertPoint& ip)
    : edit (e), insertPoint (ip)
{}

//==============================================================================
Clipboard::ContentType::~ContentType() {}

bool Clipboard::ContentType::pasteIntoEdit (Edit& edit, EditInsertPoint& insertPoint, SelectionManager* sm) const
{
    CRASH_TRACER
    Track::Ptr startTrack;
    double startPos = 0.0;
    insertPoint.chooseInsertPoint (startTrack, startPos, false, sm);

    if (startTrack == nullptr)
    {
        jassertfalse;
        return false;
    }

    Clipboard::ContentType::EditPastingOptions options (edit, insertPoint, sm);
    options.startTrack = startTrack.get();
    options.startTime = startPos;

    return pasteIntoEdit (options);
}

bool Clipboard::ContentType::pasteIntoEdit (const EditPastingOptions&) const    { return false; }


//==============================================================================
//==============================================================================
Clipboard::ProjectItems::ProjectItems() {}
Clipboard::ProjectItems::~ProjectItems() {}

static double pasteMIDIFileIntoEdit (Edit& edit, const File& midiFile, int& targetTrackIndex,
                                     double startTime, bool importTempoChanges)
{
    CRASH_TRACER
    OwnedArray<MidiList> lists;
    Array<double> tempoChangeBeatNumbers, bpms;
    Array<int> numerators, denominators;

    auto newClipEndTime = startTime;
    double len = 0.0;
    bool importAsNoteExpression = false;

    if (MidiList::looksLikeMPEData (midiFile))
        importAsNoteExpression = edit.engine.getUIBehaviour()
                                    .showOkCancelAlertBox (TRANS("Import as Note Expression?"),
                                                           TRANS("This MIDI file looks like it contains multi-channel MPE data. Do you want to convert this to note expression or import as multiple clips?"),
                                                           TRANS("Convert to Expression"),
                                                           TRANS("Separate Clips"));

    if (MidiList::readSeparateTracksFromFile (midiFile, lists,
                                              tempoChangeBeatNumbers, bpms,
                                              numerators, denominators, len,
                                              importAsNoteExpression))
    {
        auto& tempoSequence = edit.tempoSequence;

        auto startBeat = tempoSequence.timeToBeats (startTime);
        auto endBeat = startBeat + len;

        for (int i = lists.size(); --i >= 0;)
            endBeat = jmax (endBeat, startBeat + lists.getUnchecked (i)->getLastBeatNumber());

        endBeat = startBeat + (std::ceil (endBeat - startBeat));

        if (importTempoChanges)
        {
            if (tempoChangeBeatNumbers.size() > 0)
                tempoSequence.removeTemposBetween (EditTimeRange (startTime, tempoSequence.beatsToTime (endBeat))
                                                     .expanded (0.001), true);

            for (int i = 0; i < tempoChangeBeatNumbers.size(); ++i)
            {
                auto insertTime = tempoSequence.beatsToTime (startBeat + tempoChangeBeatNumbers.getUnchecked (i));
                auto& origTempo = tempoSequence.getTempoAt (insertTime);

                if (std::abs (origTempo.getBpm() - bpms.getUnchecked (i)) > 0.001)
                    if (auto tempo = tempoSequence.insertTempo (insertTime))
                        tempo->setBpm (bpms.getUnchecked (i));

                auto& origTimeSig = tempoSequence.getTimeSigAt (insertTime);

                if (origTimeSig.denominator != denominators.getUnchecked (i)
                    || origTimeSig.numerator != numerators.getUnchecked (i))
                {
                    if (auto timeSig = tempoSequence.insertTimeSig (insertTime))
                        timeSig->setStringTimeSig (String (numerators.getUnchecked (i)) + "/" + String (denominators.getUnchecked (i)));
                }
            }
        }

        auto lastTrackEndTime = Edit::maximumLength;
        --targetTrackIndex;

        for (auto list : lists)
        {
            auto listBeatStart = list->getFirstBeatNumber();
            auto listBeatEnd = jmax (listBeatStart + 1, list->getLastBeatNumber(), endBeat - startBeat);

            if (lastTrackEndTime > listBeatStart)
                ++targetTrackIndex;

            lastTrackEndTime = listBeatEnd;

            ValueTree clipState (IDs::MIDICLIP);
            clipState.setProperty (IDs::channel, list->getMidiChannel(), nullptr);

            if (list->state.isValid())
                clipState.addChild (list->state, -1, nullptr);

            if (auto at = edit.getOrInsertAudioTrackAt (targetTrackIndex))
            {
                auto time = tempoSequence.beatsToTime ({ startBeat, endBeat });

                if (auto newClip = at->insertClipWithState (clipState, list->getImportedMidiTrackName(), TrackItem::Type::midi,
                                                            { time, 0.0 }, false, false))
                {
                    if (importAsNoteExpression)
                        if (auto mc = dynamic_cast<MidiClip*> (newClip))
                            mc->setMPEMode (true);

                    newClipEndTime = std::max (newClipEndTime, newClip->getPosition().getEnd());
                }
            }
            else
            {
                break;
            }
        }
    }

    return newClipEndTime;
}

struct ProjectItemPastingOptions
{
    bool shouldImportTempoChangesFromMIDI = false;
    bool separateTracks = false;
    bool snapBWavsToOriginalTime = true;
};

static void askUserAboutProjectItemPastingOptions (const Clipboard::ProjectItems& items, UIBehaviour& ui,
                                                   ProjectItemPastingOptions& options)
{
    auto& pm = *ProjectManager::getInstance();

    bool importedMIDIFilesHaveTempoChanges = false;
    int numAudioClips = 0;
    int numAudioClipsWithBWAV = 0;

    for (auto& item : items.itemIDs)
    {
        if (auto source = pm.getProjectItem (item.itemID))
        {
            auto file = source->getSourceFile();

            if (file.exists())
            {
                if (source->isMidi())
                {
                    if (! importedMIDIFilesHaveTempoChanges)
                        importedMIDIFilesHaveTempoChanges = MidiList::fileHasTempoChanges (file);
                }
                else if (source->isWave())
                {
                    ++numAudioClips;

                    if (AudioFile (file).getMetadata()[WavAudioFormat::bwavTimeReference].isNotEmpty())
                        ++numAudioClipsWithBWAV;
                }
            }
        }
    }

    if (importedMIDIFilesHaveTempoChanges)
        options.shouldImportTempoChangesFromMIDI = ui.showOkCancelAlertBox (TRANS("MIDI Clip"),
                                                                            TRANS("Do you want to import tempo and time signature changes from the MIDI clip?"),
                                                                            TRANS("Import"),
                                                                            TRANS("Ignore"));

    if (numAudioClips > 1)
    {
        if (numAudioClipsWithBWAV > 0)
        {
           #if JUCE_MODAL_LOOPS_PERMITTED
            ToggleButton toggle (TRANS("Snap to BWAV"));
            toggle.setSize(200,20);

            std::unique_ptr<AlertWindow> aw (LookAndFeel::getDefaultLookAndFeel().createAlertWindow (TRANS("Add multiple files"),
                                                                                                     TRANS("Do you want to add multiple files to one track or to separate tracks?"),
                                                                                                     {}, {}, {}, AlertWindow::QuestionIcon, 0, nullptr));

            aw->addCustomComponent (&toggle);
            aw->addButton (TRANS("One track"), 0);
            aw->addButton (TRANS("Separate tracks"), 1);

            options.separateTracks = aw->runModalLoop() == 1;
            options.snapBWavsToOriginalTime = toggle.getToggleState();
           #else
            options.separateTracks = false;
            options.snapBWavsToOriginalTime = false;
           #endif
        }
        else
        {
            options.separateTracks = ! ui.showOkCancelAlertBox (TRANS("Add multiple files"),
                                                                TRANS("Do you want to add multiple files to one track or to separate tracks?"),
                                                                TRANS("One track"),
                                                                TRANS("Separate tracks"));
            options.snapBWavsToOriginalTime = false;
        }
    }
    else if (numAudioClips == 1 && numAudioClipsWithBWAV == 1)
    {
        options.snapBWavsToOriginalTime = ui.showOkCancelAlertBox (TRANS("BWAV Clip"),
                                                                   TRANS("Do you want clip placed at BWAV timestamp or cursor position?"),
                                                                   TRANS("BWAV timestamp"),
                                                                   TRANS("Cursor position"));
    }
}

bool isRecursiveEditClipPaste (const Clipboard::ProjectItems& items, Edit& edit)
{
    auto& pm = *ProjectManager::getInstance();

    for (auto& item : items.itemIDs)
        if (auto source = pm.getProjectItem (item.itemID))
            if (source->isEdit() && source->getID() == edit.getProjectItemID())
                return true;

    return false;
}

bool Clipboard::ProjectItems::pasteIntoEdit (const EditPastingOptions& options) const
{
    auto& pm = *ProjectManager::getInstance();
    auto& ui = options.edit.engine.getUIBehaviour();
    bool anythingPasted = false;

    ProjectItemPastingOptions pastingOptions;

    if (! options.silent)
        askUserAboutProjectItemPastingOptions (*this, ui, pastingOptions);

    if (isRecursiveEditClipPaste (*this, options.edit))
    {
        if (! options.silent)
            ui.showWarningAlert (TRANS("Can't Import Edit"),
                                 TRANS("You can't paste an edit clip into itself!"));

        return false;
    }

    double startTime = 0;
    Track::Ptr insertPointTrack;
    options.insertPoint.chooseInsertPoint (insertPointTrack, startTime, false, options.selectionManager);

    if (insertPointTrack == nullptr)
    {
        jassertfalse;
        return false;
    }

    int targetTrackIndex = insertPointTrack->getIndexInEditTrackList();

    for (auto& item : itemIDs)
    {
        if (auto sourceItem = pm.getProjectItem (item.itemID))
        {
            auto file = sourceItem->getSourceFile();
            auto newClipEndTime = startTime;

            if (file.exists())
            {
                if (auto targetTrack = options.edit.getOrInsertAudioTrackAt (targetTrackIndex))
                {
                    if (sourceItem->isMidi())
                    {
                        newClipEndTime = pasteMIDIFileIntoEdit (options.edit, file, targetTrackIndex, startTime,
                                                                pastingOptions.shouldImportTempoChangesFromMIDI);
                    }
                    else if (sourceItem->isWave())
                    {
                        if (auto newClip = targetTrack->insertWaveClip (sourceItem->getName(), sourceItem->getID(),
                                                                        { { startTime, startTime + sourceItem->getLength() }, 0.0 }, false))
                            newClipEndTime = newClip->getPosition().getEnd();

                    }
                    else if (sourceItem->isEdit())
                    {
                        if (auto newClip = targetTrack->insertEditClip ({ startTime, startTime + sourceItem->getLength() },
                                                                        sourceItem->getID()))
                            newClipEndTime = newClip->getPosition().getEnd();
                    }

                    anythingPasted = true;

                    if (pastingOptions.separateTracks)
                        ++targetTrackIndex;
                    else
                        startTime = newClipEndTime;
                }
            }
        }
    }

    return anythingPasted;
}

bool Clipboard::ProjectItems::pasteIntoProject (Project& project) const
{
    for (auto& item : itemIDs)
        if (auto source = ProjectManager::getInstance()->getProjectItem (item.itemID))
            if (auto newItem = project.createNewItem (source->getSourceFile(),
                                                      source->getType(),
                                                      source->getName(),
                                                      source->getDescription(),
                                                      source->getCategory(),
                                                      true))
                newItem->copyAllPropertiesFrom (*source);

    return ! itemIDs.empty();
}

//==============================================================================
//==============================================================================
Clipboard::Clips::Clips() {}
Clipboard::Clips::~Clips() {}

void Clipboard::Clips::addClip (int trackOffset, const ValueTree& state)
{
    ClipInfo ci;
    ci.trackOffset = trackOffset;
    ci.state = state.createCopy();

    clips.push_back (ci);
}

void Clipboard::Clips::addSelectedClips (const SelectableList& selectedObjects, EditTimeRange range, bool automationLocked)
{
    if (range.isEmpty())
        range = Edit::getMaximumEditTimeRange();

    auto selectedClipContents = getClipSelectionWithCollectionClipContents (selectedObjects);

    Clip::Array clipsToPaste;

    for (auto& c : selectedClipContents.getItemsOfType<Clip>())
        if (c->getEditTimeRange().overlaps (range))
            clipsToPaste.add (c);

    if (clipsToPaste.isEmpty())
        return;

    auto& ed = clipsToPaste.getFirst()->edit;

    auto allTracks = getAllTracks (ed);

    auto firstTrackIndex = Edit::maxNumTracks;
    auto overallStartTime = Edit::maximumLength;

    for (auto& clip : clipsToPaste)
    {
        overallStartTime = jmin (overallStartTime, jmax (clip->getPosition().getStart(), range.getStart()));
        firstTrackIndex = jmin (firstTrackIndex, jmax (0, allTracks.indexOf (clip->getTrack())));
    }

    for (auto& clip : clipsToPaste)
    {
        if (clip->getEditTimeRange().overlaps (range))
        {
            auto clipPos = clip->getPosition();
            auto clippedStart = jmax (clipPos.getStart(), range.getStart());
            auto clippedOffset = clipPos.getOffset() + (clippedStart - clipPos.getStart());
            auto clippedEnd = jmin (clipPos.getEnd(), range.getEnd());

            ClipInfo info;

            clip->flushStateToValueTree();
            info.state = clip->state.createCopy();

            info.state.setProperty (IDs::start, clippedStart - overallStartTime, nullptr);
            info.state.setProperty (IDs::length, clippedEnd - clippedStart, nullptr);
            info.state.setProperty (IDs::offset, clippedOffset, nullptr);

            auto acb = dynamic_cast<AudioClipBase*> (clip);

            if (acb != nullptr && range != Edit::getMaximumEditTimeRange())
            {
                auto inOutPoints = clip->getEditTimeRange().getIntersectionWith (range);
                EditTimeRange fadeIn (clipPos.getStart(), clipPos.getStart() + acb->getFadeIn());
                EditTimeRange fadeOut (clipPos.getEnd() - acb->getFadeOut(), clipPos.getEnd());

                info.state.setProperty (IDs::fadeIn,  fadeIn.overlaps (inOutPoints)  ? fadeIn.getIntersectionWith (inOutPoints).getLength() : 0.0, nullptr);
                info.state.setProperty (IDs::fadeOut, fadeOut.overlaps (inOutPoints) ? fadeOut.getIntersectionWith (inOutPoints).getLength() : 0.0, nullptr);
            }

            info.trackOffset = allTracks.indexOf (clip->getTrack()) - firstTrackIndex;

            if (acb == nullptr || acb->getAutoTempo())
            {
                info.hasBeatTimes = true;

                auto& ts = ed.tempoSequence;
                info.startBeats = ts.timeToBeats (clippedStart) - ts.timeToBeats (overallStartTime);
                info.lengthBeats = ts.timeToBeats (clippedEnd) - ts.timeToBeats (clippedStart);
                info.offsetBeats = ts.getBeatsPerSecondAt (clippedStart) * clippedOffset;
            }

            clips.push_back (info);
        }
    }

    if (automationLocked)
    {
        for (auto& trackSection : TrackSection::findSections (clipsToPaste))
        {
            for (auto plugin : trackSection.track->pluginList)
            {
                for (int k = 0; k < plugin->getNumAutomatableParameters(); k++)
                {
                    auto param = plugin->getAutomatableParameter(k);

                    if (param->getCurve().getNumPoints() > 0)
                    {
                        AutomationCurveSection section;
                        section.pluginName = plugin->getName();
                        section.paramID = param->paramID;
                        section.trackOffset = jmax (0, allTracks.indexOf (trackSection.track) - firstTrackIndex);

                        auto clippedStart = jmax (trackSection.range.getStart(), range.getStart()) - overallStartTime;
                        auto clippedEnd   = jmin (trackSection.range.getEnd(),   range.getEnd()) - overallStartTime;

                        const double endTolerence = 0.0001;

                        for (int l = 0; l < param->getCurve().getNumPoints(); ++l)
                        {
                            auto pt = param->getCurve().getPoint(l);

                            if (pt.time >= clippedStart - endTolerence && pt.time <= clippedEnd + endTolerence)
                                section.points.add ({ pt.time, pt.value, pt.curve });
                        }

                        if (section.points.isEmpty())
                        {
                            section.points.insert (0, { clippedStart, param->getCurve().getValueAt (clippedStart), 1.0f });
                            section.points.add ({ clippedEnd, param->getCurve().getValueAt (clippedEnd), 0.0f });
                        }
                        else
                        {
                            if (section.points.getFirst().time > clippedStart + endTolerence)
                                section.points.insert (0, { clippedStart, param->getCurve().getValueAt (clippedStart), 0.0f });

                            if (section.points.getLast().time < clippedEnd - endTolerence)
                                section.points.add ({ clippedEnd, param->getCurve().getValueAt (clippedEnd), 0.0f });
                        }

                        automationCurves.add (section);
                        section.points.sort();
                    }
                }
            }
        }
    }
}

static void fixClipTimes (ValueTree& state, const Clipboard::Clips::ClipInfo& clip, TempoSequence& tempoSequence, double startOffset)
{
    double start = 0, length = 0, offset = 0;

    if (clip.hasBeatTimes)
    {
        auto range = tempoSequence.beatsToTime ({ clip.startBeats, clip.startBeats + clip.lengthBeats }) + startOffset;
        start  = range.getStart();
        length = range.getLength();
        // TODO: this offset calculation isn't right - fix when we have a structure for start/length/offset which can be converted to beats as a whole
        offset = tempoSequence.beatsToTime (clip.offsetBeats) / tempoSequence.getBeatsPerSecondAt (start);
    }
    else
    {
        start  = static_cast<double> (state.getProperty (IDs::start)) + startOffset;
        length = state.getProperty (IDs::length);
        offset = state.getProperty (IDs::offset);
    }

    double srcBpm = state[IDs::bpm];   // if clip is coming from preset, it'll have this
                                              // property, so resize it to match tempo
    if (srcBpm > 0)
    {
        auto& destTempo = tempoSequence.getTempoAt (start);
        length = length * srcBpm / destTempo.getBpm();
    }

    state.setProperty (IDs::start, start, nullptr);
    state.setProperty (IDs::length, length, nullptr);
    state.setProperty (IDs::offset, offset, nullptr);

    state.removeProperty (IDs::bpm, nullptr);
    state.removeProperty (IDs::key, nullptr);
}

bool Clipboard::Clips::pasteIntoEdit (const EditPastingOptions& options) const
{
    if (clips.empty())
        return false;

    auto targetTrack = options.startTrack;

    if (targetTrack == nullptr)
    {
        double t;
        options.insertPoint.chooseInsertPoint (targetTrack, t, false, options.selectionManager);
        jassert (targetTrack != nullptr);
    }

    std::map<EditItemID, EditItemID> remappedIDs;
    SelectableList itemsAdded;

    for (auto& clip : clips)
    {
        auto newClipState = clip.state.createCopy();
        EditItemID::remapIDs (newClipState, nullptr, options.edit, &remappedIDs);
        fixClipTimes (newClipState, clip, options.edit.tempoSequence, options.startTime);

        if (newClipState.hasType (IDs::MARKERCLIP))
        {
            if (auto markerTrack = options.edit.getMarkerTrack())
            {
                if (auto newClip = markerTrack->insertClipWithState (newClipState))
                {
                    itemsAdded.add (newClip);

                    if (auto mc = dynamic_cast<MarkerClip*> (newClip))
                        options.edit.getMarkerManager().checkForDuplicates (*mc, false);
                }
            }
            else
            {
                jassertfalse;
            }
        }
        else
        {
            if (auto clipTrack = dynamic_cast<ClipTrack*> (targetTrack->getSiblingTrack (clip.trackOffset, false)))
            {
                if (auto newClip = clipTrack->insertClipWithState (newClipState))
                    itemsAdded.add (newClip);
            }
            else
            {
                jassertfalse;
            }
        }
    }

    for (auto& curve : automationCurves)
    {
        if (! curve.points.isEmpty())
        {
            EditTimeRange curveTimeRange (curve.points.getFirst().time,
                                          curve.points.getLast().time);

            if (auto clipTrack = dynamic_cast<ClipTrack*> (targetTrack->getSiblingTrack (curve.trackOffset, false)))
            {
                for (auto plugin : clipTrack->pluginList)
                {
                    if (plugin->getName() == curve.pluginName)
                    {
                        if (auto targetParam = plugin->getAutomatableParameterByID (curve.paramID))
                        {
                            targetParam->getCurve().removePointsInRegion (curveTimeRange.expanded (0.0001));

                            for (auto& pt : curve.points)
                                targetParam->getCurve().addPoint (pt.time, pt.value, pt.curve);

                            break;
                        }
                    }
                }
            }
            else
            {
                jassertfalse;
            }
        }
    }

    if (itemsAdded.isEmpty())
        return false;

    if (auto sm = options.selectionManager)
        sm->select (itemsAdded);

    if (options.setTransportToEnd && ! options.edit.getTransport().isPlaying())
        options.edit.getTransport().setCurrentPosition (getTimeRangeForSelectedItems (itemsAdded).getEnd());

    return true;
}

bool Clipboard::Clips::pasteIntoEdit (Edit& edit, EditInsertPoint& insertPoint, SelectionManager* sm) const
{
    Clipboard::ContentType::EditPastingOptions options (edit, insertPoint, sm);
    insertPoint.chooseInsertPoint (options.startTrack, options.startTime, false, sm);
    return pasteIntoEdit (options);
}

bool Clipboard::Clips::pasteAfterSelected (Edit& edit, EditInsertPoint& insertPoint, SelectionManager& sm) const
{
    EditPastingOptions options (edit, insertPoint, &sm);
    insertPoint.chooseInsertPoint (options.startTrack, options.startTime, true, &sm);
    return pasteIntoEdit (options);
}

static juce::Array<ClipTrack*> findTracksToInsertInto (Edit& edit, EditInsertPoint& insertPoint, SelectionManager& sm)
{
    auto tracks = sm.getItemsOfType<ClipTrack>();
    bool noFolders = true;

    for (auto ft : sm.getItemsOfType<FolderTrack>())
    {
        for (auto t : ft->getAllAudioSubTracks (true))
            tracks.addIfNotAlreadyThere (t);

        noFolders = false;
    }

    for (auto c : sm.getItemsOfType<Clip>())
    {
        tracks.addIfNotAlreadyThere (c->getClipTrack());
        insertPoint.setNextInsertPoint (edit.getTransport().position, c->getTrack());
    }

    if (tracks.isEmpty() && noFolders)
        tracks.addArray (getClipTracks (edit));

    return tracks;
}

static double getNewClipsTotalLength (const Clipboard::Clips& clips, Edit& edit)
{
    double total = 0;

    for (auto& i : clips.clips)
    {
        auto end = i.hasBeatTimes ? edit.tempoSequence.beatsToTime (i.startBeats + i.lengthBeats)
                                  : (static_cast<double> (i.state.getProperty (IDs::start))
                                       + static_cast<double> (i.state.getProperty (IDs::length)));

        total = std::max (total, end);
    }

    return total;
}

bool Clipboard::Clips::pasteInsertingAtCursorPos (Edit& edit, EditInsertPoint& insertPoint, SelectionManager& sm) const
{
    if (clips.empty())
        return false;

    auto tracks = findTracksToInsertInto (edit, insertPoint, sm);
    auto insertLength = getNewClipsTotalLength (*this, edit);

    if (tracks.isEmpty() || insertLength <= 0)
        return false;

    double cursorPos = edit.getTransport().position;
    auto firstTrackIndex = tracks.getFirst()->getIndexInEditTrackList();

    for (auto t : tracks)
    {
        t->splitAt (cursorPos);
        t->insertSpaceIntoTrack (cursorPos, insertLength);
        firstTrackIndex = std::min (firstTrackIndex, t->getIndexInEditTrackList());
    }

    EditPastingOptions options (edit, insertPoint, &sm);
    options.startTime = cursorPos;
    return pasteIntoEdit (options);
}

//==============================================================================
//==============================================================================
Clipboard::Tracks::Tracks() {}
Clipboard::Tracks::~Tracks() {}

bool Clipboard::Tracks::pasteIntoEdit (const EditPastingOptions& options) const
{
    CRASH_TRACER

    if (options.selectionManager != nullptr)
        options.selectionManager->deselectAll();

    Array<Track::Ptr> newTracks;
    std::map<EditItemID, EditItemID> remappedIDs;

    auto targetTrack = options.startTrack;

    for (auto& trackState : tracks)
    {
        auto newTrackTree = trackState.createCopy();
        EditItemID::remapIDs (newTrackTree, nullptr, options.edit, &remappedIDs);

        Track::Ptr parentTrack, precedingTrack;

        if (targetTrack != nullptr)
            parentTrack = targetTrack->getParentTrack();

        precedingTrack = targetTrack;

        if (auto newTrack = options.edit.insertTrack (TrackInsertPoint (parentTrack.get(),
                                                                        precedingTrack.get()),
                                                      newTrackTree,
                                                      options.selectionManager))
        {
            newTracks.add (newTrack);

            if (parentTrack == nullptr)
                targetTrack = newTrack;
        }
        else
        {
            break;
        }
    }

    // Find any parameters on the Track that have modifier assignments
    // Check to see if they're assigned to the old modifier IDs
    // If they are, find the new modifier ID equivalents and update them
    // If they can't be found leave them if they're global or a parent of the new track.
    for (auto track : newTracks)
    {
        for (auto param : track->getAllAutomatableParams())
        {
            auto assignments = param->getAssignments();

            for (int i = assignments.size(); --i >= 0;)
            {
                auto ass = assignments.getUnchecked (i);

                // Macro reassignment is done during Plugin::giveNewIDsToPlugins so
                // we need to make sure we don't remove these
                if (dynamic_cast<MacroParameter::Assignment*> (ass.get()) != nullptr)
                    continue;

                const auto oldID = EditItemID::fromProperty (ass->state, IDs::source);
                const auto newID = remappedIDs[oldID];

                if (newID.isValid())
                {
                    newID.setProperty (ass->state, IDs::source, nullptr);
                }
                else
                {
                    // If the modifier is on this track, keep it
                    // If oldID is found in a global track, keep it
                    // If oldID is found in a parent track, keep it
                    if (auto t = getTrackContainingModifier (options.edit, findModifierForID (options.edit, oldID)))
                        if (t == track.get() || TrackList::isFixedTrack (t->state) || track->isAChildOf (*t))
                            continue;

                    // Otherwise remove the assignment
                    param->removeModifier (*ass);
                }
            }
        }
    }

    return true;
}

//==============================================================================
//==============================================================================
Clipboard::TempoChanges::TempoChanges (const TempoSequence& ts, EditTimeRange range)
{
    auto beats = ts.timeToBeats (range);

    double startBeat = std::floor (beats.getStart() + 0.5);
    double endBeat   = std::floor (beats.getEnd() + 0.5);

    bool pointAtStart = false;
    bool pointAtEnd   = false;

    for (int i = 0; i < ts.getNumTempos(); ++i)
    {
        auto t = ts.getTempo(i);

        if (t->startBeatNumber == startBeat) pointAtStart = true;
        if (t->startBeatNumber == endBeat)   pointAtEnd   = true;

        if (range.contains (t->getStartTime()))
            changes.push_back ({ t->startBeatNumber - startBeat,
                                 t->getBpm(),
                                 t->getCurve() });
    }

    if (! pointAtStart)
        changes.insert (changes.begin(),
                        { startBeat - startBeat,
                          ts.getBpmAt (ts.beatsToTime (startBeat)),
                          ts.getTempoAtBeat (roundToInt (startBeat)).getCurve() });

    if (! pointAtEnd)
        changes.push_back ({ endBeat - startBeat,
                             ts.getBpmAt (ts.beatsToTime (endBeat)),
                             ts.getTempoAtBeat (roundToInt (endBeat)).getCurve() });
}

Clipboard::TempoChanges::~TempoChanges() {}

bool Clipboard::TempoChanges::pasteIntoEdit (const EditPastingOptions& options) const
{
    return pasteTempoSequence (options.edit.tempoSequence, options.edit, {});
}

bool Clipboard::TempoChanges::pasteTempoSequence (TempoSequence& ts, Edit& edit, EditTimeRange targetRange) const
{
    if (changes.empty())
        return false;

    auto startTime = edit.getTransport().position.get();

    EditTimecodeRemapperSnapshot snap;
    snap.savePreChangeState (ts.edit);

    auto length = changes.back().beat;

    if (targetRange.isEmpty())
        targetRange = { startTime, startTime + length };

    auto startBeat = std::floor (ts.timeToBeats (targetRange.getStart()) + 0.5);
    auto endBeat   = std::floor (ts.timeToBeats (targetRange.getEnd())   + 0.5);

    ts.removeTemposBetween (ts.beatsToTime ({ startBeat, endBeat }), false);
    ts.insertTempo (ts.beatsToTime (startBeat));
    ts.insertTempo (ts.beatsToTime (endBeat));

    for (auto& tc : changes)
        ts.insertTempo (roundToInt ((tc.beat / length) * (endBeat - startBeat) + startBeat),
                        tc.bpm, tc.curve);

    for (int i = ts.getNumTempos(); --i >= 0;)
    {
        auto tcurr = ts.getTempo (i);
        auto tprev = ts.getTempo (i - 1);

        if (tcurr->startBeatNumber >= startBeat && tcurr->startBeatNumber <= endBeat
              && tcurr->startBeatNumber == tprev->startBeatNumber
              && tcurr->getBpm() == tprev->getBpm())
            ts.removeTempo (i, false);
    }

    snap.remapEdit (ts.edit);
    return true;
}

//==============================================================================
//==============================================================================
Clipboard::AutomationPoints::AutomationPoints (const AutomationCurve& curve, EditTimeRange range)
{
    valueRange = curve.getValueLimits();

    bool pointAtStart = false;
    bool pointAtEnd = false;

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        auto p = curve.getPoint(i);

        if (p.time == range.getStart())  pointAtStart = true;
        if (p.time == range.getEnd())    pointAtEnd = true;

        if (range.contains (p.time))
        {
            p.time -= range.getStart();
            points.push_back (p);
        }
    }

    if (! pointAtStart)
        points.insert (points.begin(), AutomationCurve::AutomationPoint (0, curve.getValueAt (range.getStart()), 0));

    if (! pointAtEnd)
        points.push_back (AutomationCurve::AutomationPoint (range.getLength(), curve.getValueAt (range.getEnd()), 0));
}

Clipboard::AutomationPoints::~AutomationPoints() {}

bool Clipboard::AutomationPoints::pasteIntoEdit (const EditPastingOptions&) const
{
    jassertfalse; // TODO: what to do here?
    return false;
}

bool Clipboard::AutomationPoints::pasteAutomationCurve (AutomationCurve& targetCurve, EditTimeRange targetRange) const
{
    AutomationCurve newCurve;
    auto dstRange = targetCurve.getValueLimits();

    for (auto p : points)
    {
        if (dstRange != valueRange)
        {
            auto normalised = (p.value - valueRange.getStart()) / valueRange.getLength();
            p.value = dstRange.getStart() + dstRange.getLength() * normalised;
        }

        newCurve.addPoint (p.time, p.value, p.curve);
    }

    if (newCurve.getLength() > 0)
    {
        if (targetRange.isEmpty())
            targetRange = targetRange.withLength (newCurve.getLength());
        else
            newCurve.rescaleAllTimes (targetRange.getLength() / newCurve.getLength());

        targetCurve.mergeOtherCurve (newCurve, targetRange, 0.0, 0.0, false, false);
        return true;
    }

    return false;
}

//==============================================================================
//==============================================================================
Clipboard::MIDINotes::MIDINotes() {}
Clipboard::MIDINotes::~MIDINotes() {}

juce::Array<MidiNote*> Clipboard::MIDINotes::pasteIntoClip (MidiClip& clip, const juce::Array<MidiNote*>& selectedNotes,
                                                            double cursorPosition, const std::function<double(double)>& snapBeat) const
{
    if (notes.empty())
        return {};

    juce::Array<MidiNote> midiNotes;

    for (auto& n : notes)
        midiNotes.add (MidiNote (n));

    auto beatRange = midiNotes.getReference(0).getRangeBeats();

    for (auto& n : midiNotes)
        beatRange = beatRange.getUnionWith (n.getRangeBeats());

    auto insertPos = clip.getContentBeatAtTime (cursorPosition);

    if (! selectedNotes.isEmpty())
    {
        double endOfSelection = 0;

        for (auto* n : selectedNotes)
            endOfSelection = jmax (endOfSelection, n->getEndBeat());

        insertPos = endOfSelection;
    }

    if (clip.isLooping())
    {
        if (insertPos < 0 || insertPos >= clip.getLoopLengthBeats() - 0.001)
            return {};
    }
    else
    {
        if (insertPos < 0 || insertPos >= clip.getLengthInBeats() - 0.001)
            return {};
    }

    double deltaBeats = insertPos - beatRange.getStart();

    if (snapBeat != nullptr)
        deltaBeats = snapBeat (deltaBeats);

    auto& sequence = clip.getSequence();
    auto um = &clip.edit.getUndoManager();
    juce::Array<MidiNote*> notesAdded;

    for (auto& n : midiNotes)
    {
        n.setStartAndLength (n.getStartBeat() + deltaBeats, n.getLengthBeats(), nullptr);

        if (auto note = sequence.addNote (n, um))
            notesAdded.add (note);
    }

    return notesAdded;
}

bool Clipboard::MIDINotes::pasteIntoEdit (const EditPastingOptions&) const
{
    return false;
}

//==============================================================================
namespace
{
    static double snapTime (Edit& e, double t)
    {
        auto& tc = e.getTransport();
        return tc.snapToTimecode ? tc.getSnapType().roundTimeNearest (t, e.tempoSequence)
                                 : t;
    }
}

//==============================================================================
//==============================================================================
Clipboard::Pitches::Pitches() {}
Clipboard::Pitches::~Pitches() {}

bool Clipboard::Pitches::pasteIntoEdit (const EditPastingOptions& options) const
{
    if (pitches.empty())
        return false;

    if (options.selectionManager != nullptr)
        options.selectionManager->deselectAll();

    auto startBeat = options.edit.tempoSequence.timeToBeats (snapTime (options.edit, options.edit.getTransport().position));
    auto firstPitchBeat = static_cast<double> (pitches.front().getProperty (IDs::startBeat));
    auto offset = startBeat - firstPitchBeat;
    auto um = &options.edit.getUndoManager();

    for (auto& state : pitches)
    {
        auto time = options.edit.tempoSequence.beatsToTime (offset + static_cast<double> (state.getProperty (IDs::startBeat)));

        if (auto pitch = options.edit.pitchSequence.insertPitch (time))
        {
            jassert (pitch->state.getNumChildren() == 0); // this would need handling

            copyValueTreeProperties (pitch->state, state, um,
                                     [] (const juce::Identifier& name) { return name != IDs::startBeat; });

            if (options.selectionManager != nullptr)
                options.selectionManager->addToSelection (*pitch);
        }
    }

    return true;
}

//==============================================================================
//==============================================================================
Clipboard::TimeSigs::TimeSigs() {}
Clipboard::TimeSigs::~TimeSigs() {}

bool Clipboard::TimeSigs::pasteIntoEdit (const EditPastingOptions& options) const
{
    if (timeSigs.empty())
        return false;

    if (options.selectionManager != nullptr)
        options.selectionManager->deselectAll();

    auto startBeat = options.edit.tempoSequence.timeToBeats (snapTime (options.edit, options.edit.getTransport().position));
    auto firstTimeSigBeat = static_cast<double> (timeSigs.front().getProperty (IDs::startBeat));
    auto offset = startBeat - firstTimeSigBeat;
    auto um = &options.edit.getUndoManager();

    for (auto& state : timeSigs)
    {
        auto time = options.edit.tempoSequence.beatsToTime (offset + static_cast<double> (state.getProperty (IDs::startBeat)));

        if (auto timeSig = options.edit.tempoSequence.insertTimeSig (time))
        {
            jassert (timeSig->state.getNumChildren() == 0); // this would need handling
            copyValueTreeProperties (timeSig->state, state, um,
                                     [] (const juce::Identifier& name) { return name != IDs::startBeat; });

            if (options.selectionManager != nullptr)
                options.selectionManager->addToSelection (*timeSig);
        }
    }

    return true;
}

//==============================================================================
//==============================================================================
Clipboard::Plugins::Plugins (const Plugin::Array& items)
{
    for (auto& item : items)
    {
        item->edit.flushPluginStateIfNeeded (*item);
        plugins.push_back (item->state.createCopy());
    }
}

Clipboard::Plugins::~Plugins() {}

static bool pastePluginBasedOnSelection (Edit& edit, const Plugin::Ptr& newPlugin,
                                         SelectionManager* selectionManager)
{
    if (selectionManager == nullptr)
        return false;

    if (RackType::Ptr selectedRack = selectionManager->getFirstItemOfType<RackType>())
    {
        selectedRack->addPlugin (newPlugin, { 0.5f, 0.5f }, false);
        return true;
    }

    if (Plugin::Ptr selectedPlugin = selectionManager->getFirstItemOfType<Plugin>())
    {
        if (auto* list = selectedPlugin->getOwnerList())
        {
            auto index = list->indexOf (selectedPlugin.get());

            if (index >= 0)
            {
                list->insertPlugin (newPlugin, index, selectionManager);
                return true;
            }
        }

        if (auto selectedRack = edit.getRackList().findRackContaining (*selectedPlugin))
        {
            selectedRack->addPlugin (newPlugin, { 0.5f, 0.5f }, false);
            return true;
        }
    }

    if (auto selectedClip = selectionManager->getFirstItemOfType<Clip>())
        if (selectedClip->addClipPlugin (newPlugin, *selectionManager))
            return true;

    return false;
}

static bool pastePluginIntoTrack (const Plugin::Ptr& newPlugin, EditInsertPoint& insertPoint, SelectionManager* sm)
{
    double startPos = 0.0;
    Track::Ptr track;
    insertPoint.chooseInsertPoint (track, startPos, false, sm);
    jassert (track != nullptr);

    if (track != nullptr && track->canContainPlugin (newPlugin.get()))
    {
        track->pluginList.insertPlugin (newPlugin, 0, sm);
        return true;
    }

    return false;
}

bool Clipboard::Plugins::pasteIntoEdit (const EditPastingOptions& options) const
{
    CRASH_TRACER
    bool anyPasted = false;

    for (auto& item : plugins)
    {
        auto stateCopy = item.createCopy();
        EditItemID::remapIDs (stateCopy, nullptr, options.edit);

        if (auto newPlugin = options.edit.getPluginCache().getOrCreatePluginFor (stateCopy))
        {
            if (pastePluginBasedOnSelection (options.edit, newPlugin, options.selectionManager)
                 || pastePluginIntoTrack (newPlugin, options.insertPoint, options.selectionManager))
            {
                anyPasted = true;

                // If we've pasted a plugin into a different track, see if it should still be under modifier control
                if (auto track = newPlugin->getOwnerTrack())
                {
                    for (auto param : newPlugin->getAutomatableParameters())
                    {
                        auto assignments = param->getAssignments();

                        for (int i = assignments.size(); --i >= 0;)
                        {
                            auto ass = assignments.getUnchecked (i);

                            if (auto mpa = dynamic_cast<MacroParameter::Assignment*> (ass.get()))
                            {
                                if (auto mp = getMacroParameterForID (options.edit, mpa->macroParamID))
                                    if (auto t = mp->getTrack())
                                        if (! (t == track || track->isAChildOf (*t)))
                                            param->removeModifier (*ass);
                            }
                            else
                            {
                                if (auto t = getTrackContainingModifier (options.edit,
                                                                         findModifierForID (options.edit, EditItemID::fromProperty (ass->state, IDs::source))))
                                    if (! (t == track || TrackList::isFixedTrack (t->state) || track->isAChildOf (*t)))
                                        param->removeModifier (*ass);
                            }
                        }
                    }
                }
            }
        }
    }

    return anyPasted;
}

//==============================================================================
//==============================================================================
Clipboard::Takes::Takes (const WaveCompManager& waveCompManager)
{
    items = waveCompManager.getActiveTakeTree().createCopy();
}

Clipboard::Takes::~Takes() {}

bool Clipboard::Takes::pasteIntoClip (WaveAudioClip& c) const
{
    if (items.isValid())
        return c.getCompManager().pasteComp (items).isValid();

    return false;
}

//==============================================================================
//==============================================================================
Clipboard::Modifiers::Modifiers() {}
Clipboard::Modifiers::~Modifiers() {}

bool Clipboard::Modifiers::pasteIntoEdit (const EditPastingOptions& options) const
{
    if (modifiers.empty())
        return false;

    if (options.selectionManager != nullptr)
    {
        if (auto firstSelectedMod = options.selectionManager->getFirstItemOfType<Modifier>())
        {
            if (auto t = getTrackContainingModifier (options.edit, firstSelectedMod))
            {
                auto modList = getModifiersOfType<Modifier> (t->getModifierList());

                for (int i = modList.size(); --i >= 0;)
                {
                    if (modList.getObjectPointer (i) == firstSelectedMod)
                    {
                        for (auto m : modifiers)
                        {
                            EditItemID::remapIDs (m, nullptr, options.edit);
                            t->getModifierList().insertModifier (m, i + 1, options.selectionManager);
                        }

                        return true;
                    }
                }
            }
        }
    }

    if (options.startTrack != nullptr && ! options.startTrack->isMarkerTrack())
    {
        for (auto m : modifiers)
        {
            EditItemID::remapIDs (m, nullptr, options.edit);
            options.startTrack->getModifierList().insertModifier (m, -1, options.selectionManager);
        }

        return true;
    }

    return false;
}
