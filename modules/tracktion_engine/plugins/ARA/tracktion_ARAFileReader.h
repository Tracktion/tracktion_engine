/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#ifndef T_ARA_DBG
 // #define T_ARA_DBG(x) DBG(x)
 #define T_ARA_DBG(...)
#endif

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

    /** Finds an installed ARA plugin whose factory matches the given archive ID.
        If suggestedPlugInName is given (e.g. from an ARA audio file chunk), only
        plugins with that name are considered - this avoids loading the module of
        every installed ARA plugin just to compare archive IDs. */
    static juce::PluginDescription findPluginForARAArchiveID (Engine&, const juce::String& archiveID,
                                                              const juce::String& suggestedPlugInName = {});

    //==============================================================================
    bool isValid() const noexcept                       { return player != nullptr; }

    ExternalPlugin* getPlugin();
    void showPluginWindow();
    void hidePluginWindow();

    bool isAnalysingContent();
    juce::MidiMessageSequence getAnalysedMIDISequence();

    void sourceClipChanged();

    /** Notifies the plugin that the edit's key/chord content has changed.
        Tempo changes are picked up automatically via the tempo sequence. */
    void musicalContextContentChanged();

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

    /** Returns the document archive ID from the ARA factory, or empty if ARA is not active. */
    juce::String getDocumentArchiveID() const;

    /** Returns extra time before the playback region start reported by the ARA plugin. */
    TimeDuration getHead() const;

    /** Returns extra time after the playback region end reported by the ARA plugin. */
    TimeDuration getTail() const;

    /** Serialises audio-thread rendering of the plugin against message-thread renderer
        deactivation (ARA playback-region changes require the plugin to not be in
        render-state). The audio thread try-locks this and outputs silence if held. */
    juce::CriticalSection& getProcessLock() noexcept    { return processLock; }

private:
    std::unique_ptr<ARAClipPlayer> player;
    juce::MidiBuffer midiBuffer;
    juce::CriticalSection processLock;

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
/** Owns an ARA document-controller binding created by
    ARADocumentHolder::bindPluginToDocument. Destroying it releases the binding,
    taking the plugin out of ARA mode. */
class ARAPluginBinding
{
public:
    ~ARAPluginBinding();

private:
    friend struct ARADocumentHolder;
    struct Impl;
    explicit ARAPluginBinding (std::unique_ptr<Impl>);

    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAPluginBinding)
};

//==============================================================================
struct ARADocumentHolder
{
    ARADocumentHolder (Edit&, const juce::ValueTree&);
    ~ARADocumentHolder();

    void flushStateToValueTree();

    /** Binds an already-loaded, ARA-capable ExternalPlugin to this Edit's ARA document
        controller for the given description (lazily creating the document), putting the
        plugin into ARA mode. No audio source, playback region or musical context is created.

        Returns null if the plugin isn't ARA-capable, its instance isn't loaded yet, or the
        binding fails. The returned object owns the binding for as long as it's kept alive. */
    std::unique_ptr<ARAPluginBinding> bindPluginToDocument (ExternalPlugin&, const juce::PluginDescription&);

    struct Pimpl;
    Pimpl* getPimpl();

private:
    Edit& edit;
    juce::ValueTree lastState;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentHolder)
};

} // namespace tracktion::inline engine
