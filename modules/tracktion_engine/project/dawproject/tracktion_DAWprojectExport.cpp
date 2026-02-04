/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine { namespace dawproject
{

//==============================================================================
DAWprojectExporter::DAWprojectExporter (Edit& e, const WriteOptions& opts)
    : edit (e), options (opts)
{
}

//==============================================================================
tl::expected<std::unique_ptr<juce::XmlElement>, juce::String> DAWprojectExporter::createProjectXml()
{
    auto project = createXmlElement (xml::Project);
    project->setAttribute (xml::version, "1.0");

    // Application element
    auto* app = addChildElement (*project, xml::Application);
    app->setAttribute (xml::name, options.applicationName);
    app->setAttribute (xml::version, options.applicationVersion.isEmpty() ? "1.0" : options.applicationVersion);

    // Transport element
    createTransport (*project);

    // Structure element
    createStructure (*project);

    // Arrangement element
    createArrangement (*project);

    return project;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> DAWprojectExporter::createMetadataXml()
{
    auto metadata = createXmlElement ("MetaData");

    auto editMetadata = edit.getEditMetadata();

    if (editMetadata.title.isNotEmpty())
        metadata->setAttribute ("Title", editMetadata.title);
    if (editMetadata.artist.isNotEmpty())
        metadata->setAttribute ("Artist", editMetadata.artist);
    if (editMetadata.album.isNotEmpty())
        metadata->setAttribute ("Album", editMetadata.album);
    if (editMetadata.genre.isNotEmpty())
        metadata->setAttribute ("Genre", editMetadata.genre);
    if (editMetadata.comment.isNotEmpty())
        metadata->setAttribute ("Comment", editMetadata.comment);

    return metadata;
}

//==============================================================================
juce::Result DAWprojectExporter::writeToFile (const juce::File& file)
{
    // Create the project XML
    auto projectXmlResult = createProjectXml();
    if (! projectXmlResult.has_value())
        return juce::Result::fail (projectXmlResult.error());

    auto& projectXml = *projectXmlResult.value();

    // Create metadata XML
    auto metadataXml = createMetadataXml();

    // Build the ZIP file
    juce::ZipFile::Builder zipBuilder;

    // Add project.xml
    auto projectXmlString = projectXml.toString();
    auto projectXmlData = juce::MemoryBlock (projectXmlString.toRawUTF8(), projectXmlString.getNumBytesAsUTF8());
    zipBuilder.addEntry (new juce::MemoryInputStream (projectXmlData, false),
                         9, "project.xml", juce::Time::getCurrentTime());

    // Add metadata.xml
    auto metadataXmlString = metadataXml->toString();
    auto metadataXmlData = juce::MemoryBlock (metadataXmlString.toRawUTF8(), metadataXmlString.getNumBytesAsUTF8());
    zipBuilder.addEntry (new juce::MemoryInputStream (metadataXmlData, false),
                         9, "metadata.xml", juce::Time::getCurrentTime());

    // Add audio files
    writeAudioFiles (zipBuilder);

    // Add plugin states
    writePluginStates (zipBuilder);

    // Write the ZIP file
    juce::FileOutputStream output (file);
    if (! output.openedOk())
        return juce::Result::fail ("Failed to create output file: " + file.getFullPathName());

    if (! zipBuilder.writeToStream (output, nullptr))
        return juce::Result::fail ("Failed to write ZIP archive");

    return juce::Result::ok();
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createTransport (juce::XmlElement& parent)
{
    auto* transport = addChildElement (parent, xml::Transport);

    // Tempo
    auto& tempoSeq = edit.tempoSequence;
    if (auto* firstTempo = tempoSeq.getTempo (0))
    {
        auto* tempo = addChildElement (*transport, xml::Tempo);
        tempo->setAttribute (xml::unit, xml::bpm);
        tempo->setAttribute (xml::value, firstTempo->getBpm());
    }

    // Time signature
    if (auto* firstTimeSig = tempoSeq.getTimeSig (0))
    {
        auto* timeSig = addChildElement (*transport, xml::TimeSignature);
        timeSig->setAttribute (xml::numerator, firstTimeSig->numerator);
        timeSig->setAttribute (xml::denominator, firstTimeSig->denominator);
    }

    return transport;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createStructure (juce::XmlElement& parent)
{
    auto* structure = addChildElement (parent, xml::Structure);

    // Export all top-level tracks
    for (auto* track : getAllTracks (edit))
    {
        if (track->isPartOfSubmix())
            continue;

        // Only export top-level tracks (ones without a parent folder)
        if ([[maybe_unused]] auto* parentTrack = track->getParentTrack())
            continue;

        createTrackElement (*structure, *track);
    }

    // Add master channel
    auto* masterChannel = addChildElement (*structure, xml::Channel);
    masterChannel->setAttribute (xml::id, idGenerator.generateID());
    masterChannel->setAttribute (xml::name, "Master");
    masterChannel->setAttribute (xml::role, xml::master);

    // Master volume
    if (auto masterVolPlugin = edit.getMasterVolumePlugin())
    {
        auto* volume = addChildElement (*masterChannel, xml::Volume);
        volume->setAttribute (xml::unit, xml::decibel);
        volume->setAttribute (xml::value, masterVolPlugin->getVolumeDb());

        auto* pan = addChildElement (*masterChannel, xml::Pan);
        pan->setAttribute (xml::unit, xml::normalized);
        pan->setAttribute (xml::value, masterVolPlugin->getPan());
    }

    return structure;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createTrackElement (juce::XmlElement& parent, Track& track)
{
    auto* trackElement = addChildElement (parent, xml::Track);

    auto trackId = idGenerator.generateID();
    trackToId[&track] = trackId;
    trackElement->setAttribute (xml::id, trackId);
    trackElement->setAttribute (xml::name, track.getName());
    trackElement->setAttribute (xml::contentType, getTrackContentType (track));

    // Color
    auto colour = track.getColour();
    if (! colour.isTransparent())
        trackElement->setAttribute (xml::color, colourToDAWprojectString (colour));

    // Create channel for mixer properties
    createChannelElement (*trackElement, track);

    // Export nested tracks (for folder tracks)
    if (auto* folderTrack = dynamic_cast<FolderTrack*> (&track))
    {
        for (auto* subTrack : folderTrack->getAllSubTracks (false))
            createTrackElement (*trackElement, *subTrack);
    }

    return trackElement;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createChannelElement (juce::XmlElement& parent, Track& track)
{
    auto* channel = addChildElement (parent, xml::Channel);

    auto channelId = idGenerator.generateID();
    trackToChannelId[&track] = channelId;
    channel->setAttribute (xml::id, channelId);
    channel->setAttribute (xml::role, getMixerRole (track));

    if (auto* audioTrack = dynamic_cast<AudioTrack*> (&track))
    {
        // Volume
        if (auto* volPlugin = audioTrack->getVolumePlugin())
        {
            auto* volume = addChildElement (*channel, xml::Volume);
            volume->setAttribute (xml::unit, xml::decibel);
            volume->setAttribute (xml::value, volPlugin->getVolumeDb());

            auto* pan = addChildElement (*channel, xml::Pan);
            pan->setAttribute (xml::unit, xml::normalized);
            pan->setAttribute (xml::value, volPlugin->getPan());
        }

        // Mute
        auto* mute = addChildElement (*channel, xml::Mute);
        mute->setAttribute (xml::value, audioTrack->isMuted (true));

        // Solo
        if (audioTrack->isSolo (true))
            channel->setAttribute (xml::solo, true);

        // Devices (plugins)
        createDevices (*channel, track);
    }

    return channel;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createArrangement (juce::XmlElement& parent)
{
    auto* arrangement = addChildElement (parent, xml::Arrangement);
    arrangement->setAttribute (xml::id, idGenerator.generateID());

    // Tempo automation
    createTempoAutomation (*arrangement);

    // Time signature automation
    createTimeSignatureAutomation (*arrangement);

    // Markers
    createMarkersElement (*arrangement);

    // Lanes for each track
    auto* lanes = addChildElement (*arrangement, xml::Lanes);
    lanes->setAttribute (xml::timeUnit, xml::seconds);

    for (auto* track : getAllTracks (edit))
    {
        auto it = trackToId.find (track);
        if (it == trackToId.end())
            continue;

        if (auto* clipTrack = dynamic_cast<ClipTrack*> (track))
        {
            if (clipTrack->getClips().isEmpty())
                continue;

            auto* trackLanes = addChildElement (*lanes, xml::Lanes);
            trackLanes->setAttribute (xml::track, it->second);
            trackLanes->setAttribute (xml::timeUnit, xml::seconds);

            createClipsElement (*trackLanes, *clipTrack);
        }
    }

    return arrangement;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createTempoAutomation (juce::XmlElement& parent)
{
    auto& tempoSeq = edit.tempoSequence;
    if (tempoSeq.getNumTempos() <= 1)
        return nullptr;

    auto* tempoAuto = addChildElement (parent, xml::TempoAutomation);

    // Add target
    auto* target = addChildElement (*tempoAuto, xml::Target);
    target->setAttribute (xml::parameter, "tempo");

    for (int i = 0; i < tempoSeq.getNumTempos(); ++i)
    {
        if (auto* tempo = tempoSeq.getTempo (i))
        {
            auto* point = addChildElement (*tempoAuto, xml::RealPoint);

            auto beats = tempoSeq.toBeats (tempo->getStartTime());
            point->setAttribute (xml::time, beats.inBeats());
            point->setAttribute (xml::value, tempo->getBpm());
        }
    }

    return tempoAuto;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createTimeSignatureAutomation (juce::XmlElement& parent)
{
    auto& tempoSeq = edit.tempoSequence;
    if (tempoSeq.getNumTimeSigs() <= 1)
        return nullptr;

    auto* timeSigAuto = addChildElement (parent, xml::TimeSignatureAutomation);

    for (int i = 0; i < tempoSeq.getNumTimeSigs(); ++i)
    {
        if (auto* timeSig = tempoSeq.getTimeSig (i))
        {
            auto* point = addChildElement (*timeSigAuto, xml::TimeSignaturePoint);

            point->setAttribute (xml::time, timeSig->getStartBeat().inBeats());
            point->setAttribute (xml::numerator, timeSig->numerator);
            point->setAttribute (xml::denominator, timeSig->denominator);
        }
    }

    return timeSigAuto;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createMarkersElement (juce::XmlElement& parent)
{
    auto* markerTrack = edit.getMarkerTrack();
    if (markerTrack == nullptr)
        return nullptr;

    auto markerClips = markerTrack->getClips();
    if (markerClips.isEmpty())
        return nullptr;

    auto* markers = addChildElement (parent, xml::Markers);

    for (auto* clip : markerClips)
    {
        if (auto* markerClip = dynamic_cast<MarkerClip*> (clip))
        {
            auto* marker = addChildElement (*markers, xml::Marker);
            marker->setAttribute (xml::name, markerClip->getName());
            marker->setAttribute (xml::time, markerClip->getPosition().getStart().inSeconds());

            auto colour = markerClip->getColour();
            if (! colour.isTransparent())
                marker->setAttribute (xml::color, colourToDAWprojectString (colour));
        }
    }

    return markers;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createClipsElement (juce::XmlElement& parent, ClipTrack& track)
{
    auto* clips = addChildElement (parent, xml::Clips);

    for (auto* clip : track.getClips())
        createClipElement (*clips, *clip);

    return clips;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createClipElement (juce::XmlElement& parent, Clip& clip)
{
    auto* clipElement = addChildElement (parent, xml::Clip);

    auto pos = clip.getPosition();
    clipElement->setAttribute (xml::time, pos.getStart().inSeconds());
    clipElement->setAttribute (xml::duration, pos.getLength().inSeconds());
    setAttributeIfNotEmpty (*clipElement, xml::name, clip.getName());

    // Color
    auto colour = clip.getColour();
    if (! colour.isTransparent())
        clipElement->setAttribute (xml::color, colourToDAWprojectString (colour));

    // Enabled
    if (clip.disabled)
        clipElement->setAttribute (xml::enable, false);

    // Export content based on clip type
    if (auto* waveClip = dynamic_cast<WaveAudioClip*> (&clip))
        exportAudioClip (*clipElement, *waveClip);
    else if (auto* midiClip = dynamic_cast<MidiClip*> (&clip))
        exportMidiClip (*clipElement, *midiClip);

    return clipElement;
}

//==============================================================================
void DAWprojectExporter::exportAudioClip (juce::XmlElement& clipElement, WaveAudioClip& clip)
{
    auto sourceFile = clip.getOriginalFile();
    if (! sourceFile.existsAsFile())
        return;

    // Add audio element
    auto* audio = addChildElement (clipElement, xml::Audio);
    audio->setAttribute (xml::id, idGenerator.generateID());

    AudioFile audioFile (clip.edit.engine, sourceFile);
    if (audioFile.isValid())
    {
        audio->setAttribute (xml::channels, audioFile.getNumChannels());
        audio->setAttribute (xml::sampleRate, static_cast<int> (audioFile.getSampleRate()));
        audio->setAttribute (xml::duration, audioFile.getLength());
    }

    // File reference
    auto archivePath = addAudioFile (sourceFile);
    auto* fileElement = addChildElement (*audio, xml::File);
    fileElement->setAttribute (xml::path, archivePath);

    // Offset (playStart)
    auto offset = clip.getPosition().getOffset();
    if (offset.inSeconds() != 0.0)
        clipElement.setAttribute (xml::playStart, offset.inSeconds());

    // Fades
    auto fadeIn = clip.getFadeIn();
    auto fadeOut = clip.getFadeOut();
    if (fadeIn.inSeconds() > 0.0)
        clipElement.setAttribute (xml::fadeInTime, fadeIn.inSeconds());
    if (fadeOut.inSeconds() > 0.0)
        clipElement.setAttribute (xml::fadeOutTime, fadeOut.inSeconds());

    // Loop points
    if (clip.isLooping())
    {
        auto loopRange = clip.getLoopRange();
        clipElement.setAttribute (xml::loopStart, loopRange.getStart().inSeconds());
        clipElement.setAttribute (xml::loopEnd, loopRange.getEnd().inSeconds());
    }
}

//==============================================================================
void DAWprojectExporter::exportMidiClip (juce::XmlElement& clipElement, MidiClip& clip)
{
    clipElement.setAttribute (xml::contentTimeUnit, xml::beats);

    // Create a Lanes element to hold Notes and controller automation
    auto* lanes = addChildElement (clipElement, xml::Lanes);
    lanes->setAttribute (xml::timeUnit, xml::beats);

    // Add notes
    createNotesElement (*lanes, clip);

    // Add controller data
    createControllerLanes (*lanes, clip);

    // Offset (playStart)
    auto offset = clip.getPosition().getOffset();
    if (offset.inSeconds() != 0.0)
        clipElement.setAttribute (xml::playStart, offset.inSeconds());

    // Loop points
    if (clip.isLooping())
    {
        auto loopRange = clip.getLoopRangeBeats();
        clipElement.setAttribute (xml::loopStart, loopRange.getStart().inBeats());
        clipElement.setAttribute (xml::loopEnd, loopRange.getEnd().inBeats());
    }
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createNotesElement (juce::XmlElement& parent, MidiClip& clip)
{
    auto* notes = addChildElement (parent, xml::Notes);
    notes->setAttribute (xml::timeUnit, xml::beats);

    auto& sequence = clip.getSequence();

    for (auto* note : sequence.getNotes())
    {
        if (note != nullptr)
            createNoteElement (*notes, *note);
    }

    return notes;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createNoteElement (juce::XmlElement& parent, const MidiNote& note)
{
    auto* noteElement = addChildElement (parent, xml::Note);

    noteElement->setAttribute (xml::time, note.getStartBeat().inBeats());
    noteElement->setAttribute (xml::duration, note.getLengthBeats().inBeats());
    noteElement->setAttribute (xml::channel, note.getColour()); // MIDI channel is stored in colour
    noteElement->setAttribute (xml::key, note.getNoteNumber());
    noteElement->setAttribute (xml::vel, velocityToNormalized (note.getVelocity()));

    return noteElement;
}

//==============================================================================
void DAWprojectExporter::createControllerLanes (juce::XmlElement& parent, MidiClip& clip)
{
    auto& sequence = clip.getSequence();
    auto& controllerEvents = sequence.getControllerEvents();

    if (controllerEvents.isEmpty())
        return;

    // Group events by controller type
    std::map<int, juce::Array<MidiControllerEvent*>> eventsByType;

    for (auto* event : controllerEvents)
    {
        if (event != nullptr)
            eventsByType[event->getType()].add (event);
    }

    // Create a Points element for each controller type
    for (auto& [controllerType, events] : eventsByType)
    {
        if (events.isEmpty())
            continue;

        auto* points = addChildElement (parent, xml::Points);
        points->setAttribute (xml::timeUnit, xml::beats);

        // Add target with expression type
        auto* target = addChildElement (*points, xml::Target);
        target->setAttribute (xml::expression, controllerTypeToExpression (controllerType));

        // For regular CCs, add the controller number
        if (controllerType >= 0 && controllerType <= 127)
            target->setAttribute (xml::controller, controllerType);

        // Add points for each event
        for (auto* event : events)
        {
            auto* point = addChildElement (*points, xml::RealPoint);
            point->setAttribute (xml::time, event->getBeatPosition().inBeats());
            point->setAttribute (xml::value, controllerValueToNormalized (event->getControllerValue(), controllerType));
        }
    }
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createDevices (juce::XmlElement& parent, Track& track)
{
    auto* audioTrack = dynamic_cast<AudioTrack*> (&track);
    if (audioTrack == nullptr)
        return nullptr;

    auto& plugins = audioTrack->pluginList;
    if (plugins.size() == 0)
        return nullptr;

    auto* devices = addChildElement (parent, xml::Devices);

    for (auto* plugin : plugins)
    {
        // Skip built-in volume/pan plugins
        if (dynamic_cast<VolumeAndPanPlugin*> (plugin) != nullptr)
            continue;

        createPluginElement (*devices, *plugin);
    }

    return devices;
}

//==============================================================================
juce::XmlElement* DAWprojectExporter::createPluginElement (juce::XmlElement& parent, Plugin& plugin)
{
    auto pluginType = getPluginType (plugin);
    if (pluginType.isEmpty())
        return nullptr;

    auto* pluginElement = addChildElement (parent, pluginType.toRawUTF8());

    pluginElement->setAttribute (xml::id, idGenerator.generateID());
    pluginElement->setAttribute (xml::deviceName, plugin.getName());
    pluginElement->setAttribute (xml::deviceRole, getDeviceRole (plugin));

    if (auto* externalPlugin = dynamic_cast<ExternalPlugin*> (&plugin))
    {
        auto& desc = externalPlugin->desc;
        pluginElement->setAttribute (xml::deviceID, createIdentifierString (desc));

        if (desc.manufacturerName.isNotEmpty())
            pluginElement->setAttribute (xml::deviceVendor, desc.manufacturerName);
        if (desc.version.isNotEmpty())
            pluginElement->setAttribute (xml::pluginVersion, desc.version);

        // Enabled state
        auto* enabled = addChildElement (*pluginElement, xml::Enabled);
        enabled->setAttribute (xml::value, plugin.isEnabled());

        // Save plugin state
        if (options.embedPluginState)
        {
            juce::MemoryBlock stateData;
            externalPlugin->getPluginStateFromTree (stateData);

            if (stateData.getSize() > 0)
            {
                auto statePath = "plugins/" + juce::String (pluginStatesToEmbed.size()) + ".bin";
                pluginStatesToEmbed.push_back ({ stateData, statePath });

                auto* stateElement = addChildElement (*pluginElement, xml::State);
                stateElement->setAttribute (xml::path, statePath);
            }
        }
    }

    return pluginElement;
}

//==============================================================================
juce::String DAWprojectExporter::addAudioFile (const juce::File& file)
{
    // Check if already added
    for (const auto& entry : audioFilesToEmbed)
    {
        if (entry.sourceFile == file)
            return entry.archivePath;
    }

    auto archivePath = "audio/" + file.getFileName();

    // Handle duplicate filenames
    int counter = 1;
    while (true)
    {
        bool found = false;
        for (const auto& entry : audioFilesToEmbed)
        {
            if (entry.archivePath == archivePath)
            {
                found = true;
                break;
            }
        }

        if (! found)
            break;

        archivePath = "audio/" + file.getFileNameWithoutExtension() + "_" +
                      juce::String (counter++) + file.getFileExtension();
    }

    audioFilesToEmbed.push_back ({ file, archivePath });
    return archivePath;
}

//==============================================================================
void DAWprojectExporter::writeAudioFiles (juce::ZipFile::Builder& zipBuilder)
{
    if (! options.embedAudioFiles)
        return;

    for (const auto& entry : audioFilesToEmbed)
    {
        if (entry.sourceFile.existsAsFile())
        {
            zipBuilder.addFile (entry.sourceFile, 9, entry.archivePath);
        }
    }
}

//==============================================================================
void DAWprojectExporter::writePluginStates (juce::ZipFile::Builder& zipBuilder)
{
    if (! options.embedPluginState)
        return;

    for (const auto& entry : pluginStatesToEmbed)
    {
        zipBuilder.addEntry (new juce::MemoryInputStream (entry.state, false),
                             9, entry.archivePath, juce::Time::getCurrentTime());
    }
}

//==============================================================================
juce::String DAWprojectExporter::getTrackContentType (Track& track) const
{
    if (dynamic_cast<FolderTrack*> (&track) != nullptr)
        return xml::tracks;

    if (dynamic_cast<MarkerTrack*> (&track) != nullptr)
        return xml::markers;

    if (auto* clipTrack = dynamic_cast<ClipTrack*> (&track))
    {
        // Check if the track has MIDI or audio content
        for (auto* clip : clipTrack->getClips())
        {
            if (dynamic_cast<MidiClip*> (clip) != nullptr)
                return xml::notes;
            if (dynamic_cast<WaveAudioClip*> (clip) != nullptr)
                return xml::audio;
        }
    }

    // Default to audio for empty audio tracks
    if (dynamic_cast<AudioTrack*> (&track) != nullptr)
        return xml::audio;

    return {};
}

//==============================================================================
juce::String DAWprojectExporter::getMixerRole (Track& track) const
{
    if (dynamic_cast<FolderTrack*> (&track) != nullptr)
    {
        auto* folderTrack = dynamic_cast<FolderTrack*> (&track);
        if (folderTrack && folderTrack->isSubmixFolder())
            return xml::submix;
        return xml::regular;
    }

    return xml::regular;
}

//==============================================================================
juce::String DAWprojectExporter::getPluginType (Plugin& plugin) const
{
    if (auto* externalPlugin = dynamic_cast<ExternalPlugin*> (&plugin))
    {
        auto& desc = externalPlugin->desc;

        if (desc.pluginFormatName == "VST3")
            return xml::Vst3Plugin;
        if (desc.pluginFormatName == "VST")
            return xml::Vst2Plugin;
        if (desc.pluginFormatName == "AudioUnit")
            return xml::AuPlugin;
    }

    return {};
}

//==============================================================================
juce::String DAWprojectExporter::getDeviceRole (Plugin& plugin) const
{
    if (plugin.takesMidiInput() && ! plugin.producesAudioWhenNoAudioInput())
        return xml::noteFX;

    if (auto* externalPlugin = dynamic_cast<ExternalPlugin*> (&plugin))
    {
        if (externalPlugin->isSynth())
            return xml::instrument;
    }

    return xml::audioFX;
}

}}} // namespace tracktion::engine::dawproject
