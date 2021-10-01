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

#define Point CarbonDummyPointName
#define Component CarbonDummyCompName

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
#include "../3rd_party/rubberband/src/dsp/Resampler.cpp"
#include "../3rd_party/rubberband/src/base/Profiler.cpp"
#include "../3rd_party/rubberband/src/audiocurves/CompoundAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/ConstantAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/HighFrequencyAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/PercussiveAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/SilentAudioCurve.cpp"
#include "../3rd_party/rubberband/src/audiocurves/SpectralDifferenceAudioCurve.cpp"
#include "../3rd_party/rubberband/src/speex/resample.c"

#include "../3rd_party/rubberband/src/RubberBandStretcher.cpp"

#undef Point
#undef Component

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

bool RubberStretcher::setSpeedAndPitch (float speedRatio, float semitonesUp)
{
    float pitch = juce::jlimit (0.25f, 4.0f, tracktion_engine::Pitch::semitonesToRatio (semitonesUp));

    // Not sure what to do with sync

    // bool sync = (elastiqueMode == TimeStretcher::elastiquePro) ? elastiqueProOptions.syncTimeStrPitchShft : false;

    rubberBandStretcher.setPitchScale (pitch);
    rubberBandStretcher.setTimeRatio (speedRatio);

    const bool r = (rubberBandStretcher.getPitchScale() == pitch && rubberBandStretcher.getTimeRatio() == speedRatio);
    jassert (r == 0); juce::ignoreUnused(r);

    return r == 0;
}

int RubberStretcher::getFramesNeeded() const
{
    const int framesNeeded = rubberBandStretcher.available();
    jassert (framesNeeded <= maxFramesNeeded);
    return framesNeeded;
}

int RubberStretcher::getMaxFramesNeeded() const
{
    return maxFramesNeeded;
}

void RubberStretcher::processData(const float* const* inChannels, int numSamples, float* const* outChannels)
{
    rubberBandStretcher.process ((float**) outChannels, numSamples, false);

    while (rubberBandStretcher.getSamplesRequired() < numSamples)
    {
        rubberBandStretcher.process (inChannels, rubberBandStretcher.getSamplesRequired(), false);
    }

    rubberBandStretcher.retrieve ((float**) outChannels, numSamples);
}


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
    //==============================================================================
    void runPitchShiftTest()
    {
        beginTest ("Pitch shift");
        
        RubberStretcher rubberBandStretcher = RubberStretcher (44100, 512, 2, RubberBandStretcher::Option::OptionProcessRealTime);
        // Pitches the audio by 1 semitones, without performing time-stretching.
        rubberBandStretcher.setSpeedAndPitch (1.0f, std::pow (2.0f, 1.0f / 12.0f));

        rubberBandStretcher.reset();

        AudioBuffer<float> sinBuffer;
        sinBuffer.setSize (2, 44100);

        auto wL = sinBuffer.getWritePointer (0);
        auto wR = sinBuffer.getWritePointer (1);

        double currentAngle = 0.0, angleDelta = 0.0;
        float originalPitch = 432.0f; //A4
        auto cyclesPerSample = originalPitch / 44100.0f;
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

        // Generate sin wave and store in testBuffer
        for (int sample = 0; sample < 44100; ++sample)
        {
            auto currentSample = (float)std::sin(currentAngle);
            currentAngle += angleDelta;
            wL[sample] = currentSample;
            wR[sample] = currentSample;
        }

        // Process sin wave to shift pitch and store in testBuffer
        rubberBandStretcher.processData (sinBuffer.getArrayOfReadPointers(), sinBuffer.getNumSamples(), sinBuffer.getArrayOfWritePointers());

        // Check number of zero crossings and estimate pitch
        int numZeroCrossingsShifted = getNumZeroCrossings (sinBuffer);
        float shiftedPitch = getPitchFromNumZeroCrossings (numZeroCrossingsShifted, sinBuffer.getNumSamples(), 44100);

        // Compare shiftedPitch to originalPitch with a tolerance
        expectLessThan (std::abs (shiftedPitch - (457.69f)), 2.0f);
    }

    void runTimeStretchTest()
    {
        beginTest ("Time-stretch");

        RubberStretcher rubberBandStretcher = RubberStretcher (44100, 512, 2, RubberBandStretcher::Option::OptionProcessRealTime);
        rubberBandStretcher.reset();

        // Stretches the audio by /2, at the same pitch
        rubberBandStretcher.setSpeedAndPitch (0.5f, 1.0f);

        AudioBuffer<float> sinBuffer;
        sinBuffer.setSize (2, 44100);

        auto wL = sinBuffer.getWritePointer (0);
        auto wR = sinBuffer.getWritePointer (1);

        double currentAngle = 0.0, angleDelta = 0.0;
        float originalPitch = 432.0f; //A4
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

        // Process sin wave to time stretch and store in testBuffer
        rubberBandStretcher.processData (sinBuffer.getArrayOfReadPointers(), 44100, sinBuffer.getArrayOfWritePointers());

        AudioBuffer<float> testBuffer;
        testBuffer.setSize (2, 22050);

        int zeroCountL = 0;
        int zeroCountR = 0;

        for (int sample = 22049; sample < 44100; ++sample)
        {
            if (wL[sample] == 0)
            {
                ++zeroCountL;
            }
            if (wR[sample] == 0)
            {
                ++zeroCountR;
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
        float numCycles = numZeroCrossings / 2.0f;
        float pitchInHertz = numCycles / bufferTimeInSeconds;
        return pitchInHertz;
    }

    int getNumZeroCrossings (AudioBuffer<float>& buffer)
    {
        auto dest = buffer.getWritePointer(0);

        int numZeroCrossings = 0;

        for (int sample = 1; sample < buffer.getNumSamples(); ++sample)
        {
            if ((dest[sample] < 0.0f && dest[sample - 1] > 0.0f)
             || (dest[sample] > 0.0f && dest[sample - 1] < 0.0f))
            {
                ++numZeroCrossings;
            }
        }

        return numZeroCrossings;
    }
};

//==============================================================================
static RubberBandStretcherTests rubberBandStretcherTests;
