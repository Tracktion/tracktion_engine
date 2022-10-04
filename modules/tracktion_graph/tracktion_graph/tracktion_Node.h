/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

//==============================================================================
//==============================================================================
/**
    //==============================================================================
    Aims:
    - Separate graph structure from processing and any model
    - Ensure nodes can be processed multi-threaded which scales independantly of graph complexity
    - Processing can happen in any sized block (up to the maximum prepared for)
    - Process calls will only ever get the number of channels to fill that they report
    - Process calls will always provide empty buffers so nodes can simply "add" in to them. (Measure performance here)
    - Processing in float or double

    //==============================================================================
    Notes:
    - Each node should have pointers to its inputs
    - When a node is processed, it should check its inputs to see if they have produced outputs
    - If they have, that node can be processed. If they haven't the processor can try another node
    - If one node reports latency, every other node being summed with it will need to be delayed up to the same ammount
    - The reported latency of a node is the max of all its input latencies
 
    //==============================================================================
    Initialisation:
    - There really needs to be two stages for this:
        - Stage 1 is a simple visit of all the base nodes (i.e. no send/return connections).
          This will get a list of all the nodes in the graph
        - Stage 2 will be the initialise call with a flat list of all the nodes.
          Send/return nodes can use this to add inputs from the graph (e.g. based on busNum)
          (However, at this stage, return nodes might choose to add latency nodes to balance themselves???)
          (Maybe this should have to happen when the graph is built? That would mean latency needs to be fixed,
          and therefore plugins would need to be initialised before graph construction.)
    - Now we have a fully connected graph we can do a proper DFS:
        - This can be used to balance processing buffers between the graph by finding unconnected nodes
        - Finding a processing order by building "stacks" of nodes starting with leafs
    - This might look like the following steps:
        - visitAcyclic (Visitor) [only visit nodes that it was constructed with, used to create the flat list]
        - initialise (const PlaybackInitialisationInfo&) [used to add extra connections and balance latency etc.]
        - visitDFS (Visitor) [used to get list of nodes to process and can be called multiple times to optimise graph etc.]
 
    Avoiding incontinuities when latency changes:
    - One of the bg problems with this latency balancing approach is when the graph is rebuilt, any stateful buffers will be lost.
    - The only way I can think of solving this is to either reusue the latency nodes directly or reuse the buffers
    - The biggest problem here is finding which nodes correspond to the new and old graphs
    - The only way I can currently think of is:
        - Each node to have a UID when it is constructed based on its properties (source material, connections etc.)
        - During the initialisation stage, the old nodes can be passed on to the new nodes and if they have the same UID, the take any resources they need
        - There will probably need to be some validation to ensure that two nodes don't have the same UID
        - For things like clip/plugin nodes the UID is simple (EditItemID)
        - For latency nodes etc. it will probably have to be based on the input ID and channel index etc.
 
    Buffer optimisation:
    - As there will be a lot of nodes, it makes sense to reduce the memory footprint by reusing audio and MIDI buffers
    - There are two ways I can think of doing this:
        1. Analyse the graph and where nodes are sequential but not directly connected, use the same buffer
        2. Use a buffer pool and release it when all nodes have read from it.
            - This would probably require all nodes that need the output buffer to call "retain" on the node before the
              processing stage and then "release" once they're done with it. When the count drops to 0 the buffers can
              be released back to the pool
*/


namespace tracktion { inline namespace graph
{

class Node;

//==============================================================================
//==============================================================================
/** Creates a node of the given type and returns it as the base Node class. */
template<typename NodeType, typename... Args>
std::unique_ptr<Node> makeNode (Args&&... args)
{
    return std::unique_ptr<tracktion::graph::Node> (std::move (std::make_unique<NodeType> (std::forward<Args> (args)...)));
}


//==============================================================================
/**
    Holds a view over some data and optionally some storage for that data.
*/
struct NodeBuffer
{
    choc::buffer::ChannelArrayView<float> view;
    choc::buffer::ChannelArrayBuffer<float> data;
};

//==============================================================================
/**
    A Node and its ID cached for quick lookup (without having to traverse the graph).
*/
struct NodeAndID
{
    Node* node = nullptr;
    size_t id = 0;
};

/** Compares two NodeAndIDs. */
inline bool operator< (NodeAndID n1, NodeAndID n2)
{
    return n1.id < n2.id;
}

/** Compares two NodeAndIDs. */
inline bool operator== (NodeAndID n1, NodeAndID n2)
{
    return n1.node == n2.node && n1.id == n2.id;
}

/**
    Holds a graph in an order ready for processing and a sorted map for quick lookups.
*/
struct NodeGraph
{
    std::unique_ptr<Node> rootNode;
    std::vector<Node*> orderedNodes;
    std::vector<NodeAndID> sortedNodes;
};


/** Transforms a Node and then returns a NodeGraph of it ready to be initialised. */
std::unique_ptr<NodeGraph> createNodeGraph (std::unique_ptr<Node>);


//==============================================================================
/** Passed into Nodes when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PlaybackInitialisationInfo
{
    double sampleRate;
    int blockSize;
    NodeGraph& nodeGraph;
    NodeGraph* nodeGraphToReplace = nullptr;
    std::function<NodeBuffer (choc::buffer::Size)> allocateAudioBuffer = nullptr;
    std::function<void (NodeBuffer&&)> deallocateAudioBuffer = nullptr;
};

/** Holds some really basic properties of a node */
struct NodeProperties
{
    bool hasAudio = false;
    bool hasMidi = false;
    int numberOfChannels = 0;
    int latencyNumSamples = 0;
    size_t nodeID = 0;
};

//==============================================================================
//==============================================================================
enum class ClearBuffers
{
    no, /**< Don't clear buffers before passing them to process, your subclass will take care of that. */
    yes /**< Do clear buffers before passing to process so your subclass can simply add in to them. */
};

enum class AllocateAudioBuffer
{
    no, /**< Don't allocate an audio buffer, your subclass will ignore the dest buffer passed to process and simply use setAudioOutput to pass the buffer along. */
    yes /**< Do allocate an audio buffer so your subclass use the dest buffer passed to process. */
};

/** Holds some hints that _might_ be used by the Node or players to improve efficiency. */
struct NodeOptimisations
{
    ClearBuffers clear = ClearBuffers::yes;
    AllocateAudioBuffer allocate = AllocateAudioBuffer::yes;
};

//==============================================================================
//==============================================================================
/**
    Main graph Node processor class.
    Nodes are combined together to form a graph with a root Node which can then
    be initialsed and processed.
    
    Subclasses should implement the various virtual methods but never call these
    directly. They will be called automatically by the non-virtual methods.
*/
class Node
{
public:
    Node() = default;
    virtual ~Node() = default;
    
    //==============================================================================
    /** Call once after the graph has been constructed to initialise buffers etc. */
    void initialise (const PlaybackInitialisationInfo&);
    
    /** Call before processing the next block, used to reset the process status. */
    void prepareForNextBlock (juce::Range<int64_t> referenceSampleRange);
    
    /** Call to process the node, which will in turn call the process method with the
        buffers to fill.
        @param numSamples           The number of samples that need to be processed.
        @param referenceSampleRange The monotonic stream time in samples.
                                    This will be passed to the ProcessContext during the
                                    process callback so nodes can use this to determine file
                                    reading positions etc. Some nodes may ignore this completely.
                                    N.B. the length of this may be different to the number of
                                    samples to be processed so don't rely on them being the same!
    */
    void process (choc::buffer::FrameCount numSamples, juce::Range<int64_t> referenceSampleRange);
    
    /** Returns true if this node has processed and its outputs can be retrieved. */
    bool hasProcessed() const;
    
    /** Contains the buffers for a processing operation. */
    struct AudioAndMidiBuffer
    {
        choc::buffer::ChannelArrayView<float> audio;
        tracktion_engine::MidiMessageArray& midi;
    };

    /** Returns the processed audio and MIDI output.
        Must only be called after hasProcessed returns true.
    */
    AudioAndMidiBuffer getProcessedOutput();

    //==============================================================================
    /** Called after construction to give the node a chance to modify its topology.
        This should return true if any changes were made to the topology as this
        indicates that the method may need to be called again after other nodes have
        had their toplogy changed.
    */
    virtual bool transform (Node& /*rootNode*/) { return false; }
    
    /** Should return all the inputs directly feeding in to this node. */
    virtual std::vector<Node*> getDirectInputNodes() { return {}; }

    /** Can return Nodes that are internal to this Node but don't make up the main
        graph constructed from getDirectInputNodes().
        Most uses cases won't need to implement this but it could be used in
        situations where sub-graphs are created and processed internally to a Node
        but other Nodes in a graph still need to have access to them. This call can
        make them visible.
    */
    virtual std::vector<Node*> getInternalNodes() { return {}; }

    /** Should return the properties of the node.
        This should not be called until after initialise.
    */
    virtual NodeProperties getNodeProperties() = 0;

    /** Should return true when this node is ready to be processed.
        This is usually when its input's output buffers are ready.
    */
    virtual bool isReadyToProcess() = 0;
    
    /** Struct to describe a single iteration of a process call. */
    struct ProcessContext
    {
        choc::buffer::FrameCount numSamples;
        juce::Range<int64_t> referenceSampleRange;
        AudioAndMidiBuffer buffers;
    };

    //==============================================================================
    /** Retains the buffers so they won't be deallocated after the Node has processed.
        You shouldn't normally need to call this unles your Node player has special
        requirements.
    */
    void retain();

    /** Releases the buffers allowing internal storage to be deallocated.
        You shouldn't normally need to call this unles your Node player has special
        requirements.
    */
    void release();

    //==============================================================================
    /** @internal */
    void* internal = nullptr;
    int numOutputNodes = -1;
    virtual size_t getAllocatedBytes() const;

protected:
    /** Called once before playback begins for each node.
        Use this to allocate buffers etc.
        This step can be used to modify the topology of the graph (i.e. add/remove nodes).
        However, if you do this, you must make sure to call initialise on them so they are
        fully prepared for processing.
    */
    virtual void prepareToPlay (const PlaybackInitialisationInfo&) {}

    /** Called before once on all Nodes before they are processed.
        This can be used to prefetch audio data or update mute statuses etc..
    */
    virtual void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) {}

    /** Called when the node is to be processed.
        This should add in to the buffers available making sure not to change their size at all.
    */
    virtual void process (ProcessContext&) = 0;

    //==============================================================================
    /** This can be called to provide some hints about allocating or playing back a Node to improve efficiency.
        Be careful with these as they change the default and often expected behaviour.
    */
    void setOptimisations (NodeOptimisations);
    
    /** This can be called during your process function to set a view to the output.
        This is useful to avoid having to allocate an internal buffer and always fill it if you're
        just passing on data.
    */
    void setAudioOutput (Node* sourceNode, const choc::buffer::ChannelArrayView<float>&);
    
private:
    std::atomic<bool> hasBeenProcessed { false };
    choc::buffer::Size audioBufferSize;
    choc::buffer::ChannelArrayBuffer<float> audioBuffer;
    choc::buffer::ChannelArrayView<float> audioView, allocatedView;
    tracktion_engine::MidiMessageArray midiBuffer;
    std::atomic<int> numSamplesProcessed { 0 }, retainCount { 0 };
    NodeOptimisations nodeOptimisations;

    std::vector<Node*> directInputNodes;
    std::atomic<Node*> nodeToRelease { nullptr };
    std::function<NodeBuffer (choc::buffer::Size)> allocateAudioBuffer = nullptr;
    std::function<void (NodeBuffer&&)> deallocateAudioBuffer = nullptr;

   #if JUCE_DEBUG
    std::atomic<bool> isBeingProcessed { false };
   #endif
};

//==============================================================================
//==============================================================================
/** Should call the visitor for any direct inputs to the node exactly once.
    If preordering is true, nodes will be visited before their inputs, if
    false, inputs will be visited first.
 
    @param Visitor has the signature @code void (Node&) @endcode
*/
template<typename Visitor>
void visitNodes (Node&, Visitor&&, bool preordering);

//==============================================================================
/** Specifies the ordering algorithm. */
enum class VertexOrdering
{
    preordering,            // The order in which nodes are first visited
    postordering,           // The order in which nodes are last visited
    reversePreordering,     // The reverse of the preordering
    reversePostordering,    // The reverse of the postordering
    bfsPreordering,         // A breadth-first search
    bfsReversePreordering   // A reversed breadth-first search
};

/** Returns all the nodes in a Node graph in the order given by vertexOrdering. */
static inline std::vector<Node*> getNodes (Node&, VertexOrdering);


//==============================================================================
//==============================================================================
/** Gives the graph a chance to modify its topology e.g. to connect send/return
    nodes and balance latency etc.
    Call this once after construction and it will call the Node::transform() method
    repeatedly for Node until they all return false indicating no topological
    changes have been made.
*/
static inline std::vector<Node*> transformNodes (Node& rootNode)
{
    for (;;)
    {
        bool needToTransformAgain = false;

        auto allNodes = getNodes (rootNode, VertexOrdering::postordering);

        for (auto node : allNodes)
            if (node->transform (rootNode))
                needToTransformAgain = true;

        if (! needToTransformAgain)
            return allNodes;
    }
}


//==============================================================================
//==============================================================================
inline void Node::initialise (const PlaybackInitialisationInfo& info)
{
    prepareToPlay (info);
    
    auto props = getNodeProperties();
    audioBufferSize = choc::buffer::Size::create ((choc::buffer::ChannelCount) props.numberOfChannels,
                                                  (choc::buffer::FrameCount) info.blockSize);

    if (info.allocateAudioBuffer)
    {
        allocateAudioBuffer = info.allocateAudioBuffer;
        deallocateAudioBuffer = info.deallocateAudioBuffer;
    }
    else if (nodeOptimisations.allocate == AllocateAudioBuffer::yes)
    {
        audioBuffer.resize (audioBufferSize);
    }
    
    directInputNodes = getDirectInputNodes();
}

inline void Node::prepareForNextBlock (juce::Range<int64_t> referenceSampleRange)
{
    // Only do this once as prepare may be called multiple times
    if (retainCount == 0)
    {
        assert (directInputNodes.size() == getDirectInputNodes().size());
        nodeToRelease.store (nullptr, std::memory_order_relaxed); // Reset in case the output node behaviour changes
        
        retain();
        
        for (auto& n : directInputNodes)
            n->retain();
    }
    
    hasBeenProcessed.store (false, std::memory_order_release);
    prefetchBlock (referenceSampleRange);
}

inline void Node::process (choc::buffer::FrameCount numSamples, juce::Range<int64_t> referenceSampleRange)
{
   #if JUCE_DEBUG
    assert (! isBeingProcessed);
    isBeingProcessed = true;
    
    for (auto n : directInputNodes)
        assert (n->hasProcessed());
   #endif
    
    // First, allocate buffers if possible
    if (allocateAudioBuffer)
    {
        auto nodeBuffer = allocateAudioBuffer (audioBufferSize);
        audioBuffer = std::move (nodeBuffer.data);
        allocatedView = std::move (nodeBuffer.view);
        assert (audioBufferSize == allocatedView.getSize());
    }

    if (nodeOptimisations.clear == ClearBuffers::yes)
    {
        audioBuffer.clear();
        midiBuffer.clear();
    }
    
    const auto numChannelsBeforeProcessing = audioBuffer.getNumChannels();
    const auto numSamplesBeforeProcessing = audioBuffer.getNumFrames();
    juce::ignoreUnused (numChannelsBeforeProcessing, numSamplesBeforeProcessing);

    jassert (numSamples > 0); // This must be a valid number of samples to process
    jassert (numChannelsBeforeProcessing == 0 || numSamples <= audioBuffer.getNumFrames());
    
    if (allocatedView.getSize() == audioBufferSize)
    {
        // Use a pre-allocated view if one has been initialised
        audioView = allocatedView;
    }
    else
    {
        // Fallback to the internal buffer or an empty view
        audioView = ((nodeOptimisations.allocate == AllocateAudioBuffer::yes ? audioBuffer.getView()
                                                                             : choc::buffer::ChannelArrayView<float> { {}, audioBufferSize }));
    }
    
    audioView = audioView.getStart (numSamples);

    auto destAudioView = audioView;
    ProcessContext pc { numSamples, referenceSampleRange, { destAudioView, midiBuffer } };
    process (pc);
    numSamplesProcessed.store ((int) numSamples, std::memory_order_release);
    
    jassert (numChannelsBeforeProcessing == audioBuffer.getNumChannels());
    jassert (numSamplesBeforeProcessing == audioBuffer.getNumFrames());

    release();
    
    for (auto& n : directInputNodes)
        n->release();
    
    // If you've set a new view with setAudioOutput, they must be the same size!
    jassert (destAudioView.getSize() == audioView.getSize());

   #if JUCE_DEBUG
    isBeingProcessed = false;
   #endif

    // N.B. This must be set last to release the Node back to the player
    hasBeenProcessed.store (true, std::memory_order_release);
}

inline bool Node::hasProcessed() const
{
    return hasBeenProcessed.load (std::memory_order_acquire);
}

inline Node::AudioAndMidiBuffer Node::getProcessedOutput()
{
    jassert (hasProcessed());
    return { audioView.getStart ((choc::buffer::FrameCount) numSamplesProcessed.load (std::memory_order_acquire)),
             midiBuffer };
}

inline size_t Node::getAllocatedBytes() const
{
    return audioBuffer.getView().data.getBytesNeeded (audioBuffer.getSize())
        + (size_t (midiBuffer.size()) * sizeof (tracktion_engine::MidiMessageArray::MidiMessageWithSource));
}

inline void Node::setOptimisations (NodeOptimisations newOptimisations)
{
    nodeOptimisations = newOptimisations;
}

inline void Node::setAudioOutput (Node* sourceNode, const choc::buffer::ChannelArrayView<float>& newAudioView)
{
    if (sourceNode)
        sourceNode->retain();
    
    audioView = newAudioView;
    nodeToRelease.store (sourceNode, std::memory_order_relaxed);
}

inline void Node::retain()
{
    assert (retainCount.load() >= 0);
    retainCount.fetch_add (1, std::memory_order_relaxed);
}

inline void Node::release()
{
    assert (retainCount.load() > 0);
    
    if (retainCount.fetch_sub (1, std::memory_order_acq_rel) == 1)
    {
        if (auto node = nodeToRelease.load (std::memory_order_relaxed))
            node->release();

        if (deallocateAudioBuffer)
            deallocateAudioBuffer ({ std::move (allocatedView), std::move (audioBuffer) });
    }
}

//==============================================================================
//==============================================================================
namespace detail
{
    struct VisitNodesWithRecord
    {
        template<typename Visitor>
        static void visit (std::vector<Node*>& visitedNodes, Node& visitingNode, Visitor&& visitor, bool preordering)
        {
            if (std::find (visitedNodes.begin(), visitedNodes.end(), &visitingNode) != visitedNodes.end())
                return;
            
            if (preordering)
            {
                visitedNodes.push_back (&visitingNode);
                visitor (visitingNode);
            }

            for (auto n : visitingNode.getDirectInputNodes())
                visit  (visitedNodes, *n, visitor, preordering);

            if (! preordering)
            {
                visitedNodes.push_back (&visitingNode);
                visitor (visitingNode);
            }
        }
    };

    struct VisitNodesWithRecordBFS
    {
        template<typename Visitor>
        static void visit (std::vector<Node*>& visitedNodes, Node& visitingNode, Visitor&& visitor)
        {
            if (std::find (visitedNodes.begin(), visitedNodes.end(), &visitingNode) == visitedNodes.end())
            {
                visitedNodes.push_back (&visitingNode);
                visitor (visitingNode);
            }
            
            auto inputs = visitingNode.getDirectInputNodes();
            
            // Visit each node then go back to the first and recurse
            for (auto n : inputs)
            {
                if (std::find (visitedNodes.begin(), visitedNodes.end(), n) == visitedNodes.end())
                {
                    visitedNodes.push_back (n);
                    visitor (visitingNode);
                }
            }
            
            for (auto n : inputs)
                visit  (visitedNodes, *n, visitor);
        }
    };
}

template<typename Visitor>
inline void visitNodes (Node& node, Visitor&& visitor, bool preordering)
{
    std::vector<Node*> visitedNodes;
    detail::VisitNodesWithRecord::visit (visitedNodes, node, visitor, preordering);
}

template<typename Visitor>
inline void visitNodesBFS (Node& node, Visitor&& visitor)
{
    std::vector<Node*> visitedNodes;
    detail::VisitNodesWithRecordBFS::visit (visitedNodes, node, visitor);
}

inline std::vector<Node*> getNodes (Node& node, VertexOrdering vertexOrdering)
{
    if (vertexOrdering == VertexOrdering::bfsPreordering
        || vertexOrdering == VertexOrdering::bfsReversePreordering)
    {
        std::vector<Node*> visitedNodes;
        detail::VisitNodesWithRecordBFS::visit (visitedNodes, node, [](auto&){});

        if (vertexOrdering == VertexOrdering::bfsReversePreordering)
            std::reverse (visitedNodes.begin(), visitedNodes.end());

        return visitedNodes;
    }
    
    bool preordering = vertexOrdering == VertexOrdering::preordering
                    || vertexOrdering == VertexOrdering::reversePreordering;
    
    std::vector<Node*> visitedNodes;
    detail::VisitNodesWithRecord::visit (visitedNodes, node, [](auto&){}, preordering);

    if (vertexOrdering == VertexOrdering::reversePreordering
        || vertexOrdering == VertexOrdering::reversePostordering)
       std::reverse (visitedNodes.begin(), visitedNodes.end());
    
    return visitedNodes;
}

inline std::vector<NodeAndID> createNodeMap (const std::vector<Node*>& nodes)
{
    std::vector<NodeAndID> nodeMap;

    for (auto n : nodes)
    {
        nodeMap.push_back ({ n, n->getNodeProperties().nodeID });

        for (auto internalNode : n->getInternalNodes())
            nodeMap.push_back ({ internalNode, internalNode->getNodeProperties().nodeID });
    }

    std::sort (nodeMap.begin(), nodeMap.end());
    nodeMap.erase (std::unique (nodeMap.begin(), nodeMap.end()),
                   nodeMap.end());

    return nodeMap;
}

inline std::unique_ptr<NodeGraph> createNodeGraph (std::unique_ptr<Node> rootNode)
{
    assert (rootNode != nullptr);
    auto orderedNodes = transformNodes (*rootNode);
    auto sortedNodes = createNodeMap (orderedNodes);

    auto nodeGraph = std::make_unique<NodeGraph>();
    nodeGraph->rootNode = std::move (rootNode);
    nodeGraph->orderedNodes = std::move (orderedNodes);
    nodeGraph->sortedNodes = std::move (sortedNodes);

    return nodeGraph;
}

}}
