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
        runTimeStretchTest();
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
        
        testPitchShift (1.0f, 466.16f);
        testPitchShift (2.0f, 493.88f);
        testPitchShift (5.0f, 587.33f);
        testPitchShift (12.0f, 880.0f);
        testPitchShift (24.0f, 1760.0f);
    }

    void testPitchShift (float shiftNumSemitones, float expectedPitchValue)
    {
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        const int numChannels = 2;
        RubberStretcher rubberBandStretcher (sampleRate, 512, numChannels, RubberBandStretcher::Option::OptionProcessRealTime);
        rubberBandStretcher.setSpeedAndPitch (1.0f, shiftNumSemitones);

        AudioBuffer<float> sinBuffer (2, 44100), resultBuffer (2, 44100);
        resultBuffer.clear();

        auto wL = sinBuffer.getWritePointer (0);
        auto wR = sinBuffer.getWritePointer (1);

        double currentAngle = 0.0, angleDelta = 0.0;
        float originalPitch = 440.0f; //A4
        auto cyclesPerSample = originalPitch / 44100.0f;
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        // Generate sin wave and store in testBuffer
        for (int sample = 0; sample < 44100; ++sample)
        {
            auto currentSample = (float) std::sin (currentAngle);
            currentAngle += angleDelta;
            wL[sample] = currentSample;
            wR[sample] = currentSample;
        }

        {
            int numInputsDone = 0, numOutputsDone = 0;
            
            for (;;)
            {
                const int numInputSamplesLeft = sinBuffer.getNumSamples() - numInputsDone;
                const int numInputSamplesThisBlock = std::min (rubberBandStretcher.getFramesNeeded(), numInputSamplesLeft);
                const float* inputs[2] = { sinBuffer.getReadPointer (0, numInputsDone),
                                           sinBuffer.getReadPointer (1, numInputsDone) };
                float* outputs[2] = { resultBuffer.getWritePointer (0, numOutputsDone),
                                      resultBuffer.getWritePointer (1, numOutputsDone) };
                
                // Process sin wave to shift pitch and store in resultBuffer
                const int numOutputSamplesThisBlock = rubberBandStretcher.processData (inputs, numInputSamplesThisBlock,
                                                                                       outputs);
                jassert (numOutputSamplesThisBlock <= blockSize);
                
                numInputsDone += numInputSamplesThisBlock;
                numOutputsDone += numOutputSamplesThisBlock;
                
                if (numInputsDone >= sinBuffer.getNumSamples())
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
        }
        
        // Check number of zero crossings and estimate pitch
        int numZeroCrossingsShifted = getNumZeroCrossings (resultBuffer);
        float shiftedPitch = getPitchFromNumZeroCrossings (numZeroCrossingsShifted, resultBuffer.getNumSamples(), 44100);

        // Compare shiftedPitch to originalPitch with a tolerance
        const float percentageDiff = std::abs (shiftedPitch - expectedPitchValue) / expectedPitchValue;
        expectLessThan (percentageDiff, 0.03f);
    }

    void runTimeStretchTest()
    {
        beginTest ("Time-stretch");

        RubberStretcher rubberBandStretcher = RubberStretcher (44100, 512, 2, RubberBandStretcher::Option::OptionProcessRealTime);
        rubberBandStretcher.reset();

        // Stretches the audio by /2, at the same pitch
        rubberBandStretcher.setSpeedAndPitch (2.0f, 1.0f);

        AudioBuffer<float> sinBuffer;
        sinBuffer.setSize (2, 44100);

        auto wL = sinBuffer.getWritePointer (0);
        auto wR = sinBuffer.getWritePointer (1);

        double currentAngle = 0.0, angleDelta = 0.0;
        float originalPitch = 440; //A4
        auto cyclesPerSample = originalPitch / 44100.0f;
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        // Generate sin wave and store in testBuffer
        for (int sample = 0; sample < 44100; ++sample)
        {
            auto currentSample = (float) std::sin (currentAngle);
            currentAngle += angleDelta;
            wL[sample] = currentSample;
            wR[sample] = currentSample;
        }

        AudioBuffer<float> tempBuffer;
        tempBuffer.setSize (2, 22050);

        // Process sin wave to time stretch and store in testBuffer
        rubberBandStretcher.processData (sinBuffer.getArrayOfReadPointers(), 44100, sinBuffer.getArrayOfWritePointers());

        int zeroCountL = 0;
        int zeroCountR = 0;

        for (int sample = 22049; sample < 44100; ++sample)
        {
            if (wL[sample] == 0.0f)
            {
                zeroCountL++;
            }
            if (wR[sample] == 0.0f)
            {
                zeroCountR++;
            }
        }

        // Check whether buffer has resized
        expectEquals (zeroCountL, 22050);
        expectEquals (zeroCountR, 22050);
    }
        
    //==============================================================================
    float getPitchFromNumZeroCrossings (const int numZeroCrossings, const int numSamples, const double sampleRate)
    {
        float bufferTimeInSeconds = (float) numSamples / (float) sampleRate;
        float numCycles = (float) numZeroCrossings / 2.0f;
        float pitchInHertz = numCycles / bufferTimeInSeconds;
        return pitchInHertz;
    }

    int getNumZeroCrossings (const AudioBuffer<float>& buffer)
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
