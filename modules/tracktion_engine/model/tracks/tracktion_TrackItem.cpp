/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

TrackItem::TrackItem (Edit& ed, EditItemID id, Type t)
    : EditItem (id, ed), type (t)
{
}

TrackItem::~TrackItem()
{
}

const char* TrackItem::typeToString (TrackItem::Type t)
{
    switch (t)
    {
        case Type::wave:          return "wave";
        case Type::midi:          return "midi";
        case Type::edit:          return "edit";
        case Type::step:          return "sequencer";
        case Type::marker:        return "marker";
        case Type::pitch:         return "pitch";
        case Type::timeSig:       return "timesig";
        case Type::collection:    return "collection";
        case Type::video:         return "video";
        case Type::chord:         return "chord";
        case Type::arranger:      return "arranger";
        case Type::container:     return "container";
        case Type::recording:
        case Type::unknown:
        default:                  return "unknown";
    }
}

TrackItem::Type TrackItem::stringToType (const juce::String& s)
{
    if (s == "wave")            return Type::wave;
    if (s == "midi")            return Type::midi;
    if (s == "edit")            return Type::edit;
    if (s == "sequencer")       return Type::step;
    if (s == "marker")          return Type::marker;
    if (s == "barsBeatsMarker") return Type::marker;
    if (s == "absoluteMarker")  return Type::marker;
    if (s == "pitch")           return Type::pitch;
    if (s == "timesig")         return Type::timeSig;
    if (s == "collection")      return Type::collection;
    if (s == "video")           return Type::video;
    if (s == "chord")           return Type::chord;
    if (s == "arranger")        return Type::arranger;
    if (s == "container")       return Type::container;

    return Type::unknown;
}

juce::Identifier TrackItem::clipTypeToXMLType (TrackItem::Type t)
{
    switch (t)
    {
        case Type::wave:          return IDs::AUDIOCLIP;
        case Type::midi:          return IDs::MIDICLIP;
        case Type::edit:          return IDs::EDITCLIP;
        case Type::step:          return IDs::STEPCLIP;
        case Type::marker:        return IDs::MARKERCLIP;
        case Type::chord:         return IDs::CHORDCLIP;
        case Type::arranger:      return IDs::ARRANGERCLIP;
        case Type::container:     return IDs::CONTAINERCLIP;
        case Type::pitch:
        case Type::timeSig:
        case Type::collection:
        case Type::video:
        case Type::recording:
        case Type::unknown:
        default:                  jassertfalse; return nullptr;
    }
}

TrackItem::Type TrackItem::xmlTagToType (juce::StringRef tag)
{
    if (tag == IDs::AUDIOCLIP)      return Type::wave;
    if (tag == IDs::MIDICLIP)       return Type::midi;
    if (tag == IDs::EDITCLIP)       return Type::edit;
    if (tag == IDs::STEPCLIP)       return Type::step;
    if (tag == IDs::MARKERCLIP)     return Type::marker;
    if (tag == IDs::CHORDCLIP)      return Type::chord;
    if (tag == IDs::ARRANGERCLIP)   return Type::arranger;
    if (tag == IDs::CONTAINERCLIP)  return Type::container;

    jassertfalse;
    return Type::unknown;
}

juce::String TrackItem::getSuggestedNameForNewItem (Type t)
{
    switch (t)
    {
        case Type::wave:          return TRANS("New Audio Clip");
        case Type::midi:          return TRANS("New MIDI Clip");
        case Type::edit:          return TRANS("New Edit Clip");
        case Type::step:          return TRANS("New Step Clip");
        case Type::marker:        return TRANS("New Marker");
        case Type::arranger:      return TRANS("New Arranger");
        case Type::container:     return TRANS("New Container Clip");
        case Type::chord:         return {};
        case Type::pitch:
        case Type::timeSig:
        case Type::collection:
        case Type::video:
        case Type::recording:
        case Type::unknown:
        default:                  jassertfalse; break;
    }

    return {};
}

EditItemID TrackItem::getTrackID() const
{
    if (auto track = getTrack())
        return track->itemID;

    jassertfalse;
    return {};
}

BeatRange TrackItem::getEditBeatRange() const        { return edit.tempoSequence.toBeats (getPosition().time); }
BeatPosition TrackItem::getStartBeat() const         { return edit.tempoSequence.toBeats (getPosition().getStart()); }
BeatPosition TrackItem::getContentStartBeat() const  { return edit.tempoSequence.toBeats (getPosition().getStartOfSource()); }
BeatDuration TrackItem::getLengthInBeats() const     { return getEndBeat() - getStartBeat(); }
BeatDuration TrackItem::getOffsetInBeats() const     { return BeatDuration::fromBeats (getPosition().getOffset().inSeconds() * edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart())); }

BeatPosition TrackItem::getEndBeat() const                              { return edit.tempoSequence.toBeats (getPosition().getEnd()); }
TimePosition TrackItem::getTimeOfRelativeBeat (BeatDuration b) const    { return edit.tempoSequence.toTime (getStartBeat() + b); }
BeatPosition TrackItem::getBeatOfRelativeTime (TimeDuration t) const    { return edit.tempoSequence.toBeats (getPosition().getStart() + t); }

}} // namespace tracktion { inline namespace engine
