/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
/**
    Determines how this block releates to other previous render blocks and if the
    play head has jumped in time.
*/
class PlayHeadState
{
public:
    PlayHeadState (PlayHead& ph)
        : playHead (ph)
    {
    }

    /** Call once per block to update the jumped state.
        Usually called at the same time as the PlayHead::incrementReferenceSampleCount method.
    */
    void update (juce::Range<int64_t> referenceSampleRange);

    /** Returns true if the play head did not jump and this block is contiguous with the previous block. */
    inline bool isContiguousWithPreviousBlock() noexcept    { return ! (didPlayheadJump() || isFirstBlockOfLoop()); }

    /** Returns true if the play head jumped. */
    inline bool didPlayheadJump() noexcept                  { return playheadJumped; }

    /** Returns true if this is the first block of a loop. */
    inline bool isFirstBlockOfLoop() noexcept               { return firstBlockOfLoop; }

    /** Returns true if this is the last block of a loop. */
    inline bool isLastBlockOfLoop() noexcept                { return lastBlockOfLoop; }

    PlayHead& playHead;

private:
    std::chrono::system_clock::time_point lastUserInteractionTime;
    bool isPlayHeadRunning = false, playheadJumped = false, lastBlockOfLoop = false, firstBlockOfLoop = false;
};


//==============================================================================
inline void PlayHeadState::update (juce::Range<int64_t> referenceSampleRange)
{
    const bool isPlayingNow = playHead.isPlaying();
    bool jumped = false;

    if (lastUserInteractionTime != playHead.getLastUserInteractionTime())
    {
        lastUserInteractionTime = playHead.getLastUserInteractionTime();
        jumped = true;
    }

    if (isPlayingNow != isPlayHeadRunning)
    {
        isPlayHeadRunning = isPlayingNow;
        jumped = jumped || isPlayHeadRunning;
    }

    playheadJumped = jumped;
    
    // Next check if this is the start or end of a loop range
    if (playHead.isLooping())
    {
        const auto timelineLoopRange = playHead.getLoopRange();
        const auto startTimelinePos = playHead.referenceSamplePositionToTimelinePosition (referenceSampleRange.getStart());
        const auto endTimelinePos = playHead.referenceSamplePositionToTimelinePosition (referenceSampleRange.getEnd() - 1) + 1;
        // The -1/+1 is here to avoid the last sample being wrapped to the beginning of the loop range
        
        if (playHead.isRollingIntoLoop())
            firstBlockOfLoop = false;
        else
            firstBlockOfLoop = startTimelinePos == timelineLoopRange.getStart();
        
        lastBlockOfLoop = endTimelinePos == timelineLoopRange.getEnd();
    }
    else
    {
        firstBlockOfLoop = false;
        lastBlockOfLoop = false;
    }
}

}}
