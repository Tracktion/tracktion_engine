/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


EditClip::EditClip (const ValueTree& v, EditItemID clipID, ClipTrack& targetTrack, ProjectItemID sourceEditID)
    : AudioClipBase (v, clipID, Type::edit, targetTrack),
      waveInfo (getAudioFile().getInfo())
{
    renderOptions = RenderOptions::forEditClip (*this);

    sourceIdUpdater.setFunction ([this] { sourceMediaChanged(); });
    sourceFileReference.setToProjectFileReference (sourceEditID);

    auto um = getUndoManager();
    copyColourFromMarker.referTo (state, IDs::copyColour, um, false);
    trimToMarker.referTo (state, IDs::trimToMarker, um, false);
    renderEnabled.referTo (state, IDs::renderEnabled, um, true);
}

EditClip::~EditClip()
{
    for (auto e : referencedEdits)
        e->removeListener (this);

    notifyListenersOfDeletion();
}

AudioFile EditClip::getAudioFile() const
{
    if (isTracktionEditFile (getCurrentSourceFile()))
        return {};

    return AudioClipBase::getAudioFile();
}

//==============================================================================
void EditClip::initialise()
{
    AudioClipBase::initialise();

    if (waveInfo.sampleRate <= 0 || waveInfo.lengthInSamples <= 0)
        updateWaveInfo();

    if (! renderEnabled)
        setUsesProxy (false);

    sourceIdUpdater.triggerAsyncUpdate();
}

void EditClip::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<EditClip*> (c))
    {
        AudioClipBase::cloneFrom (other);

        copyColourFromMarker .setValue (other->copyColourFromMarker, nullptr);
        trimToMarker         .setValue (other->trimToMarker, nullptr);
        renderEnabled        .setValue (other->renderEnabled, nullptr);
    }
}

//==============================================================================
String EditClip::getSelectableDescription()
{
    return TRANS("Edit Clip") + " - \"" + getName() + "\"";
}

File EditClip::getOriginalFile() const
{
    jassert (editSnapshot == nullptr || editSnapshot->getFile() == sourceFileReference.getFile());
    return editSnapshot != nullptr ? editSnapshot->getFile() : File();
}

void EditClip::setLoopDefaults()
{
    // first check to see if these have been set
    if (loopInfo.getNumerator() == 0
         && loopInfo.getDenominator() == 0
         && loopInfo.getNumBeats() == 0)
        updateLoopInfoBasedOnSource (true);
}

//==============================================================================
bool EditClip::needsRender() const
{
    if (! renderEnabled || editSnapshot == nullptr)
        return false;

    return editSnapshot->getLength() > 0.0;
}

RenderManager::Job::Ptr EditClip::getRenderJob (const AudioFile& destFile)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    // do this here so we don't end up creating a new instance of our Edit
    if (auto existing = edit.engine.getRenderManager().getRenderJobWithoutCreating (destFile))
        return existing;

    auto j = EditRenderJob::getOrCreateRenderJob (edit.engine,
                                                  destFile, *renderOptions,
                                                  sourceFileReference.getSourceProjectItemID(),
                                                  true, getIsReversed());
    j->setName (TRANS("Creating Edit Clip") + ": " + getName());

    return j;
}

//==============================================================================
void EditClip::renderComplete()
{
    // update wave info so the render file is seen as valid
    updateWaveInfo();
    AudioClipBase::renderComplete();
}

String EditClip::getRenderMessage()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (renderJob == nullptr || getCurrentSourceFile().existsAsFile())
        return {};

    auto numRenderingJobs = edit.engine.getRenderManager().getNumJobs();
    String remainderMessage;

    if (numRenderingJobs > 0)
        remainderMessage << " (" << String (numRenderingJobs) << " " << TRANS("remaining") << ")";

    if (renderJob == nullptr)
        return TRANS("Rendering referenced Edits") + "..." + remainderMessage;

    const float progress = renderJob->getCurrentTaskProgress();

    if (progress <= 0.0f)
        return TRANS("Rendering referenced Edits") + "..." + remainderMessage;

    return TRANS("Rendering Edit: ") + String (roundToInt (progress * 100.0f)) + "%";
}

String EditClip::getClipMessage()
{
    if (! sourceFileReference.getSourceProjectItemID().isValid())
        return TRANS("No source set");

    if (renderOptions->getNumTracks() == 0)
        return TRANS("No tracks selected to render");

    if (! renderEnabled)
        return TRANS("Rendering disabled");

    return AudioClipBase::getClipMessage();
}

//==============================================================================
void EditClip::sourceMediaChanged()
{
    if (sourceMediaReEntrancyCheck)
        return;

    const ScopedValueSetter<bool> svs (sourceMediaReEntrancyCheck, true);

    auto newID = sourceFileReference.getSourceProjectItemID();

    // Check this here because when any ProjectItem in this project gets changed the Edit will call
    // sendSourceFileUpdate bypassing the usual check in setSourceProjectItemID
    if (isInitialised && lastSourceId == newID)
        return;

    const bool resetTracksToDefault = (! edit.isLoading() && ! lastSourceId.isValid());

    lastSourceId = newID;
    editSnapshot = EditSnapshot::getEditSnapshot (newID);
    const bool invalidSource = editSnapshot == nullptr || ! editSnapshot->isValid();

    if (invalidSource)
    {
        if (renderJob != nullptr)
            renderJob->removeListener (this);

        setCurrentSourceFile ({});
        renderJob = nullptr;
    }

    if (resetTracksToDefault)
    {
        if (editSnapshot != nullptr)
            renderOptions->setTrackIDs (editSnapshot->getTracks());
        else
            renderOptions->setTrackIDs ({});
    }

    updateReferencedEdits();
    updateWaveInfo();
    generateHash();

    if (! invalidSource)
        updateSourceFile();

    changed();

    if (isInitialised)
        updateLoopInfoBasedOnSource (true);
}

void EditClip::changed()
{
    // update the hash in case the source has changed and we need to re-generate the render inside AudioClipBase
    generateHash();
    AudioClipBase::changed();
}

bool EditClip::isUsingFile (const AudioFile& af)
{
    if (AudioClipBase::isUsingFile (af))
        return true;

    auto audioFile = RenderManager::getAudioFileForHash (edit.getTempDirectory (false), getHash());

    if (audioFile == af)
        return true;

    return false;
}

void EditClip::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == state && i == IDs::renderEnabled)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        renderEnabled.forceUpdateOfCachedValue();

        setUsesProxy (renderEnabled);
        changed();

        if (renderEnabled)
            updateSourceFile();
        else
            cancelCurrentRender();
    }
    else
    {
        AudioClipBase::valueTreePropertyChanged (v, i);
    }
}

ProjectItem::Ptr EditClip::createUniqueCopy()
{
    if (auto item = sourceFileReference.getSourceProjectItem())
        return item->createCopy();

    return {};
}

void EditClip::updateWaveInfo()
{
    // If the edit is empty this will cause the AudioSegmentList structure to have undefined content.
    // need to find a way around this, maybe just use a default length of 5s so silence is generated
    jassert ((! needsRender()) || getSourceLength() > 0.0);
    const double sourceLength = getSourceLength() == 0.0 ? 5.0 : getSourceLength();

    waveInfo.bitsPerSample      = renderOptions->getBitDepth();
    waveInfo.sampleRate         = renderOptions->getSampleRate();
    waveInfo.numChannels        = renderOptions->getStereo() ? 2 : 1;
    waveInfo.lengthInSamples    = int64 (sourceLength * waveInfo.sampleRate);

    updateLoopInfoBasedOnSource (false);
}

int64 EditClip::generateHash()
{
    CRASH_TRACER

    // Because edit clips can contain edit clips recursively we can't just rely
    // on the source edit time as a hash, we need to drill down retrieving any
    // nested EditClips and xor their source hash's.
    juce::int64 editClipHash = 0;

    for (auto snapshot : referencedEdits)
        editClipHash ^= snapshot->getHash();

    const juce::int64 newHash = editClipHash
                                  ^ renderOptions->getHash()
                                  ^ (int64) (getIsReversed() * 768);

    if (hash != newHash)
    {
        markAsDirty();
        hash = newHash;
    }

    return newHash;
}

void EditClip::setTracksToRender (const juce::Array<EditItemID>& trackIDs)
{
    if (renderOptions != nullptr)
    {
        renderOptions->setTrackIDs (trackIDs);
        generateHash();
    }
}

void EditClip::updateReferencedEdits()
{
    CRASH_TRACER

    ReferenceCountedArray<EditSnapshot> current, added, removed;

    if (editSnapshot != nullptr)
        current = editSnapshot->getNestedEditObjects();

    // find any new
    for (int i = current.size(); --i >= 0;)
    {
        auto snapshot = current.getUnchecked (i);

        if (referencedEdits.contains (snapshot))
            continue;

        referencedEdits.add (snapshot);
        added.add (snapshot);
    }

    // then any that have been removed
    for (int i = referencedEdits.size(); --i >= 0;)
    {
        auto snapshot = referencedEdits.getUnchecked (i);

        if (current.contains (snapshot))
            continue;

        removed.add (snapshot);
        referencedEdits.remove (i);
    }

    // then we need to de-register any removed and register any added
    for (auto snapshot : removed)
        snapshot->removeListener (this);

    for (auto snapshot : added)
        snapshot->addListener (this);
}

void EditClip::updateLoopInfoBasedOnSource (bool updateLength)
{
    if (editSnapshot == nullptr || ! editSnapshot->isValid())
        return;

    // first try and get info from the source edit
    auto tempo          = editSnapshot->getTempo();
    auto timeSigNum     = editSnapshot->getTimeSigNumerator();
    auto timeSigDenom   = editSnapshot->getTimeSigDenominator();
    auto pitch          = editSnapshot->getPitch();
    double clipNumBeats = 1.0;

    if (tempo > 0.0)
    {
        clipNumBeats = (tempo * getSourceLength()) / 60.0;
        loopInfo.setNumBeats (clipNumBeats);
        loopInfo.setBpm (tempo, waveInfo);
        // need to set num beats or the tempo will get messed up
    }

    if (timeSigNum > 0 && timeSigDenom > 0)
    {
        loopInfo.setNumerator (timeSigNum);
        loopInfo.setDenominator (timeSigDenom);
    }

    // if these haven't been set then match them to the new edit
    auto& ts = edit.tempoSequence;
    auto clipPos = getPosition();

    if (loopInfo.getNumerator() == 0 || loopInfo.getDenominator() == 0)
    {
        loopInfo.setNumerator (ts.getTimeSigAt (clipPos.getStart()).numerator);
        loopInfo.setDenominator (ts.getTimeSigAt (clipPos.getStart()).denominator);
    }

    if (loopInfo.getNumBeats() == 0)
        loopInfo.setNumBeats (length * (ts.getTempoAt (clipPos.getStart()).getBpm() / 60.0));

    // also need to adjust clip length
    if (updateLength)
    {
        if (! (loopInfo.getNumerator() == 0
                && loopInfo.getDenominator() == 0
                && loopInfo.getNumBeats() == 0))
        {
            auto editBpm = ts.getTempoAt (clipPos.getStart()).getBpm();

            if (tempo == 0.0)
                tempo = loopInfo.getBpm (getWaveInfo());

            if (editBpm > 0.0 && tempo > 0.0 && tempo < 400)
            {
                auto bpmRatio = tempo / editBpm;
                jassert (bpmRatio > 0.1 && bpmRatio < 10.0); // sensible?
                auto newLength = bpmRatio * getSourceLength();
                setLength (newLength, true);
            }

            loopInfo.setNumBeats (clipNumBeats);
            setAutoTempo (true);
        }
    }

    if (updateLength && pitch > 0)
        loopInfo.setRootNote (pitch);
}

double EditClip::getCurrentStretchRatio() const
{
    if (audioSegmentList != nullptr && ! audioSegmentList->getSegments().isEmpty())
        return audioSegmentList->getSegments().getReference (0).getStretchRatio();

    return getSpeedRatio();
}

//==============================================================================
void EditClip::editChanged (EditSnapshot&)
{
    // If any of the Edit's we are referencing have changed we need to re-check them all
    updateReferencedEdits();
    generateHash();
}
