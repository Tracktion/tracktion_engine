/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
    DAWproject interchange format support.

    DAWproject is an open interchange format for digital audio workstation projects.
    It stores project data as XML files (project.xml, metadata.xml) bundled in a ZIP
    archive along with embedded audio files.

    @see https://github.com/bitwig/dawproject
*/
namespace dawproject
{

//==============================================================================
/** Options for parsing a DAWproject file. */
struct ParseOptions
{
    /** If true, audio files referenced in the project will be extracted to a temp directory. */
    bool extractAudioFiles = true;

    /** If set, audio files will be extracted to this directory instead of a temp directory. */
    juce::File audioFileDestination;

    /** The EditRole to use when creating the Edit. */
    Edit::EditRole editRole = Edit::EditRole::forEditing;
};

/** Options for writing a DAWproject file. */
struct WriteOptions
{
    /** If true, audio files used by the Edit will be embedded in the ZIP archive. */
    bool embedAudioFiles = true;

    /** If true, plugin state data will be embedded in the ZIP archive. */
    bool embedPluginState = true;

    /** Application name to write in the project metadata. */
    juce::String applicationName = "Tracktion Engine";

    /** Application version to write in the project metadata. */
    juce::String applicationVersion;
};

//==============================================================================
/**
    Parses a .dawproject file and creates an Edit.

    @param engine           The Engine to use for the new Edit
    @param file             The .dawproject file
    @param options          Options controlling the import behavior
    @return                 A new Edit if successful, or nullptr on failure
*/
std::unique_ptr<Edit> parseDAWproject (Engine& engine,
                                       const juce::File& file,
                                       ParseOptions options = {});

//==============================================================================
/**
    Creates the project.xml content from an Edit.

    @param edit             The Edit to export
    @param options          Options controlling the export behavior
    @return                 An XmlElement representing the project, or an error string
*/
tl::expected<std::unique_ptr<juce::XmlElement>, juce::String> createDAWproject (Edit& edit,
                                                                                WriteOptions options = {});

//==============================================================================
/**
    Writes a complete .dawproject file from an Edit.

    This creates a ZIP archive containing:
    - project.xml (the main project data)
    - metadata.xml (project metadata)
    - audio/ subdirectory with embedded audio files (if enabled)
    - plugins/ subdirectory with plugin state files (if enabled)

    @param file             The destination .dawproject file
    @param edit             The Edit to export
    @param options          Options controlling the export behavior
    @return                 A Result indicating success or failure
*/
juce::Result writeDAWprojectFile (const juce::File& file,
                                  Edit& edit,
                                  WriteOptions options = {});

} // namespace dawproject

}} // namespace tracktion::engine
