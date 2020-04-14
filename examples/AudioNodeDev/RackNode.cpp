/*
  ==============================================================================

    RackNode.cpp
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#include "RackNode.h"

/**
    Tests:
    1. Build a node that generates a sin wave
    2. Add another node that generates a sin wave an octave higher
    3. Make two sin waves, add latency of the period to one of these, the output should be silent
*/

/**
    - Each node should have pointers to its inputs
    - When a node is processed, it should check its inputs to see if they have produced outputs
    - If they have, that node can be processed. If they haven't the processor can try another node
    - If one node reports latency, every other node being summed with it will need to be delayed up to the same ammount
    - The reported latency of a node is the max of all its input latencies
 
    Each node needs:
    - A flag to say if it has produced outputs yet
    - A method to report its latency
    - A method to process it
*/

namespace utilities
{
    void copyAudioBuffer (AudioBuffer<float>& dest, const AudioBuffer<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numSamples = dest.getNumSamples();
        const int numChannels = std::min (dest.getNumChannels(), source.getNumChannels());

        for (int i = 0; i < numChannels; ++i)
            dest.copyFrom (i, 0, source, i, 0, numSamples);
    }

    void addAudioBuffer (AudioBuffer<float>& dest, const AudioBuffer<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numSamples = dest.getNumSamples();
        const int numChannels = std::min (dest.getNumChannels(), source.getNumChannels());

        for (int i = 0; i < numChannels; ++i)
            dest.addFrom (i, 0, source, i, 0, numSamples);
    }

    void writeToFile (File file, const AudioBuffer<float>& buffer, double sampleRate)
    {
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                sampleRate, buffer.getNumChannels(), 16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }
}

class AudioNode;

//==============================================================================
/** Passed into AudioNodes when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PlaybackInitialisationInfo
{
    double sampleRate;
    int blockSize;
    const std::vector<AudioNode*>& allNodes;
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
    
    /** Call to process the node, which will in turn call the process method with the buffers to fill. */
    void process();
    
    /** Returns true if this node has processed and its outputs can be retrieved. */
    bool hasProcessed() const;
    
    /** Returns the processed audio output. Must only be called after hasProcessed returns true. */
    AudioBuffer<float>& getProcessedAudioOutput();

    /** Returns the processed MIDI output. Must only be called after hasProcessed returns true. */
    MidiBuffer& getProcessedMidiOutput();
    
    //==============================================================================
    /** Should return the properties of the node. */
    virtual tracktion_engine::AudioNodeProperties getAudioNodeProperties() = 0;

    /** Should return all the inputs feeding in to this node. */
    virtual std::vector<AudioNode*> getAllInputNodes() { return {}; }

    /** Called once before playback begins for each node.
        Use this to allocate buffers etc.
    */
    virtual void prepareToPlay (const PlaybackInitialisationInfo&) {}

    /** Should return true when this node is ready to be processed.
        This is usually when its input's output buffers are ready.
    */
    virtual bool isReadyToProcess() = 0;
    
    /** Called when the node is to be processed.
        This should add in to the buffers available making sure not to change their size at all.
    */
    virtual void process (AudioBuffer<float>& destAudio, MidiBuffer& destMidi) = 0;

private:
    std::atomic<bool> hasBeenProcessed { false };
    AudioBuffer<float> audioBuffer;
    MidiBuffer midiBuffer;
};

void AudioNode::initialise (const PlaybackInitialisationInfo& info)
{
    auto props = getAudioNodeProperties();
    audioBuffer.setSize (props.numberOfChannels, info.blockSize);
}

void AudioNode::prepareForNextBlock()
{
    hasBeenProcessed = false;
}

void AudioNode::process()
{
    audioBuffer.clear();
    midiBuffer.clear();
    const int numChannelsBeforeProcessing = audioBuffer.getNumChannels();
    const int numSamplesBeforeProcessing = audioBuffer.getNumSamples();
    ignoreUnused (numChannelsBeforeProcessing, numSamplesBeforeProcessing);

    process (audioBuffer, midiBuffer);
    hasBeenProcessed = true;
    
    jassert (numChannelsBeforeProcessing == audioBuffer.getNumChannels());
    jassert (numSamplesBeforeProcessing == audioBuffer.getNumSamples());
}

bool AudioNode::hasProcessed() const
{
    return hasBeenProcessed;
}

AudioBuffer<float>& AudioNode::getProcessedAudioOutput()
{
    jassert (hasProcessed());
    return audioBuffer;
}

MidiBuffer& AudioNode::getProcessedMidiOutput()
{
    jassert (hasProcessed());
    return midiBuffer;
}


//==============================================================================
//==============================================================================
class AudioNodeProcessor
{
public:
    AudioNodeProcessor (std::unique_ptr<AudioNode> nodeToProcess)
        : node (std::move (nodeToProcess))
    {
        auto nodes = node->getAllInputNodes();
        nodes.push_back (node.get());
        std::unique_copy (nodes.begin(), nodes.end(), std::back_inserter (allNodes),
                          [] (auto n1, auto n2) { return n1 == n2; });
    }

    void prepareToPlay (double sampleRate, int blockSize)
    {
        const PlaybackInitialisationInfo info { sampleRate, blockSize, allNodes };
        
        for (auto& node : allNodes)
        {
            node->initialise (info);
            node->prepareToPlay (info);
        }
    }

    void process (AudioBuffer<float>& audio, MidiBuffer& midi)
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
                    node->process();
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                utilities::copyAudioBuffer (audio, node->getProcessedAudioOutput());
                //TODO: MIDI
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::vector<AudioNode*> allNodes;
};


//==============================================================================
//==============================================================================
class LatencyAudioNode  : public AudioNode
{
public:
    LatencyAudioNode (std::unique_ptr<AudioNode> inputNode, int numSamplesToDelay)
        : input (std::move (inputNode)), latencyNumSamples (numSamplesToDelay)
    {
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        return input->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        fifo.setSize (getAudioNodeProperties().numberOfChannels, latencyNumSamples + info.blockSize + 1);
        fifo.writeSilence (latencyNumSamples);
        jassert (fifo.getNumReady() == latencyNumSamples);
    }
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer&) override
    {
        auto& inputBuffer = input->getProcessedAudioOutput();
        jassert (fifo.getNumChannels() == inputBuffer.getNumChannels());
        fifo.write (inputBuffer);
        
        jassert (fifo.getNumReady() >= outputBuffer.getNumSamples());
        jassert (inputBuffer.getNumSamples() == outputBuffer.getNumSamples());

        fifo.readAdding (outputBuffer, 0);
        
        //TODO: MIDI
    }
    
private:
    std::unique_ptr<AudioNode> input;
    const int latencyNumSamples;
    tracktion_engine::AudioFifo fifo { 1, 32 };
};


//==============================================================================
//==============================================================================
class SinAudioNode : public AudioNode
{
public:
    SinAudioNode (double frequency)
    {
        osc.setFrequency (frequency);
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        tracktion_engine::AudioNodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = 1;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        osc.prepare ({ double (info.sampleRate), uint32 (info.blockSize), 1 });
    }
    
    void process (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);
    }
    
private:
    juce::dsp::Oscillator<float> osc { [] (float in) { return std::sin (in); } };
};


//==============================================================================
//==============================================================================
class SummingAudioNode : public AudioNode
{
public:
    SummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        tracktion_engine::AudioNodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : nodes)
        {
            auto nodeProps = node->getAudioNodeProperties();
            props.hasAudio = props.hasAudio | nodeProps.hasAudio;
            props.hasMidi = props.hasMidi | nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
        }

        return props;
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        
        for (auto& node : nodes)
        {
            inputNodes.push_back (node.get());
            
            auto nodeInputs = node->getAllInputNodes();
            inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());
        }

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        for (auto& node : nodes)
            if (! node->hasProcessed())
                return false;
        
        return true;
    }
    
    void process (AudioBuffer<float>& dest, MidiBuffer& midi) override
    {
        const int numSamples = dest.getNumSamples();

        for (auto& node : nodes)
        {
            // get each of the inputs and add them to dest
            auto& inputBuffer = node->getProcessedAudioOutput();
            jassert (dest.getNumSamples() == inputBuffer.getNumSamples());
            
            const int numChannels = std::min (dest.getNumChannels(), inputBuffer.getNumChannels());

            for (int i = 0; i < numChannels; ++i)
                dest.addFrom (i, 0, inputBuffer, i, 0, numSamples);
        }
        
        //TODO:  MIDI
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
std::unique_ptr<SummingAudioNode> makeSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<SummingAudioNode> (std::move (nodeVector));
}


//==============================================================================
//==============================================================================
class FunctionAudioNode : public AudioNode
{
public:
    FunctionAudioNode (std::unique_ptr<AudioNode> input,
                       std::function<float (float)> fn)
        : node (std::move (input)),
          function (std::move (fn))
    {
        jassert (function);
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        return node->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (node.get());
        
        auto nodeInputs = node->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        return node->hasProcessed();
    }
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer&) override
    {
        auto& inputBuffer = node->getProcessedAudioOutput();
        jassert (inputBuffer.getNumSamples() == outputBuffer.getNumSamples());

        const int numSamples = outputBuffer.getNumSamples();
        const int numChannels = std::min (inputBuffer.getNumChannels(), outputBuffer.getNumChannels());

        for (int c = 0; c < numChannels; ++c)
        {
            const float* inputSamples = inputBuffer.getReadPointer (c);
            float* outputSamples = outputBuffer.getWritePointer (c);
            
            for (int i = 0; i < numSamples; ++i)
                outputSamples[i] = function (inputSamples[i]);
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::function<float (float)> function;
};


//==============================================================================
//==============================================================================
class SendAudioNode : public AudioNode
{
public:
    SendAudioNode (std::unique_ptr<AudioNode> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
    }
    
    int getBusID() const
    {
        return busID;
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        return input->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer& outputMidi) override
    {
        // Just pass out input on to our output
        utilities::copyAudioBuffer (outputBuffer, input->getProcessedAudioOutput());
        outputMidi.addEvents (input->getProcessedMidiOutput(), 0, outputBuffer.getNumSamples(), 0);
    }
    
private:
    std::unique_ptr<AudioNode> input;
    const int busID;
};


//==============================================================================
//==============================================================================
class ReturnAudioNode : public AudioNode
{
public:
    ReturnAudioNode (std::unique_ptr<AudioNode> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        return input->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        for (auto send : sendNodes)
            if (! send->hasProcessed())
                return false;
        
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        for (auto node : info.allNodes)
            if (auto send = dynamic_cast<SendAudioNode*> (node))
                if (send->getBusID() == busID)
                    sendNodes.push_back (send);
    }
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer& outputMidi) override
    {
        // Copy the input on to our output
        utilities::copyAudioBuffer (outputBuffer, input->getProcessedAudioOutput());
        outputMidi.addEvents (input->getProcessedMidiOutput(), 0, outputBuffer.getNumSamples(), 0);
        
        // And a copy of all the send nodes
        for (auto send : sendNodes)
        {
            utilities::addAudioBuffer (outputBuffer, send->getProcessedAudioOutput());
            outputMidi.addEvents (send->getProcessedMidiOutput(), 0, outputBuffer.getNumSamples(), 0);
        }
    }
    
private:
    std::unique_ptr<AudioNode> input;
    std::vector<AudioNode*> sendNodes;
    const int busID;
};




//==============================================================================
//==============================================================================
class AudioNodeTests : public juce::UnitTest
{
public:
    AudioNodeTests()
        : juce::UnitTest ("AudioNode", "AudioNode")
    {
    }
    
    void runTest() override
    {
        runSinTest();
        runSinCancellingTest();
        runSinOctaveTest();
        runSendReturnTest();
        runLatencyTests();
    }

private:
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
    };
    
    std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNode> node, double sampleRate, int blockSize,
                                                    int numChannels, double durationInSeconds)
    {
        auto context = std::make_unique<TestContext>();
        context->tempFile = std::make_unique<TemporaryFile> (".wav");
        
        // Process the node to a file
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (context->tempFile->getFile().createOutputStream().release(),
                                                                                                sampleRate, numChannels, 16, {}, 0)))
        {
            AudioNodeProcessor processor (std::move (node));
            processor.prepareToPlay (sampleRate, blockSize);
            
            AudioBuffer<float> buffer (1, blockSize);
            MidiBuffer midi;
            
            int numSamplesToDo = roundToInt (durationInSeconds * sampleRate);
            
            for (;;)
            {
                const int numThisTime = std::min (blockSize, numSamplesToDo);
                
                buffer.clear();
                midi.clear();
                
                processor.process (buffer, midi);
                
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
                
                numSamplesToDo -= numThisTime;
                
                if (numSamplesToDo <= 0)
                    break;
            }
            
            writer.reset();

            // Then read it back in to the buffer
            if (auto reader = std::unique_ptr<AudioFormatReader> (WavAudioFormat().createReaderFor (context->tempFile->getFile().createInputStream().release(), true)))
            {
                AudioBuffer<float> buffer (numChannels, (int) reader->lengthInSamples);
                reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, false);
                context->buffer = std::move (buffer);
                
                return context;
            }
        }

        return {};
    }
    
    void runSinTest()
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createTestContext (std::move (sinNode), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;
            
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
    }

    void runSinCancellingTest()
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));
            
            auto testContext = createTestContext (std::move (sumNode), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }

    void runSinOctaveTest()
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runSendReturnTest()
    {
        beginTest ("Sin send/return");
        {
            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }

        beginTest ("Sin send/return different bus#");
        {
            // This test is the same as before but uses a different bus number for the return so the output should be silent

            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 2);

            // Track 1 & 2 then get summed together
            auto node = makeSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Sin send/return non-blocking");
        {
            // Track 1 sends a sin tone to a send with a gain of 0.25
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto attenuatedSinLowerNode = std::make_unique<FunctionAudioNode> (std::move (sinLowerNode), [] (float s) { return s * 0.25f; });
            auto track1Node = std::make_unique<SendAudioNode> (std::move (attenuatedSinLowerNode), 1);

            // Track 2 has a sin source  of gain 0.5 and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto attenuatedSinUpperNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float s) { return s * 0.5f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (attenuatedSinUpperNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeSummingAudioNode ({ track1Node.release(), track2Node.release() });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runLatencyTests()
    {
        beginTest ("Basic latency test cancelling sin");
        {
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (sinFrequency));

            auto sinNode = std::make_unique<SinAudioNode> (sinFrequency);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0);
            auto& buffer = testContext->buffer;

            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                              buffer.getNumChannels(),
                                              numLatencySamples, buffer.getNumSamples() - numLatencySamples);
            
            expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
        }
    }
};

static AudioNodeTests audioNodeTests;
