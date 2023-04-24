/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

// Enable this to dump the output of the current test file to the desktop
#define TIMESTRETCHER_WRITE_WRITE_TEST_FILES 0

//==============================================================================
//==============================================================================
class TimeStretcherTests  : public juce::UnitTest
{
public:
    //==============================================================================
    TimeStretcherTests()
        : juce::UnitTest ("TimeStretcherTests", "Tracktion")
    {
    }

    void runTest() override
    {
       #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        {
            const auto mode = tracktion::engine::TimeStretcher::soundtouchBetter;
            runPitchShiftTest (mode);
            runTimestretchTest (mode);
        }
       #endif

       #if TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        {
            const auto mode = tracktion::engine::TimeStretcher::rubberbandMelodic;
            runPitchShiftTest (mode);
            runTimestretchTest (mode);
        }
       #endif
        
       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        {
            const auto mode = tracktion::engine::TimeStretcher::elastiquePro;
            runPitchShiftTest (mode);
            runTimestretchTest (mode);
        }
       #endif
    }

private:
   #if TIMESTRETCHER_WRITE_WRITE_TEST_FILES
    inline void writeToFile (juce::File file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        file.deleteFile();
        
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (juce::WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                            sampleRate,
                                                                                                            (uint32_t) buffer.getNumChannels(),
                                                                                                            16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }
   #endif

    //==============================================================================
    void runPitchShiftTest (tracktion::engine::TimeStretcher::Mode mode)
    {
        beginTest ("Pitch shift: " + tracktion::engine::TimeStretcher::getNameOfMode (mode));
        
        testStretcher (mode, 1.0f, 0.0f);
        testStretcher (mode, 1.0f, 1.0f);
        testStretcher (mode, 1.0f, 2.0f);
        testStretcher (mode, 1.0f, 5.0f);
        testStretcher (mode, 1.0f, 12.0f);
        testStretcher (mode, 1.0f, 24.0f);
    }
    
    void runTimestretchTest (tracktion::engine::TimeStretcher::Mode mode)
    {
        beginTest ("Time-stretch: " + tracktion::engine::TimeStretcher::getNameOfMode (mode));

        testStretcher (mode, 0.5f, 0.0f);
        testStretcher (mode, 1.0f, 0.0f);
        testStretcher (mode, 2.0f, 0.0f);
    }

    //==============================================================================
    void testStretcher (tracktion::engine::TimeStretcher::Mode mode, float stretchRatio, float semitonesUp)
    {
        const double sampleRate = 44100.0;
        const int numChannels = 2;
        const int blockSize = 441;
        const float sourcePitch = 440.0f;

        tracktion::engine::TimeStretcher stretcher;
        stretcher.initialise (sampleRate, blockSize, numChannels, mode,
                              {}, true);
        stretcher.setSpeedAndPitch (stretchRatio, semitonesUp);

        const auto sourceBuffer = createSinBuffer (sampleRate, numChannels, sourcePitch);
        const auto resultBuffer = processBuffer (stretcher, sourceBuffer, blockSize, stretchRatio,
                                                 mode == tracktion::engine::TimeStretcher::elastiquePro);

       #if TIMESTRETCHER_WRITE_WRITE_TEST_FILES
        writeToFile (juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("original.wav"), sourceBuffer, sampleRate);
        writeToFile (juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("pitched.wav"), resultBuffer, sampleRate);
       #endif

        const float expectedPitchValue = sourcePitch * tracktion::engine::Pitch::semitonesToRatio (semitonesUp);
        const int expectedSize = (int) std::ceil (sourceBuffer.getNumSamples() * stretchRatio);

        // Check number of zero crossings and estimate pitch
        const int numZeroCrossingsShifted = getNumZeroCrossings (resultBuffer);
        const float shiftedPitch = getPitchFromNumZeroCrossings (numZeroCrossingsShifted, resultBuffer.getNumSamples(), sampleRate);

        // Compare shiftedPitch to expectedPitchValue with a 6% tolerance
        expectWithinAbsoluteError (shiftedPitch, expectedPitchValue, expectedPitchValue * 0.06f);
        
        // Compare expectedSize with the actual size of the results 5% tolerance
        expectWithinAbsoluteError (resultBuffer.getNumSamples(), expectedSize, juce::roundToInt (expectedSize * 0.05f));
    }
        
    //==============================================================================
    static juce::AudioBuffer<float> createSinBuffer (double sampleRate, int numChannels, float pitch)
    {
        juce::AudioBuffer<float> sinBuffer (numChannels, (int) sampleRate);

        double currentAngle = 0.0, angleDelta = 0.0;
        float originalPitch = pitch; //A4
        auto cyclesPerSample = originalPitch / float (sampleRate);
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        // Generate sin wave and store in buffer channel 0
        {
            auto chan = sinBuffer.getWritePointer (0);
            
            for (int sample = 0; sample < sinBuffer.getNumSamples(); ++sample)
            {
                chan[sample] = (float) std::sin (currentAngle);
                currentAngle += angleDelta;
            }
        }
        
        // Then copy to subsequent channels
        for (int c = 1; c < sinBuffer.getNumChannels(); ++c)
            sinBuffer.copyFrom (c, 0, sinBuffer,
                                0, 0, sinBuffer.getNumSamples());
        
        return sinBuffer;
    }
    
    static juce::AudioBuffer<float> processBuffer (tracktion::engine::TimeStretcher& stretcher,
                                                   const juce::AudioBuffer<float>& sourceBuffer,
                                                   const int blockSize, float stretchRatio,
                                                   bool stretcherRequiresFramesNeeded)
    {
        const int numChannels = sourceBuffer.getNumChannels();
        jassert (numChannels == 2); // Expected stereo for now
        
        const int destSize = (int) std::ceil (sourceBuffer.getNumSamples() * stretchRatio);
        juce::AudioBuffer<float> resultBuffer (sourceBuffer.getNumChannels(), destSize + 8192);
        int numInputsDone = 0, numOutputsDone = 0;

        for (;;)
        {
            const int numInputSamplesLeft = sourceBuffer.getNumSamples() - numInputsDone;
            const int numInputSamplesThisBlock = std::min (stretcher.getFramesNeeded(), numInputSamplesLeft);
            
            if (stretcherRequiresFramesNeeded && numInputSamplesThisBlock < stretcher.getFramesNeeded())
                break;

            jassert (numInputsDone < (sourceBuffer.getNumSamples() + blockSize));
            jassert (numOutputsDone < destSize);
            const float* inputs[2] = { sourceBuffer.getReadPointer (0, numInputsDone),
                                       sourceBuffer.getReadPointer (1, numInputsDone) };
            float* outputs[2] = { resultBuffer.getWritePointer (0, numOutputsDone),
                                  resultBuffer.getWritePointer (1, numOutputsDone) };
            
            // Process sin wave to shift pitch and store in resultBuffer
            const int numOutputSamplesThisBlock = stretcher.processData (inputs, numInputSamplesThisBlock,
                                                                         outputs);
            jassert (numOutputSamplesThisBlock <= blockSize);
            
            numInputsDone += numInputSamplesThisBlock;
            numOutputsDone += numOutputSamplesThisBlock;
            
            if (numInputsDone >= sourceBuffer.getNumSamples())
                break;

            if (numOutputsDone == destSize)
                break;
        }

        // Flush until no outputs are left
        for (;;)
        {
            if (numOutputsDone >= destSize)
                break;
            
            float* outputs[2] = { resultBuffer.getWritePointer (0, numOutputsDone),
                                  resultBuffer.getWritePointer (1, numOutputsDone) };
            
            const int numOutputSamplesThisBlock = stretcher.flush (outputs);
            jassert (numOutputSamplesThisBlock <= blockSize);
            numOutputsDone += numOutputSamplesThisBlock;

            if (numOutputSamplesThisBlock <= 0)
                break;

            if (numOutputSamplesThisBlock < blockSize)
                break;
        }
        
        // Trim output buffer size
        resultBuffer.setSize (resultBuffer.getNumChannels(), numOutputsDone, true);
        
        return resultBuffer;
    }
    
    static float getPitchFromNumZeroCrossings (const int numZeroCrossings, const int numSamples, const double sampleRate)
    {
        const float bufferTimeInSeconds = (float) numSamples / (float) sampleRate;
        const float numCycles = (float) numZeroCrossings / 2.0f;
        const float pitchInHertz = numCycles / bufferTimeInSeconds;
        
        return pitchInHertz;
    }

    static int getNumZeroCrossings (const juce::AudioBuffer<float>& buffer)
    {
        int numSamplesToUse = buffer.getNumSamples();
        auto dest = buffer.getReadPointer (0);
        int numZeroCrossings = 0;

        for (int sample = 1; sample < numSamplesToUse; ++sample)
        {
            if (((dest[sample - 1] > 0.0f) && (dest[sample] <= 0.0f))
             || ((dest[sample - 1] < 0.0f) && (dest[sample] >= 0.0f)))
            {
                ++numZeroCrossings;
            }
        }

        return numZeroCrossings;
    }
};

//==============================================================================
static TimeStretcherTests timeStretcherTests;

#endif // TRACKTION_UNIT_TESTS
