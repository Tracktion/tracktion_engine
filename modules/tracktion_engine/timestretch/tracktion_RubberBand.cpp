/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_RubberBand.h"

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wextra-semi"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wsign-compare"
 #pragma GCC diagnostic ignored "-Wcast-align"
 #if ! __clang__
  #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #endif
 #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wpedantic"
 #pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#define WIN32_LEAN_AND_MEAN 1
#define Point CarbonDummyPointName
#define Component CarbonDummyCompName

#define USE_SPEEX 1

#define NO_TIMING 1
#define NO_THREADING 1
#define NO_THREAD_CHECKS 1

#if __APPLE__
 #define HAVE_VDSP 1
#else
 #define USE_BUILTIN_FFT 1
#endif

#include "../3rd_party/rubberband/src/dsp/Resampler.cpp"
#include "../3rd_party/rubberband/src/StretcherProcess.cpp"
#include "../3rd_party/rubberband/src/StretchCalculator.cpp"
#include "../3rd_party/rubberband/src/StretcherChannelData.cpp"
#include "../3rd_party/rubberband/src/StretcherImpl.cpp"
#include "../3rd_party/rubberband/src/system/Allocators.cpp"
#include "../3rd_party/rubberband/src/system/sysutils.cpp"
#include "../3rd_party/rubberband/src/system/Thread.cpp"
#include "../3rd_party/rubberband/src/system/VectorOpsComplex.cpp"
#include "../3rd_party/rubberband/src/dsp/AudioCurveCalculator.cpp"
#include "../3rd_party/rubberband/src/dsp/FFT.cpp"
#include "../3rd_party/rubberband/src/base/Profiler.cpp"
#include "../3rd_party/rubberband/src/audiocurves/CompoundAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/ConstantAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/HighFrequencyAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/PercussiveAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/SilentAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/SpectralDifferenceAudioCurve.cpp"
#include "../3rd_party/rubberband/src/speex/resample.c"

#include "../3rd_party/rubberband/src/RubberBandStretcher.cpp"

#undef WIN32_LEAN_AND_MEAN
#undef Point
#undef Component

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif


struct RubberStretcher  : public Stretcher
{
    RubberStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels,
                     RubberBand::RubberBandStretcher::Option option)
        : rubberBandStretcher ((size_t) sourceSampleRate, (size_t) numChannels, option),
          samplesPerOutputBuffer (samplesPerBlock)
    {
    }

    bool isOk() const override
    {
        return true;
    }

    void reset() override
    {
        rubberBandStretcher.reset();
    }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override
    {
        const float pitch = juce::jlimit (0.25f, 4.0f, tracktion_engine::Pitch::semitonesToRatio (semitonesUp));

        rubberBandStretcher.setPitchScale (pitch);
        rubberBandStretcher.setTimeRatio (speedRatio);
        
        const bool r = rubberBandStretcher.getPitchScale() == pitch
                    && rubberBandStretcher.getTimeRatio() == speedRatio;
        jassert (r);
        juce::ignoreUnused (r);

        return r;
    }

    int getFramesNeeded() const override
    {
        return (int) rubberBandStretcher.getSamplesRequired();
    }

    int getMaxFramesNeeded() const override
    {
        return maxFramesNeeded;
    }

    int processData (const float* const* inChannels, int numSamples, float* const* outChannels) override
    {
        jassert (numSamples <= getFramesNeeded());
        rubberBandStretcher.process (inChannels, (size_t) numSamples, false);

        // Once there is output, read this in to the output buffer
        const int numAvailable = rubberBandStretcher.available();
        jassert (numAvailable >= 0);
        
        return (int) rubberBandStretcher.retrieve (outChannels, (size_t) numAvailable);
    }

    int flush (float* const* outChannels) override
    {
        // Empty the FIFO in to the stretcher and mark the last block as final
        if (! hasDoneFinalBlock)
        {
            float* inChannels[32] = { nullptr };
            rubberBandStretcher.process (inChannels, 0, true);
            hasDoneFinalBlock = true;
        }
        
        // Then get the rest of the data out of the stretcher
        const int numAvailable = rubberBandStretcher.available();
        const int numThisBlock = std::min (numAvailable, samplesPerOutputBuffer);
        
        if (numThisBlock > 0)
            return (int) rubberBandStretcher.retrieve (outChannels, (size_t) numThisBlock);

        return 0;
    }

private:
    RubberBand::RubberBandStretcher rubberBandStretcher;
    const int maxFramesNeeded = 8192;
    const int samplesPerOutputBuffer = 0;
    bool hasDoneFinalBlock = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RubberStretcher)
};


//==============================================================================
//==============================================================================
class RubberBandStretcherTests  : public UnitTest
{
public:
    //==============================================================================
    RubberBandStretcherTests()
        : UnitTest ("RubberBandStretcherTests", "Tracktion")
    {
    }

    void runTest() override
    {
        runPitchShiftTest();
        runTimestretchTest();
    }

private:
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

    //==============================================================================
    void runPitchShiftTest()
    {
        beginTest ("Pitch shift");
        
        testStretcher (1.0f, 1.0f);
        testStretcher (1.0f, 2.0f);
        testStretcher (1.0f, 5.0f);
        testStretcher (1.0f, 12.0f);
        testStretcher (1.0f, 24.0f);
    }
    
    void runTimestretchTest()
    {
        beginTest ("Time-stretch");

        testStretcher (0.5f, 0.0f);
        testStretcher (1.0f, 0.0f);
        testStretcher (2.0f, 0.0f);
    }

    //==============================================================================
    void testStretcher (float stretchRatio, float semitonesUp)
    {
        const double sampleRate = 44100.0;
        const int numChannels = 2;
        const int blockSize = 512;
        const float sourcePitch = 440.0f;

        RubberStretcher rubberBandStretcher (sampleRate, blockSize, numChannels, RubberBandStretcher::Option::OptionProcessRealTime);
        rubberBandStretcher.setSpeedAndPitch (stretchRatio, semitonesUp);

        const auto sourceBuffer = createSinBuffer (sampleRate, numChannels, 440.0f);
        const auto resultBuffer = processBuffer (rubberBandStretcher, sourceBuffer, blockSize, stretchRatio);

        writeToFile (juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("original.wav"), sourceBuffer, sampleRate);
        writeToFile (juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("pitched.wav"), resultBuffer, sampleRate);

        const float expectedPitchValue = sourcePitch * tracktion_engine::Pitch::semitonesToRatio (semitonesUp);
        const int expectedSize = (int) std::ceil (sourceBuffer.getNumSamples() * stretchRatio);

        // Check number of zero crossings and estimate pitch
        const int numZeroCrossingsShifted = getNumZeroCrossings (resultBuffer);
        const float shiftedPitch = getPitchFromNumZeroCrossings (numZeroCrossingsShifted, resultBuffer.getNumSamples(), 44100);

        // Compare shiftedPitch to expectedPitchValue with a 3% tolerance
        const float percentageDiff = std::abs (shiftedPitch - expectedPitchValue) / expectedPitchValue;
        expectLessThan (percentageDiff, 0.03f);
        
        // Compare expectedSize with the actual size of the results
        expectEquals (resultBuffer.getNumSamples(), expectedSize);
    }
        
    //==============================================================================
    static juce::AudioBuffer<float> createSinBuffer (double sampleRate, int numChannels, float pitch)
    {
        AudioBuffer<float> sinBuffer (numChannels, (int) sampleRate);

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
    
    static juce::AudioBuffer<float> processBuffer (RubberStretcher& rubberBandStretcher,
                                                   const juce::AudioBuffer<float>& sourceBuffer,
                                                   const int blockSize, float stretchRatio)
    {
        const int destSize = (int) std::ceil (sourceBuffer.getNumSamples() * stretchRatio);
        juce::AudioBuffer<float> resultBuffer (sourceBuffer.getNumChannels(), destSize);
        int numInputsDone = 0, numOutputsDone = 0;

        for (;;)
        {
            const int numInputSamplesLeft = sourceBuffer.getNumSamples() - numInputsDone;
            const int numInputSamplesThisBlock = std::min (rubberBandStretcher.getFramesNeeded(), numInputSamplesLeft);
            const float* inputs[2] = { sourceBuffer.getReadPointer (0, numInputsDone),
                                       sourceBuffer.getReadPointer (1, numInputsDone) };
            float* outputs[2] = { resultBuffer.getWritePointer (0, numOutputsDone),
                                  resultBuffer.getWritePointer (1, numOutputsDone) };
            
            // Process sin wave to shift pitch and store in resultBuffer
            const int numOutputSamplesThisBlock = rubberBandStretcher.processData (inputs, numInputSamplesThisBlock,
                                                                                   outputs);
            jassert (numOutputSamplesThisBlock <= blockSize);
            
            numInputsDone += numInputSamplesThisBlock;
            numOutputsDone += numOutputSamplesThisBlock;
            
            if (numInputsDone >= sourceBuffer.getNumSamples())
                break;
        }

        // Flush until no outputs are left
        for (;;)
        {
            float* outputs[2] = { resultBuffer.getWritePointer (0, numOutputsDone),
                                  resultBuffer.getWritePointer (1, numOutputsDone) };
            
            const int numOutputSamplesThisBlock = rubberBandStretcher.flush (outputs);
            jassert (numOutputSamplesThisBlock <= blockSize);
            numOutputsDone += numOutputSamplesThisBlock;
            
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

    static int getNumZeroCrossings (const AudioBuffer<float>& buffer)
    {
        auto dest = buffer.getReadPointer (0);
        int numZeroCrossings = 0;

        for (int sample = 1; sample < buffer.getNumSamples(); ++sample)
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
static RubberBandStretcherTests rubberBandStretcherTests;
