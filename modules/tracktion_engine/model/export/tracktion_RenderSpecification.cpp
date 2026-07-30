/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

namespace render_spec_utils
{
    inline constexpr double maxWrapRemainderTailSeconds = 30.0;

    static juce::AudioFormat* getFormat (Engine& engine, const juce::String& format)
    {
        auto& affm = engine.getAudioFileFormatManager();

        if (format == "wav")    return affm.getWavFormat();
        if (format == "aiff")   return affm.getAiffFormat();
        if (format == "flac")   return affm.getFlacFormat();
        if (format == "ogg")    return affm.getOggFormat();
        if (format == "mp3")    return affm.getLameFormat();

        return nullptr;
    }

    static bool isKnownChannelLayout (const juce::String& layout)
    {
        return layout.isEmpty() || layout == "mono" || layout == "stereo"
                || layout == "5.1" || layout == "7.1";
    }

    /** Finds the tracks for a set of IDs along with their indexes in the
        Edit's track list, preserving the order the IDs were given in.
    */
    static juce::Array<std::pair<Track*, int>> resolveTracks (Edit& edit, const juce::Array<EditItemID>& ids)
    {
        juce::Array<std::pair<Track*, int>> result;
        auto allTracks = getAllTracks (edit);

        for (auto id : ids)
        {
            for (int i = 0; i < allTracks.size(); ++i)
            {
                if (allTracks[i]->itemID == id)
                {
                    result.add ({ allTracks[i], i });
                    break;
                }
            }
        }

        return result;
    }

    /** Like RenderOptions::findEndAllowance but substitutes the wrap-remainder
        cap for infinite tail reports rather than ignoring them, and caps the
        result so runaway feedback can't stall a render.
    */
    static TimeDuration findWrapRemainderTail (Edit& edit, const juce::Array<EditItemID>& trackIDs)
    {
        Plugin::Array plugins;

        for (auto t : getAllTracks (edit))
            if (trackIDs.isEmpty() || trackIDs.contains (t->itemID))
                for (auto p : t->pluginList)
                    plugins.addIfNotAlreadyThere (p);

        double tail = 0.0;

        for (auto p : plugins)
            tail = std::max (tail, std::min (p->getTailLength(), maxWrapRemainderTailSeconds));

        return TimeDuration::fromSeconds (tail);
    }
}

//==============================================================================
juce::var RenderSpecification::toJSON() const
{
    auto obj = new juce::DynamicObject();

    obj->setProperty ("tracks", EditItemID::listToString (tracks));
    obj->setProperty ("mutedTracks", EditItemID::listToString (mutedTracks));
    obj->setProperty ("includeSourceTracks", includeSourceTracks);

    if (time)
    {
        obj->setProperty ("startTime", time->getStart().inSeconds());
        obj->setProperty ("endTime", time->getEnd().inSeconds());
    }

    obj->setProperty ("wrapRemainder", wrapRemainder);
    obj->setProperty ("destination", destination.getFullPathName());
    obj->setProperty ("format", format);
    obj->setProperty ("sampleRate", sampleRate);
    obj->setProperty ("bitDepth", bitDepth);
    obj->setProperty ("quality", quality);
    obj->setProperty ("channelLayout", channelLayout);
    obj->setProperty ("normalise", normalise);
    obj->setProperty ("normaliseByRMS", normaliseByRMS);
    obj->setProperty ("normaliseToLevelDb", normaliseToLevelDb);
    obj->setProperty ("trimSilence", trimSilence);
    obj->setProperty ("dither", dither);
    obj->setProperty ("realTime", realTime);
    obj->setProperty ("usePlugins", usePlugins);
    obj->setProperty ("useMasterPlugins", useMasterPlugins);

    if (metadata.size() > 0)
    {
        auto metaObj = new juce::DynamicObject();

        for (int i = 0; i < metadata.size(); ++i)
            metaObj->setProperty (metadata.getAllKeys()[i], metadata.getAllValues()[i]);

        obj->setProperty ("metadata", juce::var (metaObj));
    }

    return juce::var (obj);
}

RenderSpecification RenderSpecification::fromJSON (const juce::var& v, juce::StringArray* unknownKeys)
{
    static const juce::StringArray knownKeys { "tracks", "mutedTracks", "includeSourceTracks", "startTime", "endTime",
                                              "wrapRemainder", "destination", "format", "sampleRate",
                                              "bitDepth", "quality", "channelLayout", "normalise",
                                              "normaliseByRMS", "normaliseToLevelDb", "trimSilence",
                                              "dither", "realTime", "usePlugins", "useMasterPlugins",
                                              "metadata" };
    RenderSpecification spec;
    auto obj = v.getDynamicObject();

    if (obj == nullptr)
        return spec;

    if (unknownKeys != nullptr)
        for (auto& prop : obj->getProperties())
            if (! knownKeys.contains (prop.name.toString()))
                unknownKeys->add (prop.name.toString());

    auto get = [obj] (const char* key, juce::var defaultValue)
    {
        auto value = obj->getProperty (key);
        return value.isVoid() ? defaultValue : value;
    };

    spec.tracks             = EditItemID::parseStringList (get ("tracks", juce::String()));
    spec.mutedTracks        = EditItemID::parseStringList (get ("mutedTracks", juce::String()));
    spec.includeSourceTracks = get ("includeSourceTracks", spec.includeSourceTracks);
    spec.wrapRemainder      = get ("wrapRemainder", spec.wrapRemainder);
    spec.destination        = juce::File (get ("destination", juce::String()).toString());
    spec.format             = get ("format", spec.format);
    spec.sampleRate         = get ("sampleRate", spec.sampleRate);
    spec.bitDepth           = get ("bitDepth", spec.bitDepth);
    spec.quality            = get ("quality", spec.quality);
    spec.channelLayout      = get ("channelLayout", spec.channelLayout);
    spec.normalise          = get ("normalise", spec.normalise);
    spec.normaliseByRMS     = get ("normaliseByRMS", spec.normaliseByRMS);
    spec.normaliseToLevelDb = get ("normaliseToLevelDb", spec.normaliseToLevelDb);
    spec.trimSilence        = get ("trimSilence", spec.trimSilence);
    spec.dither             = get ("dither", spec.dither);
    spec.realTime           = get ("realTime", spec.realTime);
    spec.usePlugins         = get ("usePlugins", spec.usePlugins);
    spec.useMasterPlugins   = get ("useMasterPlugins", spec.useMasterPlugins);

    if (obj->hasProperty ("startTime") || obj->hasProperty ("endTime"))
        spec.time = TimeRange (TimePosition::fromSeconds (static_cast<double> (get ("startTime", 0.0))),
                               TimePosition::fromSeconds (static_cast<double> (get ("endTime", 0.0))));

    if (auto metaObj = obj->getProperty ("metadata").getDynamicObject())
        for (auto& prop : metaObj->getProperties())
            spec.metadata.set (prop.name.toString(), prop.value.toString());

    return spec;
}

//==============================================================================
juce::Result validateRenderSpecification (Edit& edit, const RenderSpecification& spec)
{
    using namespace render_spec_utils;

    if (spec.destination == juce::File())
        return juce::Result::fail (TRANS("No destination has been set"));

    if (spec.destination.isDirectory())
        return juce::Result::fail (TRANS("The destination must be a file, not a directory"));

    if (getFormat (edit.engine, spec.format) == nullptr)
        return juce::Result::fail (spec.format == "mp3" ? TRANS("MP3 encoding is not available")
                                                        : TRANS("Unknown format: ") + spec.format);

    if (spec.sampleRate < 8000.0 || spec.sampleRate > 384000.0)
        return juce::Result::fail (TRANS("Invalid sample rate"));

    if (spec.bitDepth != 16 && spec.bitDepth != 24 && spec.bitDepth != 32)
        return juce::Result::fail (TRANS("Invalid bit depth"));

    if (! isKnownChannelLayout (spec.channelLayout))
        return juce::Result::fail (TRANS("Unknown channel layout: ") + spec.channelLayout);

    if (resolveTracks (edit, spec.tracks).size() != spec.tracks.size()
         || resolveTracks (edit, spec.mutedTracks).size() != spec.mutedTracks.size())
        return juce::Result::fail (TRANS("The specification contains tracks which aren't in this Edit"));

    if (spec.time ? spec.time->isEmpty() : (edit.getLength() == TimeDuration()))
        return juce::Result::fail (TRANS("There is nothing to render in the time range"));

    return juce::Result::ok();
}

//==============================================================================
juce::Array<EditItemID> findStemSourceTracks (Edit& edit, const juce::Array<EditItemID>& stemTracks)
{
    // The tracks whose processing runs as part of the stem: the given tracks
    // plus everything nested inside any folders among them
    juce::Array<Track*> stemSet;

    for (auto id : stemTracks)
    {
        if (auto track = findTrackForID (edit, id))
        {
            stemSet.addIfNotAlreadyThere (track);

            for (auto sub : track->getAllSubTracks (true))
                stemSet.addIfNotAlreadyThere (sub);
        }
    }

    juce::Array<Track*> sources, visited;
    auto toVisit = stemSet;

    auto addSource = [&] (Track* source)
    {
        if (source != nullptr && ! stemSet.contains (source) && ! sources.contains (source))
        {
            sources.add (source);
            toVisit.add (source);
        }
    };

    // Fixpoint: tracks added as sources are visited in turn, so sources of
    // sources are found; the visited list breaks feedback cycles
    while (! toVisit.isEmpty())
    {
        auto track = toVisit.removeAndReturn (toVisit.size() - 1);

        if (visited.contains (track))
            continue;

        visited.add (track);

        // A folder's children process along with it, so their dependencies
        // count too, but muting the folder alone silences the whole subtree
        for (auto sub : track->getAllSubTracks (true))
            toVisit.add (sub);

        for (auto p : track->getAllPlugins())
        {
            if (auto sourceID = p->getSidechainSourceID(); sourceID.isValid())
                addSource (findTrackForID (edit, sourceID));

            // An aux return receives from every send on its bus
            if (auto auxReturn = dynamic_cast<AuxReturnPlugin*> (p))
                for (auto t : getAllTracks (edit))
                    for (auto p2 : t->getAllPlugins())
                        if (auto send = dynamic_cast<AuxSendPlugin*> (p2))
                            if (send->getBusNumber() == auxReturn->busNumber.get())
                                addSource (t);

            // A rack is processed once, fed by all of its instances, so the
            // stem needs the tracks hosting the rack's other instances
            if (auto rackInstance = dynamic_cast<RackInstance*> (p))
                if (rackInstance->type != nullptr)
                    for (auto other : getRackInstancesInEditForType (*rackInstance->type))
                        if (other != rackInstance)
                            addSource (other->getOwnerTrack());
        }
    }

    juce::Array<EditItemID> result;

    for (auto t : sources)
        result.add (t->itemID);

    return result;
}

//==============================================================================
std::optional<PlannedRenderJob> createRenderJob (Edit& edit, const RenderSpecification& spec)
{
    using namespace render_spec_utils;

    if (validateRenderSpecification (edit, spec).failed())
        return {};

    Renderer::Parameters params (edit);
    params.audioFormat          = getFormat (edit.engine, spec.format);
    params.bitDepth             = spec.bitDepth;
    params.sampleRateForAudio   = spec.sampleRate;
    params.quality              = spec.quality;
    params.time                 = spec.time.value_or (TimeRange (TimePosition(), toPosition (edit.getLength())));
    params.shouldNormalise      = spec.normalise;
    params.shouldNormaliseByRMS = spec.normaliseByRMS;
    params.normaliseToLevelDb   = spec.normaliseToLevelDb;
    params.trimSilenceAtEnds    = spec.trimSilence && ! spec.wrapRemainder;
    params.ditheringEnabled     = spec.dither;
    params.realTimeRender       = spec.realTime;
    params.usePlugins           = spec.usePlugins;
    params.useMasterPlugins     = spec.useMasterPlugins;
    params.metadata             = spec.metadata;
    params.canRenderInMono      = false;
    params.destFile             = spec.destination;

    if (spec.channelLayout == "mono")           params.mustRenderInMono = true;
    else if (spec.channelLayout == "stereo")    params.channelConfig = ChannelConfiguration::stereo();
    else if (spec.channelLayout == "5.1")       params.channelConfig = ChannelConfiguration::surround5_1();
    else if (spec.channelLayout == "7.1")       params.channelConfig = ChannelConfiguration::surround7_1();

    if (spec.wrapRemainder)
    {
        params.wrapRemainder = true;
        params.endAllowance = findWrapRemainderTail (edit, spec.tracks);
    }

    auto resolved = resolveTracks (edit, spec.tracks);

    for (auto [track, index] : resolved)
        params.tracksToDo.setBit (index);

    auto mutedTracks = spec.mutedTracks;

    if (spec.includeSourceTracks && ! spec.tracks.isEmpty())
    {
        // The solver finds the tracks this stem depends on for its sound;
        // they render muted just like the explicitly-listed ones
        auto seed = spec.tracks;
        seed.addArray (spec.mutedTracks);

        for (auto id : findStemSourceTracks (edit, seed))
            mutedTracks.addIfNotAlreadyThere (id);
    }

    // Muted tracks are part of the graph so they can feed the rendered tracks'
    // processing; the queue mutes them in the Edit while the job runs, and the
    // graph is told to keep them processing despite the mute
    if (! mutedTracks.isEmpty() && ! params.tracksToDo.isZero())
    {
        for (auto [track, index] : resolveTracks (edit, mutedTracks))
            params.tracksToDo.setBit (index);

        params.tracksToProcessWhileMuted = mutedTracks;
    }

    auto name = resolved.size() == 1 ? resolved.getFirst().first->getName()
                                     : spec.destination.getFileNameWithoutExtension();

    return PlannedRenderJob { name, std::move (params), mutedTracks };
}

std::vector<RenderSpecification> createPerTrackSpecifications (Edit& edit,
                                                               const RenderSpecification& base,
                                                               const juce::File& directory)
{
    using namespace render_spec_utils;

    auto format = getFormat (edit.engine, base.format);

    if (format == nullptr)
        return {};

    const auto extension = format->getFileExtensions()[0];

    auto tracksAndIndexes = resolveTracks (edit, base.tracks);

    if (tracksAndIndexes.isEmpty() && base.tracks.isEmpty())
        for (auto at : getAudioTracks (edit))
            tracksAndIndexes.add ({ at, getAllTracks (edit).indexOf (at) });

    std::vector<RenderSpecification> specs;
    juce::StringArray usedNames;

    for (auto [track, index] : tracksAndIndexes)
    {
        // Uniquify against the other tracks in this batch as well as the disk,
        // so identically-named tracks don't collide on the same file
        auto name = juce::File::createLegalFileName (track->getName());

        for (int suffix = 2; usedNames.contains (name); ++suffix)
            name = juce::File::createLegalFileName (track->getName()) + " " + juce::String (suffix);

        usedNames.add (name);

        auto spec = base;
        spec.tracks = { track->itemID };
        spec.destination = directory.getNonexistentChildFile (name, extension, false);
        specs.push_back (std::move (spec));
    }

    return specs;
}

} // namespace tracktion::inline engine
