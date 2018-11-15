/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/** Represents a set of user properties used to control a render operation, using
    a ValueTree to hold them so that they can be saved and reloaded.
    They can also be converted into a set of Renderer::Properties.
*/
class RenderOptions  : private juce::ValueTree::Listener
{
public:
    enum TargetFileFormat
    {
        wav = 0,
        aiff,
        flac,
        ogg,
        mp3,
        midi,
        numFormats
    };

    /** Enum to set the options for renders. */
    enum AddRenderOptions
    {
        addTrack        = 0,
        replaceTrack    = 1,
        nextTrack       = 2,
        thisTrack       = 3,
        replaceClips    = 4,
        none            = 5,
    };

    //==============================================================================
    RenderOptions (Engine&);
    RenderOptions (const RenderOptions&, juce::UndoManager*);
    RenderOptions (Engine&, const juce::ValueTree&, juce::UndoManager*);
    ~RenderOptions();

    RenderOptions() = delete;
    RenderOptions (const RenderOptions&) = delete;
    RenderOptions (RenderOptions&&) = delete;
    RenderOptions operator= (const RenderOptions&) = delete;
    RenderOptions operator= (RenderOptions&&) = delete;

    void loadFromUserSettings();
    void saveToUserSettings();
    void setToDefault();

    //==============================================================================
    /** Performs the render operation on a background thread. */
    RenderManager::Job::Ptr performBackgroundRender (Edit&, SelectionManager*, EditTimeRange timeRangeToRender);

    //==============================================================================
    /** Returns a hash representing the current set of render options. */
    juce::int64 getHash() const noexcept                        { return hash; }

    /** Returns a set of renderer parameters which can be used to describe a render operation. */
    Renderer::Parameters getRenderParameters (Edit&, SelectionManager*, EditTimeRange markedRegion);
    Renderer::Parameters getRenderParameters (Edit&);
    Renderer::Parameters getRenderParameters (EditClip&);
    Renderer::Parameters getRenderParameters (MidiClip&);

    /** Returns an AudioFormat to use for the current render properties. */
    juce::AudioFormat* getAudioFormat();

    /** Adds the given ProjectItem to the Edit using the render properties for positioning info. */
    Clip::Ptr applyRenderToEdit (Edit&, ProjectItem::Ptr, EditTimeRange time, SelectionManager*) const;

    //==============================================================================
    /** Creates a default RenderOptions object for a general purpose exporter. */
    static RenderOptions* forGeneralExporter (Edit&);
    static RenderOptions* forTrackRender (juce::Array<Track*>& tracks, AddRenderOptions addOption);
    static RenderOptions* forClipRender (juce::Array<Clip*>& clips, bool midiNotes);
    static RenderOptions* forEditClip (Clip& editClip);

    //==============================================================================
    /** If you've chnaged a property that will cause the UI configuration to change
        this will return true which you should rebuild the property set from.
    */
    bool getUINeedsRefresh();
    void setUINeedsRefresh();

    //==============================================================================
    /** Enum to determine the type of UI this RenderOptions represents. */
    enum class RenderType
    {
        editClip = 0,
        clip,
        track,
        midi,
        allExport
    };

    bool isEditClipRender() const                               { return type == RenderType::editClip; }
    bool isClipRender() const                                   { return type == RenderType::clip; }
    bool isTrackRender() const                                  { return type == RenderType::track; }
    bool isMidiRender() const                                   { return type == RenderType::midi; }
    bool isExportAll() const                                    { return type == RenderType::allExport; }
    bool isRender() const                                       { return isTrackRender() || isClipRender() || isMidiRender(); }

    //==============================================================================
    TargetFileFormat setFormat (TargetFileFormat);
    void setFormatType (const juce::String& typeString);
    juce::String getFormatTypeName (TargetFileFormat);

    juce::Array<EditItemID> getTrackIDs() const                 { return tracks; }
    void setTrackIDs (const juce::Array<EditItemID>&);
    juce::int64 getTracksHash() const;

    void setSampleRate (int);
    void setBitDepth (int depth)                                { bitDepth = depth; }

    juce::StringArray getQualitiesList() const;
    int getQualityIndex() const                                 { return qualityIndex; }
    void setQualityIndex (int q)                                { qualityIndex = q; }

    juce::File getDestFile() const noexcept                     { return destFile; }
    juce::String getFileExtension() const;

    TargetFileFormat getFormat() const noexcept                 { return format; }
    int getBitDepth() const                                     { return bitDepth; }
    bool getStereo() const                                      { return stereo; }
    double getSampleRate() const                                { return sampleRate; }
    bool shouldAddMetadata() const                              { return addMetadata; }

    juce::BigInteger getTrackIndexes (const Edit&) const;
    bool getRemoveSilence() const noexcept                      { return removeSilence; }
    bool getMarkedRegion() const noexcept                       { return markedRegion; }
    int getNumTracks() const noexcept                           { return tracks.size(); }

    void setSelected (bool onlySelectedTrackAndClips);
    void setMarkedRegion (bool onlyMarked)                      { markedRegion = onlyMarked; }
    void setIncludePlugins (bool includePlugins)                { usePlugins = includePlugins; }
    void setAddRenderOption (AddRenderOptions options)          { addRenderOptions = options; }
    void setEndAllowance (double time)                          { endAllowance = time; }
    void setFilename (juce::String, bool canPromptToOverwriteExisting);
    void updateHash();

    //==============================================================================
    juce::StringArray getFormatTypes();
    juce::StringArray getAddRenderOptionText();

    static double findEndAllowance (Edit&, juce::Array<EditItemID>* tracks, juce::Array<Clip*>*);
    static bool isMarkedRegionBigEnough (EditTimeRange);

    Engine& engine;

    juce::CachedValue<RenderType> type;
    juce::CachedValue<juce::String> tracksProperty;

    juce::CachedValue<bool> createMidiFile;
    juce::CachedValue<TargetFileFormat> format;
    juce::CachedValue<bool> stereo;
    juce::CachedValue<double> sampleRate;
    juce::CachedValue<int> bitDepth, qualityIndex;
    juce::CachedValue<double> rmsLevelDb, peakLevelDb;

    juce::CachedValue<bool> removeSilence, normalise, dither, adjustBasedOnRMS,
                            markedRegion, selectedTracks, selectedClips,
                            tracksToSeparateFiles, realTime, usePlugins;

    juce::CachedValue<AddRenderOptions> addRenderOptions;
    juce::CachedValue<bool> addRenderToLibrary, reverseRender, addMetadata;

    // These optional lambdas will be called when the addRenderToLibrary flag
    // is set, and can be used to do something with the finished item
    std::function<void(AudioClipBase&)> offerToAddClipToLibrary;
    std::function<void(const juce::File&)> offerToAddFileToLibrary;

private:
    //==============================================================================
    juce::ValueTree state;

    void relinkCachedValues (juce::UndoManager*);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    //==============================================================================
    juce::Array<EditItemID> tracks;
    juce::File destFile;
    juce::Array<Clip*> allowedClips;
    double endAllowance = 0;
    juce::int64 hash = 0;
    bool uiNeedsRefresh = true;

    static void updateLastUsedRenderPath (RenderOptions&, const juce::String& itemID);

    //==============================================================================
    void updateFileName();
    void updateOptionsForFormat();
    void updateDefaultFilename (Edit*);
    juce::String getCurrentFileExtension();
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::RenderOptions::RenderType>
    {
        static tracktion_engine::RenderOptions::RenderType fromVar (const var& v)   { return (tracktion_engine::RenderOptions::RenderType) static_cast<int> (v); }
        static var toVar (tracktion_engine::RenderOptions::RenderType v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion_engine::RenderOptions::TargetFileFormat>
    {
        static tracktion_engine::RenderOptions::TargetFileFormat fromVar (const var& v)   { return (tracktion_engine::RenderOptions::TargetFileFormat) static_cast<int> (v); }
        static var toVar (tracktion_engine::RenderOptions::TargetFileFormat v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion_engine::RenderOptions::AddRenderOptions>
    {
        static tracktion_engine::RenderOptions::AddRenderOptions fromVar (const var& v)   { return (tracktion_engine::RenderOptions::AddRenderOptions) static_cast<int> (v); }
        static var toVar (tracktion_engine::RenderOptions::AddRenderOptions v)            { return static_cast<int> (v); }
    };
}
