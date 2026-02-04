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
/**
    Internal class for importing DAWproject files into an Edit.
    This handles parsing the XML and constructing Edit objects.
*/
class DAWprojectImporter
{
public:
    DAWprojectImporter (Engine& engine, const ParseOptions& options);

    /** Imports from a .dawproject ZIP file. */
    std::unique_ptr<Edit> importFromFile (const juce::File& file);

    /** Imports from parsed XML elements. */
    std::unique_ptr<Edit> importFromXml (const juce::XmlElement& projectXml,
                                         const juce::File& audioFileDirectory);

private:
    //==============================================================================
    // Structure parsing
    void parseStructure (const juce::XmlElement& structureElement, Edit& edit);
    void parseTrack (const juce::XmlElement& trackElement, Edit& edit, Track* parentTrack);
    void parseChannel (const juce::XmlElement& channelElement, Edit& edit, Track* track);

    // Transport/tempo parsing
    void parseTransport (const juce::XmlElement& transportElement, Edit& edit);
    void parseTempoAutomation (const juce::XmlElement& tempoAutomation, Edit& edit);
    void parseTimeSignatureAutomation (const juce::XmlElement& timeSigAutomation, Edit& edit);

    // Arrangement parsing
    void parseArrangement (const juce::XmlElement& arrangementElement, Edit& edit);
    void parseLanes (const juce::XmlElement& lanesElement, Edit& edit, Track* track);
    void parseClips (const juce::XmlElement& clipsElement, Edit& edit, ClipTrack& track);
    void parseMarkers (const juce::XmlElement& markersElement, Edit& edit);

    // Clip parsing
    Clip::Ptr parseClip (const juce::XmlElement& clipElement, Edit& edit, ClipTrack& track);
    void parseAudioClip (const juce::XmlElement& clipElement, WaveAudioClip& clip);
    void parseMidiClip (const juce::XmlElement& clipElement, MidiClip& clip);
    void parseNotes (const juce::XmlElement& notesElement, MidiClip& clip);
    void parseControllerPoints (const juce::XmlElement& lanesElement, MidiClip& clip);

    // Plugin parsing
    void parseDevices (const juce::XmlElement& devicesElement, Edit& edit, Track& track);
    Plugin::Ptr parsePlugin (const juce::XmlElement& pluginElement, Edit& edit);

    // Automation parsing
    void parseAutomationPoints (const juce::XmlElement& pointsElement, AutomatableParameter& param);

    // Audio file handling
    juce::File resolveAudioFile (const juce::String& path);
    void extractAudioFiles (juce::ZipFile& zipFile, const juce::File& destDir);

    //==============================================================================
    // Utility functions
    TimePosition parseTime (const juce::XmlElement& element, const char* attrName, bool isBeats) const;
    TimeDuration parseDuration (const juce::XmlElement& element, const char* attrName, bool isBeats) const;
    bool isTimeUnitBeats (const juce::XmlElement& element) const;

    //==============================================================================
    Engine& engine;
    ParseOptions options;
    IDRefResolver idResolver;
    juce::File audioFileDirectory;

    // Maps DAWproject track IDs to created tracks
    std::unordered_map<juce::String, Track*> idToTrack;

    // Maps DAWproject channel IDs to created tracks (for routing)
    std::unordered_map<juce::String, Track*> idToChannel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DAWprojectImporter)
};

}}} // namespace tracktion::engine::dawproject
