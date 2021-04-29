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

ProcessState::ProcessState (tracktion_graph::PlayHeadState& phs)
    : playHeadState (phs)
{
}

void ProcessState::update (double newSampleRate, juce::Range<int64_t> newReferenceSampleRange)
{
    if (sampleRate != newSampleRate)
        playHeadState.playHead.setScrubbingBlockLength (tracktion_graph::timeToSample (0.08, newSampleRate));
    
    playHeadState.playHead.setReferenceSampleRange (newReferenceSampleRange);
    playHeadState.update (newReferenceSampleRange);

    sampleRate = newSampleRate;
    numSamples = (int) newReferenceSampleRange.getLength();
    referenceSampleRange = newReferenceSampleRange;
    
    const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, newReferenceSampleRange);
    jassert (! splitTimelineRange.isSplit);
    timelineSampleRange = splitTimelineRange.timelineRange1;
    jassert (timelineSampleRange.getLength() == 0
             || timelineSampleRange.getLength() == newReferenceSampleRange.getLength());

    editTimeRange = EditTimeRange (tracktion_graph::sampleToTime (timelineSampleRange, sampleRate));
}


//==============================================================================
//==============================================================================
TracktionEngineNode::TracktionEngineNode (ProcessState& ps)
    : processState (ps)
{
}


} // namespace tracktion_engine
