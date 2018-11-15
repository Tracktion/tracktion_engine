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
        Segment();

        EditTimeRange getRange() const;
        juce::Range<juce::int64> getSampleRange() const;
        float getStretchRatio() const;
        float getTranspose() const;

        bool hasFadeIn() const;
        bool hasFadeOut() const;

        bool isFollowedBySilence() const;
        juce::int64 getHashCode() const;

        bool operator== (const Segment&) const;
        bool operator!= (const Segment&) const;

    private:
        friend class AudioSegmentList;

        double start = 0, length = 0;
        juce::int64 startSample = 0, lengthSample = 0;
        float stretchRatio = 1.0f, transpose = 0;
        bool fadeIn = false, fadeOut = false;
        bool followedBySilence = false;
    };

    const juce::Array<Segment>& getSegments() const     { return segments; }

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
};

} // namespace tracktion_engine
