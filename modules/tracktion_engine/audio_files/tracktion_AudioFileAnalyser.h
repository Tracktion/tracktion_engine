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
    int envelopePoints = 100;   ///< Number of points in the dynamics curve
};

//==============================================================================
/** Streams an audio file once through the given analysers, returning their
    combined results as a JSON-style object with the file's basic properties
    (sampleRate, channels, seconds) always included. All values are rounded
    for compactness: the output is designed to be read by an LLM.
*/
tl::expected<juce::var, juce::String> analyseAudioFile (Engine&, const juce::File&,
                                                        const std::vector<AudioFileAnalyser*>&);

/** Analyses a file with the standard level/spectrum/envelope analysers.
    Level stats: sample peak, oversampled true peak, RMS, EBU R128 loudness
    (integrated, max momentary, max short-term - BS.1770-4 K-weighting and
    gating), loudness range in LU (EBU Tech 3342), clipped-sample count and the
    ratio of silent time.
    Spectrum: third-octave band energies relative to the loudest band, plus
    spectral centroid, 85% rolloff and low/mid/high balance.
    Envelope: peak and RMS in dB over evenly-spaced windows.
*/
tl::expected<juce::var, juce::String> analyseAudioFile (Engine&, const juce::File&,
                                                        const AudioAnalysisOptions& = {});

} // namespace tracktion::inline engine
