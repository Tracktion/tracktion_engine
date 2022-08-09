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

namespace midi_combining_node_utils
{
    // how much extra time to give a track before it gets cut off - to allow for plugins
    // that ring on.
    static constexpr double decayTimeAllowance = 5.0;
    static constexpr int secondsPerGroup = 8;

    static inline constexpr int timeToGroupIndex (TimePosition t) noexcept
    {
        return static_cast<int> (t.inSeconds()) / secondsPerGroup;
    }
}

//==============================================================================
struct MidiCombiningNode::TimedNode
{
    TimedNode (std::unique_ptr<LoopingMidiNode> sourceNode, TimeRange t)
        : time (t), node (std::move (sourceNode))
    {
        // This only works with single depth MIDI Nodes
        assert (node->getDirectInputNodes().size() == 0);
    }

    std::vector<Node*> getNodes() const
    {
        return { node.get() };
    }

    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
    {
        node->initialise (info);
    }
    
    bool isReadyToProcess() const
    {
        return node->isReadyToProcess();
    }

    void prefetchBlock (juce::Range<int64_t> referenceSampleRange)
    {
        if (hasPrefetched)
            return;
        
        node->prepareForNextBlock (referenceSampleRange);
        hasPrefetched = true;
    }

    void process (ProcessContext& pc)
    {
        jassert (hasPrefetched);
        
        node->Node::process (pc.numSamples, pc.referenceSampleRange);
        
        // Then get the output from the source Node
        auto nodeOutput = node->getProcessedOutput();
        pc.buffers.midi.mergeFrom (nodeOutput.midi);
        
        hasPrefetched = false;
    }

    size_t getAllocatedBytes() const
    {
        return node->getAllocatedBytes();
    }
    
    TimeRange time;
    const std::unique_ptr<LoopingMidiNode> node;

private:
    bool hasPrefetched = false;

    JUCE_DECLARE_NON_COPYABLE (TimedNode)
};

//==============================================================================
MidiCombiningNode::MidiCombiningNode (EditItemID id, ProcessState& ps)
    : TracktionEngineNode (ps),
      itemID (id)
{
    hash_combine (nodeProperties.nodeID, itemID);
}

MidiCombiningNode::~MidiCombiningNode() {}

void MidiCombiningNode::addInput (std::unique_ptr<LoopingMidiNode> input, TimeRange time)
{
    assert (input != nullptr);

    if (time.isEmpty())
        return;

    auto props = input->getNodeProperties();
    
    nodeProperties.hasAudio |= props.hasAudio;
    nodeProperties.hasMidi |= props.hasMidi;
    nodeProperties.numberOfChannels = std::max (nodeProperties.numberOfChannels, props.numberOfChannels);
    nodeProperties.latencyNumSamples = std::max (nodeProperties.latencyNumSamples, props.latencyNumSamples);
    hash_combine (nodeProperties.nodeID, props.nodeID);

    int i;
    for (i = 0; i < inputs.size(); ++i)
        if (inputs.getUnchecked (i)->time.getStart() >= time.getStart())
            break;

    auto tan = inputs.insert (i, new TimedNode (std::move (input), time));

    jassert (time.getEnd() <= Edit::getMaximumEditEnd());

    // add the node to any groups it's near to.
    auto start = std::max (0, midi_combining_node_utils::timeToGroupIndex (time.getStart() - TimeDuration::fromSeconds (midi_combining_node_utils::secondsPerGroup / 2 + 2)));
    auto end   = std::max (0, midi_combining_node_utils::timeToGroupIndex (time.getEnd()   + TimeDuration::fromSeconds (midi_combining_node_utils::secondsPerGroup / 2 + 2)));

    while (groups.size() <= end)
        groups.add (new juce::Array<TimedNode*>());

    for (i = start; i <= end; ++i)
    {
        auto g = groups.getUnchecked (i);

        int j;
        for (j = 0; j < g->size(); ++j)
            if (g->getUnchecked (j)->time.getStart() >= time.getStart())
                break;

        jassert (tan != nullptr);
        g->insert (j, tan);
    }
}

int MidiCombiningNode::getNumInputs() const
{
    return inputs.size();
}

std::vector<Node*> MidiCombiningNode::getInternalNodes()
{
    std::vector<Node*> leafNodes;

    for (auto i : inputs)
        for (auto n : i->getNodes())
            leafNodes.push_back (n);

    return leafNodes;
}

std::vector<tracktion::graph::Node*> MidiCombiningNode::getDirectInputNodes()
{
    return {};
}

tracktion::graph::NodeProperties MidiCombiningNode::getNodeProperties()
{
    return nodeProperties;
}

void MidiCombiningNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    isReadyToProcessBlock.store (true, std::memory_order_release);

    for (auto& i : inputs)
    {
        i->prepareToPlay (info);
        
        if (! i->isReadyToProcess())
            isReadyToProcessBlock.store (false, std::memory_order_release);
    }

    // Inspect the old graph to find clips that need to be killed
    if (info.rootNodeToReplace != nullptr)
    {
        visitNodes (*info.rootNodeToReplace, [this, itemID = itemID] (Node& n)
                    {
                        if (auto midiCombiningNode = dynamic_cast<MidiCombiningNode*> (&n))
                            if (midiCombiningNode->itemID == itemID)
                                queueNoteOffsForClipsNoLongerPresent (*midiCombiningNode);
                    }, true);
    }
}

bool MidiCombiningNode::isReadyToProcess()
{
    return isReadyToProcessBlock.load (std::memory_order_acquire);
}

void MidiCombiningNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    SCOPED_REALTIME_CHECK

    const auto editTime = getEditTimeRange();
    prefetchGroup (referenceSampleRange, editTime);

    // Update ready to process state based on nodes intersecting this time
    isReadyToProcessBlock.store (true, std::memory_order_release);
    
    if (auto g = groups[midi_combining_node_utils::timeToGroupIndex (editTime.getStart())])
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

void MidiCombiningNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    const auto editTime = getEditTimeRange();
    const auto initialEvents = pc.buffers.midi.size();

    // Merge any note-offs from clips that have been deleted
    pc.buffers.midi.mergeFromAndClear (noteOffEventsToSend);

    // Then process the list
    if (auto g = groups[midi_combining_node_utils::timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.getEnd() > editTime.getStart())
            {
                if (tan->time.getStart() >= editTime.getEnd())
                    break;

                // Then process the buffer.
                // This will use the local buffer for the Nodes in the TimedNode and put the result in pc.buffers
                tan->process (pc);
            }
        }
    }

    if (pc.buffers.midi.size() > initialEvents)
        pc.buffers.midi.sortByTimestamp();
}

size_t MidiCombiningNode::getAllocatedBytes() const
{
    size_t size = 0;
    
    for (const auto& i : inputs)
        size += i->getAllocatedBytes();
    
    return size;
}

void MidiCombiningNode::prefetchGroup (juce::Range<int64_t> referenceSampleRange, TimeRange editTime)
{
    if (auto g = groups[midi_combining_node_utils::timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.getEnd() > editTime.getStart())
            {
                if (tan->time.getStart() >= editTime.getEnd())
                    break;

                tan->prefetchBlock (referenceSampleRange);
            }
        }
    }
}

void MidiCombiningNode::queueNoteOffsForClipsNoLongerPresent (const MidiCombiningNode& oldNode)
{
    // Find any LoopingMidiNodes that are no longer present
    // Add note-offs for any note-ons they have

    std::vector<EditItemID> currentNodeIDs;

    for (auto input : inputs)
        currentNodeIDs.push_back (input->node->getItemID());

    for (auto oldTimedNode : oldNode.inputs)
    {
        if (std::find (currentNodeIDs.begin(), currentNodeIDs.end(), oldTimedNode->node->getItemID())
            == currentNodeIDs.end())
        {
            oldTimedNode->node->getActiveNoteList()->iterate ([this, mpeSourceID = oldTimedNode->node->getMPESourceID()]
                                                              (int chan, int note)
                                                              {
                                                                  noteOffEventsToSend.addMidiMessage (juce::MidiMessage::noteOff (chan, note), mpeSourceID);
                                                              });
        }
    }
}

}} // namespace tracktion { inline namespace engine
