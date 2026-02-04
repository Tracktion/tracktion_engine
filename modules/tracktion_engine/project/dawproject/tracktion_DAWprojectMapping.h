/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine { namespace dawproject
{

//==============================================================================
/** DAWproject XML element and attribute names. */
namespace xml
{
    // Root elements
    static constexpr auto Project = "Project";
    static constexpr auto Application = "Application";
    static constexpr auto Transport = "Transport";
    static constexpr auto Structure = "Structure";
    static constexpr auto Arrangement = "Arrangement";
    static constexpr auto Scenes = "Scenes";

    // Structure elements
    static constexpr auto Track = "Track";
    static constexpr auto Channel = "Channel";

    // Timeline elements
    static constexpr auto Lanes = "Lanes";
    static constexpr auto Clips = "Clips";
    static constexpr auto Clip = "Clip";
    static constexpr auto Notes = "Notes";
    static constexpr auto Note = "Note";
    static constexpr auto Audio = "Audio";
    static constexpr auto Video = "Video";
    static constexpr auto Warps = "Warps";
    static constexpr auto Warp = "Warp";
    static constexpr auto Points = "Points";
    static constexpr auto Markers = "Markers";
    static constexpr auto Marker = "Marker";

    // Parameter elements
    static constexpr auto Tempo = "Tempo";
    static constexpr auto TimeSignature = "TimeSignature";
    static constexpr auto Volume = "Volume";
    static constexpr auto Pan = "Pan";
    static constexpr auto Mute = "Mute";
    static constexpr auto RealParameter = "RealParameter";
    static constexpr auto BoolParameter = "BoolParameter";
    static constexpr auto IntegerParameter = "IntegerParameter";
    static constexpr auto EnumParameter = "EnumParameter";
    static constexpr auto TimeSignatureParameter = "TimeSignatureParameter";

    // Automation
    static constexpr auto Target = "Target";
    static constexpr auto RealPoint = "RealPoint";
    static constexpr auto BoolPoint = "BoolPoint";
    static constexpr auto IntegerPoint = "IntegerPoint";
    static constexpr auto EnumPoint = "EnumPoint";
    static constexpr auto TimeSignaturePoint = "TimeSignaturePoint";
    static constexpr auto TempoAutomation = "TempoAutomation";
    static constexpr auto TimeSignatureAutomation = "TimeSignatureAutomation";

    // Device/Plugin elements
    static constexpr auto Devices = "Devices";
    static constexpr auto Device = "Device";
    static constexpr auto Vst2Plugin = "Vst2Plugin";
    static constexpr auto Vst3Plugin = "Vst3Plugin";
    static constexpr auto AuPlugin = "AuPlugin";
    static constexpr auto ClapPlugin = "ClapPlugin";
    static constexpr auto BuiltinDevice = "BuiltinDevice";
    static constexpr auto Parameters = "Parameters";
    static constexpr auto State = "State";
    static constexpr auto Enabled = "Enabled";

    // Mixer elements
    static constexpr auto Sends = "Sends";
    static constexpr auto Send = "Send";

    // File reference
    static constexpr auto File = "File";

    // Attributes
    static constexpr auto version = "version";
    static constexpr auto name = "name";
    static constexpr auto color = "color";
    static constexpr auto comment = "comment";
    static constexpr auto id = "id";
    static constexpr auto contentType = "contentType";
    static constexpr auto loaded = "loaded";
    static constexpr auto role = "role";
    static constexpr auto destination = "destination";
    static constexpr auto audioChannels = "audioChannels";
    static constexpr auto solo = "solo";
    static constexpr auto time = "time";
    static constexpr auto duration = "duration";
    static constexpr auto timeUnit = "timeUnit";
    static constexpr auto contentTimeUnit = "contentTimeUnit";
    static constexpr auto fadeTimeUnit = "fadeTimeUnit";
    static constexpr auto playStart = "playStart";
    static constexpr auto playStop = "playStop";
    static constexpr auto loopStart = "loopStart";
    static constexpr auto loopEnd = "loopEnd";
    static constexpr auto fadeInTime = "fadeInTime";
    static constexpr auto fadeOutTime = "fadeOutTime";
    static constexpr auto enable = "enable";
    static constexpr auto reference = "reference";
    static constexpr auto track = "track";
    static constexpr auto unit = "unit";
    static constexpr auto value = "value";
    static constexpr auto min = "min";
    static constexpr auto max = "max";
    static constexpr auto numerator = "numerator";
    static constexpr auto denominator = "denominator";
    static constexpr auto parameterID = "parameterID";
    static constexpr auto interpolation = "interpolation";
    static constexpr auto channel = "channel";
    static constexpr auto key = "key";
    static constexpr auto vel = "vel";
    static constexpr auto rel = "rel";
    static constexpr auto sampleRate = "sampleRate";
    static constexpr auto channels = "channels";
    static constexpr auto path = "path";
    static constexpr auto external = "external";
    static constexpr auto algorithm = "algorithm";
    static constexpr auto contentTime = "contentTime";
    static constexpr auto deviceID = "deviceID";
    static constexpr auto deviceName = "deviceName";
    static constexpr auto deviceRole = "deviceRole";
    static constexpr auto deviceVendor = "deviceVendor";
    static constexpr auto pluginVersion = "pluginVersion";
    static constexpr auto type = "type";
    static constexpr auto expression = "expression";
    static constexpr auto parameter = "parameter";
    static constexpr auto controller = "controller";

    // Enum values
    static constexpr auto beats = "beats";
    static constexpr auto seconds = "seconds";
    static constexpr auto linear = "linear";
    static constexpr auto hold = "hold";
    static constexpr auto audio = "audio";
    static constexpr auto notes = "notes";
    static constexpr auto tracks = "tracks";
    static constexpr auto markers = "markers";
    static constexpr auto automation = "automation";
    static constexpr auto master = "master";
    static constexpr auto regular = "regular";
    static constexpr auto effect = "effect";
    static constexpr auto submix = "submix";
    static constexpr auto vca = "vca";
    static constexpr auto instrument = "instrument";
    static constexpr auto audioFX = "audioFX";
    static constexpr auto noteFX = "noteFX";
    static constexpr auto analyzer = "analyzer";
    static constexpr auto pre = "pre";
    static constexpr auto post = "post";
    static constexpr auto bpm = "bpm";
    static constexpr auto decibel = "decibel";
    static constexpr auto normalized = "normalized";
    static constexpr auto percent = "percent";
    static constexpr auto hertz = "hertz";
    static constexpr auto semitones = "semitones";

    // Expression types for MIDI controller automation
    static constexpr auto channelController = "channelController";
    static constexpr auto pitchBend = "pitchBend";
    static constexpr auto channelPressure = "channelPressure";
    static constexpr auto polyPressure = "polyPressure";
    static constexpr auto programChange = "programChange";
}

//==============================================================================
/** ID generator for DAWproject XML elements. */
class IDGenerator
{
public:
    IDGenerator() = default;

    /** Generates a new unique ID string. */
    juce::String generateID()
    {
        return "id" + juce::String (++counter);
    }

    /** Resets the counter. */
    void reset()
    {
        counter = 0;
    }

private:
    std::atomic<int> counter { 0 };
};

//==============================================================================
/** ID reference resolver for parsing DAWproject XML. */
class IDRefResolver
{
public:
    IDRefResolver() = default;

    /** Registers an element with its ID. */
    void registerElement (const juce::String& id, juce::XmlElement* element)
    {
        if (id.isNotEmpty() && element != nullptr)
            idToElement[id] = element;
    }

    /** Resolves an IDREF to an element. */
    juce::XmlElement* resolveRef (const juce::String& idref) const
    {
        auto it = idToElement.find (idref);
        return it != idToElement.end() ? it->second : nullptr;
    }

    /** Clears all registered elements. */
    void clear()
    {
        idToElement.clear();
    }

private:
    std::unordered_map<juce::String, juce::XmlElement*> idToElement;
};

//==============================================================================
/** Velocity conversion between tracktion (0-127 int) and DAWproject (0.0-1.0 normalized). */
inline float velocityToNormalized (int velocity)
{
    return juce::jlimit (0.0f, 1.0f, velocity / 127.0f);
}

inline int normalizedToVelocity (float normalized)
{
    return juce::jlimit (0, 127, juce::roundToInt (normalized * 127.0f));
}

//==============================================================================
/** MIDI controller value conversion.
    tracktion_engine stores most controller values as 14-bit (value << 7 for 7-bit CCs).
    DAWproject uses normalized 0.0-1.0 values.
*/
inline float controllerValueToNormalized (int value, int controllerType)
{
    // Pitch wheel is 14-bit centered at 8192
    if (controllerType == MidiControllerEvent::pitchWheelType)
        return juce::jlimit (0.0f, 1.0f, static_cast<float> (value) / 16383.0f);

    // Most controllers are stored as 14-bit (value << 7)
    return juce::jlimit (0.0f, 1.0f, static_cast<float> (value >> 7) / 127.0f);
}

inline int normalizedToControllerValue (float normalized, int controllerType)
{
    // Pitch wheel is 14-bit centered at 8192
    if (controllerType == MidiControllerEvent::pitchWheelType)
        return juce::jlimit (0, 16383, juce::roundToInt (normalized * 16383.0f));

    // Most controllers stored as 14-bit (value << 7)
    return juce::jlimit (0, 16256, juce::roundToInt (normalized * 127.0f) << 7);
}

/** Maps tracktion MidiControllerEvent type to DAWproject expression type string. */
inline const char* controllerTypeToExpression (int type)
{
    if (type == MidiControllerEvent::pitchWheelType)
        return xml::pitchBend;
    if (type == MidiControllerEvent::channelPressureType)
        return xml::channelPressure;
    if (type == MidiControllerEvent::aftertouchType)
        return xml::polyPressure;
    if (type == MidiControllerEvent::programChangeType)
        return xml::programChange;

    // Regular CC (0-127)
    return xml::channelController;
}

/** Maps DAWproject expression type string to tracktion MidiControllerEvent type.
    For channelController, returns -1 (caller should use the controller attribute).
*/
inline int expressionToControllerType (const juce::String& expression)
{
    if (expression == xml::pitchBend)
        return MidiControllerEvent::pitchWheelType;
    if (expression == xml::channelPressure)
        return MidiControllerEvent::channelPressureType;
    if (expression == xml::polyPressure)
        return MidiControllerEvent::aftertouchType;
    if (expression == xml::programChange)
        return MidiControllerEvent::programChangeType;

    // channelController - return -1, caller uses controller attribute
    return -1;
}

//==============================================================================
/** Time conversion utilities.
    Note: 960 ticks per quarter note is the standard resolution used by tracktion_engine.
*/
static constexpr int defaultTicksPerQuarterNote = 960;

inline double ticksToBeats (int64_t ticks, int ticksPerQuarterNote = defaultTicksPerQuarterNote)
{
    return static_cast<double> (ticks) / static_cast<double> (ticksPerQuarterNote);
}

inline int64_t beatsToTicks (double beats, int ticksPerQuarterNote = defaultTicksPerQuarterNote)
{
    return static_cast<int64_t> (beats * ticksPerQuarterNote);
}

//==============================================================================
/** Color conversion between tracktion (juce::Colour) and DAWproject (CSS-style hex string). */
inline juce::String colourToDAWprojectString (juce::Colour colour)
{
    return "#" + colour.toDisplayString (false);
}

inline juce::Colour dawprojectStringToColour (const juce::String& str)
{
    if (str.isEmpty())
        return {};

    juce::String hexStr = str;
    if (hexStr.startsWith ("#"))
        hexStr = hexStr.substring (1);

    return juce::Colour::fromString (hexStr);
}

//==============================================================================
/** Volume conversion between linear gain and decibels. */
inline double gainToDecibels (float gain)
{
    return gain > 0.0f ? 20.0 * std::log10 (gain) : -std::numeric_limits<double>::infinity();
}

inline float decibelsToGain (double dB)
{
    return dB > -std::numeric_limits<double>::infinity()
               ? static_cast<float> (std::pow (10.0, dB / 20.0))
               : 0.0f;
}

//==============================================================================
/** XML helper functions. */
inline void setAttributeIfNotEmpty (juce::XmlElement& element, const char* name, const juce::String& value)
{
    if (value.isNotEmpty())
        element.setAttribute (name, value);
}

inline void setAttributeIfValid (juce::XmlElement& element, const char* name, double value)
{
    if (std::isfinite (value))
        element.setAttribute (name, value);
}

inline std::unique_ptr<juce::XmlElement> createXmlElement (const char* tagName)
{
    return std::make_unique<juce::XmlElement> (tagName);
}

inline juce::XmlElement* addChildElement (juce::XmlElement& parent, const char* tagName)
{
    return parent.createNewChildElement (tagName);
}

//==============================================================================
/** Parses a double from a string, handling potential formatting differences. */
inline double parseDouble (const juce::String& str, double defaultValue = 0.0)
{
    if (str.isEmpty())
        return defaultValue;

    return str.getDoubleValue();
}

/** Parses an int from a string. */
inline int parseInt (const juce::String& str, int defaultValue = 0)
{
    if (str.isEmpty())
        return defaultValue;

    return str.getIntValue();
}

/** Parses a bool from a string or attribute value. */
inline bool parseBool (const juce::String& str, bool defaultValue = false)
{
    if (str.isEmpty())
        return defaultValue;

    return str.equalsIgnoreCase ("true") || str == "1";
}

}}} // namespace tracktion::engine::dawproject
