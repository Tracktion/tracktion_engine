/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static MidiList* createLoopRangeDefinesAllRepetitionsSequence (MidiClip& clip, MidiList& sourceSequence)
{
    const double loopStartBeats = clip.getLoopStartBeats();
    const double loopLengthBeats = clip.getLoopLengthBeats();

    const int extraLoops = roundToInt (std::ceil (clip.getOffsetInBeats() / loopLengthBeats));
    const int loopTimes  = roundToInt (std::ceil (clip.getLengthInBeats() / loopLengthBeats)) + extraLoops;

    const auto& notes = sourceSequence.getNotes();
    const auto& sysex = sourceSequence.getSysexEvents();
    const auto& controllers = sourceSequence.getControllerEvents();

    auto v = MidiList::createMidiList();

    for (int i = 0; i < loopTimes; ++i)
    {
        const double loopPos = loopLengthBeats * i;
        const double nextLoopPos = loopLengthBeats * (i + 1);

        // add the midi notes
        for (auto note : notes)
        {
            double start  = (note->getStartBeat() - loopStartBeats) + loopPos;
            double length = note->getLengthBeats();

            if (start < loopPos)
            {
                length -= loopPos - start;
                start  += loopPos - start;
            }

            if (start + length > nextLoopPos)
                length -= (start + length) - nextLoopPos;

            if (start >= loopPos && start < nextLoopPos && length > 0)
                v.addChild (MidiNote::createNote (*note, start, length), -1, nullptr);
        }

        // add the sysex
        for (auto oldEvent : sysex)
        {
            const double start = (oldEvent->getBeatPosition() - loopStartBeats) + loopPos;

            if (start >= loopPos && start < nextLoopPos)
                v.addChild (MidiSysexEvent::createSysexEvent (*oldEvent, start), -1, nullptr);
        }

        // add the controller
        for (auto oldEvent : controllers)
        {
            const double start = (oldEvent->getBeatPosition() - loopStartBeats) + loopPos;

            if (start >= loopPos && start < nextLoopPos)
                v.addChild (MidiControllerEvent::createControllerEvent (*oldEvent, start), -1, nullptr);
        }
    }

    auto destSequence = new MidiList (v, nullptr);
    destSequence->setMidiChannel (sourceSequence.getMidiChannel());

    return destSequence;
}

static MidiList* createLoopRangeDefinesSubsequentRepetitionsSequence (MidiClip& clip, MidiList& sourceSequence)
{
    return createLoopRangeDefinesAllRepetitionsSequence (clip, sourceSequence);
}

//==============================================================================
MidiClip::MidiClip (const ValueTree& v, EditItemID id, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, Type::midi)
{
    auto um = getUndoManager();

    quantisation = new QuantisationType (state.getOrCreateChildWithName (IDs::QUANTISATION, um), um);

    if (state.hasProperty (IDs::quantisation))
        quantisation->setType (state.getProperty (IDs::quantisation));

    volumeDb.referTo (state, IDs::volDb, um, 0.0f);
    mute.referTo (state, IDs::mute, um, false);
    sendPatch.referTo (state, IDs::sendProgramChange, um, true);
    sendBankChange.referTo (state, IDs::sendBankChange, um, true);
    mpeMode.referTo (state, IDs::mpeMode, um, false);

    loopStartBeats.referTo (state, IDs::loopStartBeats, um, 0.0);
    loopLengthBeats.referTo (state, IDs::loopLengthBeats, um, 0.0);
    originalLength.referTo (state, IDs::originalLength, um, 0.0);

    currentTake.referTo (state, IDs::currentTake, um);

    auto grooveTree = state.getOrCreateChildWithName (IDs::GROOVE, um);
    grooveTemplate.referTo (grooveTree, IDs::current, um, {});

    const int loopType = edit.engine.getEngineBehaviour().getDefaultLoopedSequenceType();
    loopedSequenceType.referTo (state, IDs::loopedSequenceType, um, static_cast<LoopedSequenceType> (jlimit (0, 1, loopType)));
    jassert (loopType == int (LoopedSequenceType::loopRangeDefinesAllRepetitions)
             || loopType == int (LoopedSequenceType::loopRangeDefinesSubsequentRepetitions));

    auto pgen = state.getChildWithName (IDs::PATTERNGENERATOR);

    if (pgen.isValid())
        patternGenerator = new PatternGenerator (*this, pgen);
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
            ScopedPointer<XmlElement> grooveXml (grooveTree.getChild (0).createXml());

            GrooveTemplate gt (grooveXml);

            if (! gt.isEmpty())
                edit.engine.getGrooveTemplateManager().updateTemplate (-1, gt);
        }
    }

    speedRatio = 1.0; // not used in MIDI clips
}

void MidiClip::rescale (double pivotTimeInEdit, double factor)
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

        currentTake             .setValue (other->currentTake, nullptr);
        mpeMode                 .setValue (other->mpeMode, nullptr);
        volumeDb                .setValue (other->volumeDb, nullptr);
        originalLength          .setValue (other->originalLength, nullptr);
        sendPatch               .setValue (other->sendPatch, nullptr);
        mute                    .setValue (other->mute, nullptr);
        sendBankChange          .setValue (other->sendBankChange, nullptr);
        grooveTemplate          .setValue (other->grooveTemplate, nullptr);

        quantisation->setType (other->quantisation->getType (false));
        quantisation->setProportion (other->quantisation->getProportion());

        UndoManager* um = nullptr;

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
String MidiClip::getSelectableDescription()
{
    return TRANS("MIDI Clip") + " - \"" + getName() + "\"";
}

Colour MidiClip::getDefaultColour() const
{
    return Colours::red.withHue (3.0f / 9.0f);
}

AudioNode* MidiClip::createAudioNode (const CreateAudioNodeParams& params)
{
    CRASH_TRACER
    MidiMessageSequence sequence;
    getSequenceLooped().exportToPlaybackMidiSequence (sequence, *this, mpeMode);

    const auto* nodeToReplace = getClipIfPresentInNode (params.audioNodeToBeReplaced, *this);

    auto channels = mpeMode ? Range<int> (2, 15)
                            : Range<int>::withStartAndLength (getMidiChannel().getChannelNumber(), 1);

    return new MidiAudioNode (std::move (sequence), channels, getEditTimeRange(),
                              volumeDb, mute, *this, nodeToReplace);
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

MidiList* MidiClip::createSequenceLooped (MidiList& sourceSequence)
{
    switch (loopedSequenceType)
    {
        case LoopedSequenceType::loopRangeDefinesSubsequentRepetitions:
            return createLoopRangeDefinesSubsequentRepetitionsSequence (*this, sourceSequence);

        case LoopedSequenceType::loopRangeDefinesAllRepetitions:
        default:
            return createLoopRangeDefinesAllRepetitionsSequence (*this, sourceSequence);
    };
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
void MidiClip::extendStart (double newStartTime)
{
    auto offsetNeededInBeats = edit.tempoSequence.timeToBeats (getPosition().getStart())
                                 - edit.tempoSequence.timeToBeats (newStartTime);

    setStart (newStartTime, false, false);

    if (offsetNeededInBeats > 0)
        getSequence().moveAllBeatPositions (offsetNeededInBeats, getUndoManager());
}

void MidiClip::trimBeyondEnds (bool beyondStart, bool beyondEnd, UndoManager* um)
{
    if (beyondStart)
    {
        auto& sequence = getSequence();
        auto startBeats = getContentBeatAtTime (getPosition().getStart());
        sequence.trimOutside (startBeats, Edit::maximumLength, um);
        sequence.moveAllBeatPositions (-getContentBeatAtTime (getPosition().getStart()), um);
        setOffset (0.0);
    }

    if (beyondEnd)
    {
        auto endBeats = getContentBeatAtTime (getPosition().getEnd());
        getSequence().trimOutside (0.0, endBeats, um);
    }
}

void MidiClip::legatoNote (MidiNote& note, const juce::Array<MidiNote*>& notesToUse,
                           const double maxEndBeat, UndoManager& um)
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
                const double separation = maxEndBeat - noteStartBeat;
                note.setStartAndLength (note.getStartBeat(), separation, &um);
                return;
            }
        }
    }

    const double diff = nextNoteStartBeat - noteStartBeat;

    if (diff > 0.0)
        note.setStartAndLength (note.getStartBeat(), diff, &um);
}

//==============================================================================
MidiList* MidiClip::getMidiListForState (const ValueTree& v)
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
        maxNote = jmax (maxNote, n->getNoteNumber() + 3);
        minNote = jmin (minNote, n->getNoteNumber() - 3);
    }

    if (minNote < maxNote)
    {
        const double newVisProp = (maxNote - minNote) / 128.0;

        if (newVisProp < getAudioTrack()->getMidiVisibleProportion())
            getAudioTrack()->setMidiVerticalPos (newVisProp, 1.0 - (maxNote / 128.0));
    }
}

void MidiClip::addTake (MidiMessageSequence& ms, MidiList::NoteAutomationType automationType)
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

StringArray MidiClip::getTakeDescriptions() const
{
    StringArray s;
    int numTakes = 0;

    if (midiCompManager != nullptr)
    {
        for (int i = 0; i < channelSequence.size(); ++i)
        {
            if (! midiCompManager->isTakeComp (i))
            {
                s.add (String (i + 1) + ". " + TRANS("Take") + " #" + String (i + 1));
                ++numTakes;
            }
            else
            {
                s.add (String (i + 1) + ". " + TRANS("Comp") + " #" + String (i + 1 - numTakes));
            }
        }
    }
    else
    {
        for (int i = 0; i < channelSequence.size(); ++i)
            s.add ("Take #" + String (i + 1));
    }

    return s;
}

void MidiClip::setCurrentTake (int takeIndex)
{
    if (currentTake != takeIndex && isPositiveAndBelow (takeIndex, channelSequence.size()))
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
                if (auto mc = targetTrack->insertMIDIClip (TRANS("Take") + " #" + String (i + 1),
                                                           getEditTimeRange(), nullptr))
                {
                    mc->getSequence().copyFrom (*srcList, nullptr);
                    newClips.add (mc);
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
void MidiClip::mergeInMidiSequence (MidiMessageSequence& ms, MidiList::NoteAutomationType automationType)
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
    auto newStartBeat = ts.getBeatsPerSecondAt (pos.getStart()) * pos.getOffset();
    setLoopRangeBeats ({ newStartBeat, newStartBeat + originalLength });

    auto endTime = ts.beatsToTime (getStartBeat() + num * originalLength);
    setLength (endTime - pos.getStart(), true);
    setOffset (0.0);
}

void MidiClip::disableLooping()
{
    if (isLooping())
    {
        auto pos = getPosition();

        auto offsetB = getOffsetInBeats() + loopStartBeats;
        auto lengthB = getLoopLengthBeats();

        pos.time.end = jmin (getTimeOfRelativeBeat (lengthB), pos.getEnd());
        pos.offset = getTimeOfRelativeBeat (offsetB) - pos.getStart(); // TODO: is this correct? Needs testing..

        setLoopRange ({});
        setPosition (pos);
    }
}

void MidiClip::setLoopRangeBeats (juce::Range<double> newRangeBeats)
{
    jassert (newRangeBeats.getStart() >= 0);

    auto newStartBeat  = jmax (0.0, newRangeBeats.getStart());
    auto newLengthBeat = jmax (0.0, newRangeBeats.getLength());

    if (loopStartBeats != newStartBeat || loopLengthBeats != newLengthBeat)
    {
        Clip::setSpeedRatio (1.0);

        if (! isLooping())
            originalLength = getLengthInBeats();

        loopStartBeats  = newStartBeat;
        loopLengthBeats = newLengthBeat;
    }
}

void MidiClip::setLoopRange (EditTimeRange newRange)
{
    jassert (newRange.getStart() >= 0.0);

    auto& ts = edit.tempoSequence;
    auto pos = getPosition();
    auto newStartBeat = newRange.getStart() * ts.getBeatsPerSecondAt (pos.getStart());
    auto newLengthBeats = ts.timeToBeats (pos.getStart() + newRange.getLength()) - ts.timeToBeats (pos.getStart());

    setLoopRangeBeats ({ newStartBeat, newStartBeat + newLengthBeats });
}

double MidiClip::getLoopStart() const
{
    return loopStartBeats / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart());
}

double MidiClip::getLoopLength() const
{
    return loopLengthBeats / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart());
}

//==============================================================================
void MidiClip::setSendingBankChanges (bool sendBank)
{
    if (sendBank != sendBankChange)
        sendBankChange = ! sendBankChange;
}

void MidiClip::valueTreePropertyChanged (ValueTree& tree, const Identifier& id)
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
                 || id == IDs::loopedSequenceType)
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

void MidiClip::valueTreeChildAdded (ValueTree& p, ValueTree& c)
{
    if (p.hasType (IDs::SEQUENCE))
        clearCachedLoopSequence();
    else if ((p == state || p.getParent() == state) && c.hasType (IDs::SEQUENCE))
        channelSequence.add (new MidiList (c, getUndoManager()));

    if (c.hasType (IDs::PATTERNGENERATOR))
        patternGenerator = new PatternGenerator (*this, c);
}

void MidiClip::valueTreeChildRemoved (ValueTree& p, ValueTree& c, int)
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
        state.addChild (ValueTree (IDs::PATTERNGENERATOR), -1, &edit.getUndoManager());

    jassert (patternGenerator != nullptr);
    return patternGenerator;
}

void MidiClip::pitchTempoTrackChanged()
{
    if (patternGenerator != nullptr)
        patternGenerator->refreshPatternIfNeeded();

    // for the midi editor to redraw
    state.sendPropertyChangeMessage (IDs::mute);
}
