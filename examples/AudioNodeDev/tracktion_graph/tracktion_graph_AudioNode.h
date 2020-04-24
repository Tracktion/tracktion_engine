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
        - visitDFS (Visitor) [used to get list of nodes to process and can be called multiple times to optomise graph etc.]
 
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

#include <JuceHeader.h>

class AudioNode;

//==============================================================================
/** Passed into AudioNodes when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PlaybackInitialisationInfo
{
    double sampleRate;
    int blockSize;
    AudioNode& rootNode;
};

/** Holds some really basic properties of a node */
struct AudioNodeProperties
{
    bool hasAudio;
    bool hasMidi;
    int numberOfChannels;
    int latencyNumSamples = 0;
};

//==============================================================================
//==============================================================================
class AudioNode
{
public:
    AudioNode() = default;
    virtual ~AudioNode() = default;
    
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
        juce::dsp::AudioBlock<float> audio;
        MidiBuffer& midi;
    };

    /** Returns the processed audio and MIDI output.
        Must only be called after hasProcessed returns true.
    */
    AudioAndMidiBuffer getProcessedOutput();

    //==============================================================================
    /** Should return all the inputs directly feeding in to this node. */
    virtual std::vector<AudioNode*> getDirectInputNodes() { return {}; }

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
    virtual AudioNodeProperties getAudioNodeProperties() = 0;

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
    AudioBuffer<float> audioBuffer;
    MidiBuffer midiBuffer;
    int numSamplesProcessed = 0;
};


//==============================================================================
//==============================================================================
/** Should call the visitInputs for any direct inputs to the node and then call
    the visit function on this node.
*/
static inline void visitInputs (AudioNode& node, std::function<void (AudioNode&)>& visit)
{
    for (auto n : node.getDirectInputNodes())
        visitInputs (*n, visit);
    
    visit (node);
}


//==============================================================================
inline void AudioNode::initialise (const PlaybackInitialisationInfo& info)
{
    prepareToPlay (info);
    
    auto props = getAudioNodeProperties();
    audioBuffer.setSize (props.numberOfChannels, info.blockSize);
}

inline void AudioNode::prepareForNextBlock()
{
    hasBeenProcessed = false;
}

inline void AudioNode::process (juce::Range<int64_t> streamSampleRange)
{
    audioBuffer.clear();
    midiBuffer.clear();
    const int numChannelsBeforeProcessing = audioBuffer.getNumChannels();
    const int numSamplesBeforeProcessing = audioBuffer.getNumSamples();
    ignoreUnused (numChannelsBeforeProcessing, numSamplesBeforeProcessing);

    const int numSamples = (int) streamSampleRange.getLength();
    jassert (numSamples > 0); // This must be a valid number of samples to process
    
    auto inputBlock = numChannelsBeforeProcessing > 0 ? juce::dsp::AudioBlock<float> (audioBuffer).getSubBlock (0, (size_t) numSamples)
                                                      : juce::dsp::AudioBlock<float>();
    ProcessContext pc {
                        streamSampleRange,
                        { inputBlock , midiBuffer }
                      };
    process (pc);
    numSamplesProcessed = numSamples;
    hasBeenProcessed = true;
    
    jassert (numChannelsBeforeProcessing == audioBuffer.getNumChannels());
    jassert (numSamplesBeforeProcessing == audioBuffer.getNumSamples());
}

inline bool AudioNode::hasProcessed() const
{
    return hasBeenProcessed;
}

inline AudioNode::AudioAndMidiBuffer AudioNode::getProcessedOutput()
{
    jassert (hasProcessed());
    return { juce::dsp::AudioBlock<float> (audioBuffer).getSubBlock (0, (size_t) numSamplesProcessed), midiBuffer };
}
