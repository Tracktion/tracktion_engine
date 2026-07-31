/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

//==============================================================================
/**
    A plain-data description of a single render operation: one output file.

    Unlike RenderOptions this has no UI or selection dependencies: items are
    referenced by EditItemID and everything can be serialised to/from JSON with
    a single canonical schema (shared by saved presets, command line config
    files and the scripting API).

    Batch operations are lists of specifications: to render each track to its
    own file, create one specification per track (see
    createPerTrackSpecifications()) and queue the resulting jobs.

    Use validateRenderSpecification() to check a specification against a live
    Edit and createRenderJob() to turn it into concrete Renderer::Parameters.
*/
struct RenderSpecification
{
    //==============================================================================
    /** The tracks to mix into the render. If empty, the whole Edit is rendered.
        Submix folder tracks are valid entries: rendering one includes its child
        tracks and its plugin chain.
    */
    juce::Array<EditItemID> tracks;

    /** Tracks to include in the render graph but keep silent in the output:
        they are muted in the Edit while the job renders and restored
        afterwards. Use this for sidechain/rack source tracks that must keep
        feeding the rendered tracks' processing without being heard.
    */
    juce::Array<EditItemID> mutedTracks;

    /** If true, findStemSourceTracks() runs over the tracks list when the job
        is created and the tracks it finds - sidechain sources, aux-send
        sources and rack feeders the stem depends on - are added to
        mutedTracks automatically, so the stem sounds as it does in the full
        mix. Has no effect when tracks is empty (a whole-Edit render already
        includes everything).
    */
    bool includeSourceTracks = false;

    /** The time range to render. If unset, the whole Edit length is used. */
    std::optional<TimeRange> time;

    /** If true, the plugin tail past the end of the range is rendered and
        mixed back onto the start of the file so it loops seamlessly.
        Forces trimSilence off. @see Renderer::Parameters::wrapRemainder
    */
    bool wrapRemainder = false;

    //==============================================================================
    /** The destination file to write to. */
    juce::File destination;

    juce::String format { "wav" };          ///< One of "wav", "aiff", "flac", "ogg", "mp3"
    double sampleRate = 44100.0;            ///< The sample rate to render at
    int bitDepth = 16;                      ///< The bit depth to render at
    int quality = 0;                        ///< Format-specific quality index, for formats that use one

    /** Output channel layout: empty for auto-detect from the Edit,
        or one of "mono", "stereo", "5.1", "7.1".
    */
    juce::String channelLayout;

    //==============================================================================
    bool normalise = false;                 ///< Normalise the result by peak level
    bool normaliseByRMS = false;            ///< Normalise the result by RMS level instead
    bool normaliseByLUFS = false;           /**< Normalise the result to an integrated loudness
                                                 (BS.1770-4) target instead. If more than one
                                                 normalise flag is set, LUFS wins, then RMS, then peak. */
    float normaliseToLevelDb = 0;           ///< The level to normalise to, in dB or LUFS depending on the mode
    bool limitTruePeak = true;              /**< LUFS mode only: hold the gain back so the true peak stays
                                                 under truePeakCeilingDb, even if the result then lands
                                                 quieter than the loudness target. */
    float truePeakCeilingDb = -1.0f;        ///< The true-peak ceiling to keep under when limitTruePeak is set
    bool trimSilence = false;               ///< Trim leading/trailing silence
    bool dither = false;                    ///< Apply dithering for non-float formats
    bool realTime = false;                  ///< Render at 1x play speed
    bool usePlugins = true;                 ///< Include track plugins
    bool useMasterPlugins = true;           ///< Include the master plugin chain
    juce::StringPairArray metadata;         ///< Metadata pairs to write to the file where supported

    //==============================================================================
    /** Returns the canonical JSON representation of this specification. */
    juce::var toJSON() const;

    /** Creates a specification from its canonical JSON representation.
        Missing keys take their default values. Unknown keys are ignored and
        their names appended to unknownKeys if supplied.
    */
    static RenderSpecification fromJSON (const juce::var&, juce::StringArray* unknownKeys = nullptr);
};

//==============================================================================
/** Checks a specification can be rendered from the given Edit, returning a
    failure result describing the first problem found.
*/
juce::Result validateRenderSpecification (Edit&, const RenderSpecification&);

//==============================================================================
/** Finds the tracks a stem render depends on for its sound but which aren't
    part of the stem itself: sidechain sources of the stem's plugins, tracks
    with an aux send feeding an aux return in the stem, and tracks hosting
    instances of a rack the stem shares (a rack is processed once, fed by all
    its instances). The search runs to a fixpoint, so sources of sources are
    found too, and looks inside the stem's submix folders.

    Render the returned tracks via RenderSpecification::mutedTracks: included
    in the graph, silenced in the output. Tracks that merely route their
    output into the stem's tracks are NOT returned - the render graph pulls
    those in by itself.
*/
juce::Array<EditItemID> findStemSourceTracks (Edit&, const juce::Array<EditItemID>& stemTracks);

//==============================================================================
/** A concrete render job created from a RenderSpecification. */
struct PlannedRenderJob
{
    juce::String name;                      ///< A display name for the job, e.g. the track name
    Renderer::Parameters params;            ///< The parameters to render with
    juce::Array<EditItemID> tracksToMute;   ///< Tracks muted in the Edit while this job renders
};

/** Creates the render job for a valid specification.
    Call validateRenderSpecification() first: an invalid specification
    returns nullopt.
*/
std::optional<PlannedRenderJob> createRenderJob (Edit&, const RenderSpecification&);

/** Creates one specification per track from a template, for rendering each
    track to its own file in the given directory. The template's tracks list
    chooses which tracks (all audio tracks if empty); its destination is
    ignored. File names come from the track names, uniquified against each
    other and against existing files.
*/
std::vector<RenderSpecification> createPerTrackSpecifications (Edit&,
                                                               const RenderSpecification& base,
                                                               const juce::File& directory);

} // namespace tracktion::inline engine
