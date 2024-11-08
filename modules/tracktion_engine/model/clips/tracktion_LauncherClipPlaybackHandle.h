/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LauncherClipPlaybackHandle
{
public:
    //==============================================================================
    static LauncherClipPlaybackHandle forLooping (BeatRange loopRange,
                                                  BeatDuration offset);

    static LauncherClipPlaybackHandle forOneShot (BeatRange clipRange);

    //==============================================================================
    /** Signifies that the clip has started at a given position.
        This allows the source range and progress functions to calculate their results.
    */
    void start (BeatPosition);

    /** Signifies that the clip should stop. */
    void stop();

    /** If the clip has been started, this will return that position. */
    std::optional<BeatPosition> getStart() const;

    /** Represents two beat ranges that can be split. */
    struct SplitBeatRange
    {
        BeatRange range1, range2;
        bool playing1 = false, playing2 = false;
    };

    /** Converts a timeline range (e.g. returned from LaunchHandle)
        @return A SplitBeatRange representing one or two (if split) clip source ranges.
        @see LaunchHandle
    */
    SplitBeatRange timelineRangeToClipSourceRange (BeatRange) const;

    /** Returns the progress through a clip's source.
        If the clip position is before the clip has started or after it has stopped,
        this will return nullopt.
    */
    std::optional<float> getProgress (BeatPosition) const;

private:
    //==============================================================================
    BeatRange clipRange, loopRange;
    BeatDuration offset;
    bool isLooping = false;

    std::optional<BeatPosition> playStartPosition;

    LauncherClipPlaybackHandle() = default;
};

}} // namespace tracktion { inline namespace engine
