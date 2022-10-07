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

SelectedMidiEvents::SelectedMidiEvents (MidiClip& m)  : clips ({ &m })
{
}

SelectedMidiEvents::SelectedMidiEvents (juce::Array<MidiClip*> m)  : clips (m)
{
    // SelectedMidiEvents must always have one clip
    jassert (m.size() > 0);

   #if JUCE_DEBUG
    // All the clips must be frmom the same edit
    for (int i = 1; i < m.size(); i++)
        jassert (&m[0]->edit == &m[i]->edit);
   #endif
}

SelectedMidiEvents::~SelectedMidiEvents()
{
    notifyListenersOfDeletion();
}

Edit& SelectedMidiEvents::getEdit()
{
    return clips[0]->edit;
}

//==============================================================================
MidiClip* SelectedMidiEvents::clipForEvent (MidiNote* note) const
{
    for (auto* c : clips)
        if (c->getSequence().getNotes().contains (note))
            return c;

    // SelectedMidiEvents must never have events without the parent clip
    jassertfalse;
    return {};
}

MidiClip* SelectedMidiEvents::clipForEvent (MidiSysexEvent* sysex) const
{
    for (auto* c : clips)
        if (c->getSequence().getSysexEvents().contains (sysex))
            return c;

    // SelectedMidiEvents must never have events without the parent clip
    jassertfalse;
    return {};
}

MidiClip* SelectedMidiEvents::clipForEvent (MidiControllerEvent* controller) const
{
    for (auto* c : clips)
        if (c->getSequence().getControllerEvents().contains (controller))
            return c;

    // SelectedMidiEvents must never have events without the parent clip
    jassertfalse;
    return {};
}

//==============================================================================
void SelectedMidiEvents::addSelectedEvent (MidiNote* note, bool addToCurrent)
{
    if (! addToCurrent)
        selectedNotes.clearQuick();

    // don't allow notes that don't belong to this set of clips to be selected
    if (note != nullptr && clipForEvent (note) == nullptr)
        return;

    if (note != nullptr && ! contains (*note))
        selectedNotes.add (note);

    sendSelectionChangedMessage (nullptr);

    if (selectedNotes.isEmpty())
        deselect();
    else
        SelectionManager::refreshAllPropertyPanelsShowing (*this);

    if (selectedSysexes.size() > 0)
    {
        selectedSysexes.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }

    if (selectedControllers.size() > 0)
    {
        selectedControllers.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }
}

void SelectedMidiEvents::addSelectedEvent (MidiSysexEvent* sysex, bool addToCurrent)
{
    if (! addToCurrent)
        selectedSysexes.clearQuick();

    // don't allow sysexes that don't belong to this set of clips to be selected
    if (sysex != nullptr && clipForEvent (sysex) == nullptr)
        return;

    if (sysex != nullptr && ! contains (*sysex))
        selectedSysexes.add (sysex);

    if (selectedSysexes.isEmpty())
        deselect();

    sendSelectionChangedMessage (nullptr);

    if (selectedNotes.size() > 0)
    {
        selectedNotes.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }
    if (selectedControllers.size() > 0)
    {
        selectedControllers.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }
}

void SelectedMidiEvents::addSelectedEvent (MidiControllerEvent* controller, bool addToCurrent)
{
    if (! addToCurrent)
        selectedControllers.clearQuick();

    // don't allow controllers that don't belong to this set of clips to be selected
    if (controller != nullptr && clipForEvent (controller) == nullptr)
        return;

    if (controller != nullptr && ! contains (*controller))
        selectedControllers.add (controller);

    if (selectedControllers.isEmpty())
        deselect();

    sendSelectionChangedMessage (nullptr);

    if (selectedNotes.size() > 0)
    {
        selectedNotes.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }
    if (selectedSysexes.size() > 0)
    {
        selectedSysexes.clearQuick();
        SelectionManager::refreshAllPropertyPanelsShowing (*this);
    }
}

void SelectedMidiEvents::removeSelectedEvent (MidiNote* note)
{
    if (note != nullptr)
    {
        selectedNotes.removeAllInstancesOf (note);
        sendSelectionChangedMessage (nullptr);
    }

    if (! anythingSelected())
        deselect();
}

void SelectedMidiEvents::removeSelectedEvent (MidiSysexEvent* sysex)
{
    if (sysex != nullptr)
    {
        selectedSysexes.removeAllInstancesOf (sysex);
        sendSelectionChangedMessage (nullptr);
    }

    if (! anythingSelected())
        deselect();
}

void SelectedMidiEvents::removeSelectedEvent (MidiControllerEvent* controller)
{
    if (controller != nullptr)
    {
        selectedControllers.removeAllInstancesOf (controller);
        sendSelectionChangedMessage (nullptr);
    }

    if (! anythingSelected())
        deselect();
}

void SelectedMidiEvents::setSelected (SelectionManager& sm, const juce::Array<MidiNote*>& notes, bool addToSelection, bool allowMixedSelection)
{
    if (! addToSelection)
        selectedNotes.clearQuick();

    for (auto n : notes)
    {
        if (n != nullptr && clipForEvent (n) == nullptr)
            continue;

        if (n != nullptr && ! contains (*n))
            selectedNotes.add (n);
    }

    sendSelectionChangedMessage (&sm);

    if (selectedSysexes.size() > 0 && ! allowMixedSelection)
        selectedSysexes.clearQuick();

    if (selectedControllers.size() > 0 && ! allowMixedSelection)
        selectedControllers.clearQuick();

    if (getNumSelected() > 0)
        sm.selectOnly (this);
    else
        deselect();
}

void SelectedMidiEvents::setSelected (SelectionManager& sm, const juce::Array<MidiSysexEvent*>& events, bool addToSelection, bool allowMixedSelection)
{
    if (! addToSelection)
        selectedSysexes.clearQuick();

    for (auto e : events)
    {
        if (e != nullptr && clipForEvent (e) == nullptr)
            continue;

        if (e != nullptr && ! contains (*e))
            selectedSysexes.add (e);
    }

    sendSelectionChangedMessage (&sm);

    if (selectedNotes.size() > 0 && ! allowMixedSelection)
        selectedNotes.clearQuick();

    if (selectedControllers.size() > 0 && ! allowMixedSelection)
        selectedControllers.clearQuick();

    if (getNumSelected() > 0)
        sm.selectOnly (this);
    else
        deselect();
}

void SelectedMidiEvents::setSelected (SelectionManager& sm, const juce::Array<MidiControllerEvent*>& events, bool addToSelection, bool allowMixedSelection)
{
    if (! addToSelection)
        selectedControllers.clearQuick();

    for (auto e : events)
    {
        if (e != nullptr && clipForEvent (e) == nullptr)
            continue;

        if (e != nullptr && ! contains (*e))
            selectedControllers.add (e);
    }

    sendSelectionChangedMessage (&sm);

    if (selectedNotes.size() > 0 && ! allowMixedSelection)
        selectedNotes.clearQuick();

    if (selectedSysexes.size() > 0 && ! allowMixedSelection)
        selectedSysexes.clearQuick();

    if (getNumSelected() > 0)
        sm.selectOnly (this);
    else
        deselect();
}

bool SelectedMidiEvents::contains (const MidiNote& note) const noexcept
{
    for (auto n : selectedNotes)
        if (n->state == note.state)
            return true;

    return false;
}

bool SelectedMidiEvents::contains (const MidiSysexEvent& event) const noexcept
{
    for (auto s : selectedSysexes)
        if (s->state == event.state)
            return true;

    return false;
}

bool SelectedMidiEvents::contains (const MidiControllerEvent& event) const noexcept
{
    for (auto s : selectedControllers)
        if (s->state == event.state)
            return true;

    return false;
}

bool SelectedMidiEvents::isSelected (const MidiNote* event) const
{
    return event != nullptr
            && SelectionManager::findSelectionManagerContaining (this) != nullptr
            && contains (*event);
}

bool SelectedMidiEvents::isSelected (const MidiSysexEvent* event) const
{
    return event != nullptr
            && SelectionManager::findSelectionManagerContaining (this) != nullptr
            && contains (*event);
}

bool SelectedMidiEvents::isSelected (const MidiControllerEvent* event) const
{
    return event != nullptr
            && SelectionManager::findSelectionManagerContaining (this) != nullptr
            && contains (*event);
}

//==============================================================================
juce::String SelectedMidiEvents::getSelectableDescription()
{
    return TRANS("MIDI Events");
}

void SelectedMidiEvents::selectionStatusChanged (bool isNowSelected)
{
    if (! anythingSelected())
        deselect();

    if (! isNowSelected)
    {
        selectedNotes.clearQuick();
        selectedSysexes.clearQuick();
        selectedControllers.clearQuick();
        sendSelectionChangedMessage (nullptr);
    }
}

void SelectedMidiEvents::moveEvents (TimeDuration deltaStart, TimeDuration deltaLength, int deltaNote)
{
    auto* undoManager = &getEdit().getUndoManager();
    auto notes = selectedNotes; // Use a copy in case any of them get deleted while moving

    juce::Array<MidiClip*> uniqueClips;

    auto startTime = TimePosition::fromSeconds (std::numeric_limits<double>::max());
    auto endTime   = TimePosition::fromSeconds (std::numeric_limits<double>::lowest());

    std::optional<BeatDuration> deltaBeat;

    for (auto note : notes)
    {
        if (auto clip = clipForEvent (note))
        {
            note->setNoteNumber (note->getNoteNumber() + deltaNote, undoManager);

            if (shouldLockControllerToNotes && shouldLockControllerToNotes())
            {
                uniqueClips.addIfNotAlreadyThere (clip);

                startTime = std::min (startTime, note->getEditStartTime (*clip));
                endTime   = std::max (endTime, note->getEditEndTime (*clip));
            }

            auto pos = note->getEditTimeRange (*clip);
            auto newStartBeat = clip->getContentBeatAtTime (pos.getStart() + deltaStart) + toDuration (clip->getLoopStartBeats());
            auto newEndBeat = clip->getContentBeatAtTime (pos.getEnd() + deltaStart + deltaLength) + toDuration (clip->getLoopStartBeats());

            if (! deltaBeat.has_value())
                deltaBeat = newStartBeat - note->getBeatPosition();

            note->setStartAndLength (newStartBeat, newEndBeat - newStartBeat, undoManager);
        }
    }

    for (auto sysexEvent : selectedSysexes)
    {
        if (auto clip = clipForEvent (sysexEvent))
        {
            auto deltaTime = sysexEvent->getEditTime (*clip) + deltaStart;
            sysexEvent->setBeatPosition (clip->getContentBeatAtTime (deltaTime) + toDuration (clip->getLoopStartBeats()), undoManager);
        }
    }

    if (shouldLockControllerToNotes && shouldLockControllerToNotes() && notes.size() > 0)
    {
        moveControllerData (uniqueClips, nullptr, *deltaBeat, startTime, endTime, false);
    }
    else
    {
        for (auto controllerEvent : selectedControllers)
        {
            auto& clip = *clipForEvent (controllerEvent);

            uniqueClips.addIfNotAlreadyThere (&clip);

            startTime = std::min (startTime, controllerEvent->getEditTime (clip));
            endTime   = std::max (endTime, controllerEvent->getEditTime (clip));

            auto start = controllerEvent->getEditTime (clip);
            auto newStartBeat = clip.getContentBeatAtTime (start + deltaStart) + toDuration (clip.getLoopStartBeats());

            if (! deltaBeat.has_value())
                deltaBeat = newStartBeat - controllerEvent->getBeatPosition();
        }

        moveControllerData (uniqueClips, &selectedControllers, *deltaBeat,
                            startTime - TimeDuration::fromSeconds (0.001), endTime + TimeDuration::fromSeconds (0.001),
                            false);
    }
}

void SelectedMidiEvents::setNoteLengths (BeatDuration newLength)
{
    auto um = &getEdit().getUndoManager();

    for (auto note : selectedNotes)
        note->setStartAndLength (note->getStartBeat(), newLength, um);
}

void SelectedMidiEvents::setVelocities (int newVelocity)
{
    auto undoManager = &getEdit().getUndoManager();

    for (auto note : selectedNotes)
        note->setVelocity (newVelocity, undoManager);
}

void SelectedMidiEvents::changeColour (uint8_t newColour)
{
    auto undoManager = &getEdit().getUndoManager();

    for (auto note : selectedNotes)
        note->setColour (newColour, undoManager);
}

void SelectedMidiEvents::nudge (TimecodeSnapType snapType, int leftRight, int upDown)
{
    auto& ed = getEdit();
    auto undoManager = &ed.getUndoManager();

    if (upDown != 0)
        for (auto note : selectedNotes)
            note->setNoteNumber (note->getNoteNumber() + upDown, undoManager);

    if (leftRight != 0)
    {
        if (auto firstSelected = selectedNotes.getFirst())
        {
            if (auto clip = clips[0])
            {
                auto start = firstSelected->getEditStartTime (*clip);

                auto snapped = leftRight < 0
                                ? snapType.roundTimeDown (start - TimeDuration::fromSeconds (0.01), ed.tempoSequence)
                                : snapType.roundTimeUp   (start + TimeDuration::fromSeconds (0.01), ed.tempoSequence);

                auto delta = ed.tempoSequence.toBeats (snapped)
                                - ed.tempoSequence.toBeats (start);

                juce::Array<MidiClip*> uniqueClips;
                auto startTime = TimePosition::fromSeconds (std::numeric_limits<double>::max());
                auto endTime   = TimePosition::fromSeconds (std::numeric_limits<double>::lowest());

                for (auto note : selectedNotes)
                {
                    auto noteClip = clipForEvent (note);
                    uniqueClips.addIfNotAlreadyThere (noteClip);

                    startTime = std::min (startTime, note->getEditStartTime (*noteClip));
                    endTime   = std::max (endTime, note->getEditEndTime (*noteClip));

                    note->setStartAndLength (note->getStartBeat() + delta,
                                             note->getLengthBeats(),
                                             undoManager);
                }

                if (shouldLockControllerToNotes && shouldLockControllerToNotes())
                    moveControllerData (uniqueClips, nullptr, delta, startTime, endTime, false);
            }
        }
    }
}

TimeRange SelectedMidiEvents::getSelectedRange() const
{
    bool doneFirst = false;
    TimeRange time;

    for (auto n : selectedNotes)
    {
        if (auto clip = clipForEvent (n))
        {
            auto noteRange = n->getEditTimeRange (*clip);

            if (! doneFirst)
            {
                time = noteRange;
                doneFirst = true;
                continue;
            }

            time = time.getUnionWith (noteRange);
        }
    }

    return time;
}

void SelectedMidiEvents::sendSelectionChangedMessage (SelectionManager* sm)
{
    if (sm != nullptr)
    {
        ++(sm->selectionChangeCount);
        sm->sendChangeMessage();
    }

    changed();
    sendChangeMessage();
}

void SelectedMidiEvents::setClips (juce::Array<MidiClip*> clips_)
{
    // You should try and avoid changing the list of clips, instead
    // create a new SelectedMidiEvents for the new clips. If you do
    // change the clips, then this fuction will deselect and events
    // that no longer belong to a clip in the clip list.

    clips = clips_;

    for (int i = selectedNotes.size(); --i >= 0;)
        if (clipForEvent (selectedNotes[i]) == nullptr)
            selectedNotes.remove (i);

    for (int i = selectedSysexes.size(); --i >= 0;)
        if (clipForEvent (selectedSysexes[i]) == nullptr)
            selectedSysexes.remove (i);

    for (int i = selectedControllers.size(); --i >= 0;)
        if (clipForEvent (selectedControllers[i]) == nullptr)
            selectedControllers.remove (i);
}

void SelectedMidiEvents::moveControllerData (const juce::Array<MidiClip*>& clips, const juce::Array<MidiControllerEvent*>* onlyTheseEvents,
                                             BeatDuration deltaBeats, TimePosition startTime, TimePosition endTime, bool makeCopy)
{
    for (auto c : clips)
    {
        juce::Array<juce::ValueTree> itemsToRemove;

        auto& seq = c->getSequence();

        juce::Array<MidiControllerEvent*> movedEvents;

        for (auto evt : seq.getControllerEvents())
        {
            if (evt->getEditTime (*c) >= startTime && evt->getEditTime (*c) < endTime)
            {
                if (makeCopy)
                    seq.addControllerEvent (MidiControllerEvent (evt->state.createCopy()), c->getUndoManager());

                auto beat = evt->getBeatPosition() + deltaBeats;
                evt->setBeatPosition (beat, c->getUndoManager());
                movedEvents.add (evt);
            }
        }

        auto& ts = c->edit.tempoSequence;

        const auto startTimeAfter = ts.toTime (ts.toBeats (startTime) + deltaBeats);
        const auto endTimeAfter   = ts.toTime (ts.toBeats (endTime) + deltaBeats);

        for (auto evt : seq.getControllerEvents())
            if (onlyTheseEvents == nullptr || onlyTheseEvents->contains (evt))
                if (! movedEvents.contains (evt) && evt->getEditTime (*c) >= startTimeAfter && evt->getEditTime (*c) <= endTimeAfter)
                    itemsToRemove.add (evt->state);

        for (auto& v : itemsToRemove)
            seq.state.removeChild (v, c->getUndoManager());
    }
}

}} // namespace tracktion { inline namespace engine
