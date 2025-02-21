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

TimeDuration RenderOptions::findEndAllowance (Edit& edit,
                                              juce::Array<EditItemID>* tracks,
                                              juce::Array<Clip*>* clips)
{
    auto allTracks = getAllTracks (edit);
    Plugin::Array plugins;

    if (tracks != nullptr)
        for (auto t : allTracks)
            if (tracks->contains (t->itemID))
                for (auto p : t->pluginList)
                    plugins.addIfNotAlreadyThere (p);

    if (clips != nullptr)
        for (auto c : *clips)
            if (auto pl = c->getPluginList())
                for (auto p : *pl)
                    plugins.addIfNotAlreadyThere (p);

    TimeDuration allowance;

    for (auto p : plugins)
    {
        auto tailLength = p->getTailLength();

        if (tailLength == std::numeric_limits<double>::infinity())
            tailLength = 0.0;

        allowance = std::max (allowance, TimeDuration::fromSeconds (tailLength));
    }

    return allowance;
}

static TimeRange findTimeFromClips (const juce::Array<Clip*>& clips, TimeDuration endAllowance) noexcept
{
    auto time = findUnionOfEditTimeRanges (clips);
    time = time.withEnd (time.getEnd() + endAllowance);
    return time;
}

static bool isParentTrackSelected (SelectionManager& sm, Track& t)
{
    if (auto ft = t.getParentFolderTrack())
        return sm.isSelected (ft) || isParentTrackSelected (sm, *ft);

    return false;
}

static juce::BigInteger getSelectedTracks (Edit& edit, SelectionManager& sm)
{
    juce::BigInteger b;
    int i = 0;

    for (auto t : getAllTracks (edit))
    {
        if (sm.isSelected (t))
            b.setBit (i);

        if (! t->isPartOfSubmix() && isParentTrackSelected (sm, *t))
            b.setBit (i);

        ++i;
    }

    return b;
}

//==============================================================================
void RenderOptions::loadFromUserSettings()
{
    auto& storage = engine.getPropertyStorage();

    if (isRender())
    {
        format            = (TargetFileFormat) static_cast<int> (storage.getProperty (SettingID::renderFormat, (int) wav));

        if (format != wav || format != aiff)
            format = wav;

        sampleRate        = engine.getDeviceManager().getSampleRate();
        bitDepth          = storage.getProperty (SettingID::trackRenderBits, 16);
        usePlugins        = storage.getProperty (SettingID::bypassFilters, true);

        if (isTrackRender())
            markedRegion  = storage.getProperty (SettingID::markedRegion, false);
    }
    else if (isExportAll())
    {
        format            = (TargetFileFormat) static_cast<int> (storage.getProperty (SettingID::exportFormat, (int) wav));
        selectedClips     = storage.getProperty (SettingID::renderOnlySelectedClips, false);
        markedRegion      = storage.getProperty (SettingID::renderOnlyMarked, false);
        normalise         = storage.getProperty (SettingID::renderNormalise, false);
        adjustBasedOnRMS  = storage.getProperty (SettingID::renderRMS, false);
        rmsLevelDb        = storage.getProperty (SettingID::renderRMSLevelDb, -12.0);
        peakLevelDb       = storage.getProperty (SettingID::renderPeakLevelDb, 0.0);
        removeSilence     = storage.getProperty (SettingID::renderTrimSilence, false);
        stereo            = storage.getProperty (SettingID::renderStereo, true);
        sampleRate        = engine.getDeviceManager().getSampleRate();
        bitDepth          = storage.getProperty (SettingID::renderBits, 16);
        dither            = storage.getProperty (SettingID::renderDither, true);
        qualityIndex      = storage.getProperty (SettingID::quality, 5);
        addMetadata       = storage.getProperty (SettingID::addId3Info, false);
        addAcidMetadata   = storage.getProperty (SettingID::addAcidMetadata, false);
        realTime          = storage.getProperty (SettingID::realtime, false);
        usePlugins        = storage.getProperty (SettingID::passThroughFilters, true);
    }
    else if (isEditClipRender())
    {
        sampleRate        = engine.getDeviceManager().getSampleRate();
        bitDepth          = storage.getProperty (SettingID::editClipRenderBits, 16);
        dither            = storage.getProperty (SettingID::editClipRenderDither, true);
        realTime          = storage.getProperty (SettingID::editClipRealtime, false);
        stereo            = storage.getProperty (SettingID::editClipRenderStereo, true);
        normalise         = storage.getProperty (SettingID::editClipRenderNormalise, false);
        adjustBasedOnRMS  = storage.getProperty (SettingID::editClipRenderRMS, false);
        rmsLevelDb        = storage.getProperty (SettingID::editClipRenderRMSLevelDb, -12.0);
        peakLevelDb       = storage.getProperty (SettingID::editClipRenderPeakLevelDb, 0.0);
        usePlugins        = storage.getProperty (SettingID::editClipPassThroughFilters, true);
        addAcidMetadata   = storage.getProperty (SettingID::addAcidMetadata, false);

        return;
    }

    updateDefaultFilename (nullptr);
}

void RenderOptions::saveToUserSettings()
{
    auto& storage = engine.getPropertyStorage();

    if (isTrackRender() || isClipRender() || isMidiRender())
    {
        storage.setProperty (SettingID::renderFormat, (int) format);
        storage.setProperty (SettingID::trackRenderSampRate, sampleRate.get());
        storage.setProperty (SettingID::trackRenderBits, bitDepth.get());
        storage.setProperty (SettingID::bypassFilters, usePlugins.get());

        if (isTrackRender())
            storage.setProperty (SettingID::markedRegion, markedRegion.get());
    }
    else if (isEditClipRender())
    {
        storage.setProperty (SettingID::editClipRenderSampRate, sampleRate.get());
        storage.setProperty (SettingID::editClipRenderBits, bitDepth.get());
        storage.setProperty (SettingID::editClipRenderDither, dither.get());
        storage.setProperty (SettingID::editClipRealtime, realTime.get());
        storage.setProperty (SettingID::editClipRenderStereo, stereo.get());
        storage.setProperty (SettingID::editClipRenderNormalise, normalise.get());
        storage.setProperty (SettingID::editClipRenderRMS, adjustBasedOnRMS.get());
        storage.setProperty (SettingID::editClipRenderRMSLevelDb, rmsLevelDb.get());
        storage.setProperty (SettingID::editClipRenderPeakLevelDb, peakLevelDb.get());
        storage.setProperty (SettingID::editClipPassThroughFilters, usePlugins.get());
        storage.setProperty (SettingID::addAcidMetadata, addAcidMetadata.get());
    }
    else if (isExportAll())
    {
        storage.setProperty (SettingID::exportFormat, (int) format);
        storage.setProperty (SettingID::renderOnlySelectedClips, selectedClips.get());
        storage.setProperty (SettingID::renderOnlyMarked, markedRegion.get());
        storage.setProperty (SettingID::renderNormalise, normalise.get());
        storage.setProperty (SettingID::renderRMS, adjustBasedOnRMS.get());
        storage.setProperty (SettingID::renderRMSLevelDb, rmsLevelDb.get());
        storage.setProperty (SettingID::renderPeakLevelDb, peakLevelDb.get());
        storage.setProperty (SettingID::renderTrimSilence, removeSilence.get());
        storage.setProperty (SettingID::renderSampRate, sampleRate.get());
        storage.setProperty (SettingID::renderStereo, stereo.get());
        storage.setProperty (SettingID::renderBits, bitDepth.get());
        storage.setProperty (SettingID::renderDither, dither.get());
        storage.setProperty (SettingID::quality, qualityIndex.get());
        storage.setProperty (SettingID::addId3Info, addMetadata.get());
        storage.setProperty (SettingID::addAcidMetadata, addAcidMetadata.get());
        storage.setProperty (SettingID::realtime, realTime.get());
        storage.setProperty (SettingID::passThroughFilters, usePlugins.get());
    }
}

//==============================================================================
RenderManager::Job::Ptr RenderOptions::performBackgroundRender (Edit& edit, SelectionManager* sm,
                                                                TimeRange timeRangeToRender)
{
    Renderer::Parameters p (edit);

    // First get the correct renderer params
    if (isMidiRender())
    {
        if (auto mc = dynamic_cast<MidiClip*> (allowedClips.getFirst()))
            p = getRenderParameters (*mc);
        else
            return {};
    }
    else
    {
        p = getRenderParameters (edit, sm, timeRangeToRender);
    }

    // And then fill in any specific params
    p.category = isRender() ? ProjectItem::Category::rendered
                            : ProjectItem::Category::exports;

    if (isClipRender())
        p.allowedClips = allowedClips;

    if (isTrackRender())
        p.endAllowance = markedRegion ? 0.0s : 10.0s;

    if (addAcidMetadata)
        addAcidInfo (edit, p);

    return (p.audioFormat != nullptr || p.createMidiFile)
                ? EditRenderJob::getOrCreateRenderJob (edit.engine, p, false, false, false)
                : nullptr;
}

void RenderOptions::relinkCachedValues (juce::UndoManager* um)
{
    type.referTo (state, IDs::renderType, um, RenderType::allExport);
    tracksProperty.referTo (state, IDs::renderTracks, um, {});
    createMidiFile.referTo (state, IDs::renderCreateMidiFile, um, false);
    format.referTo (state, IDs::renderFormat, um, wav);

    stereo.referTo (state, IDs::renderStereo, um, true);

    sampleRate.referTo (state, IDs::renderSampleRate, um, 44100.0);
    bitDepth.referTo (state, IDs::renderBitDepth, um, 32);
    qualityIndex.referTo (state, IDs::renderQualityIndex, um, 5);
    rmsLevelDb.referTo (state, IDs::renderRMSLevelDb, um, 0.0);
    peakLevelDb.referTo (state, IDs::renderPeakLevelDb, um, 0.0);

    removeSilence.referTo (state, IDs::renderRemoveSilence, um, false);
    normalise.referTo (state, IDs::renderNormalise, um, false);
    dither.referTo (state, IDs::renderDither, um, false);
    adjustBasedOnRMS.referTo (state, IDs::renderAdjustBasedOnRMS, um, false);
    markedRegion.referTo (state, IDs::renderMarkedRegion, um, false);
    selectedTracks.referTo (state, IDs::renderSelectedTracks, um, false);
    selectedClips.referTo (state, IDs::renderSelectedClips, um, false);
    tracksToSeparateFiles.referTo (state, IDs::renderTracksToSeparateFiles, um, false);
    realTime.referTo (state, IDs::renderRealTime, um, false);
    usePlugins.referTo (state, IDs::renderPlugins, um, true);

    addRenderOptions.referTo (state, IDs::renderOptions, um, none);
    addRenderToLibrary.referTo (state, IDs::addRenderToLibrary, um, false);
    reverseRender.referTo (state, IDs::reverseRender, um, false);
    addMetadata.referTo (state, IDs::renderAddMetadata, um, false);
    addAcidMetadata.referTo (state, IDs::addAcidMetadata, um, false);

    sampleRate = engine.getDeviceManager().getSampleRate();
    tracks = EditItemID::parseStringList (tracksProperty);
    updateHash();
}

void RenderOptions::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state && i == IDs::renderTracks)
    {
        tracks = EditItemID::parseStringList (tracksProperty);
        updateHash();
    }
}

//==============================================================================
static juce::StringPairArray getMetadata (Edit& edit)
{
    juce::StringPairArray metadataList;
    auto metadata = edit.getEditMetadata();

    if (metadata.album.isNotEmpty())          metadataList.set ("id3album", metadata.album);
    if (metadata.artist.isNotEmpty())         metadataList.set ("id3artist", metadata.artist);
    if (metadata.comment.isNotEmpty())        metadataList.set ("id3comment", metadata.comment);
    if (metadata.date.isNotEmpty())           metadataList.set ("id3date", metadata.date);
    if (metadata.genre.isNotEmpty())          metadataList.set ("id3genre", metadata.genre);
    if (metadata.title.isNotEmpty())          metadataList.set ("id3title", metadata.title);
    if (metadata.trackNumber.isNotEmpty())    metadataList.set ("id3trackNumber", metadata.trackNumber);

    return metadataList;
}

Renderer::Parameters RenderOptions::getRenderParameters (Edit& edit)
{
    return getRenderParameters (edit, nullptr, {});
}

Renderer::Parameters RenderOptions::getRenderParameters (Edit& edit, SelectionManager* selectionManager,
                                                         TimeRange markedRegionTime)
{
    Renderer::Parameters params (edit);

    params.destFile                 = destFile;
    params.createMidiFile           = format == midi;
    params.audioFormat              = getAudioFormat();

    params.bitDepth                 = bitDepth;
    params.blockSizeForAudio        = edit.engine.getDeviceManager().getBlockSize();
    params.sampleRateForAudio       = sampleRate;
    params.shouldNormalise          = normalise;
    params.trimSilenceAtEnds        = removeSilence;
    params.shouldNormaliseByRMS     = adjustBasedOnRMS;
    params.normaliseToLevelDb       = (float) (adjustBasedOnRMS ? rmsLevelDb : peakLevelDb);
    params.canRenderInMono          = true;
    params.mustRenderInMono         = ! stereo;
    params.usePlugins               = (params.createMidiFile || (isTrackRender() || isClipRender() || isEditClipRender())) ? usePlugins : true;
    params.useMasterPlugins         = isRender() ? false : ((params.createMidiFile || (isEditClipRender())) ? usePlugins : true);
    params.realTimeRender           = realTime;
    params.ditheringEnabled         = dither;
    params.quality                  = qualityIndex;
    params.separateTracks           = tracksToSeparateFiles;

    if (! isMarkedRegionBigEnough (markedRegionTime))
        markedRegion = false;

    if (markedRegion)
        params.time = markedRegionTime;
    else
        params.time = { TimePosition(), edit.getLength() };

    auto allTracks = getAllTracks (edit);

    if (isRender())
    {
        for (int i = allTracks.size(); --i >= 0;)
            if (tracks.contains (allTracks.getUnchecked (i)->itemID))
                params.tracksToDo.setBit (i);

        if (isClipRender() || isMidiRender())
        {
            params.allowedClips = allowedClips;
            params.endAllowance = usePlugins ? findEndAllowance (edit, &tracks, &allowedClips) : 0.0s;
            params.time = params.time.getIntersectionWith (findTimeFromClips (params.allowedClips, params.endAllowance));
        }
    }
    else if (selectedClips)
    {
        if (selectionManager != nullptr)
        {
            if (selectionManager->containsType<Clip>())
            {
                for (int i = allTracks.size(); --i >= 0;)
                    if (allTracks.getUnchecked (i)->isAudioTrack())
                        params.tracksToDo.setBit (i);

                auto clips = selectionManager->getItemsOfType<Clip>();

                for (auto clip : clips)
                    params.allowedClips.add (clip);

                auto time = findTimeFromClips (clips, params.endAllowance);
                params.time = params.time.getIntersectionWith (time);
            }
        }
        else
        {
            jassertfalse; // need a selection manager if the user can enable this option!
        }
    }
    else if (selectedTracks)
    {
        if (selectionManager != nullptr)
            params.tracksToDo = getSelectedTracks (edit, *selectionManager);
        else
            jassertfalse; // need a selection manager if the user can enable this option!
    }

    if (params.tracksToDo.isZero())
        params.tracksToDo.setRange (0, allTracks.size(), true);

    if (addMetadata)
        params.metadata = getMetadata (edit);

    if (addAcidMetadata)
        params.metadata.addArray (createAcidInfo (edit, params.time));

    return params;
}

Renderer::Parameters RenderOptions::getRenderParameters (EditClip& clip)
{
    Renderer::Parameters params (clip.edit);

    params.destFile                 = destFile;
    params.createMidiFile           = format == midi;
    params.audioFormat              = getAudioFormat();

    params.bitDepth                 = bitDepth;
    params.blockSizeForAudio        = clip.edit.engine.getDeviceManager().getBlockSize();
    params.sampleRateForAudio       = sampleRate;
    params.shouldNormalise          = normalise;
    params.trimSilenceAtEnds        = removeSilence;
    params.shouldNormaliseByRMS     = adjustBasedOnRMS;
    params.normaliseToLevelDb       = (float) (adjustBasedOnRMS ? rmsLevelDb : peakLevelDb);
    params.canRenderInMono          = true;
    params.mustRenderInMono         = ! stereo;
    params.usePlugins               = usePlugins;
    params.useMasterPlugins         = usePlugins;
    params.realTimeRender           = realTime;
    params.ditheringEnabled         = dither;
    params.quality                  = qualityIndex;
    params.category                 = ProjectItem::Category::rendered;
    params.separateTracks           = tracksToSeparateFiles;

    if (const EditSnapshot::Ptr snapshot = clip.getEditSnapshot())
    {
        params.time = { TimePosition(), snapshot->getLength() };
        params.tracksToDo.setRange (0, snapshot->getNumTracks(), true);

        const bool markedBigEnough = snapshot->getMarkIn() < snapshot->getMarkOut() + 0.05s;

        if (markedRegion && snapshot->areMarksActive() && markedBigEnough)
            params.time = { snapshot->getMarkIn(), snapshot->getMarkOut() };
    }

    return params;
}

Renderer::Parameters RenderOptions::getRenderParameters (MidiClip& clip)
{
    Renderer::Parameters params (clip.edit);

    params.destFile                 = destFile;
    params.createMidiFile           = format == midi;
    params.audioFormat              = getAudioFormat();

    params.bitDepth                 = bitDepth;
    params.blockSizeForAudio        = clip.edit.engine.getDeviceManager().getBlockSize();
    params.sampleRateForAudio       = sampleRate;
    params.endAllowance             = endAllowance;
    params.shouldNormalise          = normalise;
    params.trimSilenceAtEnds        = removeSilence;
    params.shouldNormaliseByRMS     = adjustBasedOnRMS;
    params.normaliseToLevelDb       = (float) (adjustBasedOnRMS ? rmsLevelDb : peakLevelDb);
    params.canRenderInMono          = true;
    params.mustRenderInMono         = ! stereo;
    params.usePlugins               = true;
    params.useMasterPlugins         = false;
    params.realTimeRender           = realTime;
    params.ditheringEnabled         = dither;
    params.quality                  = qualityIndex;
    params.category                 = ProjectItem::Category::rendered;
    params.time                     = clip.getEditTimeRange();
    params.tracksToDo               = getTrackIndexes (clip.edit);
    params.allowedClips             = allowedClips;

    return params;
}

juce::AudioFormat* RenderOptions::getAudioFormat()
{
    auto& afm = engine.getAudioFileFormatManager();

    if (format == midi)      return {};
    if (format == wav)       return afm.getWavFormat();
    if (format == aiff)      return afm.getAiffFormat();
    if (format == flac)      return afm.getFlacFormat();
    if (format == ogg)       return afm.getOggFormat();

    if (format == mp3)
    {
        LAMEManager::registerAudioFormat (afm);

        if (auto af = afm.getLameFormat())
            return af;
    }

    format = wav;
    uiNeedsRefresh = true;

    return afm.getWavFormat();
}

Clip::Ptr RenderOptions::applyRenderToEdit (Edit& edit,
                                            const ProjectItem::Ptr projectItem,
                                            TimeRange time,
                                            SelectionManager* selectionManager) const
{
    CRASH_TRACER

    const AddRenderOptions addMode = addRenderOptions;

    if (projectItem == nullptr || addMode == RenderOptions::none)
        return {};

    projectItem->verifyLength();
    auto trackIndexes = getTrackIndexes (edit);
    auto alltracks = getAllTracks (edit);
    Track::Array tracksArray;

    for (int track = -1;;)
    {
        track = trackIndexes.findNextSetBit (++track);

        if (track < 0)
            break;

        if (auto t = alltracks[track])
            tracksArray.add (t);
    }

    auto lastTrack = tracksArray.getLast();

    if (lastTrack == nullptr)
        return {};

    AudioTrack::Ptr trackToUse;

    switch (addMode)
    {
        case nextTrack:
            trackToUse = dynamic_cast<AudioTrack*> (lastTrack->getSiblingTrack (1, false));
            break;

        case thisTrack:     [[ fallthrough ]];
        case replaceClips:
            trackToUse = dynamic_cast<AudioTrack*> (lastTrack.get());
            break;

        case addTrack:      [[ fallthrough ]];
        case replaceTrack:  [[ fallthrough ]];
        case none:          [[ fallthrough ]];
        default:
            break;
    }

    if (trackToUse == nullptr)
    {
        if (selectionManager != nullptr)
            selectionManager->deselectAll();

        trackToUse = edit.insertNewAudioTrack (TrackInsertPoint (lastTrack == nullptr ? nullptr : lastTrack->getParentTrack(),
                                                                 lastTrack.get()),
                                               selectionManager);

        if (trackToUse == nullptr)
            return {};

        if (addMode == replaceTrack)
            if (auto t = tracksArray.getFirst())
                trackToUse->setColour (t->getColour());

        for (auto t : tracksArray)
        {
            if (auto at = dynamic_cast<AudioTrack*> (t))
            {
                if (auto targetTrack = at->getOutput().getDestinationTrack())
                    trackToUse->getOutput().setOutputToTrack (targetTrack);
                else
                    trackToUse->getOutput().setOutputToDeviceID (at->getOutput().getOutputDeviceID());

                break;
            }
        }

        if (tracksArray.size() > 1)
            trackToUse->setName (TRANS("Rendered tracks"));
    }

    auto startTime = reverseRender ? (time.getEnd() - TimeDuration::fromSeconds (projectItem->getLength()))
                                   : time.getStart();

    const TimeRange insertPos (startTime, startTime + TimeDuration::fromSeconds (projectItem->getLength()));

    auto newClipName = isTrackRender() ? TRANS("Rendered track")
                                       : TRANS("Rendered clip");

    Clip::Ptr newClip;

    if (isMidiRender() || format == midi)
    {
        newClip = trackToUse->insertMIDIClip (newClipName, insertPos, nullptr);
    }
    else
    {
        bool allowAutoTempo = true;
        bool allowAutoPitch = true;

        for (auto c : allowedClips)
        {
            if (auto ac = dynamic_cast<WaveAudioClip*> (c))
            {
                if (! ac->getAutoTempo()) allowAutoTempo = false;
                if (! ac->getAutoPitch()) allowAutoPitch = false;
            }
            else
            {
                allowAutoTempo = false;
                allowAutoPitch = false;
            }
        }

        newClip = trackToUse->insertWaveClip (newClipName, projectItem->getID(), { insertPos, {} }, false);
        if (auto ac = dynamic_cast<WaveAudioClip*> (newClip.get()))
        {
            // We only want to enable auto tempo is the rendered clip has tempo information
            // We can't rely on the LoopInfo to determine if tempo information is present or not,
            // since if it is not present in the source file, then the WaveAudioClip will calculate a
            // bpm for the clip based on it's length and edit bpm. So we need to go to the source file
            // and check the metadata
            AudioFile af (edit.engine, ac->getOriginalFile());
            auto metadata = af.getInfo().metadata;
            if (metadata[juce::WavAudioFormat::acidBeats].getIntValue() > 0)
                ac->setAutoTempo (allowAutoTempo);

            // Only enable auto pitch, if we have pitch information
            if (ac->getLoopInfo().getRootNote() != -1)
                ac->setAutoPitch (allowAutoPitch);
        }
    }

    if (newClip == nullptr)
    {
        jassertfalse;
        return {};
    }

    if (addMode == replaceTrack)
    {
        for (auto t : tracksArray)
            edit.deleteTrack (t);
    }
    else if (addMode == replaceClips)
    {
        if (isMidiRender())
        {
            if (auto c = allowedClips.getFirst())
                if (c->getPosition().time.overlaps (time))
                    c->getClipTrack()->deleteRegionOfClip (c, time, nullptr);
        }
        else
        {
            if (markedRegion)
            {
                if (selectionManager != nullptr)
                    deleteRegionOfSelectedClips (*selectionManager, time, CloseGap::no, false);

                newClip->setStart (time.getStart(), false, true);
            }
            else
            {
                for (auto c : allowedClips)
                    c->removeFromParent();
            }
        }
    }

    if (format == midi)
    {
        if (auto mc = dynamic_cast<MidiClip*> (newClip.get()))
        {
            juce::OwnedArray<MidiList> lists;
            BeatDuration length;
            juce::Array<BeatPosition> tempoChangeBeatNumbers;
            juce::Array<double> bpms;
            juce::Array<int> numerators, denominators;

            MidiList::readSeparateTracksFromFile (projectItem->getSourceFile(), lists, tempoChangeBeatNumbers,
                                                  bpms, numerators, denominators, length, false);

            if (auto list = lists.getFirst())
                mc->getSequence().copyFrom (*list, nullptr);
        }
    }

    if (selectionManager != nullptr)
    {
        if (isTrackRender())
            selectionManager->selectOnly (trackToUse.get());
        else
            selectionManager->selectOnly (*newClip);
    }

    if (reverseRender)
        if (auto acb = dynamic_cast<AudioClipBase*> (newClip.get()))
            acb->setIsReversed (true);

    return newClip;
}

//==============================================================================
void RenderOptions::setToDefault()
{
    type                     = RenderType::allExport;

    createMidiFile           = false;
    format                   = wav;
    stereo                   = true;
    sampleRate               = 44100.0;
    bitDepth                 = 32;
    qualityIndex             = 5;
    rmsLevelDb               = 0.0;
    peakLevelDb              = 0.0;
    endAllowance             = {};

    removeSilence            = false;
    normalise                = false;
    dither                   = false;
    adjustBasedOnRMS         = false;
    markedRegion             = false;
    selectedTracks           = false;
    selectedClips            = false;
    tracksToSeparateFiles    = false;
    realTime                 = false;
    usePlugins               = true;

    addRenderOptions         = none;
    addRenderToLibrary       = false;
    reverseRender            = false;

    addMetadata              = false;
    addAcidMetadata          = false;
}

void RenderOptions::updateLastUsedRenderPath (RenderOptions& renderOptions, const juce::String& itemID)
{
    auto& storage = renderOptions.engine.getPropertyStorage();
    auto lastEditID = storage.getProperty (SettingID::lastEditRender).toString();

    if (itemID == lastEditID)
        renderOptions.destFile = storage.getDefaultLoadSaveDirectory ("exportRender");

    storage.setProperty (SettingID::lastEditRender, itemID);
}

std::unique_ptr<RenderOptions> RenderOptions::forGeneralExporter (Edit& edit)
{
    std::unique_ptr<RenderOptions> ro (new RenderOptions (edit.engine));
    ro->setToDefault();

    updateLastUsedRenderPath (*ro, edit.getProjectItemID().toString());

    for (auto t : getAllTracks (edit))
        ro->tracks.add (t->itemID);

    ro->addMetadata = getMetadata (edit).size() != 0;
    ro->updateDefaultFilename (&edit);
    ro->updateHash();

    return ro;
}

std::unique_ptr<RenderOptions> RenderOptions::forTrackRender (juce::Array<Track*> tracks,
                                                              AddRenderOptions addOption)
{
    if (auto first = tracks.getFirst())
    {
        auto& edit = first->edit;

        std::unique_ptr<RenderOptions> ro (new RenderOptions (edit.engine));
        ro->setToDefault();
        ro->type = RenderType::track;
        updateLastUsedRenderPath (*ro, edit.getProjectItemID().toString());

        for (auto t : tracks)
            ro->tracks.add (t->itemID);

        ro->addRenderOptions = addOption;
        ro->updateDefaultFilename (&edit);
        ro->updateHash();
        return ro;
    }

    jassertfalse;
    return {};
}

std::unique_ptr<RenderOptions> RenderOptions::forClipRender (juce::Array<Clip*> clips, bool midiNotes)
{
    if (auto first = clips.getFirst())
    {
        auto& edit = first->edit;

        std::unique_ptr<RenderOptions> ro (new RenderOptions (edit.engine));
        ro->setToDefault();
        updateLastUsedRenderPath (*ro, edit.getProjectItemID().toString());

        ro->allowedClips = clips;
        bool areAllClipsMono = true;

        for (auto c : clips)
        {
            if (auto t = c->getTrack())
            {
                ro->tracks.addIfNotAlreadyThere (t->itemID);

                if (auto at = dynamic_cast<AudioTrack*> (t))
                    if (auto dest = at->getOutput().getDestinationTrack())
                        ro->tracks.addIfNotAlreadyThere (dest->itemID);
            }

            // Assume any non-audio clips should be rendered in stereo
            if (auto audioClip = dynamic_cast<AudioClipBase*> (c))
            {
                if (audioClip->getWaveInfo().numChannels > 1)
                    areAllClipsMono = false;
            }
            else
            {
                areAllClipsMono = false;
            }
        }

        ro->type = midiNotes ? RenderType::midi
                             : RenderType::clip;

        ro->stereo            = ! areAllClipsMono;
        ro->selectedClips     = false;
        ro->endAllowance      = ro->usePlugins ? findEndAllowance (edit, &ro->tracks, &clips) : TimeDuration();
        ro->removeSilence     = midiNotes;
        ro->addRenderOptions  = nextTrack;
        ro->updateDefaultFilename (&edit);
        ro->updateHash();
        return ro;
    }

    jassertfalse;
    return {};
}

std::unique_ptr<RenderOptions> RenderOptions::forEditClip (Clip& clip)
{
    std::unique_ptr<RenderOptions> ro (new RenderOptions (clip.edit.engine, clip.state, &clip.edit.getUndoManager()));
    ro->type = RenderType::editClip;
    ro->updateHash();

    return ro;
}

//==============================================================================
RenderOptions::RenderOptions (Engine& e)
    : RenderOptions (e, juce::ValueTree (IDs::RENDER), nullptr)
{
}

RenderOptions::RenderOptions (Engine& e, const juce::ValueTree& s, juce::UndoManager* um)
    : engine (e), state (s)
{
    state.addListener (this);
    relinkCachedValues (um);
}

RenderOptions::RenderOptions (const RenderOptions& other, juce::UndoManager* um)
    : RenderOptions (other.engine, other.state.createCopy(), um)
{
}

RenderOptions::~RenderOptions()
{
    state.removeListener (this);

    if (! isEditClipRender())
        engine.getPropertyStorage().setDefaultLoadSaveDirectory ("exportRender", destFile);
}

void RenderOptions::setUINeedsRefresh()
{
    uiNeedsRefresh = true;
}

bool RenderOptions::getUINeedsRefresh()
{
    if (! uiNeedsRefresh)
        return false;

    uiNeedsRefresh = false;
    return true;
}

//==============================================================================
RenderOptions::TargetFileFormat RenderOptions::setFormat (TargetFileFormat f)
{
    auto& afm = engine.getAudioFileFormatManager();
    format = f;

    LAMEManager::registerAudioFormat (afm);

    if (format == mp3 && afm.getLameFormat() == nullptr)
        format = wav;

    updateFileName();
    updateOptionsForFormat();

    return format;
}

void RenderOptions::setFormatType (const juce::String& typeString)
{
    auto& am = engine.getAudioFileFormatManager();

    if      (typeString == am.getWavFormat()->getFormatName())     setFormat (wav);
    else if (typeString == am.getAiffFormat()->getFormatName())    setFormat (aiff);
    else if (typeString == am.getFlacFormat()->getFormatName())    setFormat (flac);
    else if (typeString == am.getOggFormat()->getFormatName())     setFormat (ogg);
    else if (typeString == "MP3 file")                             setFormat (mp3);
    else if (typeString == "MIDI file")                            setFormat (midi);
    else                                                           setFormat (wav);
}

juce::String RenderOptions::getFormatTypeName (TargetFileFormat fmt)
{
    auto& am = engine.getAudioFileFormatManager();

    switch (fmt)
    {
        case wav:           return am.getWavFormat()->getFormatName();
        case aiff:          return am.getAiffFormat()->getFormatName();
        case flac:          return am.getFlacFormat()->getFormatName();
        case ogg:           return am.getOggFormat()->getFormatName();
        case mp3:           return am.getLameFormat() == nullptr ? juce::String() : am.getLameFormat()->getFormatName();
        case midi:          return "MIDI file";
        case numFormats:    return "MIDI file";
        default:            return {};
    }
}

void RenderOptions::setTrackIDs (const juce::Array<EditItemID>& trackIDs)
{
    if (trackIDs.isEmpty())
        tracksProperty.resetToDefault();
    else
        tracksProperty = EditItemID::listToString (trackIDs);
}

HashCode RenderOptions::getTracksHash() const
{
    HashCode tracksHash = 0;

    for (auto& t : tracks)
        tracksHash ^= static_cast<HashCode> (t.getRawID()); // TODO: this will probably be buggy if the IDs are all low sequential integers!

    return tracksHash;
}

void RenderOptions::setSampleRate (int sr)
{
    if (auto af = getAudioFormat())
    {
        auto rates = af->getPossibleSampleRates();

        if (! rates.contains (sr))
            sr = rates.contains (44100) ? 44100 : rates[(int) std::floor (rates.size() / 2.0)];

        sampleRate = sr;
    }
}

juce::BigInteger RenderOptions::getTrackIndexes (const Edit& ed) const
{
    juce::BigInteger res;

    auto trks = getAllTracks (ed);

    for (int i = tracks.size(); --i >= 0;)
        if (auto t = findTrackForID (ed, tracks.getUnchecked (i)))
            res.setBit (trks.indexOf (t));

    return res;
}

void RenderOptions::setSelected (bool onlySelectedTrackAndClips)
{
    selectedTracks = onlySelectedTrackAndClips;
    selectedClips = onlySelectedTrackAndClips;
}

void RenderOptions::setFilename (juce::String value, bool canPromptToOverwriteExisting)
{
    juce::RecentlyOpenedFilesList recent;
    auto recentList = engine.getPropertyStorage().getProperty (SettingID::renderRecentFilesList).toString();
    recent.restoreFromString (recentList);

    juce::File f;

   #if JUCE_MODAL_LOOPS_PERMITTED
    if (value.startsWithIgnoreCase ("$browse"))
    {
        enum
        {
            cancel = 0,
            browse,
            projectDefault,
            recentOffset
        };

        juce::PopupMenu m, m2;

        m.addItem (browse, TRANS("Browse") + "...");
        m.addItem (projectDefault, TRANS("Set to project default"));

        int i = recentOffset;

        for (auto& s : recent.getAllFilenames())
            m2.addItem (i++, s);

        if (recent.getNumFiles() > 0)
            m.addSubMenu (TRANS("Recent"), m2);

        auto res = m.show();

        if (res == cancel)
            return;

        if (res == browse)
        {
            juce::FileChooser chooser (TRANS("Select the file to use"), destFile, "*" + getCurrentFileExtension());

            if (! chooser.browseForFileToSave (false))
                return;

            f = chooser.getResult();
        }
        else if (res == projectDefault)
        {
            f = juce::File();
        }
        else
        {
            f = recent.getAllFilenames()[res - recentOffset];
        }
    }
    else
   #endif
    {
        f = juce::File (value);
    }

    if (f.existsAsFile()
        #if JUCE_MODAL_LOOPS_PERMITTED
          && canPromptToOverwriteExisting
          && engine.getUIBehaviour().showOkCancelAlertBox (TRANS("File Already Exists"),
                                                           TRANS("The file you have chosen already exists, do you want to delete it?")
                                                             + juce::newLine + juce::newLine
                                                             + TRANS("If you choose to cancel, a non existent file name will be chosen automatically.")
                                                             + juce::newLine
                                                             + "(" + TRANS("This operation can't be undone") + ")")
         #endif
        )
    {
        AudioFile (engine, f).deleteFile();

        if (f.existsAsFile())
            f.moveToTrash();
    }

    destFile = f.getFullPathName();
    updateDefaultFilename (nullptr);

    if (destFile != juce::File())
    {
        recent.addFile (destFile);
        auto newRecentList = recent.toString();

        if (recentList != newRecentList)
            engine.getPropertyStorage().setProperty (SettingID::renderRecentFilesList, newRecentList);
    }
}

//==============================================================================
juce::StringArray RenderOptions::getFormatTypes()
{
    auto& am = engine.getAudioFileFormatManager();

    juce::StringArray formats;
    formats.add (am.getWavFormat()->getFormatName());
    formats.add (am.getAiffFormat()->getFormatName());

    if (! isRender())
    {
        formats.add (am.getFlacFormat()->getFormatName());
        formats.add (am.getOggFormat()->getFormatName());

      #if JUCE_USE_LAME_AUDIO_FORMAT
        auto& afm = engine.getAudioFileFormatManager();
        LAMEManager::registerAudioFormat (afm);

        if (LAMEManager::lameIsAvailable() && am.getLameFormat() != nullptr)
            formats.add (am.getLameFormat()->getFormatName());
      #endif

        formats.add ("MIDI file");
    }

    return formats;
}

juce::StringArray RenderOptions::getAddRenderOptionText()
{
    static const char* renderOptionsText[] = { NEEDS_TRANS("Replace Rendered Tracks"),
                                               NEEDS_TRANS("Add Rendered Tracks"),
                                               NEEDS_TRANS("Insert Into Next Track"),
                                               NEEDS_TRANS("Insert Into This Track"),
                                               NEEDS_TRANS("Replace Clips"),
                                               NEEDS_TRANS("None") };

    juce::StringArray renderOptions;

    renderOptions.add (TRANS(renderOptionsText[replaceTrack]));
    renderOptions.add (TRANS(renderOptionsText[addTrack]));

    if (! isTrackRender())
    {
        renderOptions.add (TRANS(renderOptionsText[nextTrack]));
        renderOptions.add (TRANS(renderOptionsText[thisTrack]));
        renderOptions.add (TRANS(renderOptionsText[replaceClips]));
        renderOptions.add (TRANS(renderOptionsText[none]));
    }

    return renderOptions;
}

//==============================================================================
bool RenderOptions::isMarkedRegionBigEnough (TimeRange region)
{
    return region.getLength() > 0.05s;
}

juce::StringArray RenderOptions::getQualitiesList() const
{
    auto& af = engine.getAudioFileFormatManager();

    if (format == flac)
        return af.getFlacFormat()->getQualityOptions();

    if (format == ogg)
        return af.getOggFormat()->getQualityOptions();

    if (auto lameFormat = af.getLameFormat())
        if (format == mp3)
            return lameFormat->getQualityOptions();

    return {};
}

//==============================================================================
void RenderOptions::updateHash()
{
    hash = (tracks.isEmpty() ? 0 : getTracksHash())
           ^ (((HashCode) createMidiFile)        << 0)
           ^ (((HashCode) format)                << 1)
           ^ (((HashCode) stereo)                << 2)
           ^ (((HashCode) sampleRate)            << 3)
           ^ (((HashCode) bitDepth)              << 4)
           ^ (((HashCode) qualityIndex)          << 5)
           ^ (((HashCode) removeSilence)         << 6)
           ^ (((HashCode) normalise)             << 7)
           ^ (((HashCode) dither)                << 8)
           ^ (((HashCode) adjustBasedOnRMS)      << 9)
           ^ ((HashCode) (rmsLevelDb * -4567.2))
           ^ ((HashCode) (peakLevelDb * 2453.1))
           ^ (((HashCode) markedRegion)          << 10)
           ^ (((HashCode) selectedTracks)        << 12)
           ^ (((HashCode) selectedClips)         << 12)
           ^ (((HashCode) tracksToSeparateFiles) << 13)
           ^ (((HashCode) realTime)              << 14)
           ^ (((HashCode) usePlugins)            << 15)
           ^ (((HashCode) addMetadata)           << 16)
           ^ (((HashCode) addAcidMetadata)       << 17);
}

void RenderOptions::updateFileName()
{
    if (format == midi)
        destFile = destFile.withFileExtension ("mid");
    else if (auto af = getAudioFormat())
        destFile = destFile.withFileExtension (af->getFileExtensions()[0]);
    else
        destFile = juce::File();

    setFilename (destFile.getFullPathName(), false);
    updateHash();
}

void RenderOptions::updateOptionsForFormat()
{
    if (auto af = getAudioFormat())
    {
        // sample rate
        auto rates = af->getPossibleSampleRates();
        juce::StringArray r;

        for (auto rate : rates)
            r.add (juce::String (rate));

        if (! rates.contains ((int) sampleRate))
            sampleRate = 44100;

        if (! rates.contains ((int) sampleRate))
            sampleRate = rates[0];

        // bit-depth
        r.clear();
        auto depths = af->getPossibleBitDepths();

        for (auto depth : depths)
            r.add (TRANS("32-bit").replace ("32", juce::String (depth)));

        if (! depths.contains (bitDepth))
            bitDepth = 16;

        if (! depths.contains (bitDepth))
            bitDepth = depths[0];

        // quality
        auto qualities = af->getQualityOptions();

        if (! juce::isPositiveAndBelow (qualityIndex.get(), qualities.size()))
            qualityIndex = (int) std::ceil (qualities.size() / 2.0);

        if (! juce::isPositiveAndBelow (qualityIndex.get(), qualities.size()))
            qualityIndex = 0;
    }
}

void RenderOptions::updateDefaultFilename (Edit* edit)
{
    if (destFile == juce::File()
        || destFile.existsAsFile()
        || destFile.getFileExtension() != getCurrentFileExtension())
    {
        const ProjectItem::Category category = isRender() ? ProjectItem::Category::rendered
                                                          : ProjectItem::Category::exports;
        juce::String nameStem;

        if (selectedTracks || selectedClips)
        {
            if (auto sm = engine.getUIBehaviour().getCurrentlyFocusedSelectionManager())
            {
                if (auto clip = sm->getFirstItemOfType<Clip>())
                    nameStem = clip->getName();

                if (auto track = sm->getFirstItemOfType<Track>())
                    nameStem = track->getName();
            }
        }

        if (nameStem.trim().isEmpty())
        {
            if (edit == nullptr)
                edit = engine.getUIBehaviour().getCurrentlyFocusedEdit();

            if (edit != nullptr)
            {
                nameStem = edit->getName();

                if (isTrackRender() && tracks.size() > 0)
                    if (auto t = findTrackForID (*edit, tracks[0]))
                        nameStem += " " + t->getName();

                nameStem += " " + (category == ProjectItem::Category::exports ? TRANS("Export")
                                                                              : TRANS("Render")) + " 1";
            }
        }

        const auto dir = destFile.existsAsFile() ? destFile.getParentDirectory()
                                                 : engine.getPropertyStorage().getDefaultLoadSaveDirectory (category);

        destFile = dir.getChildFile (juce::File::createLegalFileName (nameStem)
                                       + getCurrentFileExtension());

        destFile = getNonExistentSiblingWithIncrementedNumberSuffix (destFile, false);
    }
}

juce::String RenderOptions::getCurrentFileExtension()
{
    if (format == midi)
        return ".mid";

    if (auto af = getAudioFormat())
        return af->getFileExtensions()[0];

    return {};
}

}} // namespace tracktion { inline namespace engine
