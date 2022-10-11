# Tracktion Engine Audio Graph Rewrite
## Part 2 - The `Node` class
###### - Dave Rowland (29/04/20)
___
Now we're nearly at the point of having tracktion_engine::Rack processing working with the new audio graph the API is a bit more stable and we can begin to discuss the features in a bit more detail. We'll go over the basics of the Node class here.

A few words of caution before we get started though...  
Firstly, this is still early days so the API and functionality may change as the requirements change. If there are modifications I'll do my best to update this document and also make a note in any future documents.

Secondly, the body of this work is now available as a new module, tracktion_graph, although this is currently still distributed inside the tracktion_engine repository. The idea behind this is that it enforces separation from the main traction_engine module and as much other code as possible.  

___
### 1. Node
At the root of tracktion_graph is the `Node` class.   
A Node represents a node in a graph that can be played. Subclasses of Nodes implement specific behavior such as summing multiple nodes together or adding latency to their input. As we'll see later Node subclasses also perform the task of generating input (such as tones or MIDI files used for testing) or processing effects (such as plugins or gains).

You build a graph of nodes and then initialise and play back the graph.

The class is relatively small so we'll show the whole thing here then dissect it bit by bit.

```
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
    void prepareForNextBlock();
    
    /** Call to process the node, which will in turn call the process method with the
        buffers to fill.
        @param streamSampleRange The monotonic stream time in samples.
                                 This will be passed to the ProcessContext during the
                                 process callback so nodes can use this to determine file
                                 reading positions etc.
                                 Some nodes may ignore this completely but it should at the
                                 least specify the number to samples to process in this block.
    */
    void process (juce::Range<int64_t> streamSampleRange);
    
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
    /** Should return all the inputs directly feeding in to this node. */
    virtual std::vector<Node*> getDirectInputNodes() { return {}; }

    /** Called once before playback begins for each node.
        Use this to allocate buffers etc.
        This step can be used to modify the topology of the graph (i.e. add/remove nodes).
        However, if you do this, you must make sure to call initialise on them so they are
        fully prepared for processing.
    */
    virtual void prepareToPlay (const PlaybackInitialisationInfo&) {}

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
        juce::Range<int64_t> streamSampleRange;
        AudioAndMidiBuffer buffers;
    };
    
    /** Called when the node is to be processed.
        This should add in to the buffers available making sure not to change their size at all.
    */
    virtual void process (const ProcessContext&) = 0;

private:
    std::atomic<bool> hasBeenProcessed { false };
    juce::AudioBuffer<float> audioBuffer;
    tracktion_engine::MidiMessageArray midiBuffer;
    int numSamplesProcessed = 0;
};
```
___
### 2. Virtual Members
We'll look at the virtual member functions first as the base class is an abstract base class so can't be instantiated directly. You'll need to create a graph from some existing implementations of Node or your own Node subclasses.

#### 2.1 `virtual std::vector<Node*> getDirectInputNodes() { return {}; }`  
Because Nodes form a graph, they usually have some number of inputs. (Not all nodes have inputs however, some simply generate input and form the endpoints of the graph).

If your node subclass has any inputs this method should return them. By doing this, the graph can be analysed to find all the nodes that need to be processed and optimised for things such as processing order.

Nodes should only be returned that are direct inputs to this node. I.e. it should not recursively call `getDirectInputNodes` for any inputs a node may have.

Most Nodes only have a single input such as a LatencyNode or GainNode etc. but some special cases sum together multiple nodes.

#### 2.2 `virtual void prepareToPlay (const PlaybackInitialisationInfo&) {}`
This method will be called before the graph gets processed. It should use the `PlaybackInitialisationInfo` struct which contains the sample rate and maximum block size that it will be processed with. This can be used to initialise any oscilators or time based information that requires the sample rate or reserve any buffers that may be required to process the Node.
```
struct PlaybackInitialisationInfo
{
    double sampleRate;
    int blockSize;
    Node& rootNode;
};
```
You can also use the `rootNode` member to search the graph and create connections such as send/return nodes. If you create a new connection (such as a send Node) you should then report it in `getDirectInputNodes` so the correct graph dependency structure is visible.

Note that this gets called during the non-virtual `initialise` call we'll discuss in a bit so you shouldn't usually need to call it directly, just implement it. One caveat to this is if you need to modifiy the graph structure during preparation. For example, once the sample rate is known and a plugin is prepared, it may report some latency. This might mean that a summing node will need to add latency to other inputs in order to balance the latency. If the structure of the graph changes because new LatencyNodes are created, you should call their `initialse` method to make sure they are fully initialised and prepared ready for playback.

#### 2.3 `virtual NodeProperties getNodeProperties() = 0;`
This method should return a `NodeProperties` struct which contains some basic information about the node.
```
struct NodeProperties
{
    bool hasAudio;
    bool hasMidi;
    int numberOfChannels;
    int latencyNumSamples = 0;
};
```
Firstly it should return whether the node as any audio or MIDI, if it does have audio it should return the number of channels it outputs. This ensures that the buffer passed to the `process` method is large enough to fit the audio being produced by the Node. This was one of our key aims and avoids lots of messy channel-count checking during processing.

Finally this should also report the number of samples of latency the node introduces. This is a number from a baseline of '0' so if an input to this node reports a latency, then that latency should be passed on through this Node. This allows summing nodes to correctly balance the latency if it differs between inputs.

One final note is that `getNodeProperties` should only be called after the nodes have been initialised. This is to ensure that any plugins or nodes that depend on the sample rate have a chance to correctly report their latency.

#### 2.4 `virtual bool isReadyToProcess() = 0;`
This method should return true when the node is ready to have its `process` method called.
This is usually simply when its input nodes have all been processed so this node can get their output and use it accordingly.

This method can be called multiple times on the processing thread(s) during a single processing block so it should be as quick as possible to return.

#### 2.5 `virtual void process (const ProcessContext&) = 0;`
This method is called once after `isReadyToProcess` has returned true and should be implemented to do any processing required.
```
struct ProcessContext
{
    juce::Range<int64_t> streamSampleRange;
    AudioAndMidiBuffer buffers;
};
```    
The `ProcessContext` passed in gives the stream range and a set of buffers to fill.
The stream range can be used in different ways, possibly to sync to a play head but the minimum information you should use from it is the number of samples to be processed. This will always be the same number as provided in the audio buffer but if this node has reported it doesn't need any audio channels this buffer will be empty so the stream range should be used to convert the `MidiMessageArray`'s time stamps in to sample numbers.

A `process` call can simply fill the output buffers (which will always be empty at the start of the call) with some data such as an oscillator or part of a MIDI sequence, but more often Nodes have input Nodes so they can get the output from the input Node (with the `getProcessedOutput()` method) do something with the input and then fill the output buffers.  
N.B. You should only ever read from the buffers returned from `getProcessedOutput()`. Not only is it undefined to change these (the buffers may internally be reused elsewhere) but they may be in use by several processing Nodes concurrently so you can not race on them
___
### 3. Non-virtual Members
The top half of the class details the non-virtual member functions. These are the methods that should be called to initialise and process the Node.

#### 3.1 `void initialise (const PlaybackInitialisationInfo&);`
This should be called once on each Node after the graph has been constructed. The order should not matter as the call isn't propagated on to any input nodes (except in the case where new nodes are created as detailed above).

The `initialise` call serves two purposes. Firstly it allocates any internal buffers required to process the Node, secondly it internally calls the virtual `prepareToPlay` method to give Node subclasses a chance to prepare themselves as explained above.

This is not called concurrently with any other nodes so `prepareToPlay` implementation can traverse the graph from the `rootNode` member.

#### 3.2 `void prepareForNextBlock();`
After being initialised, `prepareForNextBlock` needs to be called on each node before the process call.
This resets the `hasProcessed` state ready for the next block.

#### 3.3 `void process (juce::Range<int64_t> streamSampleRange);`
After all the nodes have been prepared for the next block, the `process` method should be called on each node with the stream sample range. The minimum requirement for the stream sample range is that it specifies the number of samples to process (which can be fewer than the Nodes were prepared with) but it could be used in a monotonically incrementing way to sync to a play head.

The `process` call can be called concurrently by many threads in order to fully utilise CPU resources.   
A Node should not be processed more than once without all Nodes having been processed and prepared for the next block.

#### 3.4 `bool hasProcessed() const;`
This method will return true if the node has been processed.

This is usually used by nodes during their `isReadyToProcess` call to determine if their inputs are ready an available.

#### 3.5 `AudioAndMidiBuffer getProcessedOutput();`
Once a Node has been processed, other nodes can safely call `getProcessedOutput` on the Node to return the processed audio and MIDI buffers. Other Nodes can then use these as input in order to produce their output.
```
struct AudioAndMidiBuffer
{
    choc::buffer::ChannelArrayView<float> audio;
    tracktion_engine::MidiMessageArray& midi;
};
```
You should not modify these buffers in any way as they may be being used concurrently by other currently processing Nodes. After all the nodes have been processed, the root Node's output will be the result of the current processing block.
___
### 4. Final words
There's a lot to take in here but I hope the structure is starting to make sense. It will do more so once we examine the basic `NodePlayer` next.

The main concepts to grasp are that you subclass `Node`, implement the virtual methods to describe the topology of the graph and implement any processing, then a "player" class will initialise, prepare and process the nodes, ultimately generating some output for the graph.

The separation between Nodes generating some output and then getting processed outputs from other input Nodes means that processing can happen concurrently for many nodes, providing they're processed in the correct order. That order is determined by the `isReadyToProcess` state.

There's lots more to discuss in terms of resource usage optimisation, reuse of resources between graph builds and processing strategies but we'll come to those in time.
