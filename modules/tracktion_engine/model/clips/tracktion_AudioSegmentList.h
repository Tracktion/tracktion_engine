/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
/**
    Holds a list of audio regions for playback of things like warp time.
*/
class AudioSegmentList
{
public:
    //==============================================================================
    static std::unique_ptr<AudioSegmentList> create (AudioClipBase&);
    static std::unique_ptr<AudioSegmentList> create (AudioClipBase&, bool relativeTime, bool crossFade);
    static std::unique_ptr<AudioSegmentList> create (AudioClipBase&, const WarpTimeManager&, const AudioFile&);
    static std::unique_ptr<AudioSegmentList> create (AudioClipBase&, const WarpTimeManager&, const AudioFileInfo&, const LoopInfo&);

    /**
    */
    struct Segment
    {
        Segment() = default;

        EditTimeRange getRange() const;
        SampleRange getSampleRange() const;

        float getStretchRatio() const;
        float getTranspose() const;

        bool hasFadeIn() const;
        bool hasFadeOut() const;

        bool isFollowedBySilence() const;
        HashCode getHashCode() const;

        bool operator== (const Segment&) const;
        bool operator!= (const Segment&) const;

        double start = 0, length = 0;
        SampleCount startSample = 0, lengthSample = 0;
        float stretchRatio = 1.0f, transpose = 0;
        bool fadeIn = false, fadeOut = false;
        bool followedBySilence = false;
    };

    const juce::Array<Segment>& getSegments() const         { return segments; }

    double getStart() const;
    double getEnd() const;
    double getLength() const                                { return getEnd() - getStart(); }
    double getCrossfadeLength() const                       { return crossfadeTime; }

    bool operator== (const AudioSegmentList&) const noexcept;
    bool operator!= (const AudioSegmentList&) const noexcept;

private:
    //==============================================================================
    AudioSegmentList (AudioClipBase&);
    AudioSegmentList (AudioClipBase&, bool relativeTime, bool crossfade);

    void chopSegmentsForChords();
    void chopSegment (Segment& seg, double at, int insertPos);

    AudioClipBase& clip;
    juce::Array<Segment> segments;
    double crossfadeTime = 0;
    bool relativeTime = true;

    void build (bool crossfade);
    void buildAutoTempo (bool crossfade);
    void buildNormal (bool crossfade);
    float getPitchAt (double);
    void removeExtraSegments();
    void crossFadeSegments();
    void mergeSegments (double sampleRate);
    void initialiseSegment (Segment&, double startBeat, double endBeat, double sampleRate);

    juce::OwnedArray<PatternGenerator::ProgressionItem> progression;
};

} // namespace tracktion_engine
