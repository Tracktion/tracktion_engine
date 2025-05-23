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

void Clipboard::addListener (juce::ChangeListener* l)      { broadcaster.addChangeListener (l); }
void Clipboard::removeListener (juce::ChangeListener* l)   { broadcaster.removeChangeListener (l); }

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
    TimePosition startPos;
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

AudioTrack* getOrInsertAudioTrackNearestIndex (Edit& edit, int trackIndex)
{
    int i = 0;

    // find the next audio track on or after the given index..
    for (auto t : getAllTracks (edit))
    {
        if (i >= trackIndex)
            if (auto at = dynamic_cast<AudioTrack*> (t))
                return at;

        ++i;
    }

    return edit.insertNewAudioTrack (TrackInsertPoint (nullptr, getAllTracks (edit).getLast()), nullptr).get();
}

static TimePosition pasteMIDIFileIntoEdit (Edit& edit, const juce::File& midiFile,
                                           int& targetTrackIndex, int& targetSlotIndex,
                                           TimePosition startTime, bool importTempoChanges)
{
    CRASH_TRACER
    juce::OwnedArray<MidiList> lists;
    juce::Array<BeatPosition> tempoChangeBeatNumbers;
    juce::Array<double> bpms;
    juce::Array<int> numerators, denominators;

    auto newClipEndTime = startTime;
    BeatDuration len;
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

        auto startBeat = tempoSequence.toBeats (startTime);
        auto endBeat = startBeat + len;

        for (auto& list : lists)
            endBeat = std::max (endBeat, startBeat + BeatDuration::fromBeats (list->getLastBeatNumber().inBeats()));

        endBeat = startBeat + BeatDuration::fromBeats (std::ceil ((endBeat - startBeat).inBeats()));

        if (importTempoChanges)
        {
            if (tempoChangeBeatNumbers.size() > 0)
                tempoSequence.removeTemposBetween (TimeRange (startTime, tempoSequence.toTime (endBeat))
                                                     .expanded (0.001_td), true);

            for (int i = 0; i < tempoChangeBeatNumbers.size(); ++i)
            {
                auto insertTime = tempoSequence.toTime (startBeat + toDuration (tempoChangeBeatNumbers.getUnchecked (i)));
                auto& origTempo = tempoSequence.getTempoAt (insertTime);

                if (std::abs (origTempo.getBpm() - bpms.getUnchecked (i)) > 0.001)
                    if (auto tempo = tempoSequence.insertTempo (insertTime))
                        tempo->setBpm (bpms.getUnchecked (i));

                auto& origTimeSig = tempoSequence.getTimeSigAt (insertTime);

                if (origTimeSig.denominator != denominators.getUnchecked (i)
                     || origTimeSig.numerator != numerators.getUnchecked (i))
                {
                    if (auto timeSig = tempoSequence.insertTimeSig (insertTime))
                        timeSig->setStringTimeSig (juce::String (numerators.getUnchecked (i))
                                                    + "/" + juce::String (denominators.getUnchecked (i)));
                }
            }
        }

        auto lastTrackEndTime = BeatPosition::fromBeats (Edit::getMaximumEditEnd().inSeconds()); // Assumes 60bpm
        --targetTrackIndex;

        for (auto list : lists)
        {
            auto listBeatStart = list->getFirstBeatNumber();
            auto listBeatEnd = std::max (listBeatStart + 1_bd,
                                         std::max (list->getLastBeatNumber(),
                                                   toPosition (endBeat - startBeat)));

            if (lastTrackEndTime > listBeatStart)
                ++targetTrackIndex;

            lastTrackEndTime = listBeatEnd;

            juce::ValueTree clipState (IDs::MIDICLIP);
            clipState.setProperty (IDs::channel, list->getMidiChannel(), nullptr);

            if (list->state.isValid())
                clipState.addChild (list->state, -1, nullptr);

            if (auto at = getOrInsertAudioTrackNearestIndex (edit, targetTrackIndex))
            {
                const auto timeRange = tempoSequence.toTime ({ startBeat, endBeat });

                auto clipName = list->getImportedMidiTrackName();
                if (auto fn = list->getImportedFileName(); fn.isNotEmpty())
                    clipName += " - " + fn;

                auto targetOwner = [&]() -> ClipOwner*
                {
                    if (targetSlotIndex < 0)
                        return at;

                    auto& slotList = at->getClipSlotList();
                    auto slots = slotList.getClipSlots();

                    if (targetSlotIndex < slots.size())
                        return slots[targetSlotIndex];

                    if (! slots.isEmpty())
                        return slots.getFirst();

                    return at;
                }();

                if (auto newClip = insertClipWithState (*targetOwner,
                                                        clipState, clipName, TrackItem::Type::midi,
                                                        { timeRange, 0_td }, DeleteExistingClips::no, false))
                {
                    if (auto mc = dynamic_cast<MidiClip*> (newClip))
                    {
                        if (mc->getClipSlot() != nullptr)
                        {
                            mc->setUsesProxy (false);
                            mc->setStart (0_tp, false, true);
                            mc->setLoopRangeBeats (mc->getEditBeatRange());
                        }

                        if (importAsNoteExpression)
                            mc->setMPEMode(true);
                    }

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
    bool snapBWavsToOriginalTime = false;
};

static void askUserAboutProjectItemPastingOptions (Engine& engine,
                                                   const Clipboard::ProjectItems& items,
                                                   ProjectItemPastingOptions& options)
{
    auto& pm = engine.getProjectManager();
    auto& ui = engine.getUIBehaviour();

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

                    if (AudioFile (engine, file).getMetadata()[juce::WavAudioFormat::bwavTimeReference].isNotEmpty())
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
        if (numAudioClipsWithBWAV > 0 && ! engine.getEngineBehaviour().ignoreBWavTimestamps())
        {
           #if JUCE_MODAL_LOOPS_PERMITTED
            juce::ToggleButton toggle (TRANS("Snap to BWAV"));
            toggle.setSize (200, 20);

            std::unique_ptr<juce::AlertWindow> aw (juce::LookAndFeel::getDefaultLookAndFeel()
                                                    .createAlertWindow (TRANS("Add multiple files"),
                                                                        TRANS("Do you want to add multiple files to one track or to separate tracks?"),
                                                                        {}, {}, {}, juce::AlertWindow::QuestionIcon,
                                                                        0, nullptr));

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
        if (engine.getEngineBehaviour().ignoreBWavTimestamps())
            options.snapBWavsToOriginalTime = false;
        else
            options.snapBWavsToOriginalTime = ui.showOkCancelAlertBox (TRANS("BWAV Clip"),
                                                                       TRANS("Do you want clip placed at BWAV timestamp or cursor position?"),
                                                                       TRANS("BWAV timestamp"),
                                                                       TRANS("Cursor position"));
    }
}

inline bool isRecursiveEditClipPaste (const Clipboard::ProjectItems& items, Edit& edit)
{
    auto& pm = edit.engine.getProjectManager();

    for (auto& item : items.itemIDs)
        if (auto source = pm.getProjectItem (item.itemID))
            if (source->isEdit() && source->getID() == edit.getProjectItemID())
                return true;

    return false;
}

bool Clipboard::ProjectItems::pasteIntoEdit (const EditPastingOptions& options) const
{
    auto& e  = options.edit.engine;
    auto& pm = e.getProjectManager();
    auto& ui = options.edit.engine.getUIBehaviour();
    bool anythingPasted = false;

    ProjectItemPastingOptions pastingOptions;

    pastingOptions.separateTracks = options.preferredLayout == FileDragList::consecutiveTracks;

    if (! options.silent)
        askUserAboutProjectItemPastingOptions (e, *this, pastingOptions);

    if (isRecursiveEditClipPaste (*this, options.edit))
    {
        if (! options.silent)
            ui.showWarningAlert (TRANS("Can't Import Edit"),
                                 TRANS("You can't paste an edit clip into itself!"));

        return false;
    }

    auto [insertPointTrack, clipOwner, time] = options.insertPoint.chooseInsertPoint (false, options.selectionManager,
                                                                                      [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });
    auto startTime = time.value_or (0_tp);

    if (insertPointTrack == nullptr || clipOwner == nullptr)
    {
        jassertfalse;
        return false;
    }

    const bool pastingInToClipLauncher = dynamic_cast<ClipSlot*> (clipOwner) != nullptr;

    int targetTrackIndex = insertPointTrack->getIndexInEditTrackList();
    SelectableList itemsAdded;

    for (auto [index, item] : juce::enumerate (itemIDs))
    {
        if (auto sourceItem = pm.getProjectItem (item.itemID))
        {
            auto file = sourceItem->getSourceFile();
            auto newClipEndTime = startTime;

            if (file.exists())
            {
                if (sourceItem->isMidi())
                {
                    int targetSlotIndex = -1;

                    if (auto targetSlot = dynamic_cast<ClipSlot*> (clipOwner))
                        targetSlotIndex = findClipSlotIndex (*targetSlot);

                    newClipEndTime = pasteMIDIFileIntoEdit (options.edit, file, targetTrackIndex, targetSlotIndex,
                                                            startTime, pastingOptions.shouldImportTempoChangesFromMIDI);
                }
                else if (sourceItem->isWave())
                {
                    sourceItem->verifyLength();
                    jassert (sourceItem->getLength() > 0);

                    if (auto clipSlot = dynamic_cast<ClipSlot*> (clipOwner->getClipOwnerSelectable()))
                        if (auto existingClip = clipSlot->getClip())
                            existingClip->removeFromParent();

                    if (auto newClip = insertWaveClip (*clipOwner,
                                                       sourceItem->getName(), sourceItem->getID(),
                                                       { { startTime, TimePosition::fromSeconds (startTime.inSeconds() + sourceItem->getLength()) }, 0_td },
                                                       DeleteExistingClips::no))
                    {
                        newClipEndTime = newClip->getPosition().getEnd();
                        itemsAdded.add (newClip.get());

                        if (pastingOptions.snapBWavsToOriginalTime)
                            newClip->snapToOriginalBWavTime();

                        // Set sensible defaults for new launcher clips
                        if (newClip->getClipSlot())
                        {
                            if (newClip->effectsEnabled())
                                newClip->enableEffects (false, false);

                            newClip->setUsesProxy (false);
                            newClip->setAutoTempo (true);
                            newClip->setStart (0_tp, false, true);
                            newClip->setLoopRangeBeats ({ 0_bp, newClip->getLengthInBeats() });
                        }
                    }

                }
                else if (sourceItem->isEdit())
                {
                    sourceItem->verifyLength();
                    jassert (sourceItem->getLength() > 0);

                    if (auto newClip = insertEditClip (*clipOwner,
                                                       { startTime, startTime + TimeDuration::fromSeconds (sourceItem->getLength()) },
                                                         sourceItem->getID()))
                    {
                        newClipEndTime = newClip->getPosition().getEnd();
                        itemsAdded.add (newClip.get());

                        // Set sensible defaults for new launcher clips
                        if (newClip->getClipSlot())
                        {
                            if (newClip->effectsEnabled())
                                newClip->enableEffects (false, false);

                            newClip->setUsesProxy (false);
                            newClip->setAutoTempo (true);
                            newClip->setLoopRangeBeats ({ 0_bp, newClip->getLengthInBeats() });
                        }
                    }
                }

                anythingPasted = true;

                if (int (index) < int (itemIDs.size() - 1))
                {
                    if (pastingInToClipLauncher)
                    {
                        if (juce::ModifierKeys::currentModifiers.isCommandDown() || options.preferredLayout == FileDragList::consecutiveTracks)
                        {
                            ++targetTrackIndex;
                            auto newTrack = getOrInsertAudioTrackNearestIndex (options.edit, targetTrackIndex);

                            auto slot = dynamic_cast<ClipSlot*> (clipOwner);
                            auto& at = *dynamic_cast<AudioTrack*> (&slot->track);
                            auto& list = at.getClipSlotList();

                            auto idx = list.getClipSlots().indexOf (slot);

                            options.edit.getSceneList().ensureNumberOfScenes (list.getClipSlots().size());

                            clipOwner = newTrack->getClipSlotList().getClipSlots()[idx];
                        }
                        else
                        {
                            auto slot = dynamic_cast<ClipSlot*> (clipOwner);
                            auto& at = *dynamic_cast<AudioTrack*> (&slot->track);
                            auto& list = at.getClipSlotList();

                            auto idx = list.getClipSlots().indexOf (slot) + 1;

                            options.edit.getSceneList().ensureNumberOfScenes (idx + 1);

                            clipOwner = list.getClipSlots()[idx];
                        }
                    }
                    else
                    {
                        if (pastingOptions.separateTracks)
                            ++targetTrackIndex;
                        else
                            startTime = newClipEndTime;

                        clipOwner = getOrInsertAudioTrackNearestIndex (options.edit, targetTrackIndex);
                    }
                }
            }
        }
    }

    if (itemsAdded.isNotEmpty())
        if (auto sm = options.selectionManager)
            sm->select (itemsAdded);

    return anythingPasted;
}

bool Clipboard::ProjectItems::pasteIntoProject (Project& project) const
{
    for (auto& item : itemIDs)
        if (auto source = project.engine.getProjectManager().getProjectItem (item.itemID))
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

void Clipboard::Clips::addClip (int trackOffset, const juce::ValueTree& state)
{
    ClipInfo ci;
    ci.trackOffset = trackOffset;
    ci.state = state.createCopy();

    clips.push_back (ci);
}

void Clipboard::Clips::addSelectedClips (const SelectableList& selectedObjects,
                                         TimeRange range,
                                         AutomationLocked automationLocked)
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

    auto firstSlotIndex = ed.engine.getEngineBehaviour().getEditLimits().maxClipsInTrack;
    auto firstTrackIndex = ed.engine.getEngineBehaviour().getEditLimits().maxNumTracks;
    auto overallStartTime = TimePosition::fromSeconds (Edit::maximumLength);

    for (auto clip : clipsToPaste)
    {
        overallStartTime = std::min (overallStartTime, std::max (clip->getPosition().getStart(), range.getStart()));
        firstTrackIndex  = std::min (firstTrackIndex,  std::max (0, allTracks.indexOf (clip->getTrack())));

        if (auto slot = clip->getClipSlot())
            firstSlotIndex = std::min (firstSlotIndex, slot->getIndex());
    }

    for (auto clip : clipsToPaste)
    {
        if (clip->getEditTimeRange().overlaps (range))
        {
            auto clipPos = clip->getPosition();
            auto clippedStart = std::max (clipPos.getStart(), range.getStart());
            auto clippedOffset = clipPos.getOffset() + (clippedStart - clipPos.getStart());
            auto clippedEnd = std::min (clipPos.getEnd(), range.getEnd());

            ClipInfo info;

            info.grouped = clip->isGrouped();

            clip->flushStateToValueTree();
            info.state = clip->state.createCopy();

            addValueTreeProperties (info.state,
                                    IDs::start, (clippedStart - overallStartTime).inSeconds(),
                                    IDs::length, (clippedEnd - clippedStart).inSeconds(),
                                    IDs::offset, clippedOffset.inSeconds());

            auto acb = dynamic_cast<AudioClipBase*> (clip);

            if (acb != nullptr)
            {
                // If we're just pasting in to the Edit without trying to fit it in to a range,
                // we need to flush the fade in/out so pasted clips don't get default edge fades
                if (range == Edit::getMaximumEditTimeRange())
                {
                    addValueTreeProperties (info.state,
                                            IDs::fadeIn,  acb->getFadeIn().inSeconds(),
                                            IDs::fadeOut, acb->getFadeOut().inSeconds());
                }
                else
                {
                    auto inOutPoints = clip->getEditTimeRange().getIntersectionWith (range);
                    TimeRange fadeIn (clipPos.getStart(), clipPos.getStart() + acb->getFadeIn());
                    TimeRange fadeOut (clipPos.getEnd() - acb->getFadeOut(), clipPos.getEnd());

                    addValueTreeProperties (info.state,
                                            IDs::fadeIn,  fadeIn.overlaps (inOutPoints)  ? fadeIn.getIntersectionWith (inOutPoints).getLength().inSeconds() : 0.0,
                                            IDs::fadeOut, fadeOut.overlaps (inOutPoints) ? fadeOut.getIntersectionWith (inOutPoints).getLength().inSeconds() : 0.0);
                }

                // Also flush these properties so the defaults aren't picked up
                addValueTreeProperties (info.state,
                                        IDs::proxyAllowed, acb->canUseProxy(),
                                        IDs::resamplingQuality, juce::VariantConverter<ResamplingQuality>::toVar (acb->getResamplingQuality()));
            }

            info.trackOffset = allTracks.indexOf (clip->getTrack()) - firstTrackIndex;

            if (auto slot = clip->getClipSlot())
                info.slotOffset = slot->getIndex() - firstSlotIndex;

            if (acb == nullptr || acb->getAutoTempo())
            {
                info.hasBeatTimes = true;

                auto& ts = ed.tempoSequence;
                info.startBeats = BeatPosition::fromBeats ((ts.toBeats (clippedStart) - ts.toBeats (overallStartTime)).inBeats());
                info.lengthBeats = ts.toBeats (clippedEnd) - ts.toBeats (clippedStart);
                info.offsetBeats = BeatPosition::fromBeats (ts.getBeatsPerSecondAt (clippedStart) * clippedOffset.inSeconds());
            }

            clips.push_back (info);
        }
    }

    if (automationLocked == AutomationLocked::yes)
        addAutomation (TrackSection::findSections (clipsToPaste), range);
}

void Clipboard::Clips::addAutomation (const juce::Array<TrackSection>& trackSections, TimeRange range)
{
    if (range.isEmpty() || trackSections.isEmpty())
        return;

    auto& edit = trackSections.getFirst().track->edit;
    auto& ts = edit.tempoSequence;
    auto allTracks = getAllTracks (edit);
    auto firstTrackIndex = edit.engine.getEngineBehaviour().getEditLimits().maxNumTracks;
    auto overallStartTime = TimePosition::fromSeconds (Edit::maximumLength);

    for (const auto& trackSection : trackSections)
    {
        overallStartTime = std::min (overallStartTime, std::max (trackSection.range.getStart(), range.getStart()));
        firstTrackIndex  = std::min (firstTrackIndex,  std::max (0, allTracks.indexOf (trackSection.track)));
    }

    for (const auto& trackSection : trackSections)
    {
        for (auto plugin : trackSection.track->pluginList)
        {
            for (int k = 0; k < plugin->getNumAutomatableParameters(); k++)
            {
                auto param = plugin->getAutomatableParameter (k);

                if (param->getCurve().getNumPoints() > 0)
                {
                    AutomationCurveSection section;
                    section.pluginName = plugin->getName();
                    section.paramID = param->paramID;
                    section.trackOffset = std::max (0, allTracks.indexOf (trackSection.track) - firstTrackIndex);
                    section.valueRange = param->getValueRange();

                    const auto endTolerence = 0.0001_td;
                    auto intersection = trackSection.range.getIntersectionWith (range);
                    auto reducedIntersection = intersection.reduced (endTolerence);
                    auto clippedStart = intersection.getStart();
                    auto clippedEnd   = intersection.getEnd();

                    for (int l = 0; l < param->getCurve().getNumPoints(); ++l)
                    {
                        auto pt = param->getCurve().getPoint (l);

                        if (reducedIntersection.containsInclusive (toTime (pt.time, ts)))
                            section.points.push_back ({ pt.time, pt.value, pt.curve });
                    }

                    auto defaultValue = param->getCurrentBaseValue();

                    if (section.points.empty())
                    {
                        section.points.push_back ({ clippedStart, param->getCurve().getValueAt (clippedStart, defaultValue), 1.0f });
                        section.points.push_back ({ clippedEnd, param->getCurve().getValueAt (clippedEnd, defaultValue), 0.0f });
                    }
                    else
                    {
                        if (toTime (section.points[0].time, ts) > clippedStart + endTolerence)
                            section.points.insert (section.points.begin(), { clippedStart + endTolerence, param->getCurve().getValueAt (clippedStart + endTolerence, defaultValue), 0.0f });

                        if (toTime (section.points[section.points.size() - 1].time, ts) < clippedEnd - endTolerence)
                            section.points.push_back ({ clippedEnd - endTolerence, param->getCurve().getValueAt (clippedEnd - endTolerence, defaultValue), 0.0f });
                    }

                    for (auto& p : section.points)
                        p.time = toTime (p.time, ts) - TimeDuration::fromSeconds (overallStartTime.inSeconds());

                    std::sort (section.points.begin(), section.points.end());
                    automationCurves.push_back (std::move (section));
                }
            }
        }
    }
}

static void fixClipTimes (juce::ValueTree& state, const Clipboard::Clips::ClipInfo& clip,
                          const std::vector<Clipboard::Clips::ClipInfo>& otherClips,
                          TempoSequence& tempoSequence, TimePosition startOffset)
{
    TimePosition start, offset;
    TimeDuration length;

    if (clip.hasBeatTimes)
    {
        BeatDuration slotOffset;

        if (clip.slotOffset.has_value())
            for (const auto& info : otherClips)
                if (info.trackOffset == clip.trackOffset && info.slotOffset.has_value() && *info.slotOffset < *clip.slotOffset)
                    slotOffset = slotOffset + info.lengthBeats;

        auto offsetInBeats = BeatDuration::fromBeats (tempoSequence.toBeats (startOffset).inBeats());
        auto range = tempoSequence.toTime ({ clip.startBeats + offsetInBeats + slotOffset, clip.startBeats + offsetInBeats + slotOffset + clip.lengthBeats });
        start  = range.getStart();
        length = range.getLength();
        offset = TimePosition::fromSeconds (clip.offsetBeats.inBeats() / tempoSequence.getBeatsPerSecondAt (start));
    }
    else
    {
        start  = TimePosition::fromSeconds (static_cast<double> (state.getProperty (IDs::start)))
                    + TimeDuration::fromSeconds (startOffset.inSeconds());
        length = TimeDuration::fromSeconds (static_cast<double> (state.getProperty (IDs::length)));
        offset = TimePosition::fromSeconds (static_cast<double> (state.getProperty (IDs::offset)));
    }

    // if clip is coming from preset, it'll have this
    // property, so resize it to match tempo
    if (const double srcBpm = state[IDs::bpm]; srcBpm > 0)
    {
        auto& destTempo = tempoSequence.getTempoAt (start);
        length = TimeDuration::fromSeconds (length.inSeconds() * srcBpm / destTempo.getBpm());
    }

    state.setProperty (IDs::start, start.inSeconds(), nullptr);
    state.setProperty (IDs::length, length.inSeconds(), nullptr);
    state.setProperty (IDs::offset, offset.inSeconds(), nullptr);

    state.removeProperty (IDs::bpm, nullptr);
    state.removeProperty (IDs::key, nullptr);
}

static bool pastePointsToCurve (const std::vector<AutomationCurve::AutomationPoint>& points, juce::Range<float> sourceValueRange,
                                AutomationCurve& targetCurve, juce::Range<float> targetValueRange,  float defaultValue,
                                std::optional<EditTimeRange> targetRange)
{
    AutomationCurve newCurve (targetCurve.edit, targetCurve.timeBase);
    newCurve.setParameterID (targetCurve.getParameterID());

    jassert (! targetValueRange.isEmpty());
    auto um = getUndoManager_p (targetCurve.edit);
    auto& ts = getTempoSequence (targetCurve.edit);

    for (auto p : points)
    {
        if (targetValueRange != sourceValueRange)
        {
            auto normalised = (p.value - sourceValueRange.getStart()) / sourceValueRange.getLength();
            p.value = targetValueRange.getStart() + targetValueRange.getLength() * normalised;
        }

        newCurve.addPoint (p.time, p.value, p.curve, um);
    }

    if (newCurve.getLength() > 0_td)
    {
        if (targetRange)
        {
            if (isGreaterThanZero (targetRange->getLength()))
                newCurve.rescaleAllPositions (toUnderlying (targetRange->getLength()) / toUnderlying (newCurve.getLength()), um);
            else
                targetRange = EditTimeRange (toTime (*targetRange, ts).getStart(), newCurve.getLength());
        }
        else
        {
            targetRange = getFullRange (newCurve);
        }

        mergeCurve (targetCurve, *targetRange,
                    newCurve, 0_tp,
                    defaultValue, 0_td,
                    false, false);

        return true;
    }

    return false;
}

static bool pastePointsToCurve (const std::vector<AutomationCurve::AutomationPoint>& points, juce::Range<float> sourceValueRange,
                                AutomatableParameter& targetParameter, AutomationCurve& targetCurve, TimeRange targetRange)
{
    return pastePointsToCurve (points, sourceValueRange,
                               targetCurve, targetParameter.getValueRange(),  targetParameter.getCurrentBaseValue(),
                               targetRange);
}

bool Clipboard::Clips::pasteIntoEdit (const EditPastingOptions& options) const
{
    if (clips.empty())
        return false;

    auto targetTrack = options.startTrack;
    auto targetClipOwnerID = options.targetClipOwnerID;

    if (targetTrack == nullptr)
    {
        auto placement = options.insertPoint.chooseInsertPoint (false, options.selectionManager,
                                                                [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });
        targetTrack = placement.track;
        targetClipOwnerID = placement.clipOwner != nullptr ? placement.clipOwner->getClipOwnerID() : EditItemID();
        jassert (targetTrack != nullptr);
    }

    // We can't paste into a folder or submix track, so find the next clip track
    while (targetTrack != nullptr && targetTrack->isFolderTrack())
        targetTrack = targetTrack->getSiblingTrack (1, false);

    if (targetTrack == nullptr)
        return false;

    std::map<EditItemID, EditItemID> remappedIDs;
    SelectableList itemsAdded;

    for (auto& clip : clips)
    {
        auto newClipState = clip.state.createCopy();
        EditItemID::remapIDs (newClipState, nullptr, options.edit, &remappedIDs);
        fixClipTimes (newClipState, clip, clips, options.edit.tempoSequence, options.startTime);

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
        else if (newClipState.hasType (IDs::CHORDCLIP))
        {
            if (auto chordTrack = options.edit.getChordTrack())
            {
                if (auto newClip = chordTrack->insertClipWithState (newClipState))
                    itemsAdded.add (newClip);
            }
        }
        else if (newClipState.hasType (IDs::ARRANGERCLIP))
        {
            if (auto arrangerTrack = options.edit.getArrangerTrack())
            {
                if (auto newClip = arrangerTrack->insertClipWithState (newClipState))
                    itemsAdded.add (newClip);
            }
        }
        else
        {
            if (auto clipSlot = findClipSlotForID (options.edit, targetClipOwnerID))
            {
                if (clip.grouped)
                {
                    options.edit.engine.getUIBehaviour().showWarningMessage (TRANS ("Group clips can not be added to the clip launcher"));
                }
                else
                {
                    auto calcSlotOffset = [&]
                    {
                        auto offset = 0;

                        for (const auto& c : clips)
                            if (c.trackOffset == clip.trackOffset && c.startBeats < clip.startBeats)
                                offset++;

                        return offset;
                    };

                    auto slotOffset = clip.slotOffset.has_value() ? *clip.slotOffset : calcSlotOffset();

                    auto tracks = getAudioTracks (options.edit);
                    auto trackIndex = tracks.indexOf (dynamic_cast<AudioTrack*> (&clipSlot->track));
                    auto slotIndex  = clipSlot->getIndex();

                    trackIndex += clip.trackOffset;
                    slotIndex  += slotOffset;

                    options.edit.getSceneList().ensureNumberOfScenes (slotIndex + 1);
                    options.edit.ensureNumberOfAudioTracks (trackIndex + 1);

                    if (auto at = getAudioTracks (options.edit)[trackIndex])
                        clipSlot = at->getClipSlotList().getClipSlots()[slotIndex];

                    if (auto existingClip = clipSlot->getClip())
                        existingClip->removeFromParent();

                    if (auto newClip = insertClipWithState (*clipSlot, newClipState))
                        itemsAdded.add (newClip);
                }
            }
            else if (auto clipTrack = dynamic_cast<ClipTrack*> (targetTrack->getSiblingTrack (clip.trackOffset, false)))
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

    std::map<EditItemID, EditItemID> groupMap;
    for (auto c : itemsAdded.getItemsOfType<Clip>())
    {
        if (c->isGrouped())
        {
            auto originalGroup = c->getGroupID();

            if (groupMap.find (originalGroup) == groupMap.end())
                groupMap[originalGroup] = c->edit.createNewItemID();

            c->setGroup (groupMap[originalGroup]);
        }
    }

    for (auto& curve : automationCurves)
    {
        if (! curve.points.empty())
        {
            const TimeRange destCurveTimeRange (options.startTime, TimeDuration());

            if (auto clipTrack = dynamic_cast<ClipTrack*> (targetTrack->getSiblingTrack (curve.trackOffset, false)))
            {
                for (auto plugin : clipTrack->pluginList)
                {
                    if (plugin->getName() == curve.pluginName)
                    {
                        if (auto targetParam = plugin->getAutomatableParameterByID (curve.paramID))
                        {
                            pastePointsToCurve (curve.points, curve.valueRange, *targetParam, targetParam->getCurve(), destCurveTimeRange);
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
    {
        bool first = true;
        for (auto i : itemsAdded)
        {
            if (auto c = dynamic_cast<Clip*> (i); c->isGrouped())
                sm->select (c->getGroupClip(), ! first);
            else
                sm->select (i, ! first);
            first = false;
        }
    }

    if (options.setTransportToEnd && ! options.edit.getTransport().isPlaying())
        options.edit.getTransport().setPosition (getTimeRangeForSelectedItems (itemsAdded).getEnd());

    return true;
}

bool Clipboard::Clips::pasteIntoEdit (Edit& edit, EditInsertPoint& insertPoint, SelectionManager* sm) const
{
    Clipboard::ContentType::EditPastingOptions options (edit, insertPoint, sm);
    auto placement = insertPoint.chooseInsertPoint (false, sm,
                                                    [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });

    options.startTrack = placement.track;
    options.startTime = placement.time.value_or (0_tp);
    options.targetClipOwnerID = placement.clipOwner != nullptr ? placement.clipOwner->getClipOwnerID()
                                                               : EditItemID();

    return pasteIntoEdit (options);
}

bool Clipboard::Clips::pasteAfterSelected (Edit& edit, EditInsertPoint& insertPoint, SelectionManager& sm) const
{
    EditPastingOptions options (edit, insertPoint, &sm);
    auto placement = insertPoint.chooseInsertPoint (true, &sm,
                                                    [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });

    options.startTrack = placement.track;
    options.startTime = placement.time.value_or (0_tp);
    options.targetClipOwnerID = placement.clipOwner != nullptr ? placement.clipOwner->getClipOwnerID()
                                                               : EditItemID();

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
        insertPoint.setNextInsertPoint (edit.getTransport().position.get(), c->getTrack());
    }

    if (tracks.isEmpty() && noFolders)
        tracks.addArray (getClipTracks (edit));

    return tracks;
}

static TimeDuration getNewClipsTotalLength (const Clipboard::Clips& clips, Edit& edit)
{
    TimePosition total;

    for (auto& i : clips.clips)
    {
        auto end = i.hasBeatTimes ? edit.tempoSequence.toTime (i.startBeats + i.lengthBeats)
                                  : TimePosition::fromSeconds (static_cast<double> (i.state.getProperty (IDs::start))
                                                               + static_cast<double> (i.state.getProperty (IDs::length)));

        total = std::max (total, end);
    }

    return toDuration (total);
}

bool Clipboard::Clips::pasteInsertingAtCursorPos (Edit& edit, EditInsertPoint& insertPoint, SelectionManager& sm) const
{
    if (clips.empty())
        return false;

    auto tracks = findTracksToInsertInto (edit, insertPoint, sm);
    auto insertLength = getNewClipsTotalLength (*this, edit);

    if (tracks.isEmpty() || insertLength <= TimeDuration())
        return false;

    auto cursorPos = edit.getTransport().getPosition();
    auto firstTrackIndex = tracks.getFirst()->getIndexInEditTrackList();

    for (auto t : tracks)
    {
        t->splitAt (cursorPos);
        t->insertSpaceIntoTrack (cursorPos, insertLength);
        firstTrackIndex = std::min (firstTrackIndex, t->getIndexInEditTrackList());
    }

    EditPastingOptions options (edit, insertPoint, &sm);
    options.startTime = std::max (0_tp, cursorPos);
    return pasteIntoEdit (options);
}

//==============================================================================
//==============================================================================
Clipboard::Scenes::Scenes() {}
Clipboard::Scenes::~Scenes() {}

bool Clipboard::Scenes::pasteIntoEdit (const EditPastingOptions& options) const
{
    auto& sceneList = options.edit.getSceneList();

    auto insertIndex = sceneList.getNumScenes();
    if (auto sm = options.selectionManager)
    {
        auto items = sm->getSelectedObjects().getItemsOfType<Scene>();
        if (items.size() > 0)
        {
            insertIndex = 0;
            for (auto s : items)
                insertIndex = std::max (insertIndex, s->getIndex() + 1);
        }
        sm->deselectAll();
    }

    std::map<EditItemID, EditItemID> remappedIDs;
    SelectableList itemsAdded;

    for (auto info : scenes)
    {
        if (auto newScene = sceneList.insertScene (insertIndex))
        {
            newScene->state.copyPropertiesAndChildrenFrom (info.state, &options.edit.getUndoManager());

            options.edit.ensureNumberOfAudioTracks (int (info.clips.size()));
            auto tracks = getAudioTracks (options.edit);
            for (auto [idx, clip] : juce::enumerate (info.clips))
            {
                auto track = tracks[int (idx)];
                auto slot  = track->getClipSlotList().getClipSlots()[insertIndex];

                if (clip.isValid())
                {
                    auto newClipState = clip.createCopy();
                    EditItemID::remapIDs (newClipState, nullptr, options.edit, &remappedIDs);

                    insertClipWithState (*slot, newClipState);
                }
            }
            itemsAdded.add (newScene);
        }

        insertIndex++;
    }

    if (itemsAdded.isEmpty())
        return false;

    if (auto sm = options.selectionManager)
        sm->select (itemsAdded);

    return true;
}

//==============================================================================
//==============================================================================
Clipboard::Tracks::Tracks() {}
Clipboard::Tracks::~Tracks() {}

bool Clipboard::Tracks::pasteIntoEdit (const EditPastingOptions& options) const
{
    CRASH_TRACER

    juce::Array<Track::Ptr> newTracks;
    std::map<EditItemID, EditItemID> remappedIDs;

    auto targetTrack = options.startTrack;

    // When pasting tracks, always paste after the selected group of tracks if the target is
    // within the selection
    auto allTracks = getAllTracks (options.edit);

    if (options.selectionManager != nullptr && options.selectionManager->isSelected (targetTrack.get()))
        for (auto t : options.selectionManager->getItemsOfType<Track>())
            if (allTracks.indexOf (t) > allTracks.indexOf (targetTrack.get()))
                targetTrack = t;

    if (options.selectionManager != nullptr)
        options.selectionManager->deselectAll();

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

                // AutomationCurveModifier reassignment is done during updateRelativeDestinationOrRemove so
                // we need to make sure we don't remove these
                if (dynamic_cast<AutomationCurveModifier::Assignment*> (ass.get()) != nullptr)
                    continue;

                const auto oldID = EditItemID::fromProperty (ass->state, IDs::source);
                const auto newID = remappedIDs[oldID];

                if (newID.isValid())
                {
                    ass->state.setProperty (IDs::source, newID, nullptr);
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
Clipboard::TempoChanges::TempoChanges (const TempoSequence& ts, TimeRange range)
{
    auto beats = ts.toBeats (range);

    BeatPosition startBeat = BeatPosition::fromBeats (std::floor (beats.getStart().inBeats() + 0.5));
    BeatPosition endBeat   = BeatPosition::fromBeats (std::floor (beats.getEnd().inBeats() + 0.5));

    bool pointAtStart = false;
    bool pointAtEnd   = false;

    for (auto t : ts.getTempos())
    {
        if (t->startBeatNumber == startBeat) pointAtStart = true;
        if (t->startBeatNumber == endBeat)   pointAtEnd   = true;

        if (range.containsInclusive (t->getStartTime()))
            changes.push_back ({ t->startBeatNumber - toDuration (startBeat),
                                 t->getBpm(),
                                 t->getCurve() });
    }

    if (! pointAtStart)
        changes.insert (changes.begin(),
                        { BeatPosition(),
                          ts.getBpmAt (ts.toTime (startBeat)),
                          ts.getTempoAt (BeatPosition::fromBeats (juce::roundToInt (startBeat.inBeats()))).getCurve() });

    if (! pointAtEnd)
        changes.push_back ({ toPosition (endBeat - startBeat),
                             ts.getBpmAt (ts.toTime (endBeat)),
                             ts.getTempoAt (BeatPosition::fromBeats (juce::roundToInt (endBeat.inBeats()))).getCurve() });
}

Clipboard::TempoChanges::~TempoChanges() {}

bool Clipboard::TempoChanges::pasteIntoEdit (const EditPastingOptions& options) const
{
    return pasteTempoSequence (options.edit.tempoSequence, TimeRange::emptyRange (options.startTime));
}

bool Clipboard::TempoChanges::pasteTempoSequence (TempoSequence& ts, TimeRange targetRange) const
{
    if (changes.empty())
        return false;

    EditTimecodeRemapperSnapshot snap;
    snap.savePreChangeState (ts.edit);

    auto lengthInBeats = toDuration (changes.back().beat);

    if (targetRange.isEmpty())
        targetRange = targetRange.withEnd (ts.toTime (ts.toBeats (targetRange.getStart()) + lengthInBeats));

    auto startBeat = BeatPosition::fromBeats (std::floor (ts.toBeats (targetRange.getStart()).inBeats() + 0.5));
    auto endBeat   = BeatPosition::fromBeats (std::floor (ts.toBeats (targetRange.getEnd()).inBeats()   + 0.5));

    double finalBPM = ts.getBpmAt (ts.toTime (endBeat));
    ts.removeTemposBetween (ts.toTime ({ startBeat, endBeat }), false);
    ts.insertTempo (ts.toTime (startBeat));

    for (auto& tc : changes)
        ts.insertTempo (BeatPosition::fromBeats (juce::roundToInt ((tc.beat.inBeats() / lengthInBeats.inBeats()) * (endBeat - startBeat).inBeats() + startBeat.inBeats())),
                        tc.bpm, tc.curve);

    ts.insertTempo (ts.toTime (endBeat));
    ts.insertTempo (endBeat, finalBPM, 1.0f);

    for (int i = ts.getNumTempos(); --i >= 1;)
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

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPBOARD

//==============================================================================
//==============================================================================
class ClipboardTempoTests   : public juce::UnitTest
{
public:
    ClipboardTempoTests() : juce::UnitTest ("ClipboardTempoTests", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        runCopyTests();
        runCopyTestsUsingBeatInsertion();
        runTrackCopyPasteTests();
    }

private:
    template<typename TimeType>
    void expectEquals (TimeType t, double t2)
    {
        juce::UnitTest::expectEquals (t.inSeconds(), t2);
    }

    void expectEquals (BeatPosition t, double t2)
    {
        juce::UnitTest::expectEquals (t.inBeats(), t2);
    }

    void expectEquals (BeatDuration t, double t2)
    {
        juce::UnitTest::expectEquals (t.inBeats(), t2);
    }

    void expectTempoSetting (TempoSetting& tempo, double bpm, float curve)
    {
        expectWithinAbsoluteError (tempo.getBpm(), bpm, 0.001);
        expectWithinAbsoluteError (tempo.getCurve(), curve, 0.001f);
    }

    void runCopyTests()
    {
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);
        auto& ts = edit->tempoSequence;

        beginTest ("Simple copy/paste");
        {
            ts.getTempo (0)->setBpm (120.0);

            // N.B. bars start at 0!
            expectEquals (ts.toBeats ({ 0, {} }), 0.0);
            expectEquals (ts.toTime ({ 0, {} }), 0.0);
            expectEquals (ts.toBeats ({ 8, {} }), 32.0);
            expectEquals (ts.toTime ({ 8, {} }), 16.0);

            ts.insertTempo (ts.toBeats ({ 5, {} }), 60.0, 1.0f);
            ts.insertTempo (ts.toBeats ({ 9, {} }), 120.0, 1.0f);

            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 6, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 8, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 10, {} })), 120.0, 1.0f);

            const BeatRange beatRangeToCopy (ts.toBeats ({ 6, {} }), ts.toBeats ({ 8, {} }));
            const auto timeRangeToCopy = ts.toTime (beatRangeToCopy);
            const BeatDuration numBeatsToInsert = beatRangeToCopy.getLength();

            // Copy tempo changes
            Clipboard::TempoChanges tempoChanges (ts, timeRangeToCopy);

            // Insert empty space
            const auto timeToInsertAt = ts.toTime ({ 2, {} });
            auto& tempoAtInsertionPoint = ts.getTempoAt (timeToInsertAt);

            const auto beatRangeToInsert = BeatRange (ts.toBeats (timeToInsertAt), numBeatsToInsert);
            const auto lengthInTimeToInsert = ts.toTime (toPosition (beatRangeToInsert.getLength()));
            insertSpaceIntoEdit (*edit, TimeRange (timeToInsertAt, toDuration (lengthInTimeToInsert)));

            const auto numBeatsInserted = beatRangeToInsert.getLength();
            const int numBarsInserted = juce::roundToInt (numBeatsInserted.inBeats() / tempoAtInsertionPoint.getMatchingTimeSig().denominator);
            expectWithinAbsoluteError (numBeatsInserted.inBeats(), 8.0, 0.0001);
            expect (numBarsInserted == 2);

            // Ensure tempos are correct at original region
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4 + numBarsInserted, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 6 + numBarsInserted, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 8 + numBarsInserted, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 10 + numBarsInserted, {} })), 120.0, 1.0f);

            // Paste tempo changes
            tempoChanges.pasteTempoSequence (ts, TimeRange (timeToInsertAt, lengthInTimeToInsert));

            // Ensure tempos are correct at inserted region
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 0, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 1, BeatDuration::fromBeats (3) })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 2, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 3, BeatDuration::fromBeats (3) })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4, {} })), 120.0, 1.0f);
        }
    }

    void runCopyTestsUsingBeatInsertion()
    {
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);
        auto& ts = edit->tempoSequence;

        beginTest ("Simple copy/paste");
        {
            ts.getTempo (0)->setBpm (120.0);

            // N.B. bars start at 0!
            expectEquals (ts.toBeats ({ 0, {} }), 0.0);
            expectEquals (ts.toTime ({ 0, {} }), 0.0);
            expectEquals (ts.toBeats ({ 8, {} }), 32.0);
            expectEquals (ts.toTime ({ 8, {} }), 16.0);

            ts.insertTempo (ts.toBeats ({ 5, {} }), 60.0, 1.0f);
            ts.insertTempo (ts.toBeats ({ 9, {} }), 120.0, 1.0f);

            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 6, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 8, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 10, {} })), 120.0, 1.0f);

            const BeatRange beatRangeToCopy (ts.toBeats ({ 6, {} }), ts.toBeats ({ 8, {} }));
            const auto timeRangeToCopy = ts.toTime (beatRangeToCopy);

            // Copy tempo changes
            Clipboard::TempoChanges tempoChanges (ts, timeRangeToCopy);

            // Insert empty space
            const auto timeToInsertAt = ts.toTime ({ 2, {} });
            auto& tempoAtInsertionPoint = ts.getTempoAt (timeToInsertAt);
            const auto beatRangeToInsert = beatRangeToCopy.movedToStartAt (ts.toBeats (timeToInsertAt));
            insertSpaceIntoEditFromBeatRange (*edit, beatRangeToInsert);

            const auto numBeatsInserted = beatRangeToInsert.getLength();
            const int numBarsInserted = juce::roundToInt (numBeatsInserted.inBeats() / tempoAtInsertionPoint.getMatchingTimeSig().denominator);
            expectWithinAbsoluteError (numBeatsInserted.inBeats(), 8.0, 0.0001);
            expect (numBarsInserted == 2);

            // Ensure tempos are correct at original region
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4 + numBarsInserted, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 6 + numBarsInserted, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 8 + numBarsInserted, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 10 + numBarsInserted, {} })), 120.0, 1.0f);

            // Paste tempo changes
            tempoChanges.pasteTempoSequence (ts, TimeRange (timeToInsertAt, ts.toTime (beatRangeToInsert.getEnd())));

            // Ensure tempos are correct at inserted region
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 0, {} })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 1, BeatDuration::fromBeats (3) })), 120.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 2, {} })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 3, BeatDuration::fromBeats (3) })), 60.0, 1.0f);
            expectTempoSetting (ts.getTempoAt (ts.toBeats ({ 4, {} })), 120.0, 1.0f);
        }
    }

    void runTrackCopyPasteTests()
    {
        beginTest ("Root tracks copy/paste");
        {
            auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);
            edit->ensureNumberOfAudioTracks (3);

            {
                auto tracks = getAudioTracks(*edit);
                juce::UnitTest::expectEquals (tracks.size(), 3);
                tracks[0]->setName ("First");
                tracks[1]->setName ("Second");
                tracks[2]->setName ("Third");

                Clipboard::Tracks clipboardTracks;

                for (auto at : tracks)
                    clipboardTracks.tracks.push_back (at->state.createCopy());

                // Rename existing tracks for testing later
                tracks[0]->setName ("First Original");
                tracks[1]->setName ("Second Original");
                tracks[2]->setName ("Third Original");

                EditInsertPoint insertPoint (*edit);
                Clipboard::ContentType::EditPastingOptions opts (*edit, insertPoint);
                opts.startTrack = tracks[2];
                expect (clipboardTracks.pasteIntoEdit (opts));
            }

            {
                auto tracks = getAudioTracks(*edit);
                juce::UnitTest::expectEquals (tracks.size(), 6);
                juce::UnitTest::expectEquals<juce::String> (tracks[0]->getName(), "First Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[1]->getName(), "Second Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[2]->getName(), "Third Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[3]->getName(), "First");
                juce::UnitTest::expectEquals<juce::String> (tracks[4]->getName(), "Second");
                juce::UnitTest::expectEquals<juce::String> (tracks[5]->getName(), "Third");
            }
        }

        beginTest ("Root tracks paste in to folder");
        {
            auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);

            {
                edit->ensureNumberOfAudioTracks (3);
                auto tracks = getAudioTracks (*edit);
                juce::UnitTest::expectEquals (tracks.size(), 3);
                tracks[0]->setName ("First");
                tracks[1]->setName ("Second");
                tracks[2]->setName ("Third");
            }

            auto ft = edit->insertNewFolderTrack (TrackInsertPoint ({ *getAudioTracks (*edit).getLast(), false }), nullptr, false);

            {
                Clipboard::Tracks clipboardTracks;
                auto tracks = getAudioTracks (*edit);

                for (auto at : tracks)
                    clipboardTracks.tracks.push_back (at->state.createCopy());

                // Rename existing tracks for testing later
                tracks[0]->setName ("First Original");
                tracks[1]->setName ("Second Original");
                tracks[2]->setName ("Third Original");

                EditInsertPoint insertPoint (*edit);
                Clipboard::ContentType::EditPastingOptions opts (*edit, insertPoint);
                opts.startTrack = ft;
                expect (clipboardTracks.pasteIntoEdit (opts));
            }

            {
                auto tracks = getAudioTracks (*edit);
                juce::UnitTest::expectEquals (tracks.size(), 6);
                juce::UnitTest::expectEquals<juce::String> (tracks[0]->getName(), "First Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[1]->getName(), "Second Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[2]->getName(), "Third Original");
                juce::UnitTest::expectEquals<juce::String> (tracks[3]->getName(), "First");
                juce::UnitTest::expectEquals<juce::String> (tracks[4]->getName(), "Second");
                juce::UnitTest::expectEquals<juce::String> (tracks[5]->getName(), "Third");
            }
        }

        beginTest ("Tracks inside folder copy/paste");
        {
            auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);

            {
                edit->ensureNumberOfAudioTracks (3);
                auto tracks = getAudioTracks(*edit);
                juce::UnitTest::expectEquals (tracks.size(), 3);
                tracks[0]->setName ("First");
                tracks[1]->setName ("Second");
                tracks[2]->setName ("Third");
            }

            auto ft = edit->insertNewFolderTrack (TrackInsertPoint ({}), nullptr, false);

            {
                auto tracks = getAudioTracks(*edit);
                edit->moveTrack (tracks[2], TrackInsertPoint (ft.get(), nullptr));
                edit->moveTrack (tracks[1], TrackInsertPoint (ft.get(), nullptr));
                edit->moveTrack (tracks[0], TrackInsertPoint (ft.get(), nullptr));
            }

            {
                auto subTracks = ft->getInputTracks();
                juce::UnitTest::expectEquals (subTracks.size(), 3);
                juce::UnitTest::expectEquals<juce::String> (subTracks[0]->getName(), "First");
                juce::UnitTest::expectEquals<juce::String> (subTracks[1]->getName(), "Second");
                juce::UnitTest::expectEquals<juce::String> (subTracks[2]->getName(), "Third");
            }

            {
                Clipboard::Tracks clipboardTracks;

                for (auto at : ft->getInputTracks())
                    clipboardTracks.tracks.push_back (at->state.createCopy());

                // Rename existing tracks for testing later
                auto subTracks = ft->getInputTracks();
                subTracks[0]->setName ("First Original");
                subTracks[1]->setName ("Second Original");
                subTracks[2]->setName ("Third Original");

                EditInsertPoint insertPoint (*edit);
                Clipboard::ContentType::EditPastingOptions opts (*edit, insertPoint);
                opts.startTrack = subTracks[2];
                expect (clipboardTracks.pasteIntoEdit (opts));
            }

            {
                auto subTracks = ft->getInputTracks();
                juce::UnitTest::expectEquals (subTracks.size(), 6);
                juce::UnitTest::expectEquals<juce::String> (subTracks[0]->getName(), "First Original");
                juce::UnitTest::expectEquals<juce::String> (subTracks[1]->getName(), "Second Original");
                juce::UnitTest::expectEquals<juce::String> (subTracks[2]->getName(), "Third Original");
                juce::UnitTest::expectEquals<juce::String> (subTracks[3]->getName(), "First");
                juce::UnitTest::expectEquals<juce::String> (subTracks[4]->getName(), "Second");
                juce::UnitTest::expectEquals<juce::String> (subTracks[5]->getName(), "Third");
            }
        }
    }
};

static ClipboardTempoTests clipboardTempoTests;

#endif

//==============================================================================
//==============================================================================
Clipboard::AutomationPoints::AutomationPoints (AutomatableParameter& param, const AutomationCurve& curve, TimeRange range)
    : AutomationPoints (curve, param.getValueRange(), range, param.getCurrentBaseValue())
{
}

Clipboard::AutomationPoints::AutomationPoints (const AutomationCurve& curve, juce::Range<float> limits, EditTimeRange range, float defaultValue)
    : valueRange (limits)
{
    bool pointAtStart = false;
    bool pointAtEnd = false;
    auto& ts = getTempoSequence (curve.edit);

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        auto p = curve.getPoint (i);
        auto pos = p.time;

        if (equals (pos, range.getStart(), ts)) pointAtStart = true;
        if (equals (pos, range.getEnd(), ts))   pointAtEnd = true;

        if (containsInclusive (range, pos, ts))
        {
            p.time = minus (pos, toDuration (range.getStart()), ts);
            points.push_back (p);
        }
    }

    if (! pointAtStart)
        points.insert (points.begin(), AutomationCurve::AutomationPoint (0_tp, curve.getValueAt (range.getStart(), defaultValue), 0));

    if (! pointAtEnd)
        points.push_back (AutomationCurve::AutomationPoint (toPosition (range.getLength()), curve.getValueAt (range.getEnd(), defaultValue), 0));
}

Clipboard::AutomationPoints::~AutomationPoints() {}

bool Clipboard::AutomationPoints::pasteIntoEdit (const EditPastingOptions&) const
{
    return false;
}

bool Clipboard::AutomationPoints::pasteAutomationCurve (AutomationCurve& targetCurve, juce::Range<float> targetValueRange, float targetDefaultValue,
                                                        std::optional<EditTimeRange> targetRange) const
{
    return pastePointsToCurve (points, valueRange,
                               targetCurve, targetValueRange, targetDefaultValue,
                               targetRange);
}

bool Clipboard::AutomationPoints::pasteAutomationCurve (AutomatableParameter& param, AutomationCurve& targetCurve, TimeRange targetRange) const
{
    return pastePointsToCurve (points, valueRange, param, targetCurve, targetRange);
}

//==============================================================================
//==============================================================================
Clipboard::MIDIEvents::MIDIEvents() {}
Clipboard::MIDIEvents::~MIDIEvents() {}

std::pair<juce::Array<MidiNote*>, juce::Array<MidiControllerEvent*>> Clipboard::MIDIEvents::pasteIntoClip (MidiClip& clip,
                                                                                                           const juce::Array<MidiNote*>& selectedNotes,
                                                                                                           const juce::Array<MidiControllerEvent*>& selectedEvents,
                                                                                                           TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat,
                                                                                                           int destController) const
{
    auto notesAdded         = pasteNotesIntoClip (clip, selectedNotes, cursorPosition, snapBeat);
    auto controllersAdded   = pasteControllersIntoClip (clip, selectedNotes, selectedEvents, cursorPosition, snapBeat, destController);

    return { notesAdded, controllersAdded };
}

juce::Array<MidiNote*> Clipboard::MIDIEvents::pasteNotesIntoClip (MidiClip& clip, const juce::Array<MidiNote*>& selectedNotes,
                                                                  TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat) const
{
    if (notes.empty())
        return {};

    juce::Array<MidiNote> midiNotes;

    for (auto& n : notes)
        midiNotes.add (MidiNote (n));

    auto beatRange = midiNotes.getReference (0).getRangeBeats();

    for (auto& n : midiNotes)
        beatRange = beatRange.getUnionWith (n.getRangeBeats());

    BeatPosition insertPos;

    if (clip.isLooping())
        insertPos = clip.getContentBeatAtTime (cursorPosition) + toDuration (clip.getLoopStartBeats());
    else
        insertPos = clip.getContentBeatAtTime (cursorPosition);

    if (! selectedNotes.isEmpty())
    {
        BeatPosition endOfSelection;

        for (auto n : selectedNotes)
            endOfSelection = std::max (endOfSelection, n->getEndBeat());

        insertPos = endOfSelection;
    }

    if (clip.isLooping())
    {
        const auto offsetBeats = clip.getOffsetInBeats() + toDuration (clip.getLoopStartBeats());

        if ((insertPos - offsetBeats) < BeatPosition() || insertPos - offsetBeats >= toPosition (clip.getLoopLengthBeats() - 0.001_bd))
            return {};
    }
    else
    {
        const auto offsetBeats = clip.getOffsetInBeats();

        if ((insertPos - offsetBeats) < BeatPosition() || insertPos - offsetBeats >= toPosition (clip.getLengthInBeats() - 0.001_bd))
            return {};
    }

    auto deltaBeats = insertPos - beatRange.getStart();

    if (snapBeat != nullptr)
        deltaBeats = toDuration (snapBeat (toPosition (deltaBeats)));

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

juce::Array<MidiControllerEvent*> Clipboard::MIDIEvents::pasteControllersIntoClip (MidiClip& clip,
                                                                                   const juce::Array<MidiNote*>& selectedNotes,
                                                                                   const juce::Array<MidiControllerEvent*>& selectedEvents,
                                                                                   TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat,
                                                                                   int destController) const
{
    if (controllers.empty())
        return {};

    juce::Array<MidiControllerEvent> midiEvents;

    for (auto& e : controllers)
        midiEvents.add (MidiControllerEvent (e));

    if (notes.size() > 0)
        destController = -1;

    juce::Array<int> controllerTypes;
    for (auto& e : midiEvents)
        controllerTypes.addIfNotAlreadyThere (e.getType());

    if (controllerTypes.size() > 1)
        destController = -1;

    if (destController != -1)
    {
        for (auto& e : midiEvents)
            e.setType (destController, nullptr);

        controllerTypes.clear();
        controllerTypes.add (destController);
    }

    auto beatRange = BeatRange (midiEvents.getReference (0).getBeatPosition(), BeatDuration());

    for (auto& e : midiEvents)
        beatRange = beatRange.getUnionWith (BeatRange (e.getBeatPosition(), BeatDuration()));

    BeatPosition insertPos;

    if (clip.isLooping())
        insertPos = clip.getContentBeatAtTime (cursorPosition) + toDuration (clip.getLoopStartBeats());
    else
        insertPos = clip.getContentBeatAtTime (cursorPosition);

    if (! selectedNotes.isEmpty())
    {
        BeatPosition endOfSelection;

        for (auto n : selectedNotes)
            endOfSelection = std::max (endOfSelection, n->getEndBeat());

        insertPos = endOfSelection;
    }
    else if (! selectedEvents.isEmpty())
    {
        BeatPosition endOfSelection;

        for (auto e : selectedEvents)
            if (controllerTypes.contains (e->getType()))
                endOfSelection = std::max (endOfSelection, e->getBeatPosition());

        insertPos = endOfSelection + BeatDuration::fromBeats (1.0);
    }

    if (clip.isLooping())
    {
        auto offsetBeats = toDuration (clip.getLoopStartBeats() + clip.getOffsetInBeats());

        if (insertPos - offsetBeats < 0_bp || insertPos - offsetBeats >= toPosition (clip.getLoopLengthBeats() - 0.001_bd))
            return {};
    }
    else
    {
        auto offsetBeats = clip.getOffsetInBeats();

        if (insertPos - offsetBeats < 0_bp || insertPos - offsetBeats >= toPosition (clip.getLengthInBeats() - 0.001_bd))
            return {};
    }

    auto deltaBeats = insertPos - beatRange.getStart();

    if (snapBeat != nullptr)
        deltaBeats = toDuration (snapBeat (toPosition (deltaBeats)));

    auto& sequence = clip.getSequence();
    auto um = &clip.edit.getUndoManager();
    juce::Array<MidiControllerEvent*> eventsAdded;

    std::vector<juce::ValueTree> itemsToRemove;

    for (auto evt : sequence.getControllerEvents())
        if (controllerTypes.contains (evt->getType()) && evt->getBeatPosition() >= beatRange.getStart() + deltaBeats && evt->getBeatPosition() <= beatRange.getEnd() + deltaBeats)
            itemsToRemove.push_back (evt->state);

    for (auto& v : itemsToRemove)
        sequence.state.removeChild (v, um);

    for (auto& e : midiEvents)
    {
        e.setBeatPosition (e.getBeatPosition() + deltaBeats, um);

        if (auto evt = sequence.addControllerEvent (e, um))
            eventsAdded.add (evt);
    }

    return eventsAdded;
}

bool Clipboard::MIDIEvents::pasteIntoEdit (const EditPastingOptions&) const
{
    return false;
}

//==============================================================================
namespace
{
    static TimePosition snapTimeToNearestBeat (Edit& e, TimePosition t)
    {
        return TimecodeSnapType::get1BeatSnapType().roundTimeNearest (t, e.tempoSequence);
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

    auto startBeat = options.edit.tempoSequence.toBeats (snapTimeToNearestBeat (options.edit, options.startTime));
    auto firstPitchBeat = BeatPosition::fromBeats (static_cast<double> (pitches.front().getProperty (IDs::startBeat)));
    auto offset = startBeat - firstPitchBeat;
    auto um = &options.edit.getUndoManager();

    for (auto& state : pitches)
    {
        auto time = options.edit.tempoSequence.toTime (BeatPosition::fromBeats (static_cast<double> (state.getProperty (IDs::startBeat))) + offset);

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

    auto startBeat = options.edit.tempoSequence.toBeats (snapTimeToNearestBeat (options.edit, options.startTime));
    auto firstTimeSigBeat = BeatPosition::fromBeats (static_cast<double> (timeSigs.front().getProperty (IDs::startBeat)));
    auto offset = startBeat - firstTimeSigBeat;
    auto um = &options.edit.getUndoManager();

    for (auto& state : timeSigs)
    {
        auto time = options.edit.tempoSequence.toTime (BeatPosition::fromBeats (static_cast<double> (state.getProperty (IDs::startBeat))) + offset);

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

        if (auto rackInstance = dynamic_cast<RackInstance*> (item))
        {
            if (auto type = rackInstance->type)
            {
                auto newEntry = std::make_pair (makeSafeRef (type->edit), type->state);

                if (std::find (rackTypes.begin(), rackTypes.end(), newEntry) == rackTypes.end())
                    rackTypes.push_back (newEntry);
            }
        }
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
        if (auto list = selectedPlugin->getOwnerList())
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
    TimePosition startPos;
    Track::Ptr track;
    insertPoint.chooseInsertPoint (track, startPos, false, sm,
                                   [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack() || t.isMasterTrack(); });
    jassert (track != nullptr);

    if (track != nullptr && track->canContainPlugin (newPlugin.get()))
    {
        track->pluginList.insertPlugin (newPlugin, 0, sm);
        return true;
    }

    return false;
}

static EditItemID::IDMap pasteRackTypesInToEdit (Edit& edit, const std::vector<std::pair<SafeSelectable<Edit>, juce::ValueTree>>& editAndTypeStates)
{
    EditItemID::IDMap reassignedIDs;

    for (const auto& editAndTypeState : editAndTypeStates)
    {
        if (editAndTypeState.first == &edit)
            continue;

        auto typeState = editAndTypeState.second;
        auto reassignedRackType = typeState.createCopy();
        EditItemID::remapIDs (reassignedRackType, nullptr, edit, &reassignedIDs);
        edit.getRackList().addRackTypeFrom (reassignedRackType);
    }

    return reassignedIDs;
}

bool Clipboard::Plugins::pasteIntoEdit (const EditPastingOptions& options) const
{
    CRASH_TRACER
    bool anyPasted = false;

    auto rackIDMap = pasteRackTypesInToEdit (options.edit, rackTypes);

    auto pluginsToPaste = plugins;
    std::reverse (pluginsToPaste.begin(), pluginsToPaste.end()); // Reverse the array so they get pasted in the correct order

    for (auto& item : pluginsToPaste)
    {
        auto stateCopy = item.createCopy();
        EditItemID::remapIDs (stateCopy, nullptr, options.edit);

        // Remap RackTypes after the otehr IDs or it will get overwritten
        if (stateCopy[IDs::type].toString() == IDs::rack.toString())
        {
            auto oldRackID = EditItemID::fromProperty (stateCopy, IDs::rackType);
            auto remappedRackID = rackIDMap[oldRackID];

            if (remappedRackID.isValid())
                stateCopy.setProperty (IDs::rackType, remappedRackID, nullptr);
        }

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
                if (auto modifierList = t->getModifierList())
                {
                    auto modList = getModifiersOfType<Modifier> (*modifierList);

                    for (int i = modList.size(); --i >= 0;)
                    {
                        if (modList.getObjectPointer (i) == firstSelectedMod)
                        {
                            for (auto m : modifiers)
                            {
                                EditItemID::remapIDs (m, nullptr, options.edit);
                                modifierList->insertModifier (m, i + 1, options.selectionManager);
                            }

                            return true;
                        }
                    }
                }
            }
        }
    }

    if (options.startTrack != nullptr)
    {
        if (auto modifierList = options.startTrack->getModifierList())
        {
            for (auto m : modifiers)
            {
                EditItemID::remapIDs (m, nullptr, options.edit);
                modifierList->insertModifier (m, -1, options.selectionManager);
            }

            return true;
        }
    }

    return false;
}

}} // namespace tracktion { inline namespace engine
