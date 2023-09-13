/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_LoopingMidiNode.h"

#define USE_PARTITION_INSERTION 1

namespace tracktion { inline namespace engine
{

namespace combining_node_utils
{
    // how much extra time to give a track before it gets cut off - to allow for plugins
    // that ring on.
    static constexpr BeatDuration decayTimeAllowance { 8_bd };
    static constexpr int secondsPerGroup = 8;

    static inline constexpr int timeToGroupIndex (TimePosition t) noexcept
    {
        return static_cast<int> (t.inSeconds()) / secondsPerGroup;
    }
}

//==============================================================================
struct CombiningNode::TimedNode
{
    TimedNode (std::unique_ptr<Node> sourceNode, BeatRange t)
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

    std::vector<Node*> getNodes() const
    {
        return nodesToProcess;
    }

    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info,
                        choc::buffer::ChannelArrayView<float> view)
    {
        auto info2 = info;
        info2.allocateAudioBuffer = [view] (choc::buffer::Size size) -> tracktion::graph::NodeBuffer
                                    {
                                        jassert (size.numFrames == view.getNumFrames());
                                        jassert (size.numChannels <= view.getNumChannels());
                                        
                                        return { view.getFirstChannels (size.numChannels), {} };
                                    };
        info2.deallocateAudioBuffer = nullptr;
        
        for (auto n : nodesToProcess)
            n->initialise (info2);
    }
    
    bool isReadyToProcess() const
    {
        return nodesToProcess.front()->isReadyToProcess();
    }

    void prefetchBlock (juce::Range<int64_t> referenceSampleRange)
    {
        for (auto n : nodesToProcess)
            n->prepareForNextBlock (referenceSampleRange);

       #if JUCE_DEBUG
        hasPrefetched = true;
       #endif
    }

    void process (ProcessContext& pc)
    {
        jassert (hasPrefetched);

        // Process all the Nodes
        for (auto n : nodesToProcess)
            n->process (pc.numSamples, pc.referenceSampleRange);
        
        // Then get the output from the source Node
        auto nodeOutput = node->getProcessedOutput();
        const auto numDestChannels = pc.buffers.audio.getNumChannels();
        const auto numChannelsToAdd = std::min (nodeOutput.audio.getNumChannels(), numDestChannels);

        if (numChannelsToAdd > 0)
            add (pc.buffers.audio.getFirstChannels (numChannelsToAdd),
                 nodeOutput.audio.getFirstChannels (numChannelsToAdd));
        
        pc.buffers.midi.mergeFrom (nodeOutput.midi);

       #if JUCE_DEBUG
        hasPrefetched = false;
       #endif
    }

    size_t getAllocatedBytes() const
    {
        size_t size = 0;
        
        for (auto n : nodesToProcess)
            size += n->getAllocatedBytes();

        return size;
    }
    
    const BeatRange time;

private:
    const std::unique_ptr<Node> node;
    std::vector<Node*> nodesToProcess;
   #if JUCE_DEBUG
    bool hasPrefetched = false;
   #endif

    JUCE_DECLARE_NON_COPYABLE (TimedNode)
};

//==============================================================================
CombiningNode::CombiningNode (EditItemID id, ProcessState& ps)
    : TracktionEngineNode (ps),
      itemID (id)
{
    jassert (getProcessState().getTempoSequence());
    hash_combine (nodeProperties.nodeID, itemID);
}

CombiningNode::~CombiningNode() {}

void CombiningNode::addInput (std::unique_ptr<Node> input, TimeRange time)
{
    jassert (time.getEnd() <= Edit::getMaximumEditEnd());
    addInput (std::move (input), toBeats (*getProcessState().getTempoSequence(), time));
}

void CombiningNode::addInput (std::unique_ptr<Node> input, BeatRange beatRange)
{
    assert (input != nullptr);

    if (beatRange.isEmpty())
        return;

    auto props = input->getNodeProperties();

    nodeProperties.hasAudio |= props.hasAudio;
    nodeProperties.hasMidi |= props.hasMidi;
    nodeProperties.numberOfChannels = std::max (nodeProperties.numberOfChannels, props.numberOfChannels);
    nodeProperties.latencyNumSamples = std::max (nodeProperties.latencyNumSamples, props.latencyNumSamples);
    hash_combine (nodeProperties.nodeID, props.nodeID);
    hash_combine (nodeProperties.nodeID, beatRange);

   #if USE_PARTITION_INSERTION
    const auto lower = std::partition_point (inputs.begin(), inputs.end(),
                                             [&] (const auto& i)
                                             {
                                                return i->time.getStart() < beatRange.getStart();
                                            });
    int i = static_cast<int> (std::distance (inputs.begin(), lower));
   #else
    int i;
    for (i = 0; i < inputs.size(); ++i)
        if (inputs.getUnchecked (i)->time.getStart() >= beatRange.getStart())
            break;
   #endif

    beatRange = BeatRange (beatRange.getStart(), beatRange.getLength() + combining_node_utils::decayTimeAllowance);
    auto tan = inputs.insert (i, new TimedNode (std::move (input), beatRange));

    // add the node to any groups it's near to.
    const auto& ts = *getProcessState().getTempoSequence();
    const auto overlapTime = TimeDuration::fromSeconds (combining_node_utils::secondsPerGroup / 2 + 2);
    const auto timeRange = toTime (ts, beatRange).expanded (overlapTime);
    const auto start = std::max (0, combining_node_utils::timeToGroupIndex (timeRange.getStart()));
    const auto end   = std::max (0, combining_node_utils::timeToGroupIndex (timeRange.getEnd()));

    while (groups.size() <= end)
        groups.add (new juce::Array<TimedNode*>());

    for (i = start; i <= end; ++i)
    {
        auto g = groups.getUnchecked (i);

       #if USE_PARTITION_INSERTION
        const auto lowerGroup = std::partition_point (g->begin(), g->end(),
                                                      [&] (auto in)
                                                      {
                                                          return in->time.getStart() < beatRange.getStart();
                                                      });
        const int j = static_cast<int> (std::distance (g->begin(), lowerGroup));
       #else
        int j;
        for (j = 0; j < g->size(); ++j)
            if (g->getUnchecked (j)->time.getStart() >= beatRange.getStart())
                break;
       #endif

        jassert (tan != nullptr);
        g->insert (j, tan);
    }
}

int CombiningNode::getNumInputs() const
{
    return inputs.size();
}

std::vector<Node*> CombiningNode::getInternalNodes()
{
    std::vector<Node*> leafNodes;

    for (auto i : inputs)
        for (auto n : i->getNodes())
            leafNodes.push_back (n);

    return leafNodes;
}

std::vector<tracktion::graph::Node*> CombiningNode::getDirectInputNodes()
{
    return {};
}

tracktion::graph::NodeProperties CombiningNode::getNodeProperties()
{
    return nodeProperties;
}

void CombiningNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
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

    // Inspect the old graph to find clips that need to be killed
    if (info.nodeGraphToReplace != nullptr)
    {
        if (auto oldNode = findNode<CombiningNode> (*info.nodeGraphToReplace,
                                                    [itemID = itemID] (auto& cn) { return cn.itemID == itemID; }))
        {
            queueNoteOffsForClipsNoLongerPresent (*oldNode);
        }
    }
}

bool CombiningNode::isReadyToProcess()
{
    return isReadyToProcessBlock.load (std::memory_order_acquire);
}

void CombiningNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    SCOPED_REALTIME_CHECK

    const auto editTime = getEditTimeRange();
    prefetchGroup (referenceSampleRange, editTime, getEditBeatRange());

    // Update ready to process state based on nodes intersecting this time
    isReadyToProcessBlock.store (true, std::memory_order_release);
    
    if (auto g = groups[combining_node_utils::timeToGroupIndex (editTime.getStart())])
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
    const auto editBeats = getEditBeatRange();

    SCOPED_REALTIME_CHECK
    const auto initialEvents = pc.buffers.midi.size();

    // Merge any note-offs from clips that have been deleted
    pc.buffers.midi.mergeFromAndClear (noteOffEventsToSend);

    // Then process the list
    if (auto g = groups[combining_node_utils::timeToGroupIndex (getEditTimeRange().getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.getEnd() > editBeats.getStart())
            {
                if (tan->time.getStart() >= editBeats.getEnd())
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

void CombiningNode::prefetchGroup (juce::Range<int64_t> referenceSampleRange, TimeRange editTime, BeatRange editBeats)
{
    if (auto g = groups[combining_node_utils::timeToGroupIndex (editTime.getStart())])
    {
        for (auto tan : *g)
        {
            if (tan->time.getEnd() > editBeats.getStart())
            {
                if (tan->time.getStart() >= editBeats.getEnd())
                    break;

                tan->prefetchBlock (referenceSampleRange);
            }
        }
    }
}

void CombiningNode::queueNoteOffsForClipsNoLongerPresent (const CombiningNode& oldCombiningNode)
{
    // Find any LoopingMidiNodes that are no longer present
    // Add note-offs for any note-ons they have
    std::vector<EditItemID> currentNodeIDs;

    for (auto timedNode : inputs)
        for (auto node : timedNode->getNodes())
            if (auto loopingMidiNode = dynamic_cast<LoopingMidiNode*> (node))
                currentNodeIDs.push_back (loopingMidiNode->getItemID());

    for (auto oldTimedNode : oldCombiningNode.inputs)
    {
        for (auto oldNode : oldTimedNode->getNodes())
        {
            if (auto oldLoopingMidiNode = dynamic_cast<LoopingMidiNode*> (oldNode))
            {
                if (std::find (currentNodeIDs.begin(), currentNodeIDs.end(), oldLoopingMidiNode->getItemID())
                    == currentNodeIDs.end())
                {
                    oldLoopingMidiNode->getActiveNoteList()->iterate ([this, mpeSourceID = oldLoopingMidiNode->getMPESourceID()]
                                                                      (int chan, int note)
                                                                      {
                                                                          noteOffEventsToSend.addMidiMessage (juce::MidiMessage::noteOff (chan, note), mpeSourceID);
                                                                      });
                }
            }
        }
    }
}

}} // namespace tracktion { inline namespace engine
