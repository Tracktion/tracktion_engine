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

struct ClipOwner::ClipList : public ValueTreeObjectList<Clip>,
                             private juce::AsyncUpdater,
                             private TransportControl::Listener
{
    ClipList (ClipOwner& co, Edit& e, const juce::ValueTree& parentTree)
        : ValueTreeObjectList<Clip> (parentTree),
          edit (e),
          clipOwner (co)
    {
        rebuildObjects();

        editLoadedCallback.reset (new Edit::LoadFinishedCallback<ClipList> (*this, edit));
        edit.getTransport().addListener (this);
    }

    ~ClipList() override
    {
        edit.getTransport().removeListener (this);

        for (auto c : objects)
        {
            c->flushStateToValueTree();
            c->setParent (nullptr);
        }

        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return Clip::isClipState (v);
    }

    Clip* createNewObject (const juce::ValueTree& v) override
    {
        if (auto newClip = Clip::createClipForState (v, clipOwner))
        {
            clipOwner.clipCreated (*newClip);
            newClip->incReferenceCount();

            return newClip.get();
        }

        jassertfalse;
        return {};
    }

    void deleteObject (Clip* c) override
    {
        jassert (c != nullptr);
        if (c == nullptr)
            return;

        c->decReferenceCount();
    }

    void newObjectAdded (Clip* c) override
    {
        objectAddedOrRemoved (c);

        if (c && ! edit.getUndoManager().isPerformingUndoRedo())
            edit.engine.getEngineBehaviour().newClipAdded (*c, recordingIsStopping);
    }

    void objectRemoved (Clip* c) override       { objectAddedOrRemoved (c); }
    void objectOrderChanged() override          { objectAddedOrRemoved (nullptr); }

    void objectAddedOrRemoved (Clip* c)
    {
        if (c == nullptr || c->type != TrackItem::Type::unknown)
        {
            if (c == nullptr)
                clipOwner.clipOrderChanged();
            else
                clipOwner.clipAddedOrRemoved();

            if (! edit.isLoading() && ! edit.getUndoManager().isPerformingUndoRedo())
                triggerAsyncUpdate();
        }
    }

    Edit& edit;
    ClipOwner& clipOwner;
    std::unique_ptr<Edit::LoadFinishedCallback<ClipList>> editLoadedCallback;

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& id) override
    {
        if (Clip::isClipState (v))
        {
            if (id == IDs::start || id == IDs::length)
            {
                if (! edit.getUndoManager().isPerformingUndoRedo())
                    triggerAsyncUpdate();

                clipOwner.clipPositionChanged();
            }
        }
    }

    void handleAsyncUpdate() override
    {
        sortClips (parent, &edit.getUndoManager());
    }

    static void sortClips (juce::ValueTree& state, juce::UndoManager* um)
    {
        struct Sorter
        {
            static int getPriority (const juce::Identifier& i)
            {
                if (i == IDs::AUTOMATIONTRACK)  return 0;
                if (Clip::isClipState (i))      return 1;
                if (i == IDs::PLUGIN)           return 2;
                if (i == IDs::OUTPUTDEVICES)    return 3;
                if (i == IDs::LFOS)             return 4;

                return -1;
            }

            int compareElements (const juce::ValueTree& first, const juce::ValueTree& second) const noexcept
            {
                auto priority = getPriority (first.getType()) - getPriority (second.getType());

                if (priority != 0)
                    return priority;

                if (Clip::isClipState (first) && Clip::isClipState (second))
                {
                    double t1 = first[IDs::start];
                    double t2 = second[IDs::start];
                    return t1 < t2 ? -1 : (t2 < t1 ? 1 : 0);
                }

                return 0;
            }
        };

        Sorter clipSorter;
        state.sort (clipSorter, um, true);
    }

    void editFinishedLoading()
    {
        editLoadedCallback = nullptr;

        for (auto c : objects)
            if (auto acb = dynamic_cast<AudioClipBase*> (c))
                acb->updateAutoCrossfadesAsync (false);

        clipOwner.clipPositionChanged();
    }

private:
    bool recordingIsStopping = false;

    void playbackContextChanged() override {}
    void autoSaveNow() override {}
    void setAllLevelMetersActive (bool) override {}
    void setVideoPosition (TimePosition, bool) override {}
    void startVideo() override {}
    void stopVideo() override {}

    void recordingAboutToStop (InputDeviceInstance&) override
    {
        recordingIsStopping = true;
    }

    void recordingFinished (InputDeviceInstance&, const juce::ReferenceCountedArray<Clip>&) override
    {
        recordingIsStopping = false;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipList)
};


//==============================================================================
//==============================================================================
ClipOwner::ClipOwner() {}
ClipOwner::~ClipOwner() {}

void ClipOwner::initialiseClipOwner (Edit& edit, juce::ValueTree clipParentState)
{
    if (clipList)
        return;

    if (! edit.getUndoManager().isPerformingUndoRedo())
        ClipList::sortClips (clipParentState, &edit.getUndoManager());

    clipList = std::make_unique<ClipList> (*this, edit, clipParentState);
}

const juce::Array<Clip*>& ClipOwner::getClips() const
{
    assert (clipList && "You must call initialiseClipOwner before any other methods");
    return clipList->objects;
}

//==============================================================================
//==============================================================================
Clip* findClipForState (ClipOwner& co, const juce::ValueTree& v)
{
    for (auto c : co.getClips())
        if (c->state == v)
            return c;

    return {};
}

Clip* findClipForID (ClipOwner& co, EditItemID id)
{
    for (auto c : co.getClips())
        if (c->itemID == id)
            return c;

    return {};
}

//==============================================================================
namespace clip_owner
{
    inline void updateClipState (juce::ValueTree& state, const juce::String& name,
                                 EditItemID itemID, ClipPosition position)
    {
        addValueTreeProperties (state,
                                IDs::name, name,
                                IDs::start, position.getStart().inSeconds(),
                                IDs::length, position.getLength().inSeconds(),
                                IDs::offset, position.getOffset().inSeconds());

        itemID.writeID (state, nullptr);
    }

    inline juce::ValueTree createNewClipState (const juce::String& name, TrackItem::Type type,
                                               EditItemID itemID, ClipPosition position)
    {
        juce::ValueTree state (TrackItem::clipTypeToXMLType (type));
        updateClipState (state, name, itemID, position);
        return state;
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
}

Clip* insertClipWithState (ClipOwner& clipOwner, juce::ValueTree clipState)
{
    CRASH_TRACER
    jassert (clipState.isValid());
    jassert (! clipState.getParent().isValid());

    auto& edit = clipOwner.getClipOwnerEdit();
    auto& engineBehaviour = edit.engine.getEngineBehaviour();

    if (clipState.hasType (IDs::MIDICLIP))
    {
        setPropertyIfMissing (clipState, IDs::sync, engineBehaviour.areMidiClipsRemappedWhenTempoChanges()
                                                       ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
    }
    else if (clipState.hasType (IDs::AUDIOCLIP) || clipState.hasType (IDs::EDITCLIP))
    {
        if (! clipState.getChildWithName (IDs::LOOPINFO).isValid())
        {
            auto sourceFile = SourceFileReference::findFileFromString (edit, clipState[IDs::source]);

            if (sourceFile.exists())
            {
                auto loopInfo = AudioFile (edit.engine, sourceFile).getInfo().loopInfo;

                if (loopInfo.getRootNote() != -1)
                    clipState.setProperty (IDs::autoPitch, true, nullptr);

                if (loopInfo.isLoopable())
                {
                    clipState.setProperty (IDs::autoTempo, true, nullptr);
                    clipState.setProperty (IDs::stretchMode, true, nullptr);
                    clipState.setProperty (IDs::elastiqueMode, (int) TimeStretcher::elastiquePro, nullptr);

                    auto& ts = edit.tempoSequence;

                    auto startBeat = ts.toBeats (TimePosition::fromSeconds (static_cast<double> (clipState[IDs::start])));
                    auto endBeat   = startBeat + BeatDuration::fromBeats (loopInfo.getNumBeats());
                    auto newLength = ts.toTime (endBeat) - ts.toTime (startBeat);

                    clipState.setProperty (IDs::length, newLength.inSeconds(), nullptr);
                }

                auto loopSate = loopInfo.state;

                if (loopSate.getNumProperties() > 0 || loopSate.getNumChildren() > 0)
                    clipState.addChild (loopSate.createCopy(), -1, nullptr);
            }
        }

        if (! clipState.hasProperty (IDs::sync))
        {
            if (clipState.getProperty (IDs::autoTempo))
                clipState.setProperty (IDs::sync, (int) engineBehaviour.areAutoTempoClipsRemappedWhenTempoChanges()
                                                           ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
            else
                clipState.setProperty (IDs::sync, (int) engineBehaviour.areAudioClipsRemappedWhenTempoChanges()
                                                           ? Clip::syncBarsBeats : Clip::syncAbsolute, nullptr);
        }

        if (! clipState.hasProperty (IDs::autoCrossfade))
            if (edit.engine.getPropertyStorage().getProperty (SettingID::xFade, 0))
                clipState.setProperty (IDs::autoCrossfade, true, nullptr);
    }

    if (clipOwner.getClips().size() < engineBehaviour.getEditLimits().maxClipsInTrack)
    {
        clipOwner.getClipOwnerState().addChild (clipState, -1, &edit.getUndoManager());

        if (auto newClip = findClipForState (clipOwner, clipState))
        {
            if (auto at = dynamic_cast<AudioTrack*> (clipOwner.getClipOwnerSelectable()))
            {
                if (newClip->getColour() == newClip->getDefaultColour())
                {
                    float hue = ((at->getAudioTrackNumber() - 1) % 9) / 9.0f;
                    newClip->setColour (newClip->getDefaultColour().withHue (hue));
                }

                if (auto acb = dynamic_cast<AudioClipBase*> (newClip))
                {
                    if (engineBehaviour.autoAddClipEdgeFades())
                        if (! (clipState.hasProperty (IDs::fadeIn) && clipState.hasProperty (IDs::fadeOut)))
                            acb->applyEdgeFades();

                    const auto defaults = engineBehaviour.getClipDefaults();

                    if (! clipState.hasProperty (IDs::proxyAllowed))
                        acb->setUsesProxy (defaults.useProxyFile);

                    if (! clipState.hasProperty (IDs::resamplingQuality))
                        acb->setResamplingQuality (defaults.resamplingQuality);
                }
            }

            return newClip;
        }
    }
    else
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Can't add any more clips to this track!"));
    }

    jassertfalse;
    return {};
}

Clip* insertClipWithState (ClipOwner& parent,
                           const juce::ValueTree& stateToUse, const juce::String& name, TrackItem::Type type,
                           ClipPosition position, DeleteExistingClips deleteExistingClips, bool allowSpottingAdjustment)
{
    CRASH_TRACER
    auto& edit = parent.getClipOwnerEdit();

    if (position.getStart() >= Edit::getMaximumEditEnd())
        return {};

    if (position.time.getEnd() > Edit::getMaximumEditEnd())
        position.time.getEnd() = Edit::getMaximumEditEnd();

    if (auto track = dynamic_cast<Track*> (&parent))
        track->setFrozen (false, Track::groupFreeze);

    if (deleteExistingClips == DeleteExistingClips::yes)
        deleteRegion (parent, position.time);

    auto newClipID = edit.createNewItemID();

    juce::ValueTree newState;

    if (stateToUse.isValid())
    {
        jassert (stateToUse.hasType (TrackItem::clipTypeToXMLType (type)));
        newState = stateToUse;
        clip_owner::updateClipState (newState, name, newClipID, position);
    }
    else
    {
        newState = clip_owner::createNewClipState (name, type, newClipID, position);
    }

    if (auto newClip = insertClipWithState (parent, newState))
    {
        if (allowSpottingAdjustment)
            newClip->setStart (std::max (0_tp, newClip->getPosition().getStart() - toDuration (newClip->getSpottingPoint())), false, false);

        return newClip;
    }

    return {};
}

Clip* insertNewClip (ClipOwner& parent, TrackItem::Type type, const juce::String& name, TimeRange pos)
{
    return insertNewClip (parent, type, name, { pos, 0_td });
}

Clip* insertNewClip (ClipOwner& parent, TrackItem::Type type, TimeRange pos)
{
    return insertNewClip (parent, type, TrackItem::getSuggestedNameForNewItem (type), pos);
}

Clip* insertNewClip (ClipOwner& parent, TrackItem::Type type, const juce::String& name, ClipPosition position)
{
    CRASH_TRACER

    if (auto newClip = insertClipWithState (parent, {}, name, type, position, DeleteExistingClips::no, false))
        return newClip;

    return {};
}

juce::ReferenceCountedObjectPtr<WaveAudioClip> insertWaveClip (ClipOwner& parent, const juce::String& name, const juce::File& sourceFile,
                                                               ClipPosition position, DeleteExistingClips deleteExistingClips)
{
    auto& edit = parent.getClipOwnerEdit();

    if (auto proj = edit.engine.getProjectManager().getProject (edit))
    {
        if (auto source = proj->createNewItem (sourceFile, ProjectItem::waveItemType(),
                                               name, {}, ProjectItem::Category::imported, true))
            return insertWaveClip (parent, name, source->getID(), position, deleteExistingClips);

        jassertfalse;
        return {};
    }

    // Insert with a relative path if possible, otherwise an absolute
    {
        auto newState = clip_owner::createNewClipState (name, TrackItem::Type::wave, edit.createNewItemID(), position);
        const bool useRelativePath = edit.filePathResolver && edit.editFileRetriever && edit.editFileRetriever().existsAsFile();
        newState.setProperty (IDs::source, SourceFileReference::findPathFromFile (edit, sourceFile, useRelativePath), nullptr);

        if (auto c = insertClipWithState (parent, newState, name, TrackItem::Type::wave, position, deleteExistingClips, false))
        {
            if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                return *wc;

            jassertfalse;
        }
    }

    jassertfalse;
    return {};
}

WaveAudioClip::Ptr insertWaveClip (ClipOwner& parent, const juce::String& name, ProjectItemID sourceID,
                                   ClipPosition position, DeleteExistingClips deleteExistingClips)
{
    CRASH_TRACER
    auto& edit = parent.getClipOwnerEdit();

    auto newState = clip_owner::createNewClipState (name, TrackItem::Type::wave, edit.createNewItemID(), position);
    newState.setProperty (IDs::source, sourceID.toString(), nullptr);

    if (auto c = insertClipWithState (parent, newState, name, TrackItem::Type::wave, position, deleteExistingClips, false))
    {
        if (auto wc = dynamic_cast<WaveAudioClip*> (c))
            return *wc;

        jassertfalse;
    }

    return {};
}

MidiClip::Ptr insertMIDIClip (ClipOwner& parent, const juce::String& name, TimeRange position)
{
    if (auto t = dynamic_cast<AudioTrack*> (&parent))
        if (! t->containsAnyMIDIClips())
            t->setVerticalScaleToDefault();

    if (auto c = insertNewClip (parent, TrackItem::Type::midi, name, position))
    {
        if (auto mc = dynamic_cast<MidiClip*> (c))
            return *mc;

        jassertfalse;
    }

    return {};
}

MidiClip::Ptr insertMIDIClip (ClipOwner& parent, TimeRange position)
{
    return insertMIDIClip (parent, TrackItem::getSuggestedNameForNewItem (TrackItem::Type::midi), position);
}

EditClip::Ptr insertEditClip (ClipOwner& parent, TimeRange position, ProjectItemID sourceID)
{
    CRASH_TRACER

    auto name = TrackItem::getSuggestedNameForNewItem (TrackItem::Type::edit);
    auto newState = clip_owner::createNewClipState (name, TrackItem::Type::edit, parent.getClipOwnerEdit().createNewItemID(), { position, TimeDuration() });
    newState.setProperty (IDs::source, sourceID.toString(), nullptr);

    if (auto c = insertClipWithState (parent, newState, name, TrackItem::Type::edit, { position, 0_td }, DeleteExistingClips::no, false))
    {
        if (auto ec = dynamic_cast<EditClip*> (c))
            return *ec;

        jassertfalse;
    }

    return {};
}

//==============================================================================
juce::Array<Clip*> deleteRegion (ClipOwner& parent, TimeRange range)
{
    juce::Array<Clip*> newClips;

    CRASH_TRACER
    if (auto track = dynamic_cast<Track*> (&parent))
    {
        track->setFrozen (false, Track::groupFreeze);
        track->setFrozen (false, Track::individualFreeze);
    }

    // make a copied list first, as they'll get moved out-of-order..
    Clip::Array clipsToDo;

    for (auto c : parent.getClips())
        if (c->getPosition().time.overlaps (range))
            clipsToDo.add (c);

    for (int i = clipsToDo.size(); --i >= 0;)
        newClips.addArray (deleteRegion (*clipsToDo.getUnchecked (i), range));

    return newClips;
}

juce::Array<Clip*> deleteRegion (Clip& c, TimeRange range)
{
    juce::Array<Clip*> newClips;

    CRASH_TRACER
    if (auto track = dynamic_cast<Track*> (c.getParent()))
    {
        track->setFrozen (false, Track::groupFreeze);
        track->setFrozen (false, Track::individualFreeze);
    }

    auto pos = c.getPosition();

    if (range.contains (pos.time))
    {
        c.removeFromParent();
    }
    else if (pos.getStart() < range.getStart() && pos.getEnd() > range.getEnd())
    {
        if (auto newClip = split (c, range.getStart()))
        {
            newClip->setStart (range.getEnd(), true, false);
            c.setEnd (range.getStart(), true);
            c.deselect();

            newClips.add (newClip);
        }
    }
    else
    {
        c.trimAwayOverlap (range);
    }

    return newClips;
}

juce::Array<Clip*> split (ClipOwner& parent, TimePosition time)
{
    CRASH_TRACER
    juce::Array<Clip*> newClips;

    // Make a copied list first, as they'll get moved out-of-order..
    Clip::Array clipsToDo;

    for (auto c : parent.getClips())
        if (c->getPosition().time.contains (time))
            clipsToDo.add (c);

    for (auto c : clipsToDo)
        newClips.add (split (*c, time));

    return newClips;
}

Clip* split (Clip& clip, const TimePosition time)
{
    CRASH_TRACER
    auto parent = clip.getParent();

    if (parent == nullptr)
        return {};

    auto& edit = clip.edit;

    if (auto track = dynamic_cast<Track*> (parent))
        track->setFrozen (false, Track::groupFreeze);

    if (clip.getPosition().time.reduced (0.001s).contains (time)
         && ! clip.isGrouped())
    {
        auto newClipState = clip.state.createCopy();
        edit.createNewItemID().writeID (newClipState, nullptr);

        if (auto newClip = insertClipWithState (*parent, newClipState))
        {
            // special case for waveaudio clips that may have fade in/out
            if (auto acb = dynamic_cast<AudioClipBase*> (newClip))
                acb->setFadeIn ({});

            if (auto acb = dynamic_cast<AudioClipBase*> (&clip))
                acb->setFadeOut ({});

            // need to do this after setting the fades, so the fades don't
            // get mucked around with..
            const bool isChord = dynamic_cast<ChordClip*> (&clip) != nullptr;
            newClip->setStart (time, ! isChord, false);
            clip.setEnd (time, true);

            // special case for marker clips, set the marker number
            if (auto mc1 = dynamic_cast<MarkerClip*> (&clip))
            {
                if (auto mc2 = dynamic_cast<MarkerClip*> (newClip))
                {
                    auto id = edit.getMarkerManager().getNextUniqueID (mc1->getMarkerID());
                    mc2->setMarkerID (id);

                    if (mc1->getName() == (TRANS("Marker") + " " + juce::String (mc1->getMarkerID())))
                        mc2->setName (TRANS("Marker") + " " + juce::String (id));
                    else
                        mc2->setName (clip_owner::incrementLastDigit (mc1->getName()));
                }
            }

            // fiddle with offsets for looped clips
            if (newClip->getLoopLengthBeats() > BeatDuration())
            {
                auto extra = juce::roundToInt (std::floor ((newClip->getOffsetInBeats()
                                                            / newClip->getLoopLengthBeats()) + 0.00001));

                if (extra != 0)
                {
                    auto newOffsetBeats = newClip->getOffsetInBeats() - (newClip->getLoopLengthBeats() * extra);
                    auto offset = TimeDuration::fromSeconds (newOffsetBeats.inBeats() / edit.tempoSequence.getBeatsPerSecondAt (newClip->getPosition().getStart()));

                    newClip->setOffset (offset);
                }
            }

            return newClip;
        }
    }

    return {};
}

bool containsAnyMIDIClips (const ClipOwner& co)
{
    for (auto& c : co.getClips())
        if (c->isMidi())
            return true;

    return false;
}


//==============================================================================
//==============================================================================
bool isMasterTrack (const Track& t)                             { return dynamic_cast<const MasterTrack*> (&t) != nullptr; }
bool isTempoTrack (const Track& t)                              { return dynamic_cast<const TempoTrack*> (&t) != nullptr; }
bool isAutomationTrack (const Track& t)                         { return dynamic_cast<const AutomationTrack*> (&t) != nullptr; }
bool isAudioTrack (const Track& t)                              { return dynamic_cast<const AudioTrack*> (&t) != nullptr; }
bool isFolderTrack (const Track& t)                             { return dynamic_cast<const FolderTrack*> (&t) != nullptr; }
bool isMarkerTrack (const Track& t)                             { return dynamic_cast<const MarkerTrack*> (&t) != nullptr; }
bool isChordTrack (const Track& t)                              { return dynamic_cast<const ChordTrack*> (&t) != nullptr; }
bool isArrangerTrack (const Track& t)                           { return dynamic_cast<const ArrangerTrack*> (&t) != nullptr; }

bool isAudioTrack (const ClipOwner& t)                          { return dynamic_cast<const AudioTrack*> (&t) != nullptr; }
bool isFolderTrack (const ClipOwner& t)                         { return dynamic_cast<const FolderTrack*> (&t) != nullptr; }
bool isMarkerTrack (const ClipOwner& t)                         { return dynamic_cast<const MarkerTrack*> (&t) != nullptr; }
bool isChordTrack (const ClipOwner& t)                          { return dynamic_cast<const ChordTrack*> (&t) != nullptr; }
bool isArrangerTrack (const ClipOwner& t)                       { return dynamic_cast<const ArrangerTrack*> (&t) != nullptr; }

//==============================================================================
bool canContainMIDI (const ClipOwner& co)
{
    if (auto track = dynamic_cast<const Track*> (&co))
        return isAudioTrack (*track);

    return false;
}

bool canContainAudio (const ClipOwner& co)
{
    if (dynamic_cast<const ContainerClip*> (&co) != nullptr)
        return true;

    if (auto track = dynamic_cast<const Track*> (&co))
        return isAudioTrack (*track);

    return false;
}

bool isOnTop (const Track& track)
{
    return isMarkerTrack (track) || isTempoTrack (track) || isChordTrack (track) || isArrangerTrack (track) || isMasterTrack (track)
        || (isAutomationTrack (track) && track.getParentTrack() != nullptr && track.getParentTrack()->isMasterTrack());
}

}} // namespace tracktion { inline namespace engine
