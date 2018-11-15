/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


FreezePointPlugin::ScopedTrackUnsoloer::ScopedTrackUnsoloer (Edit& e)  : edit (e)
{
    int i = 0;

    for (auto t : getAllTracks (edit))
    {
        if (t->isSolo (false))
        {
            soloed.setBit (i);
            t->setSolo (false);
        }

        if (t->isSoloIsolate (false))
        {
            soloIsolated.setBit (i);
            t->setSoloIsolate (false);
        }

        ++i;
    }
}

FreezePointPlugin::ScopedTrackUnsoloer::~ScopedTrackUnsoloer()
{
    int i = 0;

    for (auto t : getAllTracks (edit))
    {
        if (soloed[i])
            t->setSolo (true);

        if (soloIsolated[i])
            t->setSoloIsolate (true);

        ++i;
    }
}

//==============================================================================
FreezePointPlugin::ScopedTrackSoloIsolator::ScopedTrackSoloIsolator (Edit& e, Track::Array& trks)
    : edit (e), tracks (trks)
{
    int i = 0;

    for (auto t : tracks)
    {
        if (! t->isSoloIsolate (false))
        {
            notSoloIsolated.setBit (i);
            t->setSoloIsolate (true);
        }

        if (t->isMuted (false))
        {
            muted.setBit (i);
            t->setMute (false);
        }

        ++i;
    }
}

FreezePointPlugin::ScopedTrackSoloIsolator::~ScopedTrackSoloIsolator()
{
    int i = 0;

    for (auto t : tracks)
    {
        if (notSoloIsolated[i])
            t->setSoloIsolate (false);

        if (muted[i])
            t->setMute (true);

        ++i;
    }
}

//==============================================================================
FreezePointPlugin::ScopedPluginDisabler::ScopedPluginDisabler (Track& t, Range<int> pluginsToDisable)
  : track (t)
{
    indexes = pluginsToDisable.getIntersectionWith (Range<int> (0, track.pluginList.size()));

    int i = 0;

    for (auto p : track.pluginList)
        enabled.setBit (i++, p->isEnabled());

    enablePlugins (false);
}

FreezePointPlugin::ScopedPluginDisabler::~ScopedPluginDisabler()
{
    enablePlugins (true);
}

void FreezePointPlugin::ScopedPluginDisabler::enablePlugins (bool enable)
{
    int i = 0;

    for (auto p : track.pluginList)
    {
        if (indexes.contains (i))
            p->setEnabled (enable && enabled[i]);

        ++i;
    }
}

//==============================================================================
FreezePointPlugin::ScopedTrackFreezer::ScopedTrackFreezer (FreezePointPlugin& o)
    : owner (o), wasFrozen (o.isTrackFrozen())
{
    if (auto at = dynamic_cast<AudioTrack*> (owner.getOwnerTrack()))
        trackID = at->itemID;
}

FreezePointPlugin::ScopedTrackFreezer::~ScopedTrackFreezer()
{
    if (! wasFrozen && trackID.isValid())
        return;

    if (auto at = dynamic_cast<AudioTrack*> (owner.getOwnerTrack()))
        if (! at->isFrozen (Track::groupFreeze))
            if (trackID != at->itemID)
                if (at->edit.engine.getPropertyStorage().getProperty (SettingID::autoFreeze, 1))
                    at->freezeTrackAsync();

    owner.updateTrackFreezeStatus();
}

FreezePointPlugin::ScopedTrackFreezer* FreezePointPlugin::createTrackFreezer (const Plugin::Ptr& p)
{
    if (auto fp = dynamic_cast<FreezePointPlugin*> (p.get()))
        return new ScopedTrackFreezer (*fp);

    return {};
}

//==============================================================================
const char* FreezePointPlugin::xmlTypeName ("freezePoint");

FreezePointPlugin::FreezePointPlugin (PluginCreationInfo info) : Plugin (info)
{
}

FreezePointPlugin::~FreezePointPlugin()
{
    notifyListenersOfDeletion();
}

ValueTree FreezePointPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

void FreezePointPlugin::initialiseFully()
{
    Plugin::initialiseFully();
    updateTrackFreezeStatus();
}

void FreezePointPlugin::initialise (const PlaybackInitialisationInfo&) {}
void FreezePointPlugin::deinitialise() {}
void FreezePointPlugin::applyToBuffer (const AudioRenderContext&) {}

//==============================================================================
void FreezePointPlugin::updateTrackFreezeStatus()
{
    if (auto at = dynamic_cast<AudioTrack*> (getOwnerTrack()))
    {
        auto newIndex = at->pluginList.indexOf (this);

        if (lastFreezeIndex != newIndex)
        {
            if (lastFreezeIndex != -1
                 && isTrackFrozen()
                 && ! at->pluginList.getEdit().isLoading())
            {
                freezeTrack (false);
                at->freezeTrackAsync();
            }

            lastFreezeIndex = newIndex;
        }
    }
}

bool FreezePointPlugin::isTrackFrozen() const
{
    if (auto at = dynamic_cast<AudioTrack*> (getOwnerTrack()))
        return at->isFrozen (Track::individualFreeze);

    return false;
}

void FreezePointPlugin::freezeTrack (bool shouldBeFrozen)
{
    if (auto at = dynamic_cast<AudioTrack*> (getOwnerTrack()))
    {
        const AudioTrack::FreezePointRemovalInhibitor inhibitor (*at);
        at->setFrozen (shouldBeFrozen, Track::individualFreeze);
    }

    changed();
}
