/*
  ==============================================================================

    tests_AudioNode.cpp
    Created: 15 Apr 2020 10:44:09am
    Author:  David Rowland

  ==============================================================================
*/

#include "AudioNodeProcessor.h"
#include "UtilityNodes.h"

//==============================================================================
//==============================================================================
/**
    Plays back a MIDI sequence.
    This simply plays it back from start to finish with no notation of a playhead.
*/
class MidiAudioNode : public AudioNode
{
public:
    MidiAudioNode (juce::MidiMessageSequence sequenceToPlay)
        : sequence (std::move (sequenceToPlay))
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        return { false, true, 0 };
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
    }
    
    void process (const ProcessContext& pc) override
    {
        const int numSamples = (int) pc.buffers.audio.getNumSamples();
        const double blockDuration = numSamples / sampleRate;
        const auto timeRange = tracktion_engine::EditTimeRange::withStartAndLength (lastTime, blockDuration);
        
        for (int index = sequence.getNextIndexAtTime (timeRange.getStart()); index >= 0; ++index)
        {
            if (auto eventHolder = sequence.getEventPointer (index))
            {
                auto time = sequence.getEventTime (index);
                
                if (! timeRange.contains (time))
                    break;
                
                const int sampleNumber = sampleRate * (time - timeRange.getStart());
                jassert (sampleNumber < numSamples);
                pc.buffers.midi.addEvent (eventHolder->message, sampleNumber);
            }
            else
            {
                break;
            }
        }
        
        lastTime = timeRange.getEnd();
    }
    
private:
    juce::MidiMessageSequence sequence;
    double sampleRate = 0.0, lastTime = 0.0;
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
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
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
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == getAudioNodeProperties().numberOfChannels);
        float* samples = pc.buffers.audio.getChannelPointer (0);
        int numSamples = (int) pc.buffers.audio.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);
    }
    
private:
    juce::dsp::Oscillator<float> osc { [] (float in) { return std::sin (in); } };
};


//==============================================================================
//==============================================================================
class BasicSummingAudioNode : public AudioNode
{
public:
    BasicSummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
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
    
    void process (const ProcessContext& pc) override
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            const auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels);

            if (numChannelsToAdd > 0)
                pc.buffers.audio.getSubsetChannelBlock (0, numChannelsToAdd)
                    .add (node->getProcessedOutput().audio.getSubsetChannelBlock (0, numChannelsToAdd));
            
            pc.buffers.midi.addEvents (inputFromNode.midi, 0, -1, 0);
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
std::unique_ptr<BasicSummingAudioNode> makeBaicSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<BasicSummingAudioNode> (std::move (nodeVector));
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
    
    AudioNodeProperties getAudioNodeProperties() override
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
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffer = node->getProcessedOutput().audio;
        jassert (inputBuffer.getNumSamples() == pc.buffers.audio.getNumSamples());

        const int numSamples = (int) pc.buffers.audio.getNumSamples();
        const int numChannels = std::min ((int) inputBuffer.getNumChannels(), (int) pc.buffers.audio.getNumChannels());

        for (int c = 0; c < numChannels; ++c)
        {
            const float* inputSamples = inputBuffer.getChannelPointer ((size_t) c);
            float* outputSamples = pc.buffers.audio.getChannelPointer ((size_t) c);
            
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
    
    AudioNodeProperties getAudioNodeProperties() override
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
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());

        // Just pass out input on to our output
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.addEvents (input->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
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
    
    AudioNodeProperties getAudioNodeProperties() override
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
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());
        
        // Copy the input on to our output
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.addEvents (input->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
        
        // And a copy of all the send nodes
        for (auto send : sendNodes)
        {
            const auto numChannelsToUse = std::min (pc.buffers.audio.getNumChannels(), send->getProcessedOutput().audio.getNumChannels());
            
            if (numChannelsToUse > 0)
                pc.buffers.audio.getSubsetChannelBlock (0, numChannelsToUse)
                    .add (send->getProcessedOutput().audio.getSubsetChannelBlock (0, numChannelsToUse));
            
            pc.buffers.midi.addEvents (send->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
        }
    }
    
private:
    std::unique_ptr<AudioNode> input;
    std::vector<AudioNode*> sendNodes;
    const int busID;
};


/** Returns a node wrapped in another node that applies a static gain to it. */
std::unique_ptr<AudioNode> makeGainNode (std::unique_ptr<AudioNode> node, float gain)
{
    return std::make_unique<FunctionAudioNode> (std::move (node), [gain] (float s) { return s * gain; });
}

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
        for (bool randomiseBlockSizes : { false, true })
        {
//ddd            runSinTest (randomiseBlockSizes);
//            runSinCancellingTest (randomiseBlockSizes);
//            runSinOctaveTest (randomiseBlockSizes);
//            runSendReturnTest (randomiseBlockSizes);
//            runLatencyTests (randomiseBlockSizes);
            
            runMidiTests (randomiseBlockSizes);
        }
    }

private:
    //==============================================================================
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
        MidiBuffer midi;
    };
    
    std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNode> node, const double sampleRate, const int blockSize,
                                                    const int numChannels, const double durationInSeconds, bool randomiseBlockSizes = false)
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
            int numSamplesDone = 0;
            Random r;
            
            for (;;)
            {
                const int numThisTime = randomiseBlockSizes ? std::min (r.nextInt ({ 1, blockSize }), numSamplesToDo)
                                                            : std::min (blockSize, numSamplesToDo);
                buffer.clear();
                midi.clear();
                
                AudioBuffer<float> subSectionBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                     0, numThisTime);

                processor.process ({ { { subSectionBuffer }, midi } });
                
                writer->writeFromAudioSampleBuffer (subSectionBuffer, 0, subSectionBuffer.getNumSamples());
                context->midi.addEvents (midi, 0, numThisTime, numSamplesDone);
                
                numSamplesToDo -= numThisTime;
                numSamplesDone += numThisTime;
                
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
    
    //==============================================================================
    static MidiMessageSequence createMidiMessageSequence (const juce::MidiBuffer& buffer, double sampleRate)
    {
        MidiMessageSequence sequence;
        
        for (MidiBuffer::Iterator iter (buffer);;)
        {
            MidiMessage result;
            int samplePosition = 0;
            
            if (! iter.getNextEvent (result, samplePosition))
                break;
            
            const double time = samplePosition / sampleRate;
            sequence.addEvent (result, time);
        }
        
        return sequence;
    }
    
    void logMidiMessageSequence (const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            logMessage (seq.getEventPointer (i)->message.getDescription());
    }

    //==============================================================================
    void expectMidiMessageSequence (const juce::MidiMessageSequence& seq1, const juce::MidiMessageSequence& seq2)
    {
        expectEquals (seq1.getNumEvents(), seq2.getNumEvents(), "Num MIDI events not equal");
        
        if (seq1.getNumEvents() != seq2.getNumEvents())
            return;
        
        bool sequencesTheSame = true;
        
        for (int i = 0; i < seq1.getNumEvents(); ++i)
        {
            auto event1 = seq1.getEventPointer (i);
            auto event2 = seq2.getEventPointer (i);
            
            auto desc1 = event1->message.getDescription();
            auto desc2 = event2->message.getDescription();

            if (desc1 != desc2)
            {
                logMessage (String ("Event at index 123 is:\n\tseq1: XXX\n\tseq2 is: YYY")
                                .replace ("123", String (i)).replace ("XXX", desc1).replace ("YYY", desc2));
                sequencesTheSame = false;
            }
        }
        
        expect (sequencesTheSame, "MIDI sequence contents not equal");
    }

    void expectMidiBuffer (const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (createMidiMessageSequence (buffer, sampleRate), seq);
    }
    
    //==============================================================================
    //==============================================================================
    void runSinTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createTestContext (std::move (sinNode), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;
            
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
    }

    void runSinCancellingTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            
            auto testContext = createTestContext (std::move (sumNode), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }

    void runSinOctaveTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runSendReturnTest (bool randomiseBlockSizes)
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
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
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
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
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
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runLatencyTests (bool randomiseBlockSizes)
    {
        beginTest ("Basic latency test cancelling sin");
        {
            /*  Has two sin nodes at the same frequency and delays one of them by half a period
                so they should cancel out completely.
                This uses the test sample rate to determine the sin frequency to avoid rounding errors.
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (sinFrequency));

            auto sinNode = std::make_unique<SinAudioNode> (sinFrequency);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                              buffer.getNumChannels(),
                                              numLatencySamples, buffer.getNumSamples() - numLatencySamples);
            
            expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Basic latency test doubling sin");
        {
            /*  This is the same test as before, two sin waves with one delayed but now the second one is compensated for.
                This has two implications:
                 1. There will be a half period of silence at the start of the output
                 2. Instead of cancelling, the sins will now constructively interfere, doubling the magnitude
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f));

            auto sinNode = makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            // Start of buffer which should be silent
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  0, numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            }

            // Part of buffer after latency which should be all sin +-1.0
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  numLatencySamples, buffer.getNumSamples() - numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Basic latency test doubling sin multiple sized blocks");
        {
            /*  This is the same test as before, two sin waves with one delayed and latency compensated
                but this time the processing happens in different sized blocks
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f));

            auto sinNode = makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            // Start of buffer which should be silent
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  0, numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            }

            // Part of buffer after latency which should be all sin +-1.0
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  numLatencySamples, buffer.getNumSamples() - numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.707f, 0.001f);
            }
        }
    }
    
    MidiMessageSequence createRandomMidiMessageSequence (double durationSeconds, Random r)
    {
        juce::MidiMessageSequence sequence;
        int noteNumber = -1;
        
        for (double time = 0.0; time < durationSeconds; time += r.nextDouble() * 0.5 + 0.1)
        {
            if (noteNumber != -1)
            {
                sequence.addEvent (MidiMessage::noteOff (1, noteNumber), time);
                noteNumber = -1;
            }
            else
            {
                noteNumber = r.nextInt ({ 1, 127 });
                sequence.addEvent (MidiMessage::noteOn (1, noteNumber, 1.0f), time);
            }
        }
        
        return sequence;
    }
    
    void runMidiTests (bool randomiseBlockSizes)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        const auto sequence = createRandomMidiMessageSequence (duration, getRandom());
        
        beginTest ("Basic MIDI");
        {
            auto node = std::make_unique<MidiAudioNode> (sequence);
            
            auto testContext = createTestContext (std::move (node), sampleRate, 512, 1, duration, randomiseBlockSizes);

            expectGreaterThan (sequence.getNumEvents(), 0);
            expectMidiBuffer (testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Delayed MIDI");
        {
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto delayedNode = std::make_unique<LatencyAudioNode> (std::move (midiNode), latencyNumSamples);

            auto testContext = createTestContext (std::move (delayedNode), sampleRate, 512, 1, duration, randomiseBlockSizes);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            expectMidiBuffer (testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Compensated MIDI");
        {
            // This test has a sin node being delayed by a block which then gets mixed with a non-deleayed MIDI stream
            // The MIDI stream should be delayed by the same amount as the sin stream
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto delayedNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), latencyNumSamples);
            
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto summedNode = makeSummingAudioNode ({ delayedNode.release(), midiNode.release() });

            auto testContext = createTestContext (std::move (summedNode), sampleRate, 512, 1, duration, randomiseBlockSizes);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            expectMidiBuffer (testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Send/return MIDI");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the original
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);
            track1 = makeAudioNode<FunctionAudioNode> (std::move (track1), [] (float) { return 0.0f; });

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0), busNum);
            
            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, duration, randomiseBlockSizes);

            expectGreaterThan (sequence.getNumEvents(), 0);
            expectMidiBuffer (testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Send/return MIDI passthrough");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the return path
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0), busNum);
            track2 = makeAudioNode<FunctionAudioNode> (std::move (track2), [] (float) { return 0.0f; });

            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, duration, randomiseBlockSizes);

            expectGreaterThan (sequence.getNumEvents(), 0);
            expectMidiBuffer (testContext->midi, sampleRate, sequence);
        }
    }
};

static AudioNodeTests audioNodeTests;
