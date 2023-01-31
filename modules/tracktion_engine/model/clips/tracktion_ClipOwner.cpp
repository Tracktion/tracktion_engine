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
                             private juce::AsyncUpdater
{
    ClipList (ClipOwner& co, Edit& e, const juce::ValueTree& parentTree)
        : ValueTreeObjectList<Clip> (parentTree),
          edit (e),
          clipOwner (co)
    {
        rebuildObjects();

        editLoadedCallback.reset (new Edit::LoadFinishedCallback<ClipList> (*this, edit));
    }

    ~ClipList() override
    {
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
        if (auto clipTrack = dynamic_cast<ClipTrack*> (clipOwner.getClipOwnerSelectable()))
        {
            if (auto newClip = Clip::createClipForState (v, *clipTrack))
            {
                clipOwner.clipCreated (*newClip);
                newClip->incReferenceCount();

                return newClip.get();
            }
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

    void newObjectAdded (Clip* c) override      { objectAddedOrRemoved (c); }
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipList)
};


//==============================================================================
//==============================================================================
ClipOwner::ClipOwner() {}
ClipOwner::~ClipOwner() {}

void ClipOwner::initialiseClipOwner (Edit& edit, juce::ValueTree clipParentState)
{
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


//==============================================================================
//==============================================================================
bool isAudioTrack (ClipOwner& co)                           { return dynamic_cast<AudioTrack*> (&co) != nullptr; }
bool isAutomationTrack (ClipOwner& co)                      { return dynamic_cast<AutomationTrack*> (&co) != nullptr; }
bool isFolderTrack (ClipOwner& co)                          { return dynamic_cast<FolderTrack*> (&co) != nullptr; }
bool isMarkerTrack (ClipOwner& co)                          { return dynamic_cast<MarkerTrack*> (&co) != nullptr; }
bool isTempoTrack (ClipOwner& co)                           { return dynamic_cast<TempoTrack*> (&co) != nullptr; }
bool isChordTrack (ClipOwner& co)                           { return dynamic_cast<ChordTrack*> (&co) != nullptr; }
bool isArrangerTrack (ClipOwner& co)                        { return dynamic_cast<ArrangerTrack*> (&co) != nullptr; }
bool isMasterTrack (ClipOwner& co)                          { return dynamic_cast<MasterTrack*> (&co) != nullptr; }

//==============================================================================
bool canContainMarkers (ClipOwner& co)                      { return isMarkerTrack (co); }
bool canContainMIDI (ClipOwner& co)                         { return isAudioTrack (co); }
bool canContainAudio (ClipOwner& co)                        { return isAudioTrack (co); }
bool canContainEditClips (ClipOwner& co)                    { return isAudioTrack (co); }
bool canContainPlugins (ClipOwner& co)                      { return isAudioTrack (co) || isFolderTrack (co) || isMasterTrack (co); }
bool isMovable (ClipOwner& co)                              { return isAudioTrack (co) || isFolderTrack (co); }
bool acceptsInput (ClipOwner& co)                           { return isAudioTrack (co); }
bool createsOutput (ClipOwner& co)                          { return isAudioTrack (co); }
bool wantsAutomation (ClipOwner& co)                        { return ! (isMarkerTrack (co) || isChordTrack (co) || isArrangerTrack (co));  }

bool isOnTop (ClipOwner& co)
{
    if (auto tack = dynamic_cast<Track*> (&co))
        return isMarkerTrack (co) || isTempoTrack (co) || isChordTrack (co) || isArrangerTrack (co) || isMasterTrack (co)
            || (isAutomationTrack (co) && tack->getParentTrack() != nullptr && tack->getParentTrack()->isMasterTrack());

    return false;
}

}} // namespace tracktion { inline namespace engine
