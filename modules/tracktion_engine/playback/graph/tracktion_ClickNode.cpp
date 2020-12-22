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

ClickNode::ClickNode (Edit& e, int numAudioChannels, bool isMidi, tracktion_graph::PlayHead& playHeadToUse)
    : edit (e), playHead (playHeadToUse),
      clickGenerator (edit, isMidi, Edit::maximumLength),
      numChannels (numAudioChannels), generateMidi (isMidi)
{
}

std::vector<tracktion_graph::Node*> ClickNode::getDirectInputNodes()
{
    return {};
}

tracktion_graph::NodeProperties ClickNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasAudio = ! generateMidi;
    props.hasMidi = generateMidi;
    props.numberOfChannels = numChannels;
    props.nodeID = (size_t) edit.getProjectItemID().getRawID();

    return props;
}

void ClickNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    clickGenerator.prepareToPlay (sampleRate,
                                  tracktion_graph::sampleToTime (playHead.getPosition(), sampleRate));
}

bool ClickNode::isReadyToProcess()
{
    return true;
}

void ClickNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    if (playHead.isUserDragging() || ! playHead.isPlaying())
        return;

    const auto splitTimelinePosition = referenceSampleRangeToSplitTimelineRange (playHead, pc.referenceSampleRange);
    const EditTimeRange editTime (tracktion_graph::sampleToTime (splitTimelinePosition.timelineRange1, sampleRate));
    clickGenerator.processBlock (&pc.buffers.audio, &pc.buffers.midi, editTime);
}

}
