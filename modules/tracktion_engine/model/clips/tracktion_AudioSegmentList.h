/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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

        TimeRange getRange() const;
        SampleRange getSampleRange() const;

        float getStretchRatio() const;
        float getTranspose() const;

        bool hasFadeIn() const;
        bool hasFadeOut() const;

        bool isFollowedBySilence() const;
        HashCode getHashCode() const;

        bool operator== (const Segment&) const;
        bool operator!= (const Segment&) const;

        TimePosition start;
        TimeDuration length;
        SampleCount startSample = 0, lengthSample = 0;
        float stretchRatio = 1.0f, transpose = 0;
        bool fadeIn = false, fadeOut = false;
        bool followedBySilence = false;
    };

    const juce::Array<Segment>& getSegments() const         { return segments; }

    TimePosition getStart() const;
    TimePosition getEnd() const;
    TimeDuration getLength() const                          { return getEnd() - getStart(); }
    TimeDuration getCrossfadeLength() const                 { return crossfadeTime; }

    bool operator== (const AudioSegmentList&) const noexcept;
    bool operator!= (const AudioSegmentList&) const noexcept;

private:
    //==============================================================================
    AudioSegmentList (AudioClipBase&);
    AudioSegmentList (AudioClipBase&, bool relativeTime, bool crossfade);

    void chopSegmentsForChords();
    void chopSegment (Segment&, TimePosition at, int insertPos);

    AudioClipBase& clip;
    juce::Array<Segment> segments;
    TimeDuration crossfadeTime;
    bool relativeTime = true;

    void build (bool crossfade);
    void buildAutoTempo (bool crossfade);
    void buildNormal (bool crossfade);
    float getPitchAt (TimePosition);
    void removeExtraSegments();
    void crossFadeSegments();
    void mergeSegments (double sampleRate);
    void initialiseSegment (Segment&, BeatPosition startBeat, BeatPosition endBeat, double sampleRate);

    juce::OwnedArray<PatternGenerator::ProgressionItem> progression;
};

}} // namespace tracktion { inline namespace engine
