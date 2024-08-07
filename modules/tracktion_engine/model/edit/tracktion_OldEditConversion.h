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

//==============================================================================
/**
    Logic for reading legacy tracktion edit files.

    (This is an internal class, not intended for public use)
*/
struct OldEditConversion
{
    static juce::ValueTree convert (const juce::ValueTree& v)
    {
        jassert (v.isValid());

        if (auto xml = v.createXml())
        {
            convert (*xml);
            return juce::ValueTree::fromXml (*xml);
        }

        return v;
    }

    static void convert (juce::XmlElement& editXML)
    {
        CRASH_TRACER

        updateV2TempoData (editXML);
        convertV2Markers (editXML);
        convertVideo (editXML);
        convertOldView (editXML);
        convertV9PitchSequence (editXML);
        convertV9Markers (editXML);
        convertMidiVersion (editXML);
        convertMpeVersion (editXML);
        convertPluginsAndClips (editXML);
        fixTrackTags (editXML);
        convertFolderTracks (editXML);
        convertLegacyLFOs (editXML);
        convertLegacyIDsIfNeeded (editXML);
    }

private:
    //==============================================================================
    static void renameAttribute (juce::XmlElement& e, const juce::Identifier& oldAtt, const juce::Identifier& newAtt)
    {
        if (e.hasAttribute (oldAtt))
        {
            e.setAttribute (newAtt, e.getStringAttribute (oldAtt));
            e.removeAttribute (oldAtt);
        }
    }

    static void moveAttribute (juce::XmlElement& oldXML, juce::XmlElement& newXML, const juce::Identifier& name)
    {
        if (oldXML.hasAttribute (name))
        {
            newXML.setAttribute (name, oldXML.getStringAttribute (name));
            oldXML.removeAttribute (name);
        }
    }

    static void moveAttribute (juce::XmlElement& oldXML, juce::XmlElement& newXML, const juce::Identifier& oldName, const juce::Identifier& newName)
    {
        if (oldXML.hasAttribute (oldName))
        {
            newXML.setAttribute (newName, oldXML.getStringAttribute (oldName));
            oldXML.removeAttribute (oldName);
        }
    }
    static void convertSequenceMidiVersion (juce::XmlElement& xml, double timeToBeatFactor)
    {
        for (auto seq : xml.getChildWithTagNameIterator ("MIDISEQUENCE"))
        {
            if (seq->getIntAttribute (IDs::ver, 0) < 1)
            {
                for (auto e : seq->getChildIterator())
                {
                    if (e->hasTagName (IDs::NOTE))
                    {
                        const double startBeat = timeToBeatFactor * e->getDoubleAttribute (IDs::t);
                        const double lengthInBeats = juce::jmax (0.0, timeToBeatFactor * e->getDoubleAttribute (IDs::l));
                        e->removeAttribute (IDs::t);
                        e->setAttribute (IDs::b, startBeat);
                        e->setAttribute (IDs::l, lengthInBeats);
                    }
                    else if (e->hasTagName (IDs::CONTROL))
                    {
                        const double startBeat = timeToBeatFactor * e->getDoubleAttribute (IDs::time);
                        e->removeAttribute (IDs::time);
                        e->setAttribute (IDs::b, startBeat);
                    }
                    else if (e->hasTagName (IDs::SYSEX))
                    {
                        const double startBeat = timeToBeatFactor * e->getDoubleAttribute (IDs::time);
                        e->setAttribute (IDs::time, startBeat);
                    }
                }
            }
        }

        for (auto e : xml.getChildIterator())
            if (! e->hasTagName ("MIDISEQUENCE"))
                convertSequenceMidiVersion (*e, timeToBeatFactor);

        // Convert note automation to midi expression, and tag the note automation as
        // converted so it doesn't happen again.
        for (auto seq : xml.getChildWithTagNameIterator ("MIDISEQUENCE"))
        {
            for (auto note : seq->getChildWithTagNameIterator (IDs::NOTE))
            {
                if (auto na = note->getChildByName (IDs::NA))
                {
                    if (auto* naseq = na->getChildByName ("MIDISEQUENCE"))
                    {
                        // If this sequence has already been converted, don't convert again
                        if (naseq->getBoolAttribute (IDs::converted, false))
                            continue;

                        naseq->setAttribute (IDs::converted, true);

                        for (auto control : naseq->getChildWithTagNameIterator (IDs::CONTROL))
                        {
                            // Timbre
                            if (control->getIntAttribute (IDs::type) == 71)
                            {
                                auto expression = new juce::XmlElement (IDs::TIMBRE);

                                expression->setAttribute (IDs::b, control->getDoubleAttribute (IDs::b));
                                expression->setAttribute (IDs::v, control->getIntAttribute (IDs::val) / 16383.0);

                                note->addChildElement (expression);
                            }
                            // Pitch wheel
                            if (control->getIntAttribute (IDs::type) == 4101)
                            {
                                auto range = juce::NormalisableRange<float> (-48.0f, 48.0f);

                                auto expression = new juce::XmlElement (IDs::PITCHBEND);
                                expression->setAttribute (IDs::b, control->getDoubleAttribute (IDs::b));
                                expression->setAttribute (IDs::v, range.convertFrom0to1 (control->getIntAttribute (IDs::val) / 16383.0f));
                                note->addChildElement (expression);
                            }
                            // Channel Pressure
                            if (control->getIntAttribute (IDs::type) == 4103)
                            {
                                auto expression = new juce::XmlElement (IDs::PRESSURE);
                                expression->setAttribute (IDs::b, control->getDoubleAttribute (IDs::b));
                                expression->setAttribute (IDs::v, control->getIntAttribute (IDs::val) / 16383.0f);
                                note->addChildElement (expression);
                            }

                        }
                    }
                }
            }
        }
    }

    static void convertTrackMidiVersion (juce::XmlElement& track, double editTempo)
    {
        for (auto clip : track.getChildWithTagNameIterator ("CLIP"))
        {
            const double clipSpeed = clip->getDoubleAttribute (IDs::speed, 1.0);
            const double timeToBeatFactor = (editTempo / 60.0) / clipSpeed;
            convertSequenceMidiVersion (*clip, timeToBeatFactor);
        }
    }

    static void convertMidiVersion (juce::XmlElement& editXml)
    {
        CRASH_TRACER

        if (auto t = editXml.getChildByName (IDs::TEMPOSEQUENCE))
        {
            auto editTempo = t->getDoubleAttribute (IDs::bpm);

            for (auto e : editXml.getChildWithTagNameIterator (IDs::TRACK))
                convertTrackMidiVersion (*e, editTempo);
        }
    }

    static void convertMpeVersion (juce::XmlElement& xmlToSearchRecursively)
    {
        // We recursively search for CLIP elements, because NOISE's clips
        // live in SLOTs, whose Identifier isn't available here.
        for (auto element : xmlToSearchRecursively.getChildIterator())
        {
            if (element->hasTagName ("CLIP"))
            {
                if (auto* sequence = element->getChildByName ("MIDISEQUENCE"))
                {
                    for (auto note : sequence->getChildWithTagNameIterator (IDs::NOTE))
                    {
                        if (note->getFirstChildElement() != nullptr
                             && ! note->hasAttribute (IDs::bend))
                        {
                            if (auto firstTimbre = note->getChildByName (IDs::TIMBRE))
                                note->setAttribute (IDs::timb, firstTimbre->getDoubleAttribute ("v"));

                            if (auto firstPressure = note->getChildByName (IDs::PRESSURE))
                                note->setAttribute (IDs::pres, firstPressure->getDoubleAttribute ("v"));

                            if (auto firstPitchbend = note->getChildByName (IDs::PITCHBEND))
                                note->setAttribute (IDs::bend, firstPitchbend->getDoubleAttribute ("v"));
                        }
                    }
                }
            }
            else
            {
                convertMpeVersion (*element);
            }
        }
    }

    static void updateOldStepClip (juce::XmlElement& xml)
    {
        auto patterns = new juce::XmlElement (IDs::PATTERNS);
        auto channels = new juce::XmlElement (IDs::CHANNELS);

        for (auto e : xml.getChildWithTagNameIterator (IDs::PATTERN))
            patterns->addChildElement (new juce::XmlElement (*e));

        for (auto e : xml.getChildWithTagNameIterator (IDs::CHANNEL))
            channels->addChildElement (new juce::XmlElement (*e));

        xml.deleteAllChildElements();

        xml.addChildElement (channels);
        xml.addChildElement (patterns);

        for (auto e : patterns->getChildIterator())
        {
            for (auto chan : e->getChildWithTagNameIterator (IDs::CHANNEL))
            {
                chan->setAttribute (IDs::pattern, chan->getAllSubText().trim());
                chan->deleteAllChildElements();
                chan->deleteAllTextElements();
            }
        }

        for (auto e : channels->getChildIterator())
        {
            renameAttribute (*e, IDs::noteNumber, IDs::note);
            renameAttribute (*e, IDs::noteValue, IDs::velocity);
        }
    }

    static void convertPluginsAndClips (juce::XmlElement& xml)
    {
        for (auto e : xml.getChildIterator())
        {
            convertPluginsAndClips (*e);

            if (e->hasTagName (IDs::PLUGIN))
            {
                if (auto* vstData = e->getChildByName (IDs::VSTDATA))
                {
                    e->setAttribute (IDs::state, vstData->getAllSubText().trim());
                    e->removeChildElement (vstData, true);
                }
                else if (e->getStringAttribute (IDs::type) == AuxSendPlugin::xmlTypeName)
                {
                    if (e->hasAttribute (IDs::gain))
                    {
                        e->setAttribute (IDs::auxSendSliderPos, decibelsToVolumeFaderPosition ((float) e->getDoubleAttribute (IDs::gain, 0.0f)));
                        e->removeAttribute (IDs::gain);
                    }
                }
            }
            else if (e->hasTagName ("CLIP"))
            {
                if (e->getChildByName (IDs::CHANNEL) != nullptr || e->getChildByName (IDs::PATTERN) != nullptr)
                    updateOldStepClip (*e);
            }
        }
    }

    static void updateV2TempoData (juce::XmlElement& xml)
    {
        juce::Array<juce::XmlElement*> oldTempos;

        for (auto e : xml.getChildWithTagNameIterator (IDs::TEMPO))
            oldTempos.add (e);

        if (oldTempos.size() > 0)
        {
            auto tempoSequence = xml.getChildByName (IDs::TEMPOSEQUENCE);

            if (tempoSequence == nullptr)
                tempoSequence = xml.createNewChildElement (IDs::TEMPOSEQUENCE);

            for (auto* e : oldTempos)
            {
                juce::XmlElement tempo (IDs::TEMPO);
                tempo.setAttribute (IDs::startBeat, e->getIntAttribute (IDs::startBeat));
                tempo.setAttribute (IDs::bpm, e->getDoubleAttribute (IDs::bpm));
                tempo.setAttribute (IDs::curve, e->getIntAttribute (IDs::ramped) ? 0.0f : 1.0f);

                juce::XmlElement timesig (IDs::TIMESIG);
                timesig.setAttribute (IDs::startBeat, e->getIntAttribute (IDs::startBeat));
                timesig.setAttribute (IDs::numerator, e->getIntAttribute (IDs::numerator));
                timesig.setAttribute (IDs::denominator, e->getIntAttribute (IDs::denominator));
                timesig.setAttribute (IDs::triplets, e->getIntAttribute (IDs::triplets));

                tempoSequence->addChildElement (new juce::XmlElement (tempo));
                tempoSequence->addChildElement (new juce::XmlElement (timesig));

                xml.removeChildElement (e, true);
            }
        }
        else if (auto tempoSequence = xml.getChildByName (IDs::TEMPOSEQUENCE))
        {
            bool foundOld = false;

            for (auto e : tempoSequence->getChildWithTagNameIterator (IDs::TEMPO))
            {
                if (! (e->hasAttribute (IDs::numerator) || e->hasAttribute (IDs::denominator) || e->hasAttribute (IDs::triplets)))
                    continue;

                oldTempos.add (e);
                foundOld = true;
            }

            if (foundOld)
            {
                for (auto e : oldTempos)
                {
                    juce::XmlElement timesig (IDs::TIMESIG);
                    timesig.setAttribute (IDs::startBeat, e->getIntAttribute (IDs::startBeat));
                    timesig.setAttribute (IDs::numerator, e->getIntAttribute (IDs::numerator));
                    timesig.setAttribute (IDs::denominator, e->getIntAttribute (IDs::denominator));
                    timesig.setAttribute (IDs::triplets, e->getIntAttribute (IDs::triplets));

                    e->setAttribute (IDs::curve, e->getIntAttribute (IDs::ramped) ? 0.0f : 1.0f);

                    e->removeAttribute (IDs::numerator);
                    e->removeAttribute (IDs::denominator);
                    e->removeAttribute (IDs::triplets);
                    e->removeAttribute (IDs::ramped);

                    tempoSequence->addChildElement (new juce::XmlElement (timesig));
                }
            }
        }
    }

    static void convertV2Markers (juce::XmlElement& xml)
    {
        if (auto viewStateXML = xml.getChildByName ("VIEWSTATE"))
        {
            juce::Array<int> numbers;
            juce::Array<double> times;

            for (auto e : viewStateXML->getChildWithTagNameIterator (IDs::MARKER))
            {
                numbers.add (e->getIntAttribute (IDs::number));
                times.add (e->getDoubleAttribute (IDs::time));
            }

            if (numbers.size() > 0)
            {
                auto markerTrack = xml.getChildByName (IDs::MARKERTRACK);

                if (markerTrack == nullptr)
                {
                    markerTrack = xml.createNewChildElement (IDs::MARKERTRACK);

                    markerTrack->setAttribute (IDs::name, TRANS("Marker"));
                    markerTrack->setAttribute (IDs::trackType, (int) 0 /*Track::Type::markerCombined*/);
                }

                for (int i = 0; i < numbers.size(); ++i)
                {
                    auto clip = markerTrack->createNewChildElement ("CLIP");
                    clip->setAttribute (IDs::name, "Marker");
                    clip->setAttribute (IDs::markerID, numbers[i]);
                    clip->setAttribute (IDs::type, "absoluteMarker");
                    clip->setAttribute (IDs::start, times[i]);
                    clip->setAttribute (IDs::length, 2.0);
                }
            }
        }
    }

    static void convertVideo (juce::XmlElement& xml)
    {
        if (xml.getChildByName (IDs::VIDEO) != nullptr)
            return;

        if (auto viewStateXML = xml.getChildByName ("VIEWSTATE"))
        {
            auto videoXML = xml.createNewChildElement (IDs::VIDEO);

            moveAttribute (*viewStateXML, *videoXML, "videoFile");
            moveAttribute (*viewStateXML, *videoXML, IDs::videoOffset);
            moveAttribute (*viewStateXML, *videoXML, IDs::videoMuted);
            moveAttribute (*viewStateXML, *videoXML, IDs::videoSource);
        }
    }

    static void convertOldView (juce::XmlElement& xml)
    {
        if (auto viewStateXML = xml.getChildByName ("VIEWSTATE"))
        {
            auto transportXML = xml.getChildByName (IDs::TRANSPORT);

            if (transportXML == nullptr)
                transportXML = xml.createNewChildElement (IDs::TRANSPORT);

            moveAttribute (*viewStateXML, *transportXML, IDs::markIn, IDs::loopPoint1);
            moveAttribute (*viewStateXML, *transportXML, IDs::markOut, IDs::loopPoint2);

            moveAttribute (*viewStateXML, *transportXML, IDs::loopPlayback, IDs::looping);
            moveAttribute (*viewStateXML, *transportXML, IDs::automationRead);
            moveAttribute (*viewStateXML, *transportXML, IDs::recordPunchInOut);
            moveAttribute (*viewStateXML, *transportXML, IDs::endToEnd);

            moveAttribute (*viewStateXML, *transportXML, IDs::midiTimecodeOffset);
            moveAttribute (*viewStateXML, *transportXML, IDs::midiTimecodeEnabled);
            moveAttribute (*viewStateXML, *transportXML, IDs::midiTimecodeIgnoringHours);
            moveAttribute (*viewStateXML, *transportXML, IDs::midiTimecodeSourceDevice);
            moveAttribute (*viewStateXML, *transportXML, IDs::midiMachineControlSourceDevice);
            moveAttribute (*viewStateXML, *transportXML, IDs::midiMachineControlDestDevice);
        }
    }

    static void convertV9PitchSequence (juce::XmlElement& xml)
    {
        if (auto pitchSequence = xml.getChildByName (IDs::PITCHSEQUENCE))
            for (auto e : pitchSequence->getChildWithTagNameIterator (IDs::PITCH))
                renameAttribute (*e, IDs::start, IDs::startBeat);
    }

    static void convertV9Markers (juce::XmlElement& xml)
    {
        if (auto firstMarkerTrack = xml.getChildByName (IDs::MARKERTRACK))
        {
            juce::Array<juce::XmlElement*> tracksToRemove;

            for (auto markerTrack : xml.getChildWithTagNameIterator (IDs::MARKERTRACK))
            {
                if (firstMarkerTrack == markerTrack)
                    continue;

                for (auto clip : markerTrack->getChildWithTagNameIterator ("CLIP"))
                    firstMarkerTrack->addChildElement (new juce::XmlElement (*clip));

                tracksToRemove.add (markerTrack);
            }

            for (auto markerTrack : tracksToRemove)
                xml.removeChildElement (markerTrack, true);

            firstMarkerTrack->removeAttribute (IDs::trackType);
        }
    }

    static juce::XmlElement* getParentWithId (juce::Array<juce::XmlElement*>& tracks, EditItemID parentID)
    {
        for (auto e : tracks)
            if (EditItemID::fromXML (*e, "mediaId") == parentID)
                return e;

        return {};
    }

    static void addTracks (juce::Array<juce::XmlElement*>& tracks, juce::XmlElement& xml)
    {
        for (auto e : xml.getChildIterator())
        {
            if (TrackList::isTrack (e->getTagName()))
            {
                tracks.add (e);
                addTracks (tracks, *e);
            }
        }
    }

    static void fixTrackTags (juce::XmlElement& xml)
    {
        juce::Array<juce::XmlElement*> tracks;
        addTracks (tracks, xml);

        for (auto e : tracks)
        {
            if (! e->hasAttribute (IDs::tags))
                continue;

            auto tagsString = e->getStringAttribute (IDs::tags);

            if (tagsString.contains (","))
                tagsString = juce::StringArray::fromTokens (tagsString, ",", "\"").joinIntoString ("|");

            auto tags = juce::StringArray::fromTokens (tagsString, "|", "\"");

            for (auto& tag : tags)
                tag = tag.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");

            tags.trim();
            tags.removeEmptyStrings();
            e->setAttribute (IDs::tags, tags.joinIntoString ("|"));
        }
    }

    static void convertFolderTracks (juce::XmlElement& xml)
    {
        juce::Array<juce::XmlElement*> tracks;
        addTracks (tracks, xml);

        for (auto e : tracks)
        {
            auto parentID = EditItemID::fromXML (*e, "parentId");
            e->removeAttribute ("parentId");

            if (! parentID.isValid())
                continue;

            if (auto p = getParentWithId (tracks, parentID))
            {
                if (auto p2 = xml.findParentElementOf (e))
                    p2->removeChildElement (e, false);
                else
                    { jassertfalse; }

                p->addChildElement (e);
            }
            else
            {
                jassertfalse; // Missing folder track?
            }
        }
    }

    static void addNodeRecursively (juce::Array<juce::XmlElement*>& plugins,
                                    juce::XmlElement& xml, const juce::Identifier& tagName)
    {
        if (xml.hasTagName (tagName))
            plugins.add (&xml);

        for (auto e : xml.getChildIterator())
            addNodeRecursively (plugins, *e, tagName);
    }

    static void convertLegacyLFOs (juce::XmlElement& xml)
    {
        juce::Array<juce::XmlElement*> tracks, plugins, lfosToDelete, automationSourcesToDelete;
        addTracks (tracks, xml);
        addNodeRecursively (plugins, xml, IDs::PLUGIN);

        // This will be done before the FILTER -> PLUGIN conversion so we need to account for both
        addNodeRecursively (plugins, xml, "FILTER");

        // - Convert LFOS to MODIFIERS
        // Find LFOS node, rename to MODIFIERS
        // For each LFO child change:
        // depth = intensity / 100
        // phase /= 360
        // syncType = transport
        // rateType = (sync 0 == time, sync 1 == beat)
        // if sync == time, rate = frequency
        // if sync == beat, rate = (whole = 1, half = 0.5, quarter = 0.25, eighth = 0.125, sixteenth = 0.0625)

        for (auto t : tracks)
        {
            if (auto lfos = t->getChildByName (IDs::LFOS))
            {
                lfosToDelete.add (lfos);

                auto modifiers = t->getChildByName (IDs::MODIFIERS);

                if (modifiers == nullptr)
                    modifiers = t->createNewChildElement (IDs::MODIFIERS);

                for (auto lfo : lfos->getChildWithTagNameIterator (IDs::LFO))
                {
                    auto newLFO = new juce::XmlElement (*lfo);
                    modifiers->addChildElement (newLFO);
                    newLFO->setAttribute (IDs::syncType, 1);
                    newLFO->setAttribute (IDs::depth, lfo->getDoubleAttribute (IDs::intensity, 50.0) / 100.0);
                    newLFO->setAttribute (IDs::phase, lfo->getDoubleAttribute (IDs::phase) / 360.0);

                    if (lfo->getIntAttribute (IDs::sync, 1) == 0)
                    {
                        // time in Hz
                        newLFO->setAttribute (IDs::rateType, 0);
                        newLFO->setAttribute (IDs::rate, lfo->getDoubleAttribute (IDs::frequency, 1.0));
                    }
                    else
                    {
                        // beat (whole = 1, half = 0.5, quarter = 0.25, eighth = 0.125, sixteenth = 0.0625)
                        newLFO->setAttribute (IDs::rateType, lfo->getIntAttribute (IDs::beat, 0) + 1.0);
                        newLFO->setAttribute (IDs::rate, 1.0);
                    }
                }
            }
        }

        // - Convert AUTOMATIONSOURCE to LFO
        // find AUTOMATIONSOURCE with lfoSource
        // rename to LFO
        // get paramName, store to paramID
        // get lfoSource, store to source
        // set value to 1
        // add to (might have to create) sibling MODIFIERASSIGNMENTS

        for (auto f : plugins)
        {
            auto modifierAssignments = f->getChildByName (IDs::MODIFIERASSIGNMENTS);

            if (modifierAssignments == nullptr)
                modifierAssignments = f->createNewChildElement (IDs::MODIFIERASSIGNMENTS);

            for (auto e : f->getChildWithTagNameIterator (IDs::AUTOMATIONSOURCE))
            {
                if (e->hasAttribute (IDs::lfoSource))
                {
                    auto lfo = modifierAssignments->createNewChildElement (IDs::LFO);
                    lfo->setAttribute (IDs::source, e->getStringAttribute (IDs::lfoSource));
                    lfo->setAttribute (IDs::paramID, e->getStringAttribute (IDs::paramName));
                    lfo->setAttribute (IDs::value, 1.0);

                    automationSourcesToDelete.add (e);
                }
            }
        }

        for (auto lfos : lfosToDelete)
            if (auto p = xml.findParentElementOf (lfos))
                p->removeChildElement (lfos, true);

        for (auto automationSource : automationSourcesToDelete)
            if (auto p = xml.findParentElementOf (automationSource))
                p->removeChildElement (automationSource, true);
    }

    static void convertLegacyIDs (juce::XmlElement& xml)
    {
        renameLegacyIDs (xml);

        uint64_t nextID = 1001;
        EditItemID::remapIDs (xml, [&] { return EditItemID::fromRawID (nextID++); });

        recurseDoingLegacyConversions (xml);
    }

    static void convertLegacyIDsIfNeeded (juce::XmlElement& xml)
    {
        if (xml.hasTagName (IDs::EDIT))
        {
            if (xml.getStringAttribute (IDs::projectID).isEmpty())
            {
                renameAttribute (xml, "mediaId", IDs::projectID);
                moveXMLAttributeToStart (xml, IDs::projectID);
                moveXMLAttributeToStart (xml, IDs::appVersion);

                convertLegacyIDs (xml);
            }

            convertLegacyTimecodes (xml);
            convertLegacyInputTargets (xml);
        }
        else
        {
            convertLegacyIDs (xml);
        }
    }

    static void tidyIDListDelimiters (juce::XmlElement& xml, const juce::Identifier& att)
    {
        if (xml.hasAttribute (att))
            xml.setAttribute (att, xml.getStringAttribute (att).replace ("|", ","));
    }

    static void renameLegacyIDs (juce::XmlElement& xml)
    {
        for (auto e : xml.getChildIterator())
            renameLegacyIDs (*e);

        if (xml.hasTagName (IDs::TAKE) || xml.hasTagName (IDs::SOUND))
        {
            renameAttribute (xml, "mediaId", IDs::source);
        }
        else if (ModifierList::isModifier (xml.getTagName())
                  || TrackList::isTrack (xml.getTagName())
                  || xml.hasTagName ("CLIP")
                  || xml.hasTagName ("PRESETMETADATA")
                  || xml.hasTagName (IDs::MACROPARAMETERS)
                  || xml.hasTagName (IDs::MACROPARAMETER))
        {
            jassert (! (xml.hasAttribute ("mediaId") && xml.hasAttribute (IDs::id)));
            renameAttribute (xml, "mediaId", IDs::id);
            renameAttribute (xml, "markerId", IDs::markerID);
            renameAttribute (xml, "groupId", IDs::groupID);
            renameAttribute (xml, "linkId", IDs::linkID);
            renameAttribute (xml, "currentAutoParamFilter", IDs::currentAutoParamPluginID);

            if (xml.hasAttribute ("maxNumChannels"))
            {
                if (! (xml.hasAttribute (IDs::mpeMode) || xml.getIntAttribute ("maxNumChannels") == 0))
                    xml.setAttribute (IDs::mpeMode, 1);

                xml.removeAttribute ("maxNumChannels");
            }

            tidyIDListDelimiters (xml, IDs::ghostTracks);
        }
        else if (xml.hasTagName (IDs::CONTROLLERMAPPINGS))
        {
            renameAttribute (xml, "filterid", IDs::pluginID);
        }
        else if (xml.hasTagName ("VIEWSTATE"))
        {
            renameAttribute (xml, "videoFileId", IDs::videoSource);
            renameAttribute (xml, "filterAreaWidth", IDs::pluginAreaWidth);
            renameAttribute (xml, "filtersVisible", IDs::pluginsVisible);
            tidyIDListDelimiters (xml, IDs::hiddenClips);
            tidyIDListDelimiters (xml, IDs::lockedClips);
        }
        else if (xml.hasTagName ("DEVICESEX"))
        {
            xml.setTagName (IDs::INPUTDEVICES);
        }
        else if (xml.hasTagName ("INPUTDEVICE"))
        {
            auto name = xml.getStringAttribute ("name");

            if (name.endsWith (" A") || name.endsWith (" M"))
            {
                xml.setAttribute (IDs::sourceTrack, name.upToLastOccurrenceOf (" ", false, false));
                xml.setAttribute (IDs::type, name.endsWith (" M") ? "MIDI" : "audio");
                xml.removeAttribute ("name");
            }
        }
        else if (xml.hasTagName (IDs::INPUTDEVICEDESTINATION))
        {
            renameAttribute (xml, "targetTrack", IDs::targetID);
        }
        else if (xml.hasTagName ("RENDER"))
        {
            renameAttribute (xml, "renderFilter", IDs::renderPlugins);
        }
        else
        {
            jassert (! xml.hasAttribute ("mediaId")); // missed a type?
        }

        moveXMLAttributeToStart (xml, IDs::id);
    }

    static void convertLegacyFilterTagsToPlugin (juce::XmlElement& xml)
    {
        if (xml.hasTagName ("FILTER"))            xml.setTagName (IDs::PLUGIN);
        if (xml.hasTagName ("FILTERINSTANCE"))    xml.setTagName (IDs::PLUGININSTANCE);
        if (xml.hasTagName ("FILTERCONNECTION"))  xml.setTagName (IDs::CONNECTION);
        if (xml.hasTagName ("MASTERFILTERS"))     xml.setTagName (IDs::MASTERPLUGINS);
        if (xml.hasTagName ("RACKFILTER"))        xml.setTagName (IDs::RACK);
        if (xml.hasTagName ("RACKFILTERS"))       xml.setTagName (IDs::RACKS);
    }

    static void convertLegacyRackConnectionIDs (juce::XmlElement& xml)
    {
        if (! xml.hasTagName (IDs::CONNECTION))
            return;

        if (xml.getStringAttribute (IDs::src) == "0/0") xml.setAttribute (IDs::src, "0");
        if (xml.getStringAttribute (IDs::dst) == "0/0") xml.setAttribute (IDs::dst, "0");
    }

    static void convertLegacyClipFormat (juce::XmlElement& xml)
    {
        if (xml.hasTagName ("CLIP"))
        {
            auto type = TrackItem::stringToType (xml.getStringAttribute ("type"));
            xml.setTagName (TrackItem::clipTypeToXMLType (type));
            xml.removeAttribute ("type");
        }
    }

    static void setNewTimecode (juce::XmlElement& xml, TimecodeType type)
    {
        xml.setAttribute (IDs::timecodeFormat, juce::VariantConverter<TimecodeDisplayFormat>::toVar (type).toString());
    }

    static void convertLegacyTimecodes (juce::XmlElement& xml)
    {
        auto timeFormat = xml.getStringAttribute (IDs::timecodeFormat);
        auto fps = xml.getIntAttribute (IDs::fps);
        xml.removeAttribute (IDs::fps);

        if (timeFormat == "0" || timeFormat.contains ("milli"))  return setNewTimecode (xml, TimecodeType::millisecs);
        if (timeFormat == "1" || timeFormat.contains ("beat"))   return setNewTimecode (xml, TimecodeType::barsBeats);

        if (timeFormat == "2")  return setNewTimecode (xml, TimecodeType::fps24);
        if (timeFormat == "3")  return setNewTimecode (xml, TimecodeType::fps25);
        if (timeFormat == "4")  return setNewTimecode (xml, TimecodeType::fps30);

        if (fps != 0 && timeFormat == "frames")
        {
            if (fps == 24)  return setNewTimecode (xml, TimecodeType::fps24);
            if (fps == 25)  return setNewTimecode (xml, TimecodeType::fps25);
            if (fps == 30)  return setNewTimecode (xml, TimecodeType::fps30);
        }
    }

    static void convertLegacyMidiSequences (juce::XmlElement& xml)
    {
        if (xml.hasTagName ("MIDISEQUENCE"))
        {
            xml.setTagName (IDs::SEQUENCE);
            xml.removeAttribute ("ver");
        }
        else if (xml.hasTagName ("NOTE"))
        {
            renameAttribute (xml, "noteOffVelocity", IDs::lift);

            if (xml.getDoubleAttribute ("initialTimbre") == MidiList::defaultInitialTimbreValue)
                xml.removeAttribute ("initialTimbre");
            else
                renameAttribute (xml, "initialTimbre", IDs::timb);

            if (xml.getDoubleAttribute ("initialPressure") == MidiList::defaultInitialPressureValue)
                xml.removeAttribute ("initialPressure");
            else
                renameAttribute (xml, "initialPressure", IDs::pres);

            if (xml.getDoubleAttribute ("initialPitchbend") == MidiList::defaultInitialPitchBendValue)
                xml.removeAttribute ("initialPitchbend");
            else
                renameAttribute (xml, "initialPitchbend", IDs::bend);
        }
    }

    static void convertLegacyInputTargets (juce::XmlElement& xml)
    {
        for (auto e : xml.getChildIterator())
            convertLegacyInputTargets (*e);

        if (xml.hasTagName (IDs::INPUTDEVICEDESTINATION))
            renameAttribute (xml, "targetTrack", IDs::targetID);
    }

    static void recurseDoingLegacyConversions (juce::XmlElement& xml)
    {
        for (auto e : xml.getChildIterator())
            recurseDoingLegacyConversions (*e);

        convertLegacyFilterTagsToPlugin (xml);
        convertLegacyRackConnectionIDs (xml);
        convertLegacyClipFormat (xml);
        convertLegacyMidiSequences (xml);
    }
};

juce::ValueTree updateLegacyEdit (const juce::ValueTree& v)   { return OldEditConversion::convert (v); }
void updateLegacyEdit (juce::XmlElement& editXML)             { OldEditConversion::convert (editXML); }

}} // namespace tracktion { inline namespace engine
