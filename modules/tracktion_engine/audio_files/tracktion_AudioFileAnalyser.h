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
    Interface for a streaming audio-file analyser.

    analyseAudioFile() reads a file once in blocks and feeds every registered
    analyser, so several analyses share a single pass over the audio. Each
    analyser adds its results to a shared JSON object. New analysers (tempo,
    key, onsets...) can be added without changing this API.
*/
class AudioFileAnalyser
{
public:
    virtual ~AudioFileAnalyser() = default;

    /** Called once before any audio, with the file's format info. */
    virtual void prepare (const AudioFileInfo&) = 0;

    /** Called repeatedly with successive blocks of the file's audio.
        Only the first numSamples of the buffer are valid.
    */
    virtual void process (const juce::AudioBuffer<float>&, int numSamples) = 0;

    /** Called once after the last block to add this analyser's results to
        the shared result object.
    */
    virtual void addResults (juce::DynamicObject&) = 0;
};

//==============================================================================
/** Options for the standard analyseAudioFile() convenience overload. */
struct AudioAnalysisOptions
{
    bool levels = true;         ///< Peak, true peak, RMS, LUFS, clipping, silence ratio
    bool spectrum = true;       ///< Banded energies, spectral centroid/rolloff, low/mid/high balance
    bool envelope = true;       ///< Downsampled peak/RMS dynamics curve over time
    bool stereo = true;         ///< Phase correlation, stereo width/balance, mono compatibility
    bool spectrogram = true;    ///< Per-band level over time, with band stats and buildup detection
    int envelopePoints = 100;   ///< Number of points in the dynamics curve
    int spectrogramSlices = 60; ///< Maximum number of time slices in the spectrogram
    bool spectrogramThirdOctave = false;    ///< 31 third-octave bands instead of the 8 broad mixing bands

    /** Checked between blocks while the file streams; return true to abort
        the analysis, which then fails rather than returning partial results.
        Lets a caller's thread shut down promptly mid-way through a long file. */
    std::function<bool()> shouldAbort;
};

//==============================================================================
/** Streams an audio file once through the given analysers, returning their
    combined results as a JSON-style object with the file's basic properties
    (sampleRate, channels, seconds) always included. All values are rounded
    for compactness: the output is designed to be read by an LLM.
*/
tl::expected<juce::var, juce::String> analyseAudioFile (Engine&, const juce::File&,
                                                        const std::vector<AudioFileAnalyser*>&,
                                                        const std::function<bool()>& shouldAbort = {});

/** Analyses a file with the standard level/spectrum/envelope analysers.
    Level stats: sample peak, oversampled true peak, RMS, EBU R128 loudness
    (integrated, max momentary, max short-term - BS.1770-4 K-weighting and
    gating), loudness range in LU (EBU Tech 3342), clipped-sample count and the
    ratio of silent time.
    Spectrum: third-octave band energies relative to the loudest band, plus
    spectral centroid, 85% rolloff and low/mid/high balance.
    Envelope: peak and RMS in dB over evenly-spaced windows.
    Stereo: phase correlation (overall and worst 400ms window), stereo width
    and balance, mono summing loss, and a low-band (sub-200Hz) correlation and
    width for mono-bass checks. Null for mono files.
    Spectrogram: per-band level over time (broad mixing bands by default, or
    third-octave), each band with its own summary stats, plus detected
    buildups (sustained level rises within one band).
*/
tl::expected<juce::var, juce::String> analyseAudioFile (Engine&, const juce::File&,
                                                        const AudioAnalysisOptions& = {});

} // namespace tracktion::inline engine
