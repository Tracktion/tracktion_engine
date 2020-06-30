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

namespace combining_node_utils
{
    // how much extra time to give a track before it gets cut off - to allow for plugins
    // that ring on.
    static constexpr double decayTimeAllowance = 5.0;
    static constexpr int secondsPerGroup = 8;

    static inline constexpr int timeToGroupIndex (double t) noexcept
    {
        return ((int) t) / secondsPerGroup;
    }
}

//==============================================================================
struct CombiningNode::TimedNode
{
    TimedNode (std::unique_ptr<Node> n, EditTimeRange t)
        : time (t), node (std::move (n))
    {
    }

    EditTimeRange time;
    const std::unique_ptr<Node> node;

    void process (const ProcessContext& pc) const
    {
        node->process (pc.referenceSampleRange);
        
        auto nodeOutput = node->getProcessedOutput();
        const auto numDestChannels = pc.buffers.audio.getNumChannels();
        const auto numChannelsToAdd = std::min (nodeOutput.audio.getNumChannels(), numDestChannels);

        if (numChannelsToAdd > 0)
            pc.buffers.audio.getSubsetChannelBlock (0, numChannelsToAdd)
                .add (nodeOutput.audio.getSubsetChannelBlock (0, numChannelsToAdd));
        
        pc.buffers.midi.mergeFrom (nodeOutput.midi);
    }

    JUCE_DECLARE_NON_COPYABLE (TimedNode)
};

//==============================================================================
CombiningNode::CombiningNode (ProcessState& ps)
    : TracktionEngineNode (ps)
{
}

CombiningNode::~CombiningNode() {}

void CombiningNode::addInput (std::unique_ptr<Node> input, EditTimeRange time)
{
    assert (input != nullptr);

    if (time.isEmpty())
        return;

    auto props = input->getNodeProperties();
    
    nodeProperties.hasAudio |= props.hasAudio;
    nodeProperties.hasMidi |= props.hasMidi;
    nodeProperties.numberOfChannels = std::max (nodeProperties.numberOfChannels, props.numberOfChannels);
    nodeProperties.latencyNumSamples = std::max (nodeProperties.latencyNumSamples, props.latencyNumSamples);
    tracktion_graph::hash_combine (nodeProperties.nodeID, props.nodeID);

    int i;
    for (i = 0; i < inputs.size(); ++i)
        if (inputs.getUnchecked (i)->time.start >= time.getStart())
            break;

    auto tan = inputs.insert (i, new TimedNode (std::move (input), time));

    jassert (time.end <= Edit::maximumLength);

    // add the node to any groups it's near to.
    auto start = std::max (0, combining_node_utils::timeToGroupIndex (time.start - (combining_node_utils::secondsPerGroup / 2 + 2)));
    auto end   = std::max (0, combining_node_utils::timeToGroupIndex (time.end   + (combining_node_utils::secondsPerGroup / 2 + 2)));

    while (groups.size() <= end)
        groups.add (new juce::Array<TimedNode*>());

    for (i = start; i <= end; ++i)
    {
        auto g = groups.getUnchecked (i);

        int j;
        for (j = 0; j < g->size(); ++j)
            if (g->getUnchecked(j)->time.start >= time.start)
                break;

        jassert (tan != nullptr);
        g->insert (j, tan);
    }
}

std::vector<tracktion_graph::Node*> CombiningNode::getDirectInputNodes()
{
    return {};
}

tracktion_graph::NodeProperties CombiningNode::getNodeProperties()
{
    return nodeProperties;
}

void CombiningNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    for (auto& i : inputs)
        i->node->initialise (info);
}

bool CombiningNode::isReadyToProcess()
{
    for (auto& i : inputs)
        if (! i->node->isReadyToProcess())
            return false;

    return true;
}

void CombiningNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    SCOPED_REALTIME_CHECK

    prefetchGroup (referenceSampleRange, getEditTimeRange().getStart());

    if (getPlayHeadState().isLastBlockOfLoop())
        prefetchGroup (referenceSampleRange, tracktion_graph::sampleToTime (getPlayHead().getLoopRange().getStart(), processState.sampleRate));
}

void CombiningNode::process (const ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    const auto editTime = getEditTimeRange();
    
    if (auto g = groups[combining_node_utils::timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.end > editTime.getStart())
            {
                if (tan->time.start >= editTime.getEnd())
                    break;

                tan->process (pc);
            }
        }
    }
}

void CombiningNode::prefetchGroup (juce::Range<int64_t> referenceSampleRange, double time)
{
    if (auto g = groups[combining_node_utils::timeToGroupIndex (time)])
        for (auto tan : *g)
            tan->node->prepareForNextBlock (referenceSampleRange);
}

}
