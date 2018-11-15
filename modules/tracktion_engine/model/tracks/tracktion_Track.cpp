/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


Track::Track (Edit& edit, const ValueTree& v, double defaultHeight, double minHeight, double maxHeight)
    : EditItem (EditItemID::readOrCreateNewID (edit, v), edit),
      MacroParameterElement (edit, v), // TODO: @Dave - this dumps an XML element in every track, including tempo, marker, etc - is that needed?
      defaultTrackHeight (defaultHeight),
      minTrackHeight (minHeight),
      maxTrackHeight (maxHeight),
      state (v),
      pluginList (edit)
{
    edit.trackCache.addItem (*this);
    auto um = &edit.getUndoManager();

    trackName.referTo (state, IDs::name, um, {});
    colour.referTo (state, IDs::colour, um, Colour());
    tags.referTo (state, IDs::tags, um);
    hidden.referTo (state, IDs::hidden, um);
    processing.referTo (state, IDs::process, um, true);

    currentAutoParamPlugin.referTo (state, IDs::currentAutoParamPluginID, um, EditItemID());
    currentAutoParamID.referTo (state, IDs::currentAutoParamTag, um, {});

    modifierList = std::make_unique<ModifierList> (edit, state.getOrCreateChildWithName (IDs::MODIFIERS, um));

    state.addListener (this);
}

Track::~Track()
{
    edit.trackCache.removeItem (*this);
    cachedParentFolderTrack = nullptr;
    masterReference.clear();
    state.removeListener (this);

    pluginList.releaseObjects();
    currentAutoParam = nullptr;
}

void Track::flushStateToValueTree()
{
    for (auto p : getAllPlugins())
        p->flushPluginStateToValueTree();
}

void Track::initialise()
{
    CRASH_TRACER

    auto um = &edit.getUndoManager();

    if (canShowImage())
    {
        imageIdOrData.referTo (state, IDs::imageIdOrData, um, {});

        if (imageIdOrData.get().isNotEmpty())
            imageChanged = true;
    }

    tagsArray = StringArray::fromTokens (tags.get().replace ("_", " "), "|", "\"");

    pluginList.setTrackAndClip (this, nullptr);

    if (canContainPlugins())
        pluginList.initialise (state);

    updateTrackList();
    updateCachedParent();
}

void Track::setName (const String& n)
{
    auto newName = n.substring (0, 64);

    if (trackName != newName)
        trackName = newName;
}

void Track::resetName()
{
    trackName.resetToDefault();
}

int Track::getIndexInEditTrackList() const
{
    int index = 0, result = -1;

    edit.visitAllTracksRecursive ([&] (Track& t)
    {
        if (this == &t)
        {
            result = index;
            return false;
        }

        ++index;
        return true;
    });

    return result;
}

juce::Array<AudioTrack*> Track::getAllAudioSubTracks (bool recursive) const
{
    juce::Array<AudioTrack*> results;

    if (trackList != nullptr)
    {
        trackList->visitAllTracks ([&] (Track& t)
                                   {
                                       if (auto at = dynamic_cast<AudioTrack*> (&t))
                                           results.addIfNotAlreadyThere (at);

                                       return true;
                                   }, recursive);
    }

    return results;
}

juce::Array<Track*> Track::getAllSubTracks (bool recursive) const
{
    juce::Array<Track*> results;

    if (trackList != nullptr)
        trackList->visitAllTracks ([&] (Track& t) { results.add (&t); return true; }, recursive);

    return results;
}

Clip* Track::findClipForID (EditItemID id) const
{
    for (auto track : getAllSubTracks (false))
        if (auto c = track->findClipForID (id))
            return c;

    return {};
}

Track* Track::getSiblingTrack (int delta, bool keepWithinSameParent) const
{
    juce::Array<Track*> tracks;

    if (keepWithinSameParent)
    {
        if (auto ft = getParentFolderTrack())
            tracks = ft->getAllSubTracks (false);
        else
            tracks = getTopLevelTracks (edit);
    }
    else
    {
        tracks = getAllTracks (edit);
    }

    auto index = tracks.indexOf (const_cast<Track*> (this));
    jassert (index >= 0);

    return tracks[index + delta];
}

//==============================================================================
bool Track::isProcessing (bool includeParents) const
{
    if (includeParents)
        for (auto p = getParentTrack(); p != nullptr; p = p->getParentTrack())
            if (! p->processing)
                return false;

    return processing;
}

bool Track::isTrackAudible (bool areAnyTracksSolo) const
{
    if (areAnyTracksSolo && ! (isSolo (true) || isSoloIsolate (true)))
        return false;

    return ! isMuted (true);
}

void Track::updateAudibility (bool areAnyTracksSolo)
{
    isAudible = isTrackAudible (areAnyTracksSolo);
}

int Track::getTrackDepth() const
{
    int depth = 0;

    for (auto p = getParentTrack(); p != nullptr; p = p->getParentTrack())
        ++depth;

    return depth;
}

bool Track::isPartOfSubmix() const
{
    for (auto p = getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
        if (p->isSubmixFolder())
            return true;

    return false;
}

Track::MuteAndSoloLightState Track::getMuteAndSoloLightState() const
{
    int flags = 0;

    if (isMuted (false))
        flags |= muteLit;
    else if (isMuted (true))
        flags |= muteFlashing;

    if (isSolo (false))
        flags |= soloLit;
    else if (isSolo (true))
        flags |= soloFlashing;
    else if (isSoloIsolate (false))
    {
        flags |= soloIsolate;

        if (! isMuted (true) && edit.areAnyTracksSolo())
            flags |= soloLit;
    }
    else if (isSoloIsolate (true) && edit.areAnyTracksSolo())
    {
        flags |= soloIsolate;

        if (! isMuted (true))
            flags |= soloFlashing;
    }

    return (MuteAndSoloLightState) flags;
}

bool Track::containsPlugin (const Plugin* p) const
{
    return pluginList.contains (p);
}

bool Track::hasFreezePointPlugin() const
{
    for (auto p : pluginList)
        if (p->getPluginType() == "freezePoint")
            return true;

    return false;
}

juce::Array<AutomatableEditItem*> Track::getAllAutomatableEditItems() const
{
    juce::Array<AutomatableEditItem*> destArray;
    auto plugins = getAllPlugins();
    destArray.addArray (plugins);

    for (auto m : modifierList->getModifiers())
        if (auto aei = dynamic_cast<AutomatableEditItem*> (m))
            destArray.add (aei);

    for (auto p : plugins)
        destArray.add (&p->macroParameterList);

    return destArray;
}

Plugin::Array Track::getAllPlugins() const
{
    return pluginList.getPlugins();
}

void Track::sendMirrorUpdateToAllPlugins (Plugin& p) const
{
    pluginList.sendMirrorUpdateToAllPlugins (p);
}

void Track::flipAllPluginsEnablement()
{
    auto isVolPan = [] (Plugin* p) { return dynamic_cast<VolumeAndPanPlugin*> (p) != nullptr; };

    auto areAnyOn = [&isVolPan] (const PluginList& pl) -> bool
    {
        for (auto p : pl)
            if (! isVolPan (p) && p->canBeDisabled() && p->isEnabled())
                return true;

        return false;
    };

    const bool enable = ! areAnyOn (pluginList);

    for (auto p : pluginList)
        if (! isVolPan (p) && p->canBeDisabled())
            p->setEnabled (enable);
}

//==============================================================================
Array<AutomatableParameter*> Track::getAllAutomatableParams() const
{
    juce::Array<AutomatableParameter*> params;

    for (auto p : getAllPlugins())
    {
        for (int j = 0; j < p->getNumAutomatableParameters(); ++j)
            params.add (p->getAutomatableParameter (j).get());

        params.addArray (p->macroParameterList.getMacroParameters());
    }

    for (auto m : modifierList->getModifiers())
        params.addArray (m->getAutomatableParameters());

    return params;
}

static AutomatableParameter::Ptr findAutomatableParam (Edit& edit, EditItemID pluginID, const String& paramID)
{
    if (pluginID.isValid() && paramID.isNotEmpty())
    {
        juce::Array<AutomatableParameter*> list (edit.getAllAutomatableParams (false));

        for (auto p : list)
            if (p->getOwnerID() == pluginID && p->paramID == paramID)
                return p;

        if (auto p = edit.getPluginCache().getPluginFor (pluginID))
        {
            p->initialiseFully();

            if (auto ap = p->getAutomatableParameterByID (paramID))
                return ap;

            // Check for param name as a fallback for old Edits
            for (auto ap : p->getAutomatableParameters())
                if (ap->paramName == paramID)
                    return ap;

            return {};
        }

        for (auto m : getAllModifiers (edit))
            if (auto aei = dynamic_cast<AutomatableEditItem*> (m))
                for (auto p : aei->getAutomatableParameters())
                    if (p->getOwnerID() == pluginID && p->paramID == paramID)
                        return p;

        for (auto mpl : getAllMacroParameterLists (edit))
            for (auto* p : mpl->getMacroParameters())
                if (p->getOwnerID() == pluginID && p->paramID == paramID)
                    return p;
    }

    return {};
}

void Track::refreshCurrentAutoParam()
{
    currentAutoParamPlugin.forceUpdateOfCachedValue();
    currentAutoParamID.forceUpdateOfCachedValue();

    if (setIfDifferent (currentAutoParam, findAutomatableParam (edit, currentAutoParamPlugin.get(),
                                                                currentAutoParamID.get()).get()))
        changed();
}

void Track::setCurrentlyShownAutoParam (const AutomatableParameter::Ptr& param)
{
    currentAutoParamPlugin  = param == nullptr ? EditItemID() : param->getOwnerID();
    currentAutoParamID      = param == nullptr ? String()     : param->paramID;
}

AutomatableParameter* Track::chooseDefaultAutomationCurve() const
{
    auto allParams (getAllAutomatableParams());

    for (int i = allParams.size(); --i >= 0;)
        if (auto p = allParams.getUnchecked (i))
            if (p->hasAutomationPoints())
                return p;

    // otherwise look for the volume parameter
    for (int i = allParams.size(); --i >= 0;)
        if (auto p = allParams.getUnchecked (i))
            if (dynamic_cast<VolumeAndPanPlugin*> (p->getPlugin()) != nullptr)
                if (p->getParameterName().containsIgnoreCase (TRANS("Volume")))
                    return p;

    return allParams[0];
}

void Track::hideAutomatableParametersForSource (EditItemID pluginOrParameterID)
{
    if (currentAutoParamPlugin == pluginOrParameterID)
    {
        if (isAutomationTrack())
            edit.deleteTrack (this);
        else
            setCurrentlyShownAutoParam ({});
    }
}

ValueTree Track::getParentTrackTree() const
{
    auto parent = state.getParent();

    if (TrackList::isTrack (parent))
        return parent;

    return {};
}

bool Track::isAChildOf (const Track& t) const
{
    for (auto p = getParentTrack(); p != nullptr; p = p->getParentTrack())
        if (p == &t)
            return true;

    return false;
}

void Track::insertSpaceIntoTrack (double time, double amountOfSpace)
{
    // shift up any automation curves too..
    for (auto p : pluginList)
    {
        for (int j = p->getNumAutomatableParameters(); --j >= 0;)
        {
            if (auto param = p->getAutomatableParameter(j))
            {
                auto& curve = param->getCurve();

                for (int k = curve.getNumPoints(); --k >= 0;)
                    if (curve.getPointTime (k) >= time)
                        curve.movePoint (k,
                                         curve.getPointTime (k) + amountOfSpace,
                                         curve.getPointValue (k), false);
            }
        }
    }
}

void Track::updateTrackList()
{
    CRASH_TRACER

    if (TrackList::hasAnySubTracks (state))
    {
        if (trackList == nullptr)
            trackList = new TrackList (edit, state);
    }
    else
    {
        trackList = nullptr;
    }
}

void Track::updateCachedParent()
{
    CRASH_TRACER
    auto parent = getParentTrackTree();

    if (parent.isValid())
    {
        if (auto t = edit.trackCache.findItem (EditItemID::fromID (parent)))
            cachedParentTrack = t;
    }
    else
    {
        cachedParentTrack = nullptr;
    }

    auto newFolder = dynamic_cast<FolderTrack*> (cachedParentTrack.get());

    if (cachedParentFolderTrack != newFolder)
    {
        if (cachedParentFolderTrack != nullptr)
            cachedParentFolderTrack->setDirtyClips();

        if (newFolder != nullptr)
            newFolder->setDirtyClips();
    }

    cachedParentFolderTrack = newFolder;
}

//==============================================================================
bool Track::canShowImage() const
{
    return isAudioTrack() || isFolderTrack();
}

void Track::setTrackImage (const String& idOrData)
{
    if (canShowImage())
        imageIdOrData = idOrData;
}

bool Track::imageHasChanged()
{
    const ScopedValueSetter<bool> svs (imageChanged, imageChanged, false);
    return imageChanged;
}

void Track::setTags (const StringArray& s)
{
    tags = s.joinIntoString ("|").replace (" ", "_");
}

void Track::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::tags)
        {
            tags.forceUpdateOfCachedValue();
            tagsArray = StringArray::fromTokens (tags.get().replace ("_", " "), "|", "\"");
            changed();
        }
        else if (i == IDs::name)
        {
            changed();
            MessageManager::callAsync ([] { SelectionManager::refreshAllPropertyPanels(); });
        }
        else if (i == IDs::colour)
        {
            changed();
        }
        else if (i == IDs::imageIdOrData)
        {
            imageChanged = true;
            changed();
        }
        else if (i == IDs::currentAutoParamPluginID || i == IDs::currentAutoParamTag)
        {
            refreshCurrentAutoParam();
        }
        else if (i == IDs::expanded)
        {
            changed();
        }
        else if (i == IDs::process)
        {
            juce::Array<Track*> tracks;
            tracks.add (this);
            tracks.addArray (getAllSubTracks (true));

            for (auto t : tracks)
                for (auto p : t->getAllPlugins())
                    p->setProcessingEnabled (p->getOwnerTrack()->isProcessing (true));
        }
    }
}

void Track::valueTreeChildAdded (ValueTree& p, ValueTree& c)
{
    if (p == state)
        if (TrackList::isTrack (c))
            updateTrackList();
}

void Track::valueTreeChildRemoved (ValueTree& p, ValueTree& c, int)
{
    if (p == state)
        if (TrackList::isTrack (c))
            updateTrackList();
}

void Track::valueTreeParentChanged (ValueTree& v)
{
    if (v == state)
        updateCachedParent();
}
