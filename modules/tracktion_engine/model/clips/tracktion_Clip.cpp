/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


//==============================================================================
Clip::Clip (const ValueTree& v, ClipTrack& targetTrack, EditItemID id, Type t)
    : TrackItem (targetTrack.edit, id, t),
      state (v), track (&targetTrack),
      sourceFileReference (edit, state, IDs::source)
{
    jassert (isClipState (state));
    edit.clipCache.addItem (*this);

    auto um = getUndoManager();
    clipName.referTo (state, IDs::name, um);
    clipStart.referTo (state, IDs::start, um);
    length.referTo (state, IDs::length, um);
    offset.referTo (state, IDs::offset, um);
    colour.referTo (state, IDs::colour, um);
    speedRatio.referTo (state, IDs::speed, um, 1.0);
    syncType.referTo (state, IDs::sync, um);
    showingTakes.referTo (state, IDs::showingTakes, nullptr, false);
    groupID.referTo (state, IDs::groupID, um);
    linkID.referTo (state, IDs::linkID, um);

    if (! (length >= 0 && length < 1.0e10)) // reverse logic to check for NANs
    {
        jassertfalse;
        length = 0;
    }

    state.addListener (this);

    updateLinkedClipsCaller.setFunction ([this] { updateLinkedClips(); });
}

Clip::~Clip()
{
    edit.clipCache.removeItem (*this);
    state.removeListener (this);
}

void Clip::initialise()
{
    jassert (track != nullptr);

    const ScopedValueSetter<bool> initialiser (isInitialised, false, true);

    if (colour.isUsingDefault())
        colour = getDefaultColour();

    if (sourceFileReference.isUsingProjectReference())
        sourceMediaChanged();

    Selectable::changed(); // call superclass so not to mark the edit as altered

    if (track != nullptr)
    {
        if (auto f = track->getParentFolderTrack())
            f->setDirtyClips();

        track->setFrozen (false, Track::groupFreeze);
    }

    cancelAnyPendingUpdates();
}

UndoManager* Clip::getUndoManager() const
{
    return &edit.getUndoManager();
}

//==============================================================================
bool Clip::isClipState (const ValueTree& v)
{
    return v.hasType (IDs::CLIP) || v.hasType (IDs::AUDIOCLIP) || v.hasType (IDs::MIDICLIP) || v.hasType (IDs::MARKERCLIP)
            || v.hasType (IDs::STEPCLIP) || v.hasType (IDs::CHORDCLIP) || v.hasType (IDs::EDITCLIP);
}

bool Clip::isClipState (const Identifier& i)
{
    return i == IDs::CLIP || i == IDs::AUDIOCLIP || i == IDs::MIDICLIP || i == IDs::MARKERCLIP
            || i == IDs::STEPCLIP || i == IDs::CHORDCLIP || i == IDs::EDITCLIP;
}

//==============================================================================
static Clip::Ptr createNewEditClip (const ValueTree& v, EditItemID newClipID, ClipTrack& targetTrack)
{
    ProjectItemID sourceItemID (v.getProperty (IDs::source).toString());
    auto sourceItem = ProjectManager::getInstance()->getProjectItem (sourceItemID);
    String warning;

    if (sourceItem != nullptr && sourceItem->getLength() > 0.0)
    {
        if (auto snapshot = EditSnapshot::getEditSnapshot (sourceItemID))
        {
            // check for recursion
            auto referencedEdits = snapshot->getNestedEditObjects();
            referencedEdits.remove (0); // 0 will be the source so we need to remove it

            if (referencedEdits.contains (snapshot))
                warning = NEEDS_TRANS("You can't add an Edit which contains this Edit as a clip to itself");
        }
        else
        {
            warning = NEEDS_TRANS("Unable to load Edit from source file");
        }
    }

    // If sourceItemID is invalid it means we're creating an empty EditClip
    if (warning.isEmpty() || ! sourceItemID.isValid())
        return new EditClip (v, newClipID, targetTrack, sourceItemID);

    targetTrack.edit.engine.getUIBehaviour().showWarningAlert (TRANS("Can't Import Edit"), TRANS(warning));
    return {};
}

static Clip::Ptr createNewClipObject (const ValueTree& v, EditItemID newClipID, ClipTrack& targetTrack)
{
    auto type = v.getType();

    // TODO: remove this legacy-style CLIP tag + type handling when definitely not needed
    if (type == IDs::CLIP)
    {
        jassertfalse;
        type = TrackItem::clipTypeToXMLType (TrackItem::stringToType (v.getProperty (IDs::type).toString()));
    }

    if (type == IDs::AUDIOCLIP)     return new WaveAudioClip (v, newClipID, targetTrack);
    if (type == IDs::MIDICLIP)      return new MidiClip (v, newClipID, targetTrack);
    if (type == IDs::MARKERCLIP)    return new MarkerClip (v, newClipID, targetTrack);
    if (type == IDs::STEPCLIP)      return new StepClip (v, newClipID, targetTrack);
    if (type == IDs::CHORDCLIP)     return new ChordClip (v, newClipID, targetTrack);
    if (type == IDs::EDITCLIP)      return createNewEditClip (v, newClipID, targetTrack);

    jassertfalse;
    return {};
}

Clip::Ptr Clip::createClipForState (const ValueTree& v, ClipTrack& targetTrack)
{
    jassert (Clip::isClipState (v));
    jassert (TrackList::isTrack (v.getParent()));

    auto& edit = targetTrack.edit;
    auto newClipID = EditItemID::readOrCreateNewID (edit, v);

    Clip::Ptr c = edit.clipCache.findItem (newClipID);
    jassert (c == nullptr || &c->edit == &edit);

    if (c == nullptr)
    {
        c = createNewClipObject (v, newClipID, targetTrack);

        if (c != nullptr)
        {
            c->initialise();
            c->Selectable::changed(); // call superclass so not to mark the edit as altered
            c->cancelAnyPendingUpdates();
        }
    }

    return c;
}

void Clip::flushStateToValueTree()
{
    for (auto p : getAllPlugins())
        p->flushPluginStateToValueTree();
}

//==============================================================================
void Clip::setName (const String& newName)
{
    if (clipName != newName)
    {
        clipName = newName;

        if (edit.engine.getPropertyStorage().getProperty (SettingID::renameClipRenamesSource))
        {
            auto wc = dynamic_cast<WaveAudioClip*> (this);

            if (wc != nullptr && wc->hasAnyTakes())
            {
                const auto& takes = wc->getTakes();

                for (int i = 0; i < takes.size(); ++i)
                {
                    if (auto source = ProjectManager::getInstance()->getProjectItem (takes.getReference (i)))
                    {
                        if (i == 0)
                            source->setName (newName, ProjectItem::SetNameMode::doDefault);
                        else
                            source->setName (newName + " #" + String (i + 1), ProjectItem::SetNameMode::doDefault);
                    }
                }
            }
            else
            {
                if (auto sourceItem = sourceFileReference.getSourceProjectItem())
                    sourceItem->setName (newName, ProjectItem::SetNameMode::doDefault);
            }
        }
    }
}

//==============================================================================
void Clip::setTrack (ClipTrack* t)
{
    if (track != t)
    {
        if (track != nullptr)
            if (auto f = track->getParentFolderTrack())
                f->setDirtyClips();

        track = t;

        if (track != nullptr)
            if (auto f = track->getParentFolderTrack())
                f->setDirtyClips();
    }
}

Track* Clip::getTrack() const
{
    return track;
}

//==============================================================================
void Clip::changed()
{
    Selectable::changed();

    if (track != nullptr)
        track->setFrozen (false, Track::groupFreeze);

    if (! cloneInProgress && isLinked() && ! edit.isLoading())
        updateLinkedClipsCaller.triggerAsyncUpdate();
}

//==============================================================================
void Clip::sourceMediaChanged()
{
    setCurrentSourceFile (sourceFileReference.getFile());
}

//==============================================================================
void Clip::setPosition (ClipPosition newPosition)
{
    newPosition.time.start = jlimit (0.0, Edit::maximumLength, newPosition.time.start);
    newPosition.time.end   = jlimit (newPosition.time.start, Edit::maximumLength, newPosition.time.end);
    newPosition.offset = jmax (0.0, newPosition.offset);

    clipStart = newPosition.time.start;
    length = newPosition.time.getLength();
    offset = newPosition.offset;
}

void Clip::setStart (double newStart, bool preserveSync, bool keepLength)
{
    auto pos = getPosition();
    auto delta = jlimit (0.0, Edit::maximumLength, newStart) - pos.time.start;

    pos.time.start += delta;

    if (keepLength)
        pos.time.end += delta;

    if (preserveSync)
        pos.offset = jmax (0.0, pos.offset + delta);

    setPosition (pos);
}

void Clip::setLength (double newLength, bool preserveSync)
{
    setEnd (getPosition().time.start + newLength, preserveSync);
}

void Clip::setEnd (double newEnd, bool preserveSync)
{
    auto pos = getPosition();
    auto delta = jlimit (pos.time.start, Edit::maximumLength, newEnd) - pos.time.end;

    if (! preserveSync)
        pos.offset -= delta;

    pos.time.end += delta;
    setPosition (pos);
}

void Clip::setOffset (double newOffset)
{
    auto pos = getPosition();
    pos.offset = jmax (0.0, newOffset);
    setPosition (pos);
}

juce::Array<double> Clip::getInterestingTimes()
{
    juce::Array<double> times;

    auto pos = getPosition();
    times.add (pos.time.start);

    for (double m : getRescaledMarkPoints())
        times.add (m + pos.time.start);

    times.add (pos.time.end);

    return times;
}

juce::Array<double> Clip::getRescaledMarkPoints() const
{
    juce::Array<double> times;
    auto pos = getPosition();

    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
    {
        times = sourceItem->getMarkedPoints();

        for (double& t : times)
            t /= speedRatio - pos.offset;
    }

    return times;
}

double Clip::getSpottingPoint() const
{
    auto marks = getRescaledMarkPoints();

    if (marks.isEmpty())
        return {};

    auto pos = getPosition();

    return jlimit (0.0, pos.time.getLength(),
                   marks.getFirst() - pos.offset);
}

void Clip::trimAwayOverlap (EditTimeRange r)
{
    auto pos = getPosition();

    if (r.end > pos.time.start)
    {
        if (r.end < pos.time.end)
            setStart (r.end, true, false);
        else if (pos.time.start < r.start)
            setEnd (r.start, true);
    }
}

void Clip::removeFromParentTrack()
{
    if (state.getParent().isValid())
        state.getParent().removeChild (state, getUndoManager());
}

bool Clip::moveToTrack (Track& newTrack)
{
    if (auto to = dynamic_cast<ClipTrack*> (&newTrack))
    {
        if (canGoOnTrack (*to))
        {
            if (track != to)
            {
                if (! to->isFrozen (Track::anyFreeze))
                {
                    Clip::Ptr refHolder (this);
                    removeFromParentTrack();
                    to->addClip (this);
                }
            }

            return true;
        }
    }

    return false;
}

//==============================================================================
void Clip::setSpeedRatio (double r)
{
    r = jlimit (ClipConstants::speedRatioMin, ClipConstants::speedRatioMax, r);

    if (speedRatio != r)
        speedRatio = r;
}

void Clip::rescale (double pivotTimeInEdit, double factor)
{
    auto pos = getPosition();

    // limit the start to 0
    if (pivotTimeInEdit + (factor * (pos.getStart() - pivotTimeInEdit)) < 0)
        factor = -pivotTimeInEdit / (pos.getStart() - pivotTimeInEdit);

    if (factor > 0.01 && factor != 1.0)
    {
        bool canRescale = false;

        if (auto acb = dynamic_cast<AudioClipBase*> (this))
            canRescale = ! acb->getAutoTempo();
        else if (type == Type::midi || type == Type::step)
            canRescale = true;

        if (canRescale)
        {
            setPosition (pos.rescaled (pivotTimeInEdit, factor));
            setSpeedRatio (speedRatio / factor);
        }
    }
}

//==============================================================================
CollectionClip* Clip::getGroupClip() const
{
    if (auto ct = getClipTrack())
        return ct->getCollectionClip (const_cast<Clip*> (this));

    return {};
}

//==============================================================================
void Clip::setCurrentSourceFile (const File& f)
{
    if (currentSourceFile != f)
    {
        currentSourceFile = f;
        changed();
    }
}

//==============================================================================
void Clip::valueTreePropertyChanged (ValueTree& tree, const Identifier& id)
{
    if (tree == state)
    {
        if (id == IDs::start || id == IDs::length || id == IDs::offset)
        {
            if (track != nullptr)
            {
                if (auto f = track->getParentFolderTrack())
                    f->setDirtyClips();

                changed();
            }
        }
        else if (id == IDs::source)
        {
            sourceFileReference.source.forceUpdateOfCachedValue();
            sourceMediaChanged();
        }
        else if (id == IDs::colour || id == IDs::speed
                 || id == IDs::sync || id == IDs::linkID)
        {
            changed();
        }
        else if (id == IDs::name)
        {
            auto weakRef = getWeakRef();
            clipName.forceUpdateOfCachedValue();
            changed();

            MessageManager::callAsync ([weakRef]
            {
                if (weakRef != nullptr)
                    SelectionManager::refreshAllPropertyPanels();
            });
        }
    }
}

void Clip::valueTreeParentChanged (ValueTree& v)
{
    if (v == state)
        updateParentTrack();
}

void Clip::updateParentTrack()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto parent = state.getParent();

    if (TrackList::isTrack (parent))
        setTrack (dynamic_cast<ClipTrack*> (findTrackForID (edit, EditItemID::fromID (parent))));
    else
        setTrack ({});
}

//==============================================================================
void Clip::cloneFrom (Clip* c)
{
    clipName                    .setValue (c->clipName, nullptr);
    speedRatio                  .setValue (c->speedRatio, nullptr);
    sourceFileReference.source  .setValue (c->sourceFileReference.source, nullptr);
    colour                      .setValue (c->colour, nullptr);
    syncType                    .setValue (c->syncType, nullptr);
    showingTakes                .setValue (c->showingTakes, nullptr);
}

void Clip::updateLinkedClips()
{
    CRASH_TRACER

    for (auto c : edit.findClipsInLinkGroup (linkID))
    {
        jassert (Selectable::isSelectableValid (c));
        jassert (typeid (*c) == typeid (*this));

        if (c != this && typeid (*c) == typeid (*this))
        {
            const ScopedValueSetter<bool> setter (c->cloneInProgress, true);
            c->cloneFrom (this);
        }
    }
}

ClipPosition Clip::getPosition() const
{
    auto s = clipStart.get();
    return { { s, s + length.get() }, offset.get() };
}

double Clip::getContentBeatAtTime (double t) const
{
    return edit.tempoSequence.timeToBeats (t) - getContentStartBeat();
}

double Clip::getTimeOfContentBeat (double beat) const
{
    return edit.tempoSequence.beatsToTime (beat + getContentStartBeat());
}

TrackItem* Clip::getGroupParent() const
{
    return getGroupClip();
}

Colour Clip::getColour() const
{
    return colour.get();
}
