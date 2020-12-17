# Tracktion Engine Audio Graph Rewrite
## Part 3 - The `NodePlayer` class
###### - Dave Rowland (29/04/20)
___
Once you've built a graph of Nodes (which we haven't covered yet but you can skip ahead to the `NodeTests` class in the source if you want to see some examples) you'll want to play it back.

This is being covered now before graph creation because it summarises the various initialise/prepare/process calls we discussed in part 2. There's no set way to play a Node, how you do it will depend on the trade-offs you make between threading models, graph analysis and getting input/play heads etc. in to the graph. This is an example of the most basic form of playback with a Node that simply generates some output. It is used extensively in the tests.
___
### 1. NodePlayer
So you have a Node, and you'd like play it back. Here's a basic class for doing so:
```
class NodePlayer
{
public:
    /** Creates an NodePlayer to process an Node. */
    NodePlayer (std::unique_ptr<Node> nodeToProcess);

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize);

    /** Processes a block of audio and MIDI data. */
    void process (const Node::ProcessContext&);
    
private:
    std::unique_ptr<Node> input;
    std::vector<Node*> allNodes;
};

```
Simple. This class takes ownership of the Node to be played and exposes a method to prepare it and then process it. Let's have a look in more detail.
___
#### 1.1. Constructor

```
NodePlayer (std::unique_ptr<Node> nodeToProcess)
    : input (std::move (nodeToProcess))
{
}
```
Not really much do do here apart from transfer ownership to our member Node.

#### 1.2. `prepareToPlay`
After a NodePlayer has been created, you'll need to prepare it.
```
void prepareToPlay (double sampleRate, int blockSize)
{
    // First, initiliase all the nodes, this will call prepareToPlay on them and also
    // give them a chance to do things like balance latency
    const PlaybackInitialisationInfo info { sampleRate, blockSize, *input };
    std::function<void (Node&)> visitor = [&] (Node& n) { n.initialise (info); };
    visitInputs (*input, visitor);

    // Then find all the nodes as it might have changed after initialisation
    allNodes.clear();

    std::function<void (Node&)> visitor2 = [&] (Node& n) { allNodes.push_back (&n); };
    visitInputs (*input, visitor2);
}
```
This does two things.

Firstly, it visits all the nodes with the `visitInputs` function which simply calls the `visitor` callable on each Node in a recursive way. We use this visitor to call each `Node`'s `initialise` method. Once we've done that the Node and its contents should be ready to process.

Remember from the Node overview however that the `initialise` and hence internal `prepareToPlay` calls can change the topology of the graph and the Nodes it contains. Because of this we need to visit the nodes again to create a list of all the Nodes the graph contains. These will be the nodes that we process.


#### 1.2. `process`
The `NodePlayer`'s `process` method is relatively straightforward, remember this is the most basic approach to playing a Node:
```
void process (const Node::ProcessContext& pc)
{
    for (auto node : allNodes)
        node->prepareForNextBlock();

    for (;;)
    {
        int processedAnyNodes = false;

        for (auto node : allNodes)
        {
            if (! node->hasProcessed() && node->isReadyToProcess())
            {
                node->process (pc.streamSampleRange);
                processedAnyNodes = true;
            }
        }

        if (! processedAnyNodes)
        {
            auto output = input->getProcessedOutput();
            pc.buffers.audio.copyFrom (output.audio);
            pc.buffers.midi.copyFrom (output.midi);

            break;
        }
    }
}
```
The `process` method takes an audio buffer (choc::buffer::ChannelArrayView) and a MidiMessageArray and a streamSampleRange contained in the `ProcessContext`. However, in this instance we're not using the audio and MIDI as input, the buffers will just be used to fill the output of the Node and be passed on to an audio device or file writer etc, whoever is playing the Node.

The first step to processing is to call `Node::prepareForNextBlock()` on all the Nodes. As we mentioned before, this resets the Nodes ready for the next process call.

Once that's done we simply iterate all the Nodes, checking which ones are ready to process. If a Node s ready to process and has not been processed yet, we can call its `process` call with the stream range. We continuously do this until no Nodes are processed in the iteration. At this point we know that all the nodes have been processed and we can get the output from the root Node.

As we're just passing this on to our caller's buffers, we can simply copy the audio and MIDI data to them. This assumes that the number of channels in the block is the same as the Node contains. If these are different we'd have to only copy the minimum number of channels between the two probably clearing the unused channels from the input.
___
### 2. Future thoughts
As you can probably tell this is a relatively naive approach to processing but it works relatively well and the overhead of checking the `hasProcessed` and `isReadyToProcess` flags is small in comparison to the work executed in each Node's `process` call.

We'll be examining this in more detail in a future article but it should be fairly obvious how some sorting of the nodes in to an order of dependency would reduce repeated calls a lot.
