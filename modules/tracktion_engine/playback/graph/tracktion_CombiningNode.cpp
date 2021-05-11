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
    TimedNode (std::unique_ptr<Node> sourceNode, EditTimeRange t)
        : time (t), node (std::move (sourceNode))
    {
        for (auto n = node.get();;)
        {
            nodesToProcess.insert (nodesToProcess.begin(), n);
            auto inputNodes = n->getDirectInputNodes();
            
            if (inputNodes.empty())
                break;
            
            // This doesn't work with parallel input Nodes
            assert (inputNodes.size() == 1);
            n = inputNodes.front();
        }
    }

    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info,
                        choc::buffer::ChannelArrayView<float> view)
    {
        auto info2 = info;
        info2.allocateAudioBuffer = [&view] (choc::buffer::Size size)
                                    {
                                        jassert (size.numFrames == view.getNumFrames());
                                        jassert (size.numChannels <= view.getNumChannels());
                                        
                                        return view.getFirstChannels (size.numChannels);
                                    };
        
        for (auto n : nodesToProcess)
            n->initialise (info2);
    }
    
    bool isReadyToProcess() const
    {
        return nodesToProcess.front()->isReadyToProcess();
    }

    void prefetchBlock (juce::Range<int64_t> referenceSampleRange) const
    {
        for (auto n : nodesToProcess)
            n->prepareForNextBlock (referenceSampleRange);
    }

    void process (ProcessContext& pc) const
    {
        // Process all the Nodes
        for (auto n : nodesToProcess)
            n->process (pc.referenceSampleRange);
        
        // Then get the output from the source Node
        auto nodeOutput = node->getProcessedOutput();
        const auto numDestChannels = pc.buffers.audio.getNumChannels();
        const auto numChannelsToAdd = std::min (nodeOutput.audio.getNumChannels(), numDestChannels);

        if (numChannelsToAdd > 0)
            add (pc.buffers.audio.getFirstChannels (numChannelsToAdd),
                 nodeOutput.audio.getFirstChannels (numChannelsToAdd));
        
        pc.buffers.midi.mergeFrom (nodeOutput.midi);
    }

    size_t getAllocatedBytes() const
    {
        size_t size = 0;
        
        for (auto n : nodesToProcess)
            size += n->getAllocatedBytes();

        return size;
    }
    
    EditTimeRange time;

private:
    const std::unique_ptr<Node> node;
    std::vector<Node*> nodesToProcess;

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
            if (g->getUnchecked (j)->time.start >= time.start)
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
    isReadyToProcessBlock.store (true, std::memory_order_release);
    tempAudioBuffer.resize (choc::buffer::Size::create ((choc::buffer::ChannelCount) nodeProperties.numberOfChannels,
                                                        (choc::buffer::FrameCount) info.blockSize));

    for (auto& i : inputs)
    {
        i->prepareToPlay (info, tempAudioBuffer.getView());
        
        if (! i->isReadyToProcess())
            isReadyToProcessBlock.store (false, std::memory_order_release);
    }
}

bool CombiningNode::isReadyToProcess()
{
    return isReadyToProcessBlock.load (std::memory_order_acquire);
}

void CombiningNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    SCOPED_REALTIME_CHECK

    const double time = getEditTimeRange().getStart();
    prefetchGroup (referenceSampleRange, time);

    if (getPlayHeadState().isLastBlockOfLoop())
        prefetchGroup (referenceSampleRange, tracktion_graph::sampleToTime (getPlayHead().getLoopRange().getStart(), processState.sampleRate));

    // Update ready to process state based on nodes intersecting this time
    isReadyToProcessBlock.store (true, std::memory_order_release);
    
    if (auto g = groups[combining_node_utils::timeToGroupIndex (time)])
    {
        for (auto tan : *g)
        {
            if (! tan->isReadyToProcess())
            {
                isReadyToProcessBlock.store (false, std::memory_order_release);
                break;
            }
        }
    }
}

void CombiningNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    const auto editTime = getEditTimeRange();
    const auto initialEvents = pc.buffers.midi.size();

    if (auto g = groups[combining_node_utils::timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.end > editTime.getStart())
            {
                if (tan->time.start >= editTime.getEnd())
                    break;

                // Clear the allocated storage
                tempAudioBuffer.clear();
                
                // Then process the buffer.
                // This will use the local buffer for the Nodes in the TimedNode and put the result in pc.buffers
                tan->process (pc);
            }
        }
    }

    if (pc.buffers.midi.size() > initialEvents)
        pc.buffers.midi.sortByTimestamp();
}

size_t CombiningNode::getAllocatedBytes() const
{
    size_t size = tempAudioBuffer.getView().data.getBytesNeeded (tempAudioBuffer.getSize());
    
    for (const auto& i : inputs)
        size += i->getAllocatedBytes();
    
    return size;
}

void CombiningNode::prefetchGroup (juce::Range<int64_t> referenceSampleRange, double time)
{
    if (auto g = groups[combining_node_utils::timeToGroupIndex (time)])
        for (auto tan : *g)
            tan->prefetchBlock (referenceSampleRange);
}

}
