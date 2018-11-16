/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


WaveAudioClip::WaveAudioClip (const ValueTree& v, EditItemID clipID, ClipTrack& ct)
    : AudioClipBase (v, clipID, Type::wave, ct)
{
}

WaveAudioClip::~WaveAudioClip()
{
    notifyListenersOfDeletion();
}

//==============================================================================
void WaveAudioClip::initialise()
{
    AudioClipBase::initialise();

    if (getTakesTree().isValid())
        callBlocking ([this]
                      {
                          getCompManager();
                          sourceMediaChanged();
                      });
}

void WaveAudioClip::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<WaveAudioClip*> (c))
    {
        AudioClipBase::cloneFrom (other);
        auto takes = state.getChildWithName (IDs::TAKES);
        copyValueTree (takes, other->state.getChildWithName (IDs::TAKES), nullptr);

        Selectable::changed();
    }
}

//==============================================================================
String WaveAudioClip::getSelectableDescription()
{
    return TRANS("Audio Clip") + " - \"" + getName() + "\"";
}

double WaveAudioClip::getSourceLength() const
{
    if (sourceLength == 0)
    {
        // If we're using clip effects the audio file may currently be invalid
        // However, we know that the effects will produce an audio file of the same length as the originial so we'll return this
        // This could however be a problem with standard warp time, Edit clips and reverse etc...

        sourceLength = clipEffects != nullptr ? AudioFile (getOriginalFile()).getLength()
                                              : getAudioFile().getLength();
    }

    jassert (sourceLength >= 0.0);
    return sourceLength;
}

void WaveAudioClip::sourceMediaChanged()
{
    AudioClipBase::sourceMediaChanged();

    sourceLength = 0.0;
    markAsDirty();

    if (needsRender())
        updateSourceFile();
}

File WaveAudioClip::getOriginalFile() const
{
    if (hasAnyTakes() && compManager != nullptr && compManager->isCurrentTakeComp())
        return compManager->getCurrentCompFile();

    return sourceFileReference.getFile();
}

bool WaveAudioClip::needsRender() const
{
    return ! isUsingMelodyne()
        && (isReversed || warpTime || (clipEffects != nullptr && canHaveEffects()))
        && AudioFile (getOriginalFile()).isValid();
}

int64 WaveAudioClip::getHash() const
{
    return AudioFile (getOriginalFile()).getHash()
         ^ (int64) (getWarpTime() ? getWarpTimeManager().getHash() : 0)
         ^ (int64) (getIsReversed() * 768)
         ^ (int64) ((clipEffects == nullptr || ! canHaveEffects())  ? 0 : clipEffects->getHash());
}

void WaveAudioClip::renderComplete()
{
    sourceLength = 0;
    AudioClipBase::renderComplete();
}

void WaveAudioClip::setLoopDefaults()
{
    auto& ts = edit.tempoSequence;
    auto pos = getPosition();

    if (loopInfo.getNumerator() == 0)
        loopInfo.setNumerator (ts.getTimeSigAt (pos.getStart()).numerator);

    if (loopInfo.getDenominator() == 0)
        loopInfo.setDenominator (ts.getTimeSigAt (pos.getStart()).denominator);

    if (loopInfo.getNumBeats() == 0.0)
        loopInfo.setNumBeats (getSourceLength() * (ts.getTempoAt (pos.getStart()).getBpm() / 60.0));
}

void WaveAudioClip::reassignReferencedItem (const ReferencedItem& item,
                                            ProjectItemID newItemID, double newStartTime)
{
    if (hasAnyTakes())
    {
        auto indexInList = getReferencedItems().indexOf (item);

        if (indexInList < 0)
        {
            jassertfalse;
            return;
        }

        if (indexInList == getCurrentTake())
            sourceFileReference.setToProjectFileReference (newItemID);

        auto take = getTakesTree().getChild (indexInList);

        if (take.isValid())
            take.setProperty (IDs::source, newItemID, getUndoManager());

        if (indexInList == 0)
        {
            if (! isLooping())
                setOffset (getPosition().getOffset() - (newStartTime / getSpeedRatio()));
            else
                loopStart = loopStart - (newStartTime / getSpeedRatio());
        }
    }
    else
    {
        AudioClipBase::reassignReferencedItem (item, newItemID, newStartTime);
    }
}

//==============================================================================
void WaveAudioClip::addTake (ProjectItemID id)
{
    auto um = getUndoManager();
    auto takesTree = state.getOrCreateChildWithName (IDs::TAKES, um);

    ValueTree take (IDs::TAKE);
    take.setProperty (IDs::source, id, um);
    takesTree.addChild (take, -1, um);
}

int WaveAudioClip::getNumTakes (bool includeComps)
{
    if (includeComps)
        return getTakesTree().getNumChildren();

    return hasAnyTakes() ? getCompManager().getNumTakes() : 0;
}

juce::Array<ProjectItemID> WaveAudioClip::getTakes() const
{
    juce::Array<ProjectItemID> takes;

    auto takesTree = state.getChildWithName (IDs::TAKES);

    for (auto t : takesTree)
        if (t.hasProperty (IDs::source))
            takes.add (ProjectItemID::fromProperty (t, IDs::source));

    return takes;
}

void WaveAudioClip::clearTakes()
{
    if (getNumTakes (true) != 0)
    {
        state.removeChild (getTakesTree(), getUndoManager());
        compManager = nullptr;
        changed();
    }
}

int WaveAudioClip::getCurrentTake() const
{
    if (currentTakeIndex == takeIndexNeedsUpdating)
    {
        currentTakeIndex = -1;

        auto takesTree = getTakesTree();
        auto pid = sourceFileReference.getSourceProjectItemID().toString();

        for (int i = takesTree.getNumChildren(); --i >= 0;)
        {
            if (takesTree.getChild (i).getProperty (IDs::source) == pid)
            {
                currentTakeIndex = i;
                break;
            }
        }
    }

    return currentTakeIndex;
}

void WaveAudioClip::invalidateCurrentTake() noexcept
{
    currentTakeIndex = takeIndexNeedsUpdating;
}

void WaveAudioClip::invalidateCurrentTake (const ValueTree& parent) noexcept
{
    if (parent.hasType (IDs::TAKES))
        invalidateCurrentTake();
}

void WaveAudioClip::valueTreePropertyChanged (ValueTree& treeWhosePropertyHasChanged, const Identifier& property)
{
    AudioClipBase::valueTreePropertyChanged (treeWhosePropertyHasChanged, property);

    if (property == IDs::source)
        invalidateCurrentTake();
}

void WaveAudioClip::valueTreeChildAdded (ValueTree& p, ValueTree& c)
{
    AudioClipBase::valueTreeChildAdded (p, c);
    invalidateCurrentTake (p);
}

void WaveAudioClip::valueTreeChildRemoved (ValueTree& p, ValueTree& c, int oldIndex)
{
    AudioClipBase::valueTreeChildRemoved (p, c, oldIndex);
    invalidateCurrentTake (p);
}

void WaveAudioClip::valueTreeChildOrderChanged (ValueTree& p, int oldIndex, int newIndex)
{
    AudioClipBase::valueTreeChildOrderChanged (p, oldIndex, newIndex);
    invalidateCurrentTake (p);
}

void WaveAudioClip::setCurrentTake (int takeIndex)
{
    CRASH_TRACER
    auto takesTree = getTakesTree();
    auto numTakes = takesTree.getNumChildren();
    jassert (isPositiveAndBelow (takeIndex, numTakes));
    takeIndex = jlimit (0, numTakes - 1, takeIndex);

    auto take = takesTree.getChild (takeIndex);
    jassert (take.isValid());

    auto takeSourceID = ProjectItemID::fromProperty (take, IDs::source);
    auto mo = ProjectManager::getInstance()->getProjectItem (takeSourceID);

    if (mo != nullptr || getCompManager().isTakeComp (takeIndex))
        sourceFileReference.setToProjectFileReference (takeSourceID);
    else
        takesTree.removeChild (take, getUndoManager());

    getCompManager().setActiveTakeIndex (takeIndex);
}

bool WaveAudioClip::isCurrentTakeComp()
{
    if (hasAnyTakes())
        return getCompManager().isCurrentTakeComp();

    return false;
}

StringArray WaveAudioClip::getTakeDescriptions() const
{
    CRASH_TRACER
    auto takes = getTakes();
    StringArray s;
    int numTakes = 0;

    for (int i = 0; i < takes.size(); ++i)
    {
        if (compManager == nullptr || ! compManager->isTakeComp (i))
        {
            if (auto projectItem = ProjectManager::getInstance()->getProjectItem (takes.getReference (i)))
                s.add (String (i + 1) + ". " + projectItem->getName());
            else
                s.add (String (i + 1) + ". <" + TRANS("Deleted") + ">");

            ++numTakes;
        }
        else
        {
            s.add (String (i + 1) + ". " + TRANS("Comp") + " #" + String (i + 1 - numTakes));
        }
    }

    return s;
}

void WaveAudioClip::deleteAllUnusedTakesConfirmingWithUser (bool deleteSourceFiles)
{
    CRASH_TRACER

   #if JUCE_MODAL_LOOPS_PERMITTED
    auto showWarning = [] (const String& title, const String& message, bool& delFiles) -> int
    {
        const std::unique_ptr<AlertWindow> w (LookAndFeel::getDefaultLookAndFeel()
                                                .createAlertWindow (title, message,
                                                                    {}, {}, {},
                                                                    AlertWindow::QuestionIcon, 0, nullptr));

        ToggleButton delFilesButton (TRANS("Delete Source Files?"));
        delFilesButton.setSize (400, 20);
        delFilesButton.setName ({});
        w->addCustomComponent (&delFilesButton);
        w->addTextBlock (TRANS("(This will also delete these from any other Edits in this project)"));
        w->addButton (TRANS("OK"), 1, KeyPress (KeyPress::returnKey));
        w->addButton (TRANS("Cancel"), 2, KeyPress (KeyPress::escapeKey));

        const int res = w->runModalLoop();
        delFiles = delFilesButton.getToggleState();

        return res;
    };

    if (getCompManager().isCurrentTakeComp())
    {
        if (showWarning (TRANS("Flatten Takes"),
                         TRANS("This will permanently remove all takes in this clip, replacing it with"
                               " the current comp. This operation can not be undone.")
                         + "\n\n"
                         + TRANS("Are you sure you want to do this?"),
                         deleteSourceFiles) == 1)
            getCompManager().flattenTake (getCurrentTake(), deleteSourceFiles);

        return;
    }
   #else
    juce::ignoreUnused (warn);
   #endif

    bool userIsSure = true;

   #if JUCE_MODAL_LOOPS_PERMITTED
    userIsSure = (showWarning (TRANS("Delete Unused Takes"),
                               TRANS("This will permanently delete all wave files that are listed as takes in this "
                                     "clip, apart from the ones currently being used.")
                                + "\n\n"
                                + TRANS("Are you sure you want to do this?"),
                                deleteSourceFiles) == 1);
   #endif

    if (userIsSure)
        deleteAllUnusedTakes (deleteSourceFiles);
}

static bool isTakeInUse (const WaveAudioClip& clip, ProjectItemID takeProjectItemID)
{
    for (auto t : getClipTracks (clip.edit))
    {
        for (auto& c : t->getClips())
        {
            if (c->getSourceFileReference().getSourceProjectItemID() == takeProjectItemID)
                return true;

            if (auto wac = dynamic_cast<WaveAudioClip*> (c))
                if (wac != &clip && wac->getTakes().contains (takeProjectItemID))
                    return true;
        }
    }

    return false;
}

void WaveAudioClip::deleteAllUnusedTakes (bool deleteSourceFiles)
{
    CRASH_TRACER

    auto takes = getTakes();
    bool errors = false;

    if (auto proj = ProjectManager::getInstance()->getProject (edit))
    {
        for (int i = takes.size(); --i >= 0;)
        {
            auto takeProjectItemID = takes.getReference (i);

            if (! isTakeInUse (*this, takeProjectItemID))
            {
                bool removedOk = ! deleteSourceFiles
                                  || proj->getProjectItemForID (takeProjectItemID) == nullptr
                                  || proj->removeProjectItem (takeProjectItemID, true);

                if (removedOk)
                {
                    for (auto child : state)
                    {
                        if (ProjectItemID::fromProperty (child, IDs::source) == takeProjectItemID)
                        {
                            state.removeChild (child, getUndoManager());
                            break;
                        }
                    }
                }
                else
                {
                    errors = true;
                }
            }
        }

        clearTakes();

        if (errors)
            edit.engine.getUIBehaviour().showWarningAlert (TRANS("Delete Unused Takes"),
                                                           TRANS("Some of the wave files couldn't be deleted"));
    }

    if (! hasAnyTakes())
        compManager = nullptr;
}

Clip::Array WaveAudioClip::unpackTakes (bool toNewTracks)
{
    CRASH_TRACER
    Clip::Array newClips;

    if (Track::Ptr t = getTrack())
    {
        const bool shouldBeShowingTakes = isShowingTakes();

        if (shouldBeShowingTakes)
            AudioClipBase::setShowingTakes (false);

        auto clipNode = state.createCopy();

        if (! clipNode.isValid())
            return newClips;

        clipNode.removeChild (clipNode.getChildWithName (IDs::TAKES), nullptr);

        int trackIndex = t->getIndexInEditTrackList();
        auto allTracks = getAllTracks (t->edit);
        auto takes = getTakes();

        for (int i = 0; i < takes.size(); ++i)
        {
            if (compManager->isTakeComp (i))
                continue;

            AudioTrack::Ptr targetTrack (dynamic_cast<AudioTrack*> (allTracks[++trackIndex]));

            if (toNewTracks || targetTrack == nullptr)
                targetTrack = t->edit.insertNewAudioTrack (TrackInsertPoint (t->getParentTrack(), t.get()), nullptr);

            if (targetTrack != nullptr)
                newClips.add (targetTrack->insertWaveClip (getTakeDescriptions()[i], takes[i], getPosition(), false));

            t = targetTrack;
        }

        if (shouldBeShowingTakes)
            AudioClipBase::setShowingTakes (true);
    }

    return newClips;
}

WaveCompManager& WaveAudioClip::getCompManager()
{
    jassert (hasAnyTakes());

    if (compManager == nullptr)
    {
        CRASH_TRACER
        auto ptr = edit.engine.getCompFactory().getCompManager (*this);
        compManager = dynamic_cast<WaveCompManager*> (ptr.get());

        if (compManager != nullptr)
            compManager->initialise();
        else
            jassertfalse;
    }

    return *compManager;
}

//==============================================================================
RenderManager::Job::Ptr WaveAudioClip::getRenderJob (const AudioFile& destFile)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    // do this here so we don't end up creating a multiple copies of a File
    if (auto existing = edit.engine.getRenderManager().getRenderJobWithoutCreating (destFile))
        return existing;

    if (clipEffects != nullptr && canHaveEffects())
    {
        auto j = clipEffects->createRenderJob (AudioFile (destFile.getFile()), AudioFile (getOriginalFile()));
        j->setName (TRANS("Rendering Clip Effects") + ": " + getName());
        return j;
    }

    if (getIsReversed())
    {
        auto j = ReverseRenderJob::getOrCreateRenderJob (edit.engine, getOriginalFile(), destFile.getFile());
        j->setName (TRANS("Reversing") + ": " + getName());
        return j;
    }

    if (getWarpTime())
    {
        auto j = WarpTimeRenderJob::getOrCreateRenderJob (*this, getOriginalFile(), destFile.getFile());
        j->setName (TRANS("Warping") + ": " + getName());
        return j;
    }

    return {};
}

String WaveAudioClip::getRenderMessage()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (renderJob == nullptr || getAudioFile().isValid())
        return {};

    const float progress = renderJob == nullptr ? 1.0f : renderJob->getCurrentTaskProgress();
    const String m (clipEffects != nullptr ? TRANS("Rendering effects")
                                           : (getWarpTime() ? TRANS("Warping") : TRANS("Reversing")));

    if (progress < 0.0f)
        return m + "...";

    return m + ": " + String (roundToInt (progress * 100.0f)) + "%";
}

bool WaveAudioClip::isUsingFile (const AudioFile& af)
{
    if (AudioClipBase::isUsingFile (af))
        return true;

    if (hasAnyTakes() && getCompManager().getCurrentCompFile() == af.getFile())
        return true;

    return false;
}

AudioFile WaveAudioClip::getCompFileFor (int64 takeHash) const
{
    auto tempDir = edit.getTempDirectory (true);

    // TODO: unify all proxy filename functionality..
    return AudioFile (tempDir.getChildFile (getCompPrefix()
                                             + "0_" + itemID.toString()
                                             + "_" + String::toHexString (takeHash)
                                             + ".wav"));
}
