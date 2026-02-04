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
DAWprojectImporter::DAWprojectImporter (Engine& e, const ParseOptions& opts)
    : engine (e), options (opts)
{
}

//==============================================================================
std::unique_ptr<Edit> DAWprojectImporter::importFromFile (const juce::File& file)
{
    juce::ZipFile zipFile (file);

    if (zipFile.getNumEntries() == 0)
    {
        DBG ("DAWproject: Invalid or empty ZIP file: " << file.getFullPathName());
        return nullptr;
    }

    // Find and parse project.xml
    auto* projectEntry = zipFile.getEntry ("project.xml");
    if (projectEntry == nullptr)
    {
        DBG ("DAWproject: No project.xml found in archive");
        return nullptr;
    }

    std::unique_ptr<juce::InputStream> projectStream (zipFile.createStreamForEntry (*projectEntry));
    if (projectStream == nullptr)
    {
        DBG ("DAWproject: Failed to read project.xml");
        return nullptr;
    }

    auto projectXml = juce::parseXML (projectStream->readEntireStreamAsString());
    if (projectXml == nullptr)
    {
        DBG ("DAWproject: Failed to parse project.xml");
        return nullptr;
    }

    // Determine audio file directory
    if (options.audioFileDestination.exists())
        audioFileDirectory = options.audioFileDestination;
    else if (options.extractAudioFiles)
        audioFileDirectory = juce::File::createTempFile ("dawproject_audio");

    // Extract audio files if needed
    if (options.extractAudioFiles && audioFileDirectory.isDirectory())
        extractAudioFiles (zipFile, audioFileDirectory);
    else if (options.extractAudioFiles)
    {
        audioFileDirectory.createDirectory();
        extractAudioFiles (zipFile, audioFileDirectory);
    }

    return importFromXml (*projectXml, audioFileDirectory);
}

//==============================================================================
std::unique_ptr<Edit> DAWprojectImporter::importFromXml (const juce::XmlElement& projectXml,
                                                         const juce::File& audioDir)
{
    audioFileDirectory = audioDir;

    if (! projectXml.hasTagName (xml::Project))
    {
        DBG ("DAWproject: Root element is not 'Project'");
        return nullptr;
    }

    // Build ID resolver from all elements with id attributes
    std::function<void(const juce::XmlElement&)> collectIds = [&] (const juce::XmlElement& element)
    {
        auto id = element.getStringAttribute (xml::id);
        if (id.isNotEmpty())
            idResolver.registerElement (id, const_cast<juce::XmlElement*> (&element));

        for (auto* child : element.getChildIterator())
            collectIds (*child);
    };
    collectIds (projectXml);

    // Create a new Edit
    auto edit = Edit::createSingleTrackEdit (engine, options.editRole);
    if (edit == nullptr)
        return nullptr;

    // Delete the default track that was created
    if (auto defaultTrack = getAudioTracks (*edit).getFirst())
        edit->deleteTrack (defaultTrack);

    // Parse Transport (tempo/time signature)
    if (auto* transportElement = projectXml.getChildByName (xml::Transport))
        parseTransport (*transportElement, *edit);

    // Parse Structure (tracks and channels)
    if (auto* structureElement = projectXml.getChildByName (xml::Structure))
        parseStructure (*structureElement, *edit);

    // Parse Arrangement (clips, automation, markers)
    if (auto* arrangementElement = projectXml.getChildByName (xml::Arrangement))
        parseArrangement (*arrangementElement, *edit);

    return edit;
}

//==============================================================================
void DAWprojectImporter::parseTransport (const juce::XmlElement& transportElement, Edit& edit)
{
    // Parse initial tempo
    if (auto* tempoElement = transportElement.getChildByName (xml::Tempo))
    {
        auto bpm = parseDouble (tempoElement->getStringAttribute (xml::value), 120.0);
        if (auto* tempo = edit.tempoSequence.getTempo (0))
            tempo->setBpm (bpm);
    }

    // Parse initial time signature
    if (auto* timeSigElement = transportElement.getChildByName (xml::TimeSignature))
    {
        int numerator = parseInt (timeSigElement->getStringAttribute (xml::numerator), 4);
        int denominator = parseInt (timeSigElement->getStringAttribute (xml::denominator), 4);

        if (auto* timeSig = edit.tempoSequence.getTimeSig (0))
            timeSig->setStringTimeSig (juce::String (numerator) + "/" + juce::String (denominator));
    }
}

//==============================================================================
void DAWprojectImporter::parseStructure (const juce::XmlElement& structureElement, Edit& edit)
{
    for (auto* child : structureElement.getChildIterator())
    {
        if (child->hasTagName (xml::Track))
            parseTrack (*child, edit, nullptr);
        else if (child->hasTagName (xml::Channel))
            parseChannel (*child, edit, nullptr);
    }
}

//==============================================================================
void DAWprojectImporter::parseTrack (const juce::XmlElement& trackElement, Edit& edit, Track* parentTrack)
{
    auto id = trackElement.getStringAttribute (xml::id);
    auto name = trackElement.getStringAttribute (xml::name);
    auto contentType = trackElement.getStringAttribute (xml::contentType);

    Track::Ptr newTrack;

    // Determine track type based on contentType
    if (contentType.contains (xml::tracks))
    {
        // Folder track
        TrackInsertPoint insertPoint (parentTrack, nullptr);
        newTrack = edit.insertNewFolderTrack (insertPoint, nullptr, false);
    }
    else if (contentType.contains (xml::markers))
    {
        // Marker track
        edit.ensureMarkerTrack();
        newTrack = edit.getMarkerTrack();
    }
    else
    {
        // Audio or MIDI track (both map to AudioTrack in tracktion_engine)
        TrackInsertPoint insertPoint (parentTrack, nullptr);
        newTrack = edit.insertNewAudioTrack (insertPoint, nullptr, false);
    }

    if (newTrack != nullptr)
    {
        newTrack->setName (name);

        // Set color if present
        auto colorStr = trackElement.getStringAttribute (xml::color);
        if (colorStr.isNotEmpty())
            newTrack->setColour (dawprojectStringToColour (colorStr));

        // Register the track ID
        if (id.isNotEmpty())
            idToTrack[id] = newTrack.get();

        // Parse embedded channel
        if (auto* channelElement = trackElement.getChildByName (xml::Channel))
            parseChannel (*channelElement, edit, newTrack.get());

        // Parse nested tracks
        for (auto* child : trackElement.getChildIterator())
        {
            if (child->hasTagName (xml::Track))
                parseTrack (*child, edit, newTrack.get());
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseChannel (const juce::XmlElement& channelElement, Edit& edit, Track* track)
{
    auto id = channelElement.getStringAttribute (xml::id);
    auto role = channelElement.getStringAttribute (xml::role);

    // Handle master channel
    if (role == xml::master)
    {
        // Parse master volume/pan
        if (auto* volumeElement = channelElement.getChildByName (xml::Volume))
        {
            auto unit = volumeElement->getStringAttribute (xml::unit);
            auto value = parseDouble (volumeElement->getStringAttribute (xml::value), 0.0);

            if (auto masterVol = edit.getMasterVolumePlugin())
            {
                if (unit == xml::decibel)
                    masterVol->setVolumeDb (static_cast<float> (value));
                else if (unit == xml::linear || unit == xml::normalized)
                    masterVol->setVolumeDb (static_cast<float> (gainToDecibels (static_cast<float> (value))));
            }
        }

        if (auto* panElement = channelElement.getChildByName (xml::Pan))
        {
            auto value = parseDouble (panElement->getStringAttribute (xml::value), 0.0);
            if (auto masterVol = edit.getMasterVolumePlugin())
                masterVol->setPan (static_cast<float> (value));
        }

        return;
    }

    // Register channel ID
    if (id.isNotEmpty() && track != nullptr)
        idToChannel[id] = track;

    // Apply channel properties to track
    if (auto* audioTrack = dynamic_cast<AudioTrack*> (track))
    {
        // Parse volume
        if (auto* volumeElement = channelElement.getChildByName (xml::Volume))
        {
            auto unit = volumeElement->getStringAttribute (xml::unit);
            auto value = parseDouble (volumeElement->getStringAttribute (xml::value), 0.0);

            if (auto* volPlugin = audioTrack->getVolumePlugin())
            {
                if (unit == xml::decibel)
                    volPlugin->setVolumeDb (static_cast<float> (value));
                else if (unit == xml::linear || unit == xml::normalized)
                    volPlugin->setVolumeDb (static_cast<float> (gainToDecibels (static_cast<float> (value))));
            }
        }

        // Parse pan
        if (auto* panElement = channelElement.getChildByName (xml::Pan))
        {
            auto value = parseDouble (panElement->getStringAttribute (xml::value), 0.0);
            if (auto* volPlugin = audioTrack->getVolumePlugin())
                volPlugin->setPan (static_cast<float> (value));
        }

        // Parse mute
        if (auto* muteElement = channelElement.getChildByName (xml::Mute))
        {
            auto value = parseBool (muteElement->getStringAttribute (xml::value), false);
            audioTrack->setMute (value);
        }

        // Parse solo
        auto solo = parseBool (channelElement.getStringAttribute (xml::solo), false);
        audioTrack->setSolo (solo);

        // Parse devices (plugins)
        if (auto* devicesElement = channelElement.getChildByName (xml::Devices))
            parseDevices (*devicesElement, edit, *audioTrack);
    }
}

//==============================================================================
void DAWprojectImporter::parseArrangement (const juce::XmlElement& arrangementElement, Edit& edit)
{
    // Parse tempo automation
    if (auto* tempoAutomation = arrangementElement.getChildByName (xml::TempoAutomation))
        parseTempoAutomation (*tempoAutomation, edit);

    // Parse time signature automation
    if (auto* timeSigAutomation = arrangementElement.getChildByName (xml::TimeSignatureAutomation))
        parseTimeSignatureAutomation (*timeSigAutomation, edit);

    // Parse markers
    if (auto* markersElement = arrangementElement.getChildByName (xml::Markers))
        parseMarkers (*markersElement, edit);

    // Parse lanes
    if (auto* lanesElement = arrangementElement.getChildByName (xml::Lanes))
    {
        for (auto* child : lanesElement->getChildIterator())
        {
            if (child->hasTagName (xml::Lanes))
            {
                // Find the associated track
                auto trackRef = child->getStringAttribute (xml::track);
                auto it = idToTrack.find (trackRef);
                if (it != idToTrack.end())
                    parseLanes (*child, edit, it->second);
            }
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseTempoAutomation (const juce::XmlElement& tempoAutomation, Edit& edit)
{
    for (auto* child : tempoAutomation.getChildIterator())
    {
        if (child->hasTagName (xml::RealPoint))
        {
            auto timeStr = child->getStringAttribute (xml::time);
            auto valueStr = child->getStringAttribute (xml::value);

            double beats = parseDouble (timeStr, 0.0);
            double bpm = parseDouble (valueStr, 120.0);

            // Add or update tempo point
            if (beats == 0.0)
            {
                if (auto* tempo = edit.tempoSequence.getTempo (0))
                    tempo->setBpm (bpm);
            }
            else
            {
                auto newTempo = edit.tempoSequence.insertTempo (BeatPosition::fromBeats (beats), bpm, 0.0f);
                juce::ignoreUnused (newTempo);
            }
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseTimeSignatureAutomation (const juce::XmlElement& timeSigAutomation, Edit& edit)
{
    for (auto* child : timeSigAutomation.getChildIterator())
    {
        if (child->hasTagName (xml::TimeSignaturePoint))
        {
            auto timeStr = child->getStringAttribute (xml::time);
            int numerator = parseInt (child->getStringAttribute (xml::numerator), 4);
            int denominator = parseInt (child->getStringAttribute (xml::denominator), 4);

            double beats = parseDouble (timeStr, 0.0);
            auto timeSigStr = juce::String (numerator) + "/" + juce::String (denominator);

            if (beats == 0.0)
            {
                if (auto* timeSig = edit.tempoSequence.getTimeSig (0))
                    timeSig->setStringTimeSig (timeSigStr);
            }
            else
            {
                auto newTimeSig = edit.tempoSequence.insertTimeSig (BeatPosition::fromBeats (beats));
                if (newTimeSig != nullptr)
                    newTimeSig->setStringTimeSig (timeSigStr);
            }
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseMarkers (const juce::XmlElement& markersElement, Edit& edit)
{
    edit.ensureMarkerTrack();
    auto* markerTrack = edit.getMarkerTrack();
    if (markerTrack == nullptr)
        return;

    bool isBeats = isTimeUnitBeats (markersElement);

    for (auto* child : markersElement.getChildIterator())
    {
        if (child->hasTagName (xml::Marker))
        {
            auto name = child->getStringAttribute (xml::name);
            auto time = parseTime (*child, xml::time, isBeats);

            // Create marker clip using insertNewClip
            auto clip = markerTrack->insertNewClip (TrackItem::Type::marker,
                                                     { time, time + 0.001_td },
                                                     nullptr);
            if (clip != nullptr)
            {
                clip->setName (name);

                auto colorStr = child->getStringAttribute (xml::color);
                if (colorStr.isNotEmpty())
                    clip->setColour (dawprojectStringToColour (colorStr));
            }
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseLanes (const juce::XmlElement& lanesElement, Edit& edit, Track* track)
{
    auto* clipTrack = dynamic_cast<ClipTrack*> (track);
    if (clipTrack == nullptr)
        return;

    for (auto* child : lanesElement.getChildIterator())
    {
        if (child->hasTagName (xml::Clips))
            parseClips (*child, edit, *clipTrack);
        else if (child->hasTagName (xml::Lanes))
            parseLanes (*child, edit, track);
    }
}

//==============================================================================
void DAWprojectImporter::parseClips (const juce::XmlElement& clipsElement, Edit&, ClipTrack& track)
{
    for (auto* child : clipsElement.getChildIterator())
    {
        if (child->hasTagName (xml::Clip))
            parseClip (*child, track.edit, track);
    }
}

//==============================================================================
Clip::Ptr DAWprojectImporter::parseClip (const juce::XmlElement& clipElement, Edit&, ClipTrack& track)
{
    auto name = clipElement.getStringAttribute (xml::name);
    auto timeAttr = parseDouble (clipElement.getStringAttribute (xml::time), 0.0);
    auto durationAttr = parseDouble (clipElement.getStringAttribute (xml::duration), 0.0);

    // Determine clip type based on content
    // Content can be directly in the clip or inside a Lanes element
    auto* audioElement = clipElement.getChildByName (xml::Audio);
    auto* notesElement = clipElement.getChildByName (xml::Notes);
    auto* lanesElement = clipElement.getChildByName (xml::Lanes);

    // If there's a Lanes element, look for Notes/Audio inside it
    if (lanesElement != nullptr)
    {
        if (notesElement == nullptr)
            notesElement = lanesElement->getChildByName (xml::Notes);
        if (audioElement == nullptr)
            audioElement = lanesElement->getChildByName (xml::Audio);
    }

    Clip::Ptr clip;

    if (audioElement != nullptr)
    {
        // Create audio clip
        auto* fileElement = audioElement->getChildByName (xml::File);
        if (fileElement != nullptr)
        {
            auto path = fileElement->getStringAttribute (xml::path);
            auto audioFile = resolveAudioFile (path);

            if (audioFile.existsAsFile())
            {
                auto* audioTrack = dynamic_cast<AudioTrack*> (&track);
                if (audioTrack != nullptr)
                {
                    ClipPosition position { { TimePosition::fromSeconds (timeAttr),
                                              TimeDuration::fromSeconds (durationAttr) }, {} };

                    auto waveClip = audioTrack->insertWaveClip (name, audioFile, position, false);
                    if (waveClip != nullptr)
                    {
                        parseAudioClip (clipElement, *waveClip);
                        clip = waveClip;
                    }
                }
            }
        }
    }
    else if (notesElement != nullptr)
    {
        // Create MIDI clip
        auto* audioTrack = dynamic_cast<AudioTrack*> (&track);
        if (audioTrack != nullptr)
        {
            TimeRange range { TimePosition::fromSeconds (timeAttr),
                              TimePosition::fromSeconds (timeAttr + durationAttr) };

            auto midiClip = audioTrack->insertMIDIClip (range, nullptr);
            if (midiClip != nullptr)
            {
                midiClip->setName (name);
                parseNotes (*notesElement, *midiClip);

                // Parse controller data from Points elements in Lanes
                if (lanesElement != nullptr)
                    parseControllerPoints (*lanesElement, *midiClip);

                parseMidiClip (clipElement, *midiClip);
                clip = midiClip;
            }
        }
    }

    if (clip != nullptr)
    {
        // Set clip color
        auto colorStr = clipElement.getStringAttribute (xml::color);
        if (colorStr.isNotEmpty())
            clip->setColour (dawprojectStringToColour (colorStr));

        // Handle enable/disable
        auto enabled = parseBool (clipElement.getStringAttribute (xml::enable), true);
        clip->disabled = ! enabled;
    }

    return clip;
}

//==============================================================================
void DAWprojectImporter::parseAudioClip (const juce::XmlElement& clipElement, WaveAudioClip& clip)
{
    // Parse playStart (offset)
    auto playStart = parseDouble (clipElement.getStringAttribute (xml::playStart), 0.0);
    if (playStart != 0.0)
        clip.setOffset (TimeDuration::fromSeconds (playStart));

    // Parse fades
    auto fadeInTime = parseDouble (clipElement.getStringAttribute (xml::fadeInTime), 0.0);
    auto fadeOutTime = parseDouble (clipElement.getStringAttribute (xml::fadeOutTime), 0.0);

    if (fadeInTime > 0.0)
        clip.setFadeIn (TimeDuration::fromSeconds (fadeInTime));
    if (fadeOutTime > 0.0)
        clip.setFadeOut (TimeDuration::fromSeconds (fadeOutTime));

    // Parse loop points
    auto loopStart = clipElement.getStringAttribute (xml::loopStart);
    auto loopEnd = clipElement.getStringAttribute (xml::loopEnd);

    if (loopStart.isNotEmpty() && loopEnd.isNotEmpty())
    {
        auto loopStartTime = parseDouble (loopStart, 0.0);
        auto loopEndTime = parseDouble (loopEnd, 0.0);

        if (loopEndTime > loopStartTime)
        {
            clip.setLoopRange (TimeRange { TimePosition::fromSeconds (loopStartTime),
                                           TimePosition::fromSeconds (loopEndTime) });
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseMidiClip (const juce::XmlElement& clipElement, MidiClip& clip)
{
    // Parse playStart (offset)
    auto playStart = parseDouble (clipElement.getStringAttribute (xml::playStart), 0.0);
    if (playStart != 0.0)
        clip.setOffset (TimeDuration::fromSeconds (playStart));

    // Parse loop points
    auto loopStart = clipElement.getStringAttribute (xml::loopStart);
    auto loopEnd = clipElement.getStringAttribute (xml::loopEnd);

    if (loopStart.isNotEmpty() && loopEnd.isNotEmpty())
    {
        auto loopStartBeats = parseDouble (loopStart, 0.0);
        auto loopEndBeats = parseDouble (loopEnd, 0.0);

        if (loopEndBeats > loopStartBeats)
        {
            clip.setLoopRangeBeats (BeatRange { BeatPosition::fromBeats (loopStartBeats),
                                                BeatDuration::fromBeats (loopEndBeats - loopStartBeats) });
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseNotes (const juce::XmlElement& notesElement, MidiClip& clip)
{
    auto& sequence = clip.getSequence();
    auto* undoManager = &clip.edit.getUndoManager();

    bool isBeats = isTimeUnitBeats (notesElement);

    for (auto* child : notesElement.getChildIterator())
    {
        if (child->hasTagName (xml::Note))
        {
            auto timeStr = child->getStringAttribute (xml::time);
            auto durationStr = child->getStringAttribute (xml::duration);
            int key = parseInt (child->getStringAttribute (xml::key), 60);
            float vel = static_cast<float> (parseDouble (child->getStringAttribute (xml::vel), 0.8));

            double timeVal = parseDouble (timeStr, 0.0);
            double durationVal = parseDouble (durationStr, 0.25);

            BeatPosition startBeat;
            BeatDuration lengthInBeats;

            if (isBeats)
            {
                startBeat = BeatPosition::fromBeats (timeVal);
                lengthInBeats = BeatDuration::fromBeats (durationVal);
            }
            else
            {
                // Convert from seconds to beats using tempo sequence
                auto& tempoSeq = clip.edit.tempoSequence;
                auto startTime = TimePosition::fromSeconds (timeVal);
                auto endTime = TimePosition::fromSeconds (timeVal + durationVal);

                startBeat = tempoSeq.toBeats (startTime);
                lengthInBeats = tempoSeq.toBeats (endTime) - startBeat;
            }

            // MidiList::addNote takes: pitch, startBeat, lengthInBeats, velocity, colourIndex, undoManager
            sequence.addNote (key,
                              startBeat,
                              lengthInBeats,
                              normalizedToVelocity (vel),
                              0,  // colourIndex (used for MIDI channel in tracktion)
                              undoManager);
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseControllerPoints (const juce::XmlElement& lanesElement, MidiClip& clip)
{
    auto& sequence = clip.getSequence();
    auto* undoManager = &clip.edit.getUndoManager();

    // Look for Points elements with controller targets
    for (auto* child : lanesElement.getChildIterator())
    {
        if (! child->hasTagName (xml::Points))
            continue;

        // Get the target to determine what type of controller this is
        auto* targetElement = child->getChildByName (xml::Target);
        if (targetElement == nullptr)
            continue;

        auto expressionStr = targetElement->getStringAttribute (xml::expression);
        if (expressionStr.isEmpty())
            continue;

        // Determine controller type
        int controllerType = expressionToControllerType (expressionStr);

        // For regular CCs, get the controller number from the target
        if (controllerType < 0 && expressionStr == xml::channelController)
        {
            controllerType = parseInt (targetElement->getStringAttribute (xml::controller), -1);
            if (controllerType < 0 || controllerType > 127)
                continue;
        }
        else if (controllerType < 0)
        {
            continue; // Unknown expression type
        }

        bool isBeats = isTimeUnitBeats (*child);

        // Parse all the RealPoint children
        for (auto* pointElement : child->getChildIterator())
        {
            if (! pointElement->hasTagName (xml::RealPoint))
                continue;

            double timeVal = parseDouble (pointElement->getStringAttribute (xml::time), 0.0);
            float normalizedValue = static_cast<float> (parseDouble (pointElement->getStringAttribute (xml::value), 0.5));

            BeatPosition beatPos;
            if (isBeats)
            {
                beatPos = BeatPosition::fromBeats (timeVal);
            }
            else
            {
                auto& tempoSeq = clip.edit.tempoSequence;
                beatPos = tempoSeq.toBeats (TimePosition::fromSeconds (timeVal));
            }

            int controllerValue = normalizedToControllerValue (normalizedValue, controllerType);

            sequence.addControllerEvent (beatPos, controllerType, controllerValue, undoManager);
        }
    }
}

//==============================================================================
void DAWprojectImporter::parseDevices (const juce::XmlElement& devicesElement, Edit& edit, Track& track)
{
    auto* audioTrack = dynamic_cast<AudioTrack*> (&track);
    if (audioTrack == nullptr)
        return;

    for (auto* child : devicesElement.getChildIterator())
    {
        if (child->hasTagName (xml::Vst2Plugin) ||
            child->hasTagName (xml::Vst3Plugin) ||
            child->hasTagName (xml::AuPlugin))
        {
            auto plugin = parsePlugin (*child, edit);
            if (plugin != nullptr)
                audioTrack->pluginList.insertPlugin (plugin, -1, nullptr);
        }
    }
}

//==============================================================================
Plugin::Ptr DAWprojectImporter::parsePlugin (const juce::XmlElement& pluginElement, Edit& edit)
{
    auto deviceName = pluginElement.getStringAttribute (xml::deviceName);
    auto deviceID = pluginElement.getStringAttribute (xml::deviceID);

    // Try to find the plugin by ID or name
    juce::String pluginFormat;
    if (pluginElement.hasTagName (xml::Vst3Plugin))
        pluginFormat = "VST3";
    else if (pluginElement.hasTagName (xml::Vst2Plugin))
        pluginFormat = "VST";
    else if (pluginElement.hasTagName (xml::AuPlugin))
        pluginFormat = "AudioUnit";

    // Create the plugin using the plugin manager
    auto& pluginManager = edit.engine.getPluginManager();
    auto knownPlugins = pluginManager.knownPluginList.getTypes();

    for (const auto& desc : knownPlugins)
    {
        bool matches = false;

        if (deviceID.isNotEmpty())
            matches = desc.createIdentifierString() == deviceID || desc.fileOrIdentifier == deviceID;

        if (! matches && deviceName.isNotEmpty())
            matches = desc.name == deviceName || desc.descriptiveName == deviceName;

        if (matches && (pluginFormat.isEmpty() || desc.pluginFormatName == pluginFormat))
        {
            auto plugin = edit.getPluginCache().createNewPlugin (ExternalPlugin::xmlTypeName, desc);
            if (plugin != nullptr)
            {
                // Set enabled state
                if (auto* enabledElement = pluginElement.getChildByName (xml::Enabled))
                {
                    auto enabled = parseBool (enabledElement->getStringAttribute (xml::value), true);
                    plugin->setEnabled (enabled);
                }

                return plugin;
            }
        }
    }

    DBG ("DAWproject: Could not find plugin: " << deviceName << " (" << deviceID << ")");
    return nullptr;
}

//==============================================================================
juce::File DAWprojectImporter::resolveAudioFile (const juce::String& path)
{
    if (path.isEmpty())
        return {};

    // Check if it's an absolute path
    if (juce::File::isAbsolutePath (path))
    {
        juce::File file (path);
        if (file.existsAsFile())
            return file;
    }

    // Try relative to audio file directory
    auto relativeFile = audioFileDirectory.getChildFile (path);
    if (relativeFile.existsAsFile())
        return relativeFile;

    return {};
}

//==============================================================================
void DAWprojectImporter::extractAudioFiles (juce::ZipFile& zipFile, const juce::File& destDir)
{
    for (int i = 0; i < zipFile.getNumEntries(); ++i)
    {
        auto* entry = zipFile.getEntry (i);
        if (entry == nullptr)
            continue;

        auto filename = entry->filename;

        // Extract files from audio/ directory
        if (filename.startsWith ("audio/") || filename.endsWithIgnoreCase (".wav") ||
            filename.endsWithIgnoreCase (".aiff") || filename.endsWithIgnoreCase (".mp3") ||
            filename.endsWithIgnoreCase (".flac") || filename.endsWithIgnoreCase (".ogg"))
        {
            auto destFile = destDir.getChildFile (juce::File::createLegalFileName (filename.fromLastOccurrenceOf ("/", false, false)));
            zipFile.uncompressEntry (i, destFile);
        }
    }
}

//==============================================================================
TimePosition DAWprojectImporter::parseTime (const juce::XmlElement& element, const char* attrName, bool isBeats) const
{
    auto value = parseDouble (element.getStringAttribute (attrName), 0.0);
    juce::ignoreUnused (isBeats); // TODO: Convert from beats using tempo sequence if needed
    return TimePosition::fromSeconds (value);
}

TimeDuration DAWprojectImporter::parseDuration (const juce::XmlElement& element, const char* attrName, bool isBeats) const
{
    auto value = parseDouble (element.getStringAttribute (attrName), 0.0);
    juce::ignoreUnused (isBeats); // TODO: Convert from beats using tempo sequence if needed
    return TimeDuration::fromSeconds (value);
}

bool DAWprojectImporter::isTimeUnitBeats (const juce::XmlElement& element) const
{
    auto timeUnit = element.getStringAttribute (xml::timeUnit);
    return timeUnit.isEmpty() || timeUnit == xml::beats;
}

}}} // namespace tracktion::engine::dawproject
