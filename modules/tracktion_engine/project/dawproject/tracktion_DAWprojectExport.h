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
    Internal class for exporting an Edit to DAWproject format.
    This handles generating XML and writing the ZIP archive.
*/
class DAWprojectExporter
{
public:
    DAWprojectExporter (Edit& edit, const WriteOptions& options);

    /** Creates the project.xml content. */
    tl::expected<std::unique_ptr<juce::XmlElement>, juce::String> createProjectXml();

    /** Creates the metadata.xml content. */
    std::unique_ptr<juce::XmlElement> createMetadataXml();

    /** Writes the complete .dawproject file. */
    juce::Result writeToFile (const juce::File& file);

private:
    //==============================================================================
    // Structure export
    juce::XmlElement* createStructure (juce::XmlElement& parent);
    juce::XmlElement* createTrackElement (juce::XmlElement& parent, Track& track);
    juce::XmlElement* createChannelElement (juce::XmlElement& parent, Track& track);

    // Transport/tempo export
    juce::XmlElement* createTransport (juce::XmlElement& parent);
    juce::XmlElement* createTempoAutomation (juce::XmlElement& parent);
    juce::XmlElement* createTimeSignatureAutomation (juce::XmlElement& parent);

    // Arrangement export
    juce::XmlElement* createArrangement (juce::XmlElement& parent);
    juce::XmlElement* createLanes (juce::XmlElement& parent, Track& track);
    juce::XmlElement* createClipsElement (juce::XmlElement& parent, ClipTrack& track);
    juce::XmlElement* createMarkersElement (juce::XmlElement& parent);

    // Clip export
    juce::XmlElement* createClipElement (juce::XmlElement& parent, Clip& clip);
    void exportAudioClip (juce::XmlElement& clipElement, WaveAudioClip& clip);
    void exportMidiClip (juce::XmlElement& clipElement, MidiClip& clip);
    juce::XmlElement* createNotesElement (juce::XmlElement& parent, MidiClip& clip);
    juce::XmlElement* createNoteElement (juce::XmlElement& parent, const MidiNote& note);
    void createControllerLanes (juce::XmlElement& parent, MidiClip& clip);

    // Plugin export
    juce::XmlElement* createDevices (juce::XmlElement& parent, Track& track);
    juce::XmlElement* createPluginElement (juce::XmlElement& parent, Plugin& plugin);

    // Automation export
    juce::XmlElement* createAutomationPoints (juce::XmlElement& parent, AutomatableParameter& param);

    // Audio file handling
    juce::String addAudioFile (const juce::File& file);
    void writeAudioFiles (juce::ZipFile::Builder& zipBuilder);
    void writePluginStates (juce::ZipFile::Builder& zipBuilder);

    //==============================================================================
    // Utility functions
    juce::String getTrackContentType (Track& track) const;
    juce::String getMixerRole (Track& track) const;
    juce::String getPluginType (Plugin& plugin) const;
    juce::String getDeviceRole (Plugin& plugin) const;

    //==============================================================================
    Edit& edit;
    WriteOptions options;
    IDGenerator idGenerator;

    // Maps tracks to their generated IDs
    std::unordered_map<Track*, juce::String> trackToId;
    std::unordered_map<Track*, juce::String> trackToChannelId;

    // Audio files to embed
    struct AudioFileEntry
    {
        juce::File sourceFile;
        juce::String archivePath;
    };
    std::vector<AudioFileEntry> audioFilesToEmbed;

    // Plugin states to embed
    struct PluginStateEntry
    {
        juce::MemoryBlock state;
        juce::String archivePath;
    };
    std::vector<PluginStateEntry> pluginStatesToEmbed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DAWprojectExporter)
};

}}} // namespace tracktion::engine::dawproject
