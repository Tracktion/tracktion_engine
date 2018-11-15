/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wsign-conversion"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wshadow"
 #pragma clang diagnostic ignored "-Wmacro-redefined"
 #pragma clang diagnostic ignored "-Wconversion"
 #pragma clang diagnostic ignored "-Wunused"
#endif

#include "../3rd_party/soundtouch/include/BPMDetect.h"

#ifdef __clang__
 #pragma clang diagnostic pop
#endif


/**
    Uses the SoundTouch BPMDetect class to guess the tempo of some audio.
*/
class TempoDetect
{
public:
    TempoDetect (int numChannels_, double sampleRate)
        : numChannels (numChannels_),
          bpmDetect (numChannels, (int) sampleRate)
    {
    }

    /** Processes an entire AudioFormatReader returning the tempo for it.
        @returns the tempo in BPM for the block.
    */
    float processReader (juce::AudioFormatReader& reader)
    {
        jassert ((int) reader.numChannels == numChannels);

        const int blockSize = 65536;
        const bool useRightChan = numChannels > 1;

        // can't use an AudioScratchBuffer yet
        juce::AudioBuffer<float> buffer (numChannels, blockSize);

        juce::int64 numLeft = reader.lengthInSamples;
        int startSample = 0;

        while (numLeft > 0)
        {
            const int numThisTime = (int) juce::jmin (numLeft, (juce::int64) blockSize);
            reader.read (&buffer, 0, numThisTime, startSample, true, useRightChan);
            processSection (buffer, numThisTime);

            startSample += numThisTime;
            numLeft -= numThisTime;
        }

        return finishAndDetect();
    }

    /** Processes a block of audio returning the tempo for it.
        @returns the tempo in BPM for the block.
    */
    float processAndDetect (const float** const inputSamples, int numSamples)
    {
        processSection (inputSamples, numSamples);

        return bpm = bpmDetect.getBpm();
    }

    /** Returns the last BPM detected. */
    float getBpm() const                            { return  bpm; }

    bool isBpmSensible() const                      { return getSensibleRange().contains (bpm); }
    static juce::Range<float> getSensibleRange()    { return { 29, 200 }; }

    //==============================================================================
    /** Processes a non-interleaved buffer section.  */
    void processSection (juce::AudioBuffer<float>& buffer, int numSamplesToProcess)
    {
        processSection (buffer.getArrayOfReadPointers(), numSamplesToProcess);
    }

    /** Completes the detection process and returns the BPM. */
    float finishAndDetect()
    {
        return bpm = bpmDetect.getBpm();
    }

    void processSection (const float** const inputSamples, int numSamples)
    {
        AudioScratchBuffer scratch (1, numChannels * numSamples);
        float* interleaved = scratch.buffer.getWritePointer (0);
        juce::AudioDataConverters::interleaveSamples (inputSamples, interleaved, numSamples, numChannels);

        bpmDetect.inputSamples (interleaved, numSamples);
    }

private:
    //==============================================================================
    int numChannels;
    soundtouch::BPMDetect bpmDetect;
    float bpm = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoDetect)
};
