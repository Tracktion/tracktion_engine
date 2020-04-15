/*
  ==============================================================================

    tests_AudioNode.cpp
    Created: 15 Apr 2020 10:44:09am
    Author:  David Rowland

  ==============================================================================
*/

#include "AudioNodeProcessor.h"
#include "UtilityNodes.h"
#include "TestAudioNodes.h"


//==============================================================================
namespace test_utilities
{
    /** Creates a MidiMessageSequence from a MidiBuffer. */
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
    
    /** Logs a MidiMessageSequence. */
    void logMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            ut.logMessage (seq.getEventPointer (i)->message.getDescription());
    }

    //==============================================================================
    /** Compares two MidiMessageSequences and expects them to be equal. */
    static void expectMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq1, const juce::MidiMessageSequence& seq2)
    {
        ut.expectEquals (seq1.getNumEvents(), seq2.getNumEvents(), "Num MIDI events not equal");
        
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
                ut.logMessage (String ("Event at index 123 is:\n\tseq1: XXX\n\tseq2 is: YYY")
                                .replace ("123", String (i)).replace ("XXX", desc1).replace ("YYY", desc2));
                sequencesTheSame = false;
            }
        }
        
        ut.expect (sequencesTheSame, "MIDI sequence contents not equal");
    }

    /** Compares a MidiBuffer and a MidiMessageSequence and expects them to be equal. */
    static void expectMidiBuffer (juce::UnitTest& ut, const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (ut, createMidiMessageSequence (buffer, sampleRate), seq);
    }
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
    
    struct TestSetup
    {
        double sampleRate = 44100.0;
        int blockSize = 512;
        bool randomiseBlockSizes = false;
    };
    
    void runTest() override
    {
        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            for (int blockSize : { 64, 256, 512, 1024 })
            {
                for (bool randomiseBlockSizes : { false, true })
                {
                    TestSetup setup { sampleRate, blockSize, randomiseBlockSizes };
                    logMessage (String ("Test setup: sample rate SR, block size BS, random blocks RND")
                                .replace ("SR", String (sampleRate))
                                .replace ("BS", String (blockSize))
                                .replace ("RND", randomiseBlockSizes ? "Y" : "N"));

                    runSinTests (setup);
                    runSinCancellingTests (setup);
                    runSinOctaveTests (setup);
                    runSendReturnTests (setup);
                    runLatencyTests (setup);
                    
                    runMidiTests (setup);
                }
            }
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
                                                    const int numChannels, const double durationInSeconds,
                                                    bool randomiseBlockSizes, Random r)
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
    //==============================================================================
    void runSinTests (TestSetup testSetup)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createTestContext (std::move (sinNode), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
            auto& buffer = testContext->buffer;
            
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
    }

    void runSinCancellingTests (TestSetup testSetup)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            
            auto testContext = createTestContext (std::move (sumNode), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }

    void runSinOctaveTests (TestSetup testSetup)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runSendReturnTests (TestSetup testSetup)
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

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
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

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
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
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runLatencyTests (TestSetup testSetup)
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

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
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

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
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

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, testSetup.randomiseBlockSizes, getRandom());
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
    
    void runMidiTests (TestSetup testSetup)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        const auto sequence = createRandomMidiMessageSequence (duration, getRandom());
        
        beginTest ("Basic MIDI");
        {
            auto node = std::make_unique<MidiAudioNode> (sequence);
            
            auto testContext = createTestContext (std::move (node), sampleRate, 512, 1, duration, testSetup.randomiseBlockSizes, getRandom());

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Delayed MIDI");
        {
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto delayedNode = std::make_unique<LatencyAudioNode> (std::move (midiNode), latencyNumSamples);

            auto testContext = createTestContext (std::move (delayedNode), sampleRate, 512, 1, duration, testSetup.randomiseBlockSizes, getRandom());
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
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

            auto testContext = createTestContext (std::move (summedNode), sampleRate, 512, 1, duration, testSetup.randomiseBlockSizes, getRandom());
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
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

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, duration, testSetup.randomiseBlockSizes, getRandom());

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
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

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, duration, testSetup.randomiseBlockSizes, getRandom());

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
    }
};

static AudioNodeTests audioNodeTests;
