/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_graph_AudioNodeProcessor.h"


//==============================================================================
namespace test_utilities
{
    /** Creates a random MidiMessageSequence sequence. */
    static inline MidiMessageSequence createRandomMidiMessageSequence (double durationSeconds, Random r)
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

    /** Creates a MidiMessageSequence from a MidiBuffer. */
    static inline MidiMessageSequence createMidiMessageSequence (const juce::MidiBuffer& buffer, double sampleRate)
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

    static inline void fillBufferWithSinData (AudioBuffer<float>& buffer)
    {
        const float incremement = MathConstants<float>::twoPi / buffer.getNumSamples();
        float* sample = buffer.getWritePointer (0);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            *sample++ = std::sin (incremement * i);
        
        for (int i = 1; i < buffer.getNumChannels(); ++i)
            buffer.copyFrom (i, 0, buffer, 0, 0, buffer.getNumSamples());
    }

    /** Logs a MidiMessageSequence. */
    static inline void logMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            ut.logMessage (seq.getEventPointer (i)->message.getDescription());
    }

    /** Logs a MidiBuffer. */
    static inline void dgbMidiBuffer (const juce::MidiBuffer& buffer)
    {
        for (MidiBuffer::Iterator iter (buffer);;)
        {
            MidiMessage result;
            int samplePosition = 0;

            if (! iter.getNextEvent (result, samplePosition))
                break;

            DBG(result.getDescription());
        }
    }

    /** Writes an audio buffer to a file. */
    static inline void writeToFile (File file, const AudioBuffer<float>& buffer, double sampleRate)
    {
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                sampleRate, (uint32_t) buffer.getNumChannels(), 16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }

    /** Writes an audio block to a file. */
    static inline void writeToFile (File file, const juce::dsp::AudioBlock<float>& block, double sampleRate)
    {
        const int numChannels = (int) block.getNumChannels();
        float* chans[32] = {};

        for (int i = 0; i < numChannels; ++i)
            chans[i] = block.getChannelPointer ((size_t) i);

        const juce::AudioBuffer<float> buffer (chans, numChannels, (int) block.getNumSamples());
        writeToFile (file, buffer, sampleRate);
    }

    //==============================================================================
    /** Compares two MidiMessageSequences and expects them to be equal. */
    static inline void expectMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq1, const juce::MidiMessageSequence& seq2)
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
    static inline void expectMidiBuffer (juce::UnitTest& ut, const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (ut, createMidiMessageSequence (buffer, sampleRate), seq);
    }

    /** Expects a specific magnitude and RMS from an AudioBuffer's channel. */
    static inline void expectAudioBuffer (juce::UnitTest& ut, const AudioBuffer<float>& buffer, int channel, float mag, float rms)
    {
        ut.expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), mag, 0.001f);
        ut.expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), rms, 0.001f);
    }

    /** Splits a buffer in to two and expects a specific magnitude and RMS from each half AudioBuffer. */
    static inline void expectAudioBuffer (juce::UnitTest& ut, AudioBuffer<float>& buffer, int channel, int numSampleToSplitAt,
                                          float mag1, float rms1, float mag2, float rms2)
    {
        {
            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                              0, numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag1, rms1);
        }
        
        {
            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                              buffer.getNumChannels(),
                                              numSampleToSplitAt, buffer.getNumSamples() - numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag2, rms2);
        }
    }


    //==============================================================================
    //==============================================================================
    struct TestSetup
    {
        double sampleRate = 44100.0;
        int blockSize = 512;
        bool randomiseBlockSizes = false;
        Random random;
    };
    
    //==============================================================================
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
        MidiBuffer midi;
    };

    template<typename AudioNodeProcessorType>
    static inline std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNodeProcessorType> processor, TestSetup ts,
                                                                  const int numChannels, const double durationInSeconds)
    {
        auto context = std::make_unique<TestContext>();
        context->tempFile = std::make_unique<TemporaryFile> (".wav");
        
        // Process the node to a file
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (context->tempFile->getFile().createOutputStream().release(),
                                                                                                ts.sampleRate, (uint32_t) numChannels, 16, {}, 0)))
        {
            processor->prepareToPlay (ts.sampleRate, ts.blockSize);
            
            AudioBuffer<float> buffer (numChannels, ts.blockSize);
            MidiBuffer midi;
            
            int numSamplesToDo = roundToInt (durationInSeconds * ts.sampleRate);
            int numSamplesDone = 0;
            
            for (;;)
            {
                const int numThisTime = ts.randomiseBlockSizes ? std::min (ts.random.nextInt ({ 1, ts.blockSize }), numSamplesToDo)
                                                               : std::min (ts.blockSize, numSamplesToDo);
                buffer.clear();
                midi.clear();
                
                AudioBuffer<float> subSectionBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                     0, numThisTime);

                processor->process ({ juce::Range<int64_t>::withStartAndLength ((int64_t) numSamplesDone, (int64_t) numThisTime),
                                      { { subSectionBuffer }, midi } });
                
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
                AudioBuffer<float> tempBuffer (numChannels, (int) reader->lengthInSamples);
                reader->read (&tempBuffer, 0, tempBuffer.getNumSamples(), 0, true, false);
                context->buffer = std::move (tempBuffer);
                
                return context;
            }
        }

        return {};
    }

    static inline std::unique_ptr<TestContext> createBasicTestContext (std::unique_ptr<AudioNode> node, const TestSetup ts,
                                                                       const int numChannels, const double durationInSeconds)
    {
        auto processor = std::make_unique<AudioNodeProcessor> (std::move (node));
        return createTestContext (std::move (processor), ts, numChannels, durationInSeconds);
    }
}
