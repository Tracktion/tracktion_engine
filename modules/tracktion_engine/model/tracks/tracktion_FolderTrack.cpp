/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


FolderTrack::FolderTrack (Edit& edit, const ValueTree& v)
    : Track (edit, v, 50, 13, 2000)
{
    soloed.referTo (state, IDs::solo, nullptr);
    muted.referTo (state, IDs::mute, nullptr);
    soloIsolated.referTo (state, IDs::soloIsolate, nullptr);

    pluginUpdater.setFunction ([this] { updatePlugins(); });
}

FolderTrack::~FolderTrack()
{
    notifyListenersOfDeletion();
}

void FolderTrack::initialise()
{
    Track::initialise();
    updatePlugins();
}

juce::String FolderTrack::getSelectableDescription()
{
    return (isSubmixFolder() ? TRANS("Submix") : TRANS("Folder")) + " - \"" + getName() + "\"";
}

bool FolderTrack::isFolderTrack() const
{
    return true;
}

void FolderTrack::sanityCheckName()
{
    const String n = Track::getName();

    auto checkName = [&n] (const String& type)
    {
        return ((n.startsWithIgnoreCase ("Folder ")
                 || n.startsWithIgnoreCase (TRANS(type) + " "))
                && n.substring (6).trim().containsOnly ("0123456789"));
    };

    if (checkName ("Folder") || checkName ("Submix"))
    {
        // For "Track 1" type names, leave the actual name empty.
        resetName();
    }
}

String FolderTrack::getName()
{
    auto n = Track::getName();

    if (n.isEmpty())
        n << (isSubmixFolder() ? TRANS("Submix")
                               : TRANS("Folder")) << ' ' << getFolderTrackNumber();

    return n;
}

int FolderTrack::getFolderTrackNumber() noexcept
{
    int result = 1;

    edit.visitAllTracksRecursive ([&] (Track& t)
    {
        if (this == &t)
            return false;

        if (t.isFolderTrack())
            ++result;

        return true;
    });

    return result;
}

bool FolderTrack::isSubmixFolder() const
{
    for (auto p : pluginList)
        if (dynamic_cast<VCAPlugin*> (p) == nullptr && dynamic_cast<TextPlugin*> (p) == nullptr)
            return true;

    return false;
}

bool FolderTrack::outputsToDevice (const OutputDevice& dev) const
{
    for (AudioTrack* t : getAllAudioSubTracks (true))
        if (t->createsOutput() && &dev == t->getOutput().getOutputDevice (false))
            return true;

    return false;
}

AudioNode* FolderTrack::createAudioNode (const CreateAudioNodeParams& params)
{
    CRASH_TRACER

    if (! isSubmixFolder())
        return {};

    juce::Array<Track*> allTracks (getAllTracks (edit));
    juce::Array<AudioTrack*> subTracks (getAllAudioSubTracks (false));

    juce::Array<FolderTrack*> subFolders;

    for (auto* t : getAllSubTracks (false))
        if (auto ft = dynamic_cast<FolderTrack*> (t))
            subFolders.add (ft);

    if (subTracks.isEmpty() && subFolders.isEmpty())
        return {};

    const bool use64Bit = (params.forRendering || edit.engine.getPropertyStorage().getProperty (SettingID::use64Bit, false))
                            && subTracks.size() > 1; // no point using 64 bit unless there's a lot of tracks to sum up

    const bool shouldUseMultiCPU = (subTracks.size() + subFolders.size()) > 1
                                    && edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() > 1;

    auto* mixer = new MixerAudioNode (use64Bit, shouldUseMultiCPU);

    // Create nodes for any submix tracks
    for (auto t : subFolders)
    {
        if (! t->isProcessing (true))
            continue;

        if (t->isSubmixFolder())
        {
            mixer->addInput (t->createAudioNode (params));
        }
        else
        {
            for (auto* at : t->getAllAudioSubTracks (false))
                if (params.allowedTracks == nullptr || (*params.allowedTracks)[allTracks.indexOf (at)])
                    mixer->addInput (at->createAudioNode (params));
        }
    }

    // Then add any audio tracks
    for (auto at : subTracks)
        if (params.allowedTracks == nullptr || (*params.allowedTracks)[allTracks.indexOf (at)])
            mixer->addInput (at->createAudioNode (params));

    // And finally the effects
    AudioNode* finalNode = mixer;

    if (finalNode != nullptr)
        finalNode = params.includePlugins ? pluginList.createAudioNode (finalNode, params.addAntiDenormalisationNoise)
                                          : finalNode;

    finalNode = new TrackMutingAudioNode (*this, finalNode, false);

    return finalNode;
}

bool FolderTrack::isMuted (bool includeMutingByDestination) const
{
    if (muted)
        return true;

    if (includeMutingByDestination)
        if (auto p = getParentFolderTrack())
            return p->isMuted (true);

    return false;
}

bool FolderTrack::isSolo (bool includeIndirectSolo) const
{
    if (soloed)
        return true;

    if (includeIndirectSolo)
    {
        // If any of the parent tracks are soloed, this needs to be indirectly soloed
        for (auto p = getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
            if (p->isSolo (false))
                return true;

        // If any sub-tracks are soloed, this needs to be indirectly soloed
        bool anySubTracksSolo = false;

        if (auto tl = getSubTrackList())
            tl->visitAllRecursive ([&anySubTracksSolo] (Track& t)
                                   {
                                       if (t.isSolo (false))
                                       {
                                           anySubTracksSolo = true;
                                           return false;
                                       }

                                       return true;
                                   });

        return anySubTracksSolo;
    }

    return false;
}

bool FolderTrack::isSoloIsolate (bool includeIndirectSolo) const
{
    if (soloIsolated)
        return true;

    if (includeIndirectSolo)
    {
        // If any of the parent tracks are solo isolate, this needs to be indirectly solo isolate
        for (auto p = getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
            if (p->isSoloIsolate (false))
                return true;

        // If any sub-tracks are solo isoloate, this needs to be indirectly solo isolate
        bool anySubTracksSolo = false;

        if (auto tl = getSubTrackList())
            tl->visitAllRecursive ([&anySubTracksSolo] (Track& t)
                                   {
                                       if (t.isSoloIsolate (false))
                                       {
                                           anySubTracksSolo = true;
                                           return false;
                                       }

                                       return true;
                                   });

        return anySubTracksSolo;
    }

    return false;
}

float FolderTrack::getVcaDb (double time)
{
    if (auto ptr = vcaPlugin)
        if (ptr->isEnabled())
            return ptr->updateAutomationStreamAndGetVolumeDb (time);

    return 0.0f;
}

EditTimeRange FolderTrack::getClipExtendedBounds (Clip& c)
{
    if (c.isGrouped())
        if (auto cc = dynamic_cast<CollectionClip*> (c.getGroupParent()))
            return cc->getEditTimeRange();

    return c.getEditTimeRange();
}

void FolderTrack::generateCollectionClips (SelectionManager& sm)
{
    CRASH_TRACER

    if (dirtyClips)
    {
        dirtyClips = false;
        juce::Array<Clip*> selectedClips;

        for (int i = collectionClips.size(); --i >= 0;)
        {
            if (auto cc = collectionClips[i])
            {
                // find all the clips that are indirectly selected
                if (sm.isSelected (*cc) && ! cc->isDragging())
                {
                    sm.deselect (cc.get());
                    selectedClips.addArray (cc->getClips());
                }

                // don't recreate an collection clips that are being dragged
                if (! cc->isDragging())
                    collectionClips.remove (i);
            }
        }

        // any collection clips that remain, adjust their edges
        for (int i = 0; i < collectionClips.size(); ++i)
        {
            if (auto cc = collectionClips[i])
            {
                if (cc->getNumClips() > 0)
                {
                    EditTimeRange totalRange;
                    bool first = true;

                    for (int j = cc->getNumClips(); --j >= 0;)
                    {
                        auto c = cc->getClip(j);

                        if (c == nullptr || c->getTrack() == nullptr)
                        {
                            cc->removeClip (c.get());
                        }
                        else
                        {
                            auto pos = c->getPosition();

                            if (pos.getLength() > 0.000001)
                            {
                                if (first)
                                    totalRange = pos.time;
                                else
                                    totalRange = totalRange.getUnionWith (pos.time);

                                first = false;
                            }
                        }
                    }

                    cc->range = totalRange;
                }
            }
        }

        juce::Array<Clip*> clips;

        // find all the clips on sub tracks
        for (auto at : getAllAudioSubTracks (true))
            clips.addArray (at->getClips());

        TrackItemStartTimeSorter sorter;
        clips.sort (sorter);

        // remove any that are already in a collection clip
        for (int i = clips.size(); --i >= 0;)
        {
            auto c = clips.getUnchecked(i);

            for (auto cc : collectionClips)
            {
                if (cc->containsClip (c))
                {
                    clips.remove (i);
                    break;
                }
            }
        }

        // start recreating collection clips
        CollectionClip* colClip = nullptr;

        for (auto clip : clips)
        {
            auto bounds = getClipExtendedBounds (*clip);

            if (bounds.getLength() > 0.000001)
            {
                if (colClip == nullptr || bounds.getStart() + 0.000001 >= colClip->getPosition().getEnd())
                {
                    colClip = new CollectionClip (*this);
                    colClip->range = bounds;
                    colClip->addClip (clip);

                    collectionClips.add (colClip);
                }
                else
                {
                    if (bounds.getEnd() > colClip->getPosition().getEnd())
                        colClip->range.end = bounds.getEnd();

                    colClip->addClip (clip);
                }
            }
        }

        // select collection clips that contain at least one clip that was indirectly selected before
        for (auto cc : collectionClips)
            for (auto c : selectedClips)
                if (cc->containsClip (c))
                    sm.addToSelection (cc);
    }
    else
    {
        for (auto cc : collectionClips)
        {
            jassert (Selectable::isSelectableValid (cc));

            EditTimeRange totalRange;
            bool first = true;

            for (auto* c : cc->getClips())
            {
                auto bounds = getClipExtendedBounds (*c);

                if (bounds.getLength() > 0.000001)
                {
                    if (first)
                        totalRange = bounds;
                    else
                        totalRange = totalRange.getUnionWith (bounds);

                    first = false;
                }
            }

            cc->range = totalRange;
        }
    }
}

CollectionClip* FolderTrack::getCollectionClip (int index) const noexcept
{
    return collectionClips[index].get();
}

int FolderTrack::getNumCollectionClips()  const noexcept
{
    return collectionClips.size();
}

int FolderTrack::indexOfCollectionClip (CollectionClip* c) const
{
    return collectionClips.indexOf (c);
}

int FolderTrack::getIndexOfNextCollectionClipAt (double time)
{
    return findIndexOfNextItemAt (collectionClips, time);
}

CollectionClip* FolderTrack::getNextCollectionClipAt (double time)
{
    return collectionClips[getIndexOfNextCollectionClipAt (time)].get();
}

bool FolderTrack::contains (CollectionClip* clip) const
{
    return collectionClips.contains (clip);
}

int FolderTrack::indexOfTrackItem (TrackItem* ti) const
{
    return indexOfCollectionClip (dynamic_cast<CollectionClip*> (ti));
}

int FolderTrack::getNumTrackItems() const
{
    return getNumCollectionClips();
}

int FolderTrack::getIndexOfNextTrackItemAt (double time)
{
    return getIndexOfNextCollectionClipAt (time);
}

TrackItem* FolderTrack::getTrackItem (int idx) const
{
    return getCollectionClip (idx);
}

TrackItem* FolderTrack::getNextTrackItemAt (double time)
{
    return getNextCollectionClipAt (time);
}

VCAPlugin* FolderTrack::getVCAPlugin()                  { return pluginList.findFirstPluginOfType<VCAPlugin>(); }
VolumeAndPanPlugin* FolderTrack::getVolumePlugin()      { return pluginList.findFirstPluginOfType<VolumeAndPanPlugin>(); }

void FolderTrack::setDirtyClips()
{
    dirtyClips = true;

    if (auto p = getParentFolderTrack())
        p->setDirtyClips();
}

bool FolderTrack::isFrozen (FreezeType t) const
{
    for (auto at : getAllAudioSubTracks (true))
        if (at->isFrozen (t))
            return true;

    return false;
}

bool FolderTrack::canContainPlugin (Plugin* p) const
{
    return dynamic_cast<FreezePointPlugin*> (p) == nullptr;
}

bool FolderTrack::willAcceptPlugin (Plugin& p)
{
    if (! canContainPlugin (&p))
        return false;

    if (dynamic_cast<TextPlugin*> (&p) != nullptr)
        return true;

    if (dynamic_cast<VCAPlugin*> (&p) != nullptr)
        return getVCAPlugin() == nullptr;

    if (! isSubmixFolder())
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("Converted to submix track"));

    return true;
}

void FolderTrack::setMute (bool b)          { muted = b; }
void FolderTrack::setSolo (bool b)          { soloed = b; }
void FolderTrack::setSoloIsolate (bool b)   { soloIsolated = b; }

void FolderTrack::updatePlugins()
{
    vcaPlugin = pluginList.findFirstPluginOfType<VCAPlugin>();
    volumePlugin = pluginList.findFirstPluginOfType<VolumeAndPanPlugin>();
}

void FolderTrack::valueTreeChildAdded (ValueTree& p, ValueTree& c)
{
    if (c.hasType (IDs::PLUGIN))
        pluginUpdater.triggerAsyncUpdate();

    Track::valueTreeChildAdded (p, c);
}

void FolderTrack::valueTreeChildRemoved (ValueTree& p, ValueTree& c, int index)
{
    if (c.hasType (IDs::PLUGIN))
        pluginUpdater.triggerAsyncUpdate();

    Track::valueTreeChildRemoved (p, c, index);
}

void FolderTrack::valueTreeChildOrderChanged (ValueTree& p, int oldIndex, int newIndex)
{
    if (p.getChild (oldIndex).hasType (IDs::PLUGIN) || p.getChild (newIndex).hasType (IDs::PLUGIN))
        pluginUpdater.triggerAsyncUpdate();

    Track::valueTreeChildOrderChanged (p, oldIndex, newIndex);
}
