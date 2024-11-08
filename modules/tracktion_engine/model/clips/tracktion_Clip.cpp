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

//==============================================================================
Clip::Clip (const juce::ValueTree& v, ClipOwner& targetParent, EditItemID id, Type t)
    : TrackItem (targetParent.getClipOwnerEdit(), id, t),
      state (v), parent (&targetParent),
      sourceFileReference (edit, state, IDs::source)
{
    jassert (isClipState (state));
    jassert (getParent() == &targetParent);

    auto um = getUndoManager();
    clipName.referTo (state, IDs::name, um);
    clipStart.referTo (state, IDs::start, um);
    length.referTo (state, IDs::length, um);
    offset.referTo (state, IDs::offset, um);
    colour.referTo (state, IDs::colour, um);
    disabled.referTo (state, IDs::disabled, um);
    speedRatio.referTo (state, IDs::speed, um, 1.0);
    syncType.referTo (state, IDs::sync, um);
    showingTakes.referTo (state, IDs::showingTakes, nullptr, false);
    groupID.referTo (state, IDs::groupID, um);
    linkID.referTo (state, IDs::linkID, um);
    followActionDurationType.referTo (state, IDs::followActionDurationType, um);
    followActionBeats.referTo (state, IDs::followActionBeats, um);
    followActionNumLoops.referTo (state, IDs::followActionNumLoops, um, 1.0);

    convertPropertyToType<bool> (state, IDs::disabled);

    if (! (length >= 0_td && length < 1.0e10s)) // reverse logic to check for NANs
    {
        jassertfalse;
        length = 0_td;
    }

    state.addListener (this);

    updateLinkedClipsCaller.setFunction ([this] { updateLinkedClips(); });
    edit.clipCache.addItem (*this);
}

Clip::~Clip()
{
    edit.clipCache.removeItem (*this);
    state.removeListener (this);
}

void Clip::initialise()
{
    jassert (parent != nullptr);

    const juce::ScopedValueSetter<bool> initialiser (isInitialised, false, true);

    if (colour.isUsingDefault())
        colour = getDefaultColour();

    if (sourceFileReference.isUsingProjectReference())
        sourceMediaChanged();

    Selectable::changed(); // call superclass so not to mark the edit as altered

    if (auto track = getTrack())
    {
        if (auto f = track->getParentFolderTrack())
            f->setDirtyClips();

        track->setFrozen (false, Track::groupFreeze);
    }

    cancelAnyPendingUpdates();
}

juce::UndoManager* Clip::getUndoManager() const
{
    return &edit.getUndoManager();
}

//==============================================================================
bool Clip::isClipState (const juce::ValueTree& v)
{
    return isClipState (v.getType());
}

bool Clip::isClipState (const juce::Identifier& i)
{
    return i == IDs::AUDIOCLIP || i == IDs::MIDICLIP || i == IDs::MARKERCLIP
        || i == IDs::STEPCLIP || i == IDs::CHORDCLIP || i == IDs::ARRANGERCLIP
        || i == IDs::EDITCLIP || i == IDs::CONTAINERCLIP;
}

//==============================================================================
static Clip::Ptr createNewEditClip (const juce::ValueTree& v, EditItemID newClipID, ClipOwner& targetParent)
{
    auto& edit = targetParent.getClipOwnerEdit();
    ProjectItemID sourceItemID (v.getProperty (IDs::source).toString());
    auto sourceItem = edit.engine.getProjectManager().getProjectItem (sourceItemID);
    juce::String warning;

    if (sourceItem != nullptr && sourceItem->getLength() > 0.0)
    {
        if (auto snapshot = EditSnapshot::getEditSnapshot (edit.engine, sourceItemID))
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
        return new EditClip (v, newClipID, targetParent, sourceItemID);

    edit.engine.getUIBehaviour().showWarningAlert (TRANS("Can't Import Edit"), TRANS(warning));
    return {};
}

static Clip::Ptr createNewClipObject (const juce::ValueTree& v, EditItemID newClipID, ClipOwner& targetParent)
{
    auto type = v.getType();

    // TODO: remove this legacy-style CLIP tag + type handling when definitely not needed
    if (type.toString() == "CLIP")
    {
        jassertfalse;
        type = TrackItem::clipTypeToXMLType (TrackItem::stringToType (v.getProperty (IDs::type).toString()));
    }

    if (type == IDs::AUDIOCLIP)     return new WaveAudioClip (v, newClipID, targetParent);
    if (type == IDs::MIDICLIP)      return new MidiClip (v, newClipID, targetParent);
    if (type == IDs::MARKERCLIP)    return new MarkerClip (v, newClipID, targetParent);
    if (type == IDs::STEPCLIP)      return new StepClip (v, newClipID, targetParent);
    if (type == IDs::CHORDCLIP)     return new ChordClip (v, newClipID, targetParent);
    if (type == IDs::ARRANGERCLIP)  return new ArrangerClip (v, newClipID, targetParent);
    if (type == IDs::CONTAINERCLIP) return new ContainerClip (v, newClipID, targetParent);
    if (type == IDs::EDITCLIP)      return createNewEditClip (v, newClipID, targetParent);

    jassertfalse;
    return {};
}

Clip::Ptr Clip::createClipForState (const juce::ValueTree& v, ClipOwner& targetParent)
{
    jassert (Clip::isClipState (v));
    jassert (TrackList::isTrack (v.getParent())
             || v.getParent().hasType (IDs::CLIPLIST)
             || v.getParent().hasType (IDs::CLIPSLOT));

    auto& edit = targetParent.getClipOwnerEdit();
    auto newClipID = EditItemID::readOrCreateNewID (edit, v);

    Clip::Ptr c = edit.clipCache.findItem (newClipID);
    jassert (c == nullptr || &c->edit == &edit);

    if (c == nullptr)
    {
        c = createNewClipObject (v, newClipID, targetParent);
        jassert (c->getParent() == &targetParent); // If this is hit it means two clips share the same ID!

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
void Clip::setName (const juce::String& newName)
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
                    if (auto source = edit.engine.getProjectManager().getProjectItem (takes.getReference (i)))
                    {
                        if (i == 0)
                            source->setName (newName, ProjectItem::SetNameMode::doDefault);
                        else
                            source->setName (newName + " #" + juce::String (i + 1), ProjectItem::SetNameMode::doDefault);
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
void Clip::setParent (ClipOwner* newParent)
{
    if (parent == newParent)
        return;

    if (auto track = getTrack())
        if (auto f = track->getParentFolderTrack())
            f->setDirtyClips();

    parent = newParent;

    if (auto track = getTrack())
        if (auto f = track->getParentFolderTrack())
            f->setDirtyClips();
}

ClipOwner* Clip::getParent() const
{
    return parent;
}

ClipTrack* Clip::getClipTrack() const
{
    return dynamic_cast<ClipTrack*> (getTrack());
}

Track* Clip::getTrack() const
{
    if (auto t = dynamic_cast<Track*> (parent))
        return t;

    if (auto cs = getClipSlot())
        return &cs->track;

    return nullptr;
}

ClipSlot* Clip::getClipSlot() const
{
    return dynamic_cast<ClipSlot*> (parent);
}

//==============================================================================
void Clip::changed()
{
    Selectable::changed();

    if (auto track = getTrack())
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
    const auto maxEnd = Edit::getMaximumEditEnd();
    const auto start = juce::jlimit (0_tp, maxEnd, newPosition.time.getStart());
    newPosition.time = { start,
                         juce::jlimit (start, maxEnd, newPosition.time.getEnd()) };
    newPosition.offset = juce::jmax (0_td, newPosition.offset);

    clipStart = newPosition.time.getStart();
    length = newPosition.time.getLength();
    offset = newPosition.offset;
}

void Clip::setStart (TimePosition newStart, bool preserveSync, bool keepLength)
{
    auto pos = getPosition();
    auto delta = juce::jlimit (0_tp, Edit::getMaximumEditEnd(), newStart) - pos.time.getStart();

    if (keepLength)
        pos.time = pos.time + delta;
    else
        pos.time = pos.time.withStart (pos.time.getStart() + delta);

    if (preserveSync)
        pos.offset = juce::jmax (0_td, pos.offset + delta);

    setPosition (pos);
}

void Clip::setLength (TimeDuration newLength, bool preserveSync)
{
    setEnd (getPosition().time.getStart() + newLength, preserveSync);
}

void Clip::setEnd (TimePosition newEnd, bool preserveSync)
{
    auto pos = getPosition();
    auto delta = juce::jlimit (pos.time.getStart(), Edit::getMaximumEditEnd(), newEnd) - pos.time.getEnd();

    if (! preserveSync)
        pos.offset = pos.offset - delta;

    pos.time = pos.time.withEnd (pos.time.getEnd() + delta);
    setPosition (pos);
}

void Clip::setOffset (TimeDuration newOffset)
{
    auto pos = getPosition();
    pos.offset = juce::jmax (0_td, newOffset);
    setPosition (pos);
}

juce::Array<TimePosition> Clip::getInterestingTimes()
{
    juce::Array<TimePosition> times;

    auto pos = getPosition();
    times.add (pos.time.getStart());

    for (auto m : getRescaledMarkPoints())
        times.add (m + toDuration (pos.time.getStart()));

    times.add (pos.time.getEnd());

    return times;
}

juce::Array<TimePosition> Clip::getRescaledMarkPoints() const
{
    juce::Array<TimePosition> times;
    const auto offsetSecs = getPosition().offset.inSeconds();

    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
        for (auto t : sourceItem->getMarkedPoints())
            times.add (TimePosition::fromSeconds (t.inSeconds() / speedRatio - offsetSecs));

    return times;
}

TimePosition Clip::getSpottingPoint() const
{
    auto marks = getRescaledMarkPoints();

    if (marks.isEmpty())
        return {};

    auto pos = getPosition();

    return juce::jlimit (0_tp, toPosition (pos.time.getLength()),
                         marks.getFirst() - pos.offset);
}

void Clip::trimAwayOverlap (TimeRange r)
{
    auto pos = getPosition();

    if (! pos.time.intersects (r))
        return;

    if (r.getEnd() < pos.time.getEnd())
        setStart (r.getEnd(), true, false);
    else if (pos.time.getStart() < r.getStart())
        setEnd (r.getStart(), true);
}

void Clip::removeFromParent()
{
    if (state.getParent().isValid())
        state.getParent().removeChild (state, getUndoManager());
}

bool Clip::moveTo (ClipOwner& newParent)
{
    if (parent == &newParent)
        return false;

    if (! canBeAddedTo (newParent))
        return false;

    if (auto to = dynamic_cast<ClipTrack*> (&newParent))
    {
        if (to->isFrozen (Track::anyFreeze))
            return false;

        Clip::Ptr refHolder (this);
        to->addClip (this);
        return true;
    }
    else if (auto cs = dynamic_cast<ClipSlot*> (&newParent))
    {
        Clip::Ptr refHolder (this);
        cs->setClip (this);
    }

    return false;
}

//==============================================================================
void Clip::setSpeedRatio (double r)
{
    r = juce::jlimit (ClipConstants::speedRatioMin, ClipConstants::speedRatioMax, r);

    if (speedRatio != r)
        speedRatio = r;
}

void Clip::rescale (TimePosition pivotTimeInEdit, double factor)
{
    auto pos = getPosition();

    // limit the start to 0
    if (pivotTimeInEdit + ((pos.getStart() - pivotTimeInEdit) * factor) < TimePosition())
        factor = -(toDuration (pivotTimeInEdit) / (pos.getStart() - pivotTimeInEdit));

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
void Clip::setCurrentSourceFile (const juce::File& f)
{
    if (currentSourceFile != f)
    {
        currentSourceFile = f;
        changed();
    }
}

//==============================================================================
void Clip::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree == state)
    {
        if (id == IDs::start || id == IDs::length || id == IDs::offset)
        {
            if (auto track = getTrack())
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
            clipName.forceUpdateOfCachedValue();

            juce::MessageManager::callAsync ([weakRef = makeSafeRef (*this)]
            {
                if (weakRef != nullptr)
                    SelectionManager::refreshAllPropertyPanels();
            });

            changed();
        }
        else if (id == IDs::followActionDurationType)
        {
            changed();
            propertiesChanged();
        }
        else if (id == IDs::followActionBeats)
        {
            if (! getUndoManager()->isPerformingUndoRedo())
            {
                followActionBeats.forceUpdateOfCachedValue();

                if (followActionBeats.get() > 0_bd)
                    if (auto fa = getFollowActions(); fa && fa->getActions().empty())
                        fa->addAction();
            }
        }
        else if (id == IDs::followActionNumLoops)
        {
            if (! getUndoManager()->isPerformingUndoRedo())
            {
                followActionNumLoops.forceUpdateOfCachedValue();

                if (followActionNumLoops.get() > 0.0)
                    if (auto fa = getFollowActions(); fa && fa->getActions().empty())
                        fa->addAction();
            }
        }
    }
}

void Clip::valueTreeParentChanged (juce::ValueTree& v)
{
    if (v == state)
        updateParent();
}

void Clip::updateParent()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto parentState = state.getParent();

    if (TrackList::isTrack (parentState))
        setParent (dynamic_cast<ClipTrack*> (findTrackForID (edit, EditItemID::fromID (parentState))));
    else if (parentState.hasType (IDs::CLIPLIST) && parentState.getParent().hasType (IDs::CONTAINERCLIP))
        setParent (dynamic_cast<ContainerClip*> (findClipForID (edit, EditItemID::fromID (parentState.getParent()))));
    else if (parentState.hasType (IDs::CLIPSLOT))
        setParent (dynamic_cast<ClipSlot*> (findClipSlotForID (edit, EditItemID::fromID (parentState))));
    else
        setParent ({});
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
            const juce::ScopedValueSetter<bool> setter (c->cloneInProgress, true);
            c->cloneFrom (this);
        }
    }
}

juce::Array<Exportable::ReferencedItem> Clip::getReferencedItems()
{
    return edit.engine.getEngineBehaviour().getReferencedItems (*this);
}

void Clip::reassignReferencedItem (const ReferencedItem& itm, ProjectItemID newID, double newStartTime)
{
    edit.engine.getEngineBehaviour().reassignReferencedItem (*this, itm, newID, newStartTime);
}

ClipPosition Clip::getPosition() const
{
    auto s = clipStart.get();
    return { { s, s + length.get() }, offset.get() };
}

BeatPosition Clip::getContentBeatAtTime (TimePosition t) const
{
    return edit.tempoSequence.toBeats (t) - toDuration (getContentStartBeat());
}

TimePosition Clip::getTimeOfContentBeat (BeatPosition beat) const
{
    return edit.tempoSequence.toTime (beat + toDuration (getContentStartBeat()));
}

TrackItem* Clip::getGroupParent() const
{
    return getGroupClip();
}

void Clip::setGroup (EditItemID newGroupID)
{
    groupID = newGroupID;
    if (groupID == EditItemID())
        state.removeProperty (IDs::groupID, getUndoManager());
}

juce::Colour Clip::getColour() const
{
    return colour.get();
}

//==============================================================================
void Clip::addListener (Listener* l)
{
    if (listeners.isEmpty())
        edit.restartPlayback();

    listeners.add (l);
}

void Clip::removeListener (Listener* l)
{
    listeners.remove (l);

    if (listeners.isEmpty())
        edit.restartPlayback();
}

//==============================================================================
namespace details
{
    Clip::FollowActionDurationType followActionDurationTypeFromString (juce::String s)
    {
        return magic_enum::enum_cast<Clip::FollowActionDurationType> (s.toRawUTF8()).value_or (Clip::FollowActionDurationType::beats);
    }

    juce::String toString (Clip::FollowActionDurationType t)
    {
        return std::string (magic_enum::enum_name (t));
    }
}

}} // namespace tracktion { inline namespace engine

//==============================================================================
//==============================================================================
#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIPS

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    TEST_CASE("Trim away overlap")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);

        auto clip = getAudioTracks (*edit)[0]->insertMIDIClip ({ 4_tp, 8_tp }, nullptr);
        auto positionBeforeTrim = clip->getPosition();

        SUBCASE ("Overlap at start")
        {
            clip->trimAwayOverlap ({ 2_tp, 6_tp });
            CHECK (clip->getPosition().time == TimeRange (6_tp, 8_tp));
        }

        SUBCASE ("Overlap at end")
        {
            clip->trimAwayOverlap ({ 6_tp, 10_tp });
            CHECK (clip->getPosition().time == TimeRange (4_tp, 6_tp));
        }

        SUBCASE ("No overlap at start")
        {
            clip->trimAwayOverlap ({ 2_tp, 4_tp });
            CHECK (clip->getPosition().time == positionBeforeTrim.time);
        }

        SUBCASE ("No overlap at end")
        {
            clip->trimAwayOverlap ({ 10_tp, 12_tp });
            CHECK (clip->getPosition().time == positionBeforeTrim.time);
        }
    }
}

} // namespace tracktion::inline engine

#endif
