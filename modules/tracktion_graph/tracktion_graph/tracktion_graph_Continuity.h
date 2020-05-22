/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
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
    void update();

    /** Returns true if the play head did not jump and this block is contiguous with the previous block. */
    inline bool isContiguousWithPreviousBlock() noexcept    { return ! didPlayheadJump(); }

    /** Returns true if the play head jumped. */
    inline bool didPlayheadJump() noexcept                  { return playheadJumped; }

    PlayHead& playHead;

private:
    std::chrono::system_clock::time_point lastUserInteractionTime;
    bool playheadJumped = false, isPlayHeadRunning = false;
};


//==============================================================================
//==============================================================================
/**
    Holds some functions about exactly how a block relates the the previous block.
*/
namespace Continuity
{
    enum class Flags
    {
        contiguous           = 1,
        playheadJumped       = 2,
        lastBlockBeforeLoop  = 4,
        firstBlockOfLoop     = 8
    };

    static inline bool isContiguousWithPreviousBlock (int flags) noexcept    { return (flags & (int) Flags::contiguous) != 0; }
    static inline bool isFirstBlockOfLoop (int flags) noexcept               { return (flags & (int) Flags::firstBlockOfLoop) != 0; }
    static inline bool isLastBlockOfLoop (int flags) noexcept                { return (flags & (int) Flags::lastBlockBeforeLoop) != 0; }
    static inline bool didPlayheadJump (int flags) noexcept                  { return (flags & (int) Flags::playheadJumped) != 0; }
}


//==============================================================================
//==============================================================================
/** Breaks a process call in to two if it intersects a loop boundry and calls the
    processSection method twice, once for each side of the loop and passes the
    correct continuity flags to it.
*/
template<typename CallbackType>
static void invokeSplitProcessor (const tracktion_graph::Node::ProcessContext& pc,
                                  PlayHeadState& playHeadState,
                                  CallbackType& target)
{
    auto& playHead = playHeadState.playHead;
    const auto splitTimelineRange = tracktion_graph::referenceSampleRangeToSplitTimelineRange (playHead, pc.streamSampleRange);
    int continuityFlags = (int) (playHeadState.isContiguousWithPreviousBlock() ? Continuity::Flags::contiguous
                                                                               : Continuity::Flags::playheadJumped);
    
    if (splitTimelineRange.isSplit)
    {
        const auto firstNumSamples = splitTimelineRange.timelineRange1.getLength();
        const auto firstRange = pc.streamSampleRange.withLength (firstNumSamples);
        
        {
            auto inputAudio = pc.buffers.audio.getSubBlock (0, (size_t) firstNumSamples);
            auto& inputMidi = pc.buffers.midi;
            
            tracktion_graph::Node::ProcessContext pc1 { firstRange, { inputAudio , inputMidi } };
            continuityFlags |= (int) Continuity::Flags::lastBlockBeforeLoop;
            target.processSection (pc1, splitTimelineRange.timelineRange1, continuityFlags);
        }
        
        {
            const auto secondNumSamples = splitTimelineRange.timelineRange2.getLength();
            const auto secondRange = juce::Range<int64_t>::withStartAndLength (firstRange.getEnd(), secondNumSamples);
            
            auto inputAudio = pc.buffers.audio.getSubBlock ((size_t) firstNumSamples, (size_t) secondNumSamples);
            auto& inputMidi = pc.buffers.midi;
            
            //TODO: Use a scratch MidiMessageArray and then merge it back with the offset time
            tracktion_graph::Node::ProcessContext pc2 { secondRange, { inputAudio , inputMidi } };
            continuityFlags = (int) Continuity::Flags::firstBlockOfLoop;
            target.processSection (pc2, splitTimelineRange.timelineRange2, continuityFlags);
        }
    }
    else if (playHead.isLooping() && ! playHead.isRollingIntoLoop())
    {
        // In the case where looping happens to line up exactly with the audio
        // blocks being rendered, set the proper continuity flags
        auto loop = playHead.getLoopRange();

        auto s = splitTimelineRange.timelineRange1.getStart();
        auto e = splitTimelineRange.timelineRange1.getEnd();

        if (e == loop.getEnd())
            continuityFlags |= (int) Continuity::Flags::lastBlockBeforeLoop;
        if (s == loop.getStart())
            continuityFlags = (int) Continuity::Flags::firstBlockOfLoop;

        target.processSection (pc, splitTimelineRange.timelineRange1, continuityFlags);
    }
    else
    {
        target.processSection (pc, splitTimelineRange.timelineRange1, continuityFlags);
    }
}


//==============================================================================
inline void PlayHeadState::update()
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

    playheadJumped = isPlayingNow && jumped;
}

}
