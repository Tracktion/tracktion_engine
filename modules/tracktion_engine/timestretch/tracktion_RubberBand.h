/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#define USE_SPEEX 1
#define USE_BUILTIN_FFT 1
#define NO_THREADING 1
#define NO_TIMING 1

#if __APPLE__
 #define HAVE_VDSP 1
#endif

#include "../3rd_party/rubberband/rubberband/RubberBandStretcher.h"

#include "../3rd_party/rubberband/src/base/RingBuffer.h"
#include <tracktion_engine/3rd_party/soundtouch/include/FIFOSamplePipe.h>


struct Stretcher
{
    virtual ~Stretcher() {}

    virtual bool isOk() const = 0;
    virtual void reset() = 0;
    virtual bool setSpeedAndPitch (float speedRatio, float semitonesUp) = 0;
    virtual int getFramesNeeded() const = 0;
    virtual int getMaxFramesNeeded() const = 0;
    virtual void processData (const float* const* inChannels, int numSamples, float* const* outChannels) = 0;
    virtual void flush (float* const* outChannels) = 0;
};

struct RubberStretcher  : public Stretcher
{
    RubberStretcher (double sourceSampleRate, int samplesPerBlock, int numChannels, RubberBand::RubberBandStretcher::Option option)
                   : rubberBandStretcher(sourceSampleRate, numChannels, option, 1.0f, 1.0f),
                     m_reserve (samplesPerBlock),
                     m_blockSize (samplesPerBlock),
                     m_minfill (0)

    {
        sampleRate = sourceSampleRate;

        if (&rubberBandStretcher == nullptr)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR ("Cannot create Rubber Band Stretcher");
        }
        else
        {
            // This must be called first, before any other functions and can not
            // be called again
            maxFramesNeeded = rubberBandStretcher.available();
        }

        rubberBandStretcher.reset();

        m_input = new float* [numChannels];
        m_output = new float* [numChannels];

        int bufsize = m_blockSize + m_reserve + 8192;

        ringBuffer = new RubberBand::RingBuffer<float> *[numChannels];
        m_scratch = new float* [numChannels];

        for (size_t c = 0; c < numChannels; ++c) 
        {
            m_input[c] = 0;
            m_output[c] = 0;

            ringBuffer[c] = new RubberBand::RingBuffer<float> (bufsize);

            m_scratch[c] = new float[bufsize];
            for (int i = 0; i < bufsize; ++i) 
                m_scratch[c][i] = 0.f;
        }

        for (size_t c = 0; c < numChannels; ++c) 
        {
            ringBuffer[c]->reset();
            ringBuffer[c]->zero (m_reserve);
        }
    }

    bool isOk() const override { return &rubberBandStretcher != nullptr; };



    void reset() override { rubberBandStretcher.reset(); }

    bool setSpeedAndPitch (float speedRatio, float semitonesUp) override;
    int getFramesNeeded() const override;
    int getMaxFramesNeeded() const override;

    void processData(const float* const* inChannels, int numSamples, float* const* outChannels) override;
    void processData(const float* const* inChannels, int numSamples, float* const* outChannels, unsigned long offset);

    int getSamplesRequired()
    {
        return rubberBandStretcher.getSamplesRequired();
    }

    void flush(float* const* outChannels) override
    {
    }

private:
    int maxFramesNeeded = 0;

    AudioBuffer<float> TempBuffer;

    RubberBand::RubberBandStretcher rubberBandStretcher;

    float sampleRate;

    float** m_input;
    float** m_output;
    RubberBand::RingBuffer<float>** ringBuffer;
    float** m_scratch;

    size_t m_blockSize;
    size_t m_reserve;
    size_t m_minfill = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RubberStretcher)

};
