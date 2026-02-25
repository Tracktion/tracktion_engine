/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

struct ARAClipPlayer;

//==============================================================================
/**
    Manages an ARA plugin instance for an audio clip.

    This class handles the ARA (Audio Random Access) integration, allowing
    plugins like Melodyne to provide advanced audio analysis and manipulation.
*/
class ARAFileReader   : public juce::ReferenceCountedObject,
                        public juce::ChangeBroadcaster
{
public:
    ARAFileReader (Edit&, AudioClipBase&);
    ARAFileReader (Edit&, AudioClipBase&, ARAFileReader& oneToClone);
    ~ARAFileReader();

    using Ptr = juce::ReferenceCountedObjectPtr<ARAFileReader>;

    static void cleanUpOnShutdown();

    //==============================================================================
    /** Parsed info from an ARA audio file chunk embedded in iXML. */
    struct ARAChunkInfo
    {
        juce::String documentArchiveID;
        bool openAutomatically = false;
        juce::String persistentID;
        juce::MemoryBlock archiveData;
        juce::String suggestedPlugInName;
        juce::String manufacturerName;
    };

    /** Reads the raw iXML chunk from a WAV or AIFF file. */
    static juce::String readRawIXMLFromSourceFile (const juce::File&);

    /** Parses ARA audio file chunks from a raw iXML string. */
    static juce::Array<ARAChunkInfo> parseARAAudioFileChunksFromIXML (const juce::String& ixmlString);

    /** Finds an installed ARA plugin whose factory matches the given archive ID. */
    static juce::PluginDescription findPluginForARAArchiveID (Engine&, const juce::String& archiveID);

    //==============================================================================
    bool isValid() const noexcept                       { return player != nullptr; }

    ExternalPlugin* getPlugin();
    void showPluginWindow();
    void hidePluginWindow();

    bool isAnalysingContent();
    juce::MidiMessageSequence getAnalysedMIDISequence();

    void sourceClipChanged();

    /** Notifies that the ARA content has changed (e.g. notes edited in Melodyne).
        This re-reads the content and broadcasts a change message. */
    void contentHasChanged();

    /** Store a partial ARA archive of this clip's plugin edits for copy/paste.
        Returns an empty MemoryBlock if ARA is not active. */
    juce::MemoryBlock storeARAArchiveForCopy();

    /** Restore a partial ARA archive into this clip after paste.
        @param data            The archive data (from storeARAArchiveForCopy)
        @param archivedSourceID  The audio source persistent ID from when the archive was created
        @param archivedModID     The audio modification persistent ID from when the archive was created
    */
    void restoreARAArchiveForPaste (const juce::MemoryBlock& data,
                                    const juce::String& archivedSourceID,
                                    const juce::String& archivedModID,
                                    const juce::String& documentArchiveID = {});

    /** Returns the persistent ID of the current audio source, or empty if ARA is not active. */
    juce::String getAudioSourcePersistentID() const;

    /** Returns the persistent ID of the current audio modification, or empty if ARA is not active. */
    juce::String getAudioModificationPersistentID() const;

private:
    std::unique_ptr<ARAClipPlayer> player;
    juce::MidiBuffer midiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAFileReader)
};

/** @deprecated Use ARAFileReader instead */
using MelodyneFileReader = ARAFileReader;

//==============================================================================
/** Result of detecting ARA iXML chunks in an audio file. */
struct ARAIXMLResult
{
    juce::PluginDescription pluginDescription;
    juce::MemoryBlock archiveData;
    juce::String persistentID;
    juce::String documentArchiveID;
    bool isValid() const { return pluginDescription.name.isNotEmpty(); }
};

/** Scans a source audio file for ARA iXML chunks and returns the first
    matching installed plugin description along with archive data. */
ARAIXMLResult detectARAFromIXMLChunks (Engine&, const juce::File& sourceFile);


//==============================================================================
struct ARADocumentHolder
{
    ARADocumentHolder (Edit&, const juce::ValueTree&);
    ~ARADocumentHolder();

    void flushStateToValueTree();

    struct Pimpl;
    Pimpl* getPimpl();

private:
    Edit& edit;
    juce::ValueTree lastState;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentHolder)
};

} // namespace tracktion::inline engine
