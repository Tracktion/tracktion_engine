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

static std::unique_ptr<MidiList> createLoopRangeDefinesAllRepetitionsSequence (MidiClip& clip, MidiList& sourceSequence)
{
    const auto loopStartBeats = clip.getLoopStartBeats();
    const auto loopLengthBeats = clip.getLoopLengthBeats();

    const auto extraLoops = juce::roundToInt (std::ceil (clip.getOffsetInBeats() / loopLengthBeats));
    const auto loopTimes  = juce::roundToInt (std::ceil (clip.getLengthInBeats() / loopLengthBeats)) + extraLoops;

    const auto& notes = sourceSequence.getNotes();
    const auto& sysex = sourceSequence.getSysexEvents();
    const auto& controllers = sourceSequence.getControllerEvents();

    auto v = MidiList::createMidiList();

    for (int i = 0; i < loopTimes; ++i)
    {
        const auto loopPos = loopLengthBeats * i;
        const auto nextLoopPos = loopLengthBeats * (i + 1);

        // add the midi notes
        for (auto note : notes)
        {
            auto start  = (note->getStartBeat() - loopStartBeats) + loopPos;
            auto length = note->getLengthBeats();

            if (start < loopPos)
            {
                length = length - (loopPos - start);
                start  = start + (loopPos - start);
            }

            if (start + length > nextLoopPos)
                length = length - ((start + length) - nextLoopPos);

            if (start >= loopPos && start < nextLoopPos && length > BeatDuration())
                v.addChild (MidiNote::createNote (*note, toPosition (start), length), -1, nullptr);
        }

        // add the sysex
        for (auto oldEvent : sysex)
        {
            const auto start = (oldEvent->getBeatPosition() - loopStartBeats) + loopPos;

            if (start >= loopPos && start < nextLoopPos)
                v.addChild (MidiSysexEvent::createSysexEvent (*oldEvent, toPosition (start)), -1, nullptr);
        }

        // add the controller
        for (auto oldEvent : controllers)
        {
            const auto start = (oldEvent->getBeatPosition() - loopStartBeats) + loopPos;

            if (start >= loopPos && start < nextLoopPos)
                v.addChild (MidiControllerEvent::createControllerEvent (*oldEvent, toPosition (start)), -1, nullptr);
        }
    }

    std::unique_ptr<MidiList> destSequence (new MidiList (v, nullptr));
    destSequence->setMidiChannel (sourceSequence.getMidiChannel());

    return destSequence;
}

static std::unique_ptr<MidiList> createLoopRangeDefinesSubsequentRepetitionsSequence (MidiClip& clip, MidiList& sourceSequence)
{
    // TODO: what's the point of this function?
    return createLoopRangeDefinesAllRepetitionsSequence (clip, sourceSequence);
}

//==============================================================================
MidiClip::MidiClip (const juce::ValueTree& v, EditItemID id, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, Type::midi)
{
    auto um = getUndoManager();

    quantisation.reset (new QuantisationType (state.getOrCreateChildWithName (IDs::QUANTISATION, um), um));

    if (state.hasProperty (IDs::quantisation))
        quantisation->setType (state.getProperty (IDs::quantisation));

    level->dbGain.referTo (state, IDs::volDb, um, 0.0f);
    level->mute.referTo (state, IDs::mute, um, false);
    sendPatch.referTo (state, IDs::sendProgramChange, um, true);
    sendBankChange.referTo (state, IDs::sendBankChange, um, true);
    mpeMode.referTo (state, IDs::mpeMode, um, false);
    grooveStrength.referTo (state, IDs::grooveStrength, um, 0.1f);

    loopStartBeats.referTo (state, IDs::loopStartBeats, um, BeatPosition());
    loopLengthBeats.referTo (state, IDs::loopLengthBeats, um, BeatDuration());
    originalLength.referTo (state, IDs::originalLength, um, BeatDuration());

    proxyAllowed.referTo (state, IDs::proxyAllowed, um, true);
    currentTake.referTo (state, IDs::currentTake, um);

    auto grooveTree = state.getOrCreateChildWithName (IDs::GROOVE, um);
    grooveTemplate.referTo (grooveTree, IDs::current, um, {});

    const int loopType = edit.engine.getEngineBehaviour().getDefaultLoopedSequenceType();
    loopedSequenceType.referTo (state, IDs::loopedSequenceType, um,
                                static_cast<LoopedSequenceType> (juce::jlimit (0, 1, loopType)));
    jassert (loopType == int (LoopedSequenceType::loopRangeDefinesAllRepetitions)
             || loopType == int (LoopedSequenceType::loopRangeDefinesSubsequentRepetitions));

    auto pgen = state.getChildWithName (IDs::PATTERNGENERATOR);

    if (pgen.isValid())
        patternGenerator.reset (new PatternGenerator (*this, pgen));
}

MidiClip::~MidiClip()
{
    notifyListenersOfDeletion();
}

//==============================================================================
void MidiClip::initialise()
{
    Clip::initialise();

    auto um = getUndoManager();
    auto takesTree = state.getChildWithName (IDs::TAKES);

    if (takesTree.isValid() && takesTree.getNumChildren() > 0)
    {
        channelSequence.clearQuick (true);

        for (int i = 0; i < takesTree.getNumChildren(); ++i)
        {
            auto sequence = takesTree.getChild (i);

            if (! sequence.isValid())
                continue;

            channelSequence.add (new MidiList (sequence, um));
        }

        if (state.getChildWithName (IDs::COMPS).isValid())
            callBlocking ([this] { getCompManager(); });
    }
    else
    {
        auto list = state.getChildWithName (IDs::SEQUENCE);

        if (list.isValid())
            channelSequence.add (new MidiList (list, um));
        else
            state.addChild (MidiList::createMidiList(), -1, um);

        currentTake = 0;
    }

    if (grooveTemplate.get().isNotEmpty())
    {
        auto g = edit.engine.getGrooveTemplateManager().getTemplateByName (grooveTemplate);

        auto grooveTree = state.getChildWithName (IDs::GROOVE);

        if (g == nullptr && grooveTree.getNumChildren() > 0)
        {
            auto grooveXml = grooveTree.getChild (0).createXml();

            GrooveTemplate gt (grooveXml.get());

            if (! gt.isEmpty())
                edit.engine.getGrooveTemplateManager().updateTemplate (-1, gt);
        }
    }

    speedRatio = 1.0; // not used in MIDI clips
}

void MidiClip::rescale (TimePosition pivotTimeInEdit, double factor)
{
    getSequence().rescale (factor, getUndoManager());
    setLoopRangeBeats ({ loopStartBeats * factor, (loopStartBeats + loopLengthBeats) * factor });
    Clip::rescale (pivotTimeInEdit, factor);
}

void MidiClip::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<MidiClip*> (c))
    {
        Clip::cloneFrom (other);

        level->dbGain           .setValue (other->level->dbGain, nullptr);
        level->mute             .setValue (other->level->mute, nullptr);
        currentTake             .setValue (other->currentTake, nullptr);
        mpeMode                 .setValue (other->mpeMode, nullptr);
        originalLength          .setValue (other->originalLength, nullptr);
        sendPatch               .setValue (other->sendPatch, nullptr);
        sendBankChange          .setValue (other->sendBankChange, nullptr);
        grooveTemplate          .setValue (other->grooveTemplate, nullptr);
        grooveStrength          .setValue (other->grooveStrength, nullptr);

        quantisation->setType (other->quantisation->getType (false));
        quantisation->setProportion (other->quantisation->getProportion());

        juce::UndoManager* um = nullptr;

        for (int i = 0; i < other->channelSequence.size(); i++)
        {
            MidiList* cs = i < channelSequence.size() ? channelSequence[i]
                                                      : channelSequence.add (new MidiList());
            cs->copyFrom (*other->channelSequence[i], um);
        }

        while (channelSequence.size() > other->channelSequence.size())
            channelSequence.removeLast();
    }
}

//==============================================================================
juce::String MidiClip::getSelectableDescription()
{
    return TRANS("MIDI Clip") + " - \"" + getName() + "\"";
}

juce::Colour MidiClip::getDefaultColour() const
{
    return juce::Colours::red.withHue (3.0f / 9.0f);
}

bool MidiClip::usesGrooveStrength() const
{
    auto gt = edit.engine.getGrooveTemplateManager().getTemplateByName (getGrooveTemplate());

    if (gt != nullptr && gt->isEmpty())
        gt = nullptr;

    if (gt != nullptr)
        return gt->isParameterized();

    return false;
}

MidiList& MidiClip::getSequence() const noexcept
{
    if (! hasValidSequence())
    {
        jassertfalse; // This shouldn't get hit, let Dave know if it does
        static MidiList dummyList;
        return dummyList;
    }

    jassert (channelSequence.size() > 0);
    return *channelSequence.getUnchecked (currentTake);
}

MidiList& MidiClip::getSequenceLooped()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (! isLooping())
        return getSequence();

    if (cachedLoopedSequence == nullptr)
        cachedLoopedSequence = createSequenceLooped (getSequence());

    return *cachedLoopedSequence;
}

std::unique_ptr<MidiList> MidiClip::createSequenceLooped (MidiList& sourceSequence)
{
    switch (loopedSequenceType)
    {
        case LoopedSequenceType::loopRangeDefinesSubsequentRepetitions:
            return createLoopRangeDefinesSubsequentRepetitionsSequence (*this, sourceSequence);

        case LoopedSequenceType::loopRangeDefinesAllRepetitions:
        default:
            return createLoopRangeDefinesAllRepetitionsSequence (*this, sourceSequence);
    }
}

MidiClip::ScopedEventsList::ScopedEventsList (MidiClip& c, SelectedMidiEvents* e)
    : clip (c)
{
    clip.setSelectedEvents (e);
}

MidiClip::ScopedEventsList::~ScopedEventsList()
{
    clip.setSelectedEvents (nullptr);
}

//==============================================================================
void MidiClip::extendStart (TimePosition newStartTime)
{
    auto offsetNeededInBeats = edit.tempoSequence.toBeats (getPosition().getStart())
                                 - edit.tempoSequence.toBeats (newStartTime);

    setStart (newStartTime, false, false);

    if (offsetNeededInBeats > BeatDuration())
        getSequence().moveAllBeatPositions (offsetNeededInBeats, getUndoManager());
}

void MidiClip::trimBeyondEnds (bool beyondStart, bool beyondEnd, juce::UndoManager* um)
{
    if (beyondStart)
    {
        auto& sequence = getSequence();
        auto startBeats = getContentBeatAtTime (getPosition().getStart());
        sequence.trimOutside (startBeats, BeatPosition::fromBeats (Edit::maximumLength), um);
        sequence.moveAllBeatPositions (-toDuration (getContentBeatAtTime (getPosition().getStart())), um);
        setOffset ({});
    }

    if (beyondEnd)
    {
        auto endBeats = getContentBeatAtTime (getPosition().getEnd());
        getSequence().trimOutside ({}, endBeats, um);
    }
}

void MidiClip::legatoNote (MidiNote& note, const juce::Array<MidiNote*>& notesToUse,
                           const BeatPosition maxEndBeat, juce::UndoManager& um)
{
    // this looks rather convoluted but must account for various edge cases in order to behave in a similar way to Live
    auto noteStartBeat = note.getQuantisedStartBeat (*this);
    auto nextNoteStartBeat = noteStartBeat;

    auto nextNote = notesToUse[notesToUse.indexOf (&note) + 1];

    while (nextNote != nullptr)
    {
        nextNoteStartBeat = nextNote->getQuantisedStartBeat (*this);

        if (nextNoteStartBeat != noteStartBeat)
            break;

        nextNote = notesToUse[notesToUse.indexOf (nextNote) + 1];
    }

    auto& sequence = getSequence();

    if (nextNote == nullptr)
    {
        if (auto lastInSequence = sequence.getNotes().getLast())
        {
            if (*lastInSequence == note)
            {
                const auto separation = maxEndBeat - noteStartBeat;
                note.setStartAndLength (note.getStartBeat(), separation, &um);
                return;
            }
        }
    }

    const auto diff = nextNoteStartBeat - noteStartBeat;

    if (diff > BeatDuration())
        note.setStartAndLength (note.getStartBeat(), diff, &um);
}

//==============================================================================
MidiList* MidiClip::getMidiListForState (const juce::ValueTree& v)
{
    for (int i = channelSequence.size(); --i >= 0;)
        if (auto ml = channelSequence.getUnchecked (i))
            if (ml->state == v)
                return ml;

    return {};
}

//==============================================================================
void MidiClip::setQuantisation (const QuantisationType& newType)
{
    if (newType.getType (false) != quantisation->getType (false))
        *quantisation = newType;
}

//==============================================================================
MidiCompManager& MidiClip::getCompManager()
{
    jassert (hasAnyTakes());

    if (midiCompManager == nullptr)
    {
        auto ptr = edit.engine.getCompFactory().getCompManager (*this);
        midiCompManager = dynamic_cast<MidiCompManager*> (ptr.get());
        jassert (midiCompManager != nullptr);
        midiCompManager->initialise();
    }

    return *midiCompManager;
}

void MidiClip::scaleVerticallyToFit()
{
    int maxNote = 0;
    int minNote = 256;

    for (auto n : getSequence().getNotes())
    {
        maxNote = std::max (maxNote, n->getNoteNumber() + 3);
        minNote = std::min (minNote, n->getNoteNumber() - 3);
    }

    if (minNote < maxNote)
    {
        const double newVisProp = (maxNote - minNote) / 128.0;

        if (newVisProp < getAudioTrack()->getMidiVisibleProportion())
            getAudioTrack()->setMidiVerticalPos (newVisProp, 1.0 - (maxNote / 128.0));
    }
}

void MidiClip::addTake (juce::MidiMessageSequence& ms,
                        MidiList::NoteAutomationType automationType)
{
    auto um = getUndoManager();
    auto takesTree = state.getChildWithName (IDs::TAKES);

    if (! takesTree.isValid())
    {
        takesTree = state.getOrCreateChildWithName (IDs::TAKES, um);
        auto seq = state.getChildWithName (IDs::SEQUENCE);

        if (seq.isValid())
        {
            seq.getParent().removeChild (seq, um);
            takesTree.addChild (seq, -1, um);
        }
    }

    auto seq = MidiList::createMidiList();
    takesTree.addChild (seq, -1, um);

    if (auto take = getMidiListForState (seq))
    {
        auto chan = getSequence().getMidiChannel();
        currentTake = channelSequence.size() - 1;
        take->setMidiChannel (chan);

        if (automationType == MidiList::NoteAutomationType::none)
        {
            for (int i = ms.getNumEvents(); --i >= 0;)
                ms.getEventPointer (i)->message.setChannel (chan.getChannelNumber());

            take->importMidiSequence (ms, &edit, getPosition().getStartOfSource(), getUndoManager());
        }
        else if (automationType == MidiList::NoteAutomationType::expression)
        {
            take->importFromEditTimeSequenceWithNoteExpression (ms, &edit, getPosition().getStartOfSource(), getUndoManager());
        }

        changed();
    }
    else
    {
        jassertfalse;
    }
}

void MidiClip::clearTakes()
{
    if (auto current = channelSequence[currentTake])
    {
        auto um = getUndoManager();
        auto take = current->state;
        auto takes = state.getChildWithName (IDs::TAKES);

        for (int i = takes.getNumChildren(); --i >= 0;)
        {
            auto t = takes.getChild (i);
            t.getParent().removeChild (t, um);
        }

        auto exisiting = state.getChildWithName (IDs::SEQUENCE);
        jassert (! exisiting.isValid());

        if (exisiting.isValid())
            state.removeChild (exisiting, um);

        state.addChild (take, -1, um);

        if (auto newList = channelSequence.getFirst())
        {
            jassert (newList->state == take);
            newList->setCompList (false);
            channelSequence.minimiseStorageOverheads();
        }

        currentTake = 0;
        midiCompManager = nullptr;

        state.removeChild (state.getChildWithName (IDs::TAKES), um);
        state.removeChild (state.getChildWithName (IDs::COMPS), um);

        Selectable::changed();
    }
}

int MidiClip::getNumTakes (bool includeComps)
{
    if (includeComps)
        return channelSequence.size();

    return hasAnyTakes() ? getCompManager().getNumTakes() : 0;
}

juce::StringArray MidiClip::getTakeDescriptions() const
{
    juce::StringArray s;
    int numTakes = 0;

    if (midiCompManager != nullptr)
    {
        for (int i = 0; i < channelSequence.size(); ++i)
        {
            if (! midiCompManager->isTakeComp (i))
            {
                s.add (juce::String (i + 1) + ". " + TRANS("Take") + " #" + juce::String (i + 1));
                ++numTakes;
            }
            else
            {
                s.add (juce::String (i + 1) + ". " + TRANS("Comp") + " #" + juce::String (i + 1 - numTakes));
            }
        }
    }
    else
    {
        for (int i = 0; i < channelSequence.size(); ++i)
            s.add ("Take #" + juce::String (i + 1));
    }

    return s;
}

void MidiClip::setCurrentTake (int takeIndex)
{
    if (currentTake != takeIndex && juce::isPositiveAndBelow (takeIndex, channelSequence.size()))
        currentTake = takeIndex;
}

bool MidiClip::isCurrentTakeComp()
{
    if (hasAnyTakes())
        return getCompManager().isCurrentTakeComp();

    return false;
}

void MidiClip::deleteAllUnusedTakesConfirmingWithUser()
{
    if (edit.engine.getUIBehaviour()
            .showOkCancelAlertBox (TRANS("Delete Unused Takes"),
                                   TRANS("This will permanently delete all unused MIDI takes in this clip.")
                                     + "\n\n"
                                     + TRANS("Are you sure you want to do this?"),
                                   TRANS("Delete")))
        clearTakes();
}

Clip::Array MidiClip::unpackTakes (bool toNewTracks)
{
    CRASH_TRACER
    Clip::Array newClips;

    if (Track::Ptr t = getTrack())
    {
        const bool shouldBeShowingTakes = isShowingTakes();

        if (shouldBeShowingTakes)
            Clip::setShowingTakes (false);

        auto clipNode = state.createCopy();

        if (! clipNode.isValid())
            return newClips;

        clipNode.removeChild (clipNode.getChildWithName (IDs::TAKES), nullptr);
        clipNode.removeChild (clipNode.getChildWithName (IDs::COMPS), nullptr);

        int trackIndex = t->getIndexInEditTrackList();
        auto allTracks = getAllTracks (t->edit);

        for (int i = 0; i < channelSequence.size(); ++i)
        {
            auto srcList = channelSequence[i];

            if (srcList == nullptr || getCompManager().isTakeComp (i))
                continue;

            AudioTrack::Ptr targetTrack (dynamic_cast<AudioTrack*> (allTracks[toNewTracks ? ++trackIndex : trackIndex]));

            if (toNewTracks || targetTrack == nullptr)
                targetTrack = t->edit.insertNewAudioTrack (TrackInsertPoint (t->getParentTrack(), t.get()), nullptr);

            if (targetTrack != nullptr)
            {
                if (auto mc = targetTrack->insertMIDIClip (TRANS("Take") + " #" + juce::String (i + 1),
                                                           getEditTimeRange(), nullptr))
                {
                    mc->getSequence().copyFrom (*srcList, nullptr);
                    newClips.add (mc.get());
                }
                else
                {
                    jassertfalse;
                }
            }

            t = targetTrack;
        }

        if (shouldBeShowingTakes)
            Clip::setShowingTakes (true);
    }

    return newClips;
}

//==============================================================================
void MidiClip::mergeInMidiSequence (juce::MidiMessageSequence& ms,
                                    MidiList::NoteAutomationType automationType)
{
    auto& take = getSequence();

    if (automationType == MidiList::NoteAutomationType::none)
    {
        auto chan = take.getMidiChannel();

        for (int i = ms.getNumEvents(); --i >= 0;)
            ms.getEventPointer (i)->message.setChannel (chan.getChannelNumber());

        take.importMidiSequence (ms, &edit, getPosition().getStartOfSource(), getUndoManager());
    }
    else if (automationType == MidiList::NoteAutomationType::expression)
    {
        take.importFromEditTimeSequenceWithNoteExpression (ms, &edit, getPosition().getStartOfSource(), getUndoManager());
    }

    changed();
}

//==============================================================================
bool MidiClip::canGoOnTrack (Track& t)
{
    return t.canContainMIDI();
}

AudioTrack* MidiClip::getAudioTrack() const
{
    return dynamic_cast<AudioTrack*> (getTrack());
}

//==============================================================================
void MidiClip::setNumberOfLoops (int num)
{
    if (! isLooping())
        originalLength = getLengthInBeats();

    auto& ts = edit.tempoSequence;
    auto pos = getPosition();
    auto newStartBeat = BeatPosition::fromBeats (pos.getOffset().inSeconds() * ts.getBeatsPerSecondAt (pos.getStart()));
    setLoopRangeBeats ({ newStartBeat, newStartBeat + originalLength.get() });

    auto endTime = ts.toTime (getStartBeat() + originalLength.get() * num);
    setLength (endTime - pos.getStart(), true);
    setOffset ({});
}

void MidiClip::disableLooping()
{
    if (isLooping())
    {
        auto pos = getPosition();

        auto offsetB = loopStartBeats.get() + getOffsetInBeats();
        auto lengthB = getLoopLengthBeats();

        pos.time = pos.time.withEnd (std::min (getTimeOfRelativeBeat (lengthB), pos.getEnd()));
        pos.offset = getTimeOfRelativeBeat (toDuration (offsetB)) - pos.getStart(); // TODO: is this correct? Needs testing..

        setLoopRange ({});
        setPosition (pos);
    }
}

void MidiClip::setLoopRangeBeats (BeatRange newRangeBeats)
{
    jassert (newRangeBeats.getStart() >= BeatPosition());

    auto newStartBeat  = std::max (BeatPosition(), newRangeBeats.getStart());
    auto newLengthBeat = std::max (BeatDuration(), newRangeBeats.getLength());

    if (loopStartBeats != newStartBeat || loopLengthBeats != newLengthBeat)
    {
        Clip::setSpeedRatio (1.0);

        if (! isLooping())
            originalLength = getLengthInBeats();

        loopStartBeats  = newStartBeat;
        loopLengthBeats = newLengthBeat;
    }
}

void MidiClip::setLoopRange (TimeRange newRange)
{
    jassert (newRange.getStart() >= TimePosition());

    auto& ts = edit.tempoSequence;
    auto pos = getPosition();
    auto newStartBeat = BeatPosition::fromBeats (newRange.getStart().inSeconds() * ts.getBeatsPerSecondAt (pos.getStart()));
    auto newLengthBeats = ts.toBeats (pos.getStart() + newRange.getLength()) - ts.toBeats (pos.getStart());

    setLoopRangeBeats ({ newStartBeat, newStartBeat + newLengthBeats });
}

TimePosition MidiClip::getLoopStart() const
{
    return TimePosition::fromSeconds (loopStartBeats.get().inBeats() / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

TimeDuration MidiClip::getLoopLength() const
{
    return TimeDuration::fromSeconds (loopLengthBeats.get().inBeats() / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

//==============================================================================
void MidiClip::setSendingBankChanges (bool sendBank)
{
    if (sendBank != sendBankChange)
        sendBankChange = ! sendBankChange;
}

LiveClipLevel MidiClip::getLiveClipLevel()
{
    return { level };
}

void MidiClip::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree == state)
    {
        if (id == IDs::mute)
        {
            jassert (track != nullptr); // Should have been set by now..

            if (track != nullptr)
                if (auto parent = track->getParentFolderTrack())
                    parent->setDirtyClips();

            clearCachedLoopSequence();
        }
        else if (id == IDs::volDb || id == IDs::mpeMode
                 || id == IDs::loopStartBeats || id == IDs::loopLengthBeats
                 || id == IDs::loopedSequenceType
                 || id == IDs::grooveStrength)
        {
            clearCachedLoopSequence();
        }
        else if (id == IDs::currentTake)
        {
            currentTake.forceUpdateOfCachedValue();

            for (SelectionManager::Iterator sm; sm.next();)
                if (sm->isSelected (getSelectedEvents()))
                    sm->deselectAll();

            clearCachedLoopSequence();
        }
        else
        {
            if (id == IDs::length || id == IDs::offset)
                clearCachedLoopSequence();

            Clip::valueTreePropertyChanged (tree, id);
        }
    }
    else if (tree.hasType (IDs::NOTE)
             || tree.hasType (IDs::CONTROL)
             || tree.hasType (IDs::SYSEX)
             || tree.hasType (IDs::QUANTISATION)
             || (tree.hasType (IDs::SEQUENCE) && id == IDs::channelNumber)
             || (tree.hasType (IDs::GROOVE) && id == IDs::current))
    {
        clearCachedLoopSequence();
    }
    else
    {
        Clip::valueTreePropertyChanged (tree, id);
    }
}

void MidiClip::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c)
{
    if (p.hasType (IDs::SEQUENCE))
        clearCachedLoopSequence();
    else if ((p == state || p.getParent() == state) && c.hasType (IDs::SEQUENCE))
        channelSequence.add (new MidiList (c, getUndoManager()));

    if (c.hasType (IDs::PATTERNGENERATOR))
        patternGenerator.reset (new PatternGenerator (*this, c));
}

void MidiClip::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int)
{
    if (p.hasType (IDs::SEQUENCE))
    {
        clearCachedLoopSequence();
    }
    else if ((p == state || p.getParent() == state) && c.hasType (IDs::SEQUENCE))
    {
        for (int i = channelSequence.size(); --i >= 0;)
            if (auto ml = channelSequence.getUnchecked (i))
                if (ml->state == c)
                    channelSequence.remove (i);
    }
    else if (p == state && c.hasType (IDs::COMPS))
    {
        midiCompManager = nullptr;
        clearCachedLoopSequence();
    }
    else if (p == state && c.hasType (IDs::PATTERNGENERATOR))
    {
        patternGenerator = nullptr;
    }
}

void MidiClip::clearCachedLoopSequence()
{
    cachedLoopedSequence = nullptr;
    changed();
}

//==============================================================================
PatternGenerator* MidiClip::getPatternGenerator()
{
    if (! state.getChildWithName (IDs::PATTERNGENERATOR).isValid())
        state.addChild (juce::ValueTree (IDs::PATTERNGENERATOR), -1, &edit.getUndoManager());

    jassert (patternGenerator != nullptr);
    return patternGenerator.get();
}

void MidiClip::pitchTempoTrackChanged()
{
    if (patternGenerator != nullptr)
        patternGenerator->refreshPatternIfNeeded();

    // for the midi editor to redraw
    state.sendPropertyChangeMessage (IDs::mute);
}

}} // namespace tracktion { inline namespace engine
