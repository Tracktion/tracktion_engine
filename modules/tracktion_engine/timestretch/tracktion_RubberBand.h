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

struct Stretcher
{
    virtual ~Stretcher() {}

    virtual bool isOk() const = 0;
    virtual void reset() = 0;
    virtual bool setSpeedAndPitch(float speedRatio, float semitonesUp) = 0;
    virtual int getFramesNeeded() const = 0;
    virtual int getMaxFramesNeeded() const = 0;
    virtual void processData(const float* const* inChannels, int numSamples, float* const* outChannels) = 0;
    virtual void flush(float* const* outChannels) = 0;
};

struct RubberStretcher  : public Stretcher
{
    RubberStretcher(double sourceSampleRate, int samplesPerBlock, int numChannels, RubberBand::RubberBandStretcher::Option option)
                  : thisRubberBandStretcher(sourceSampleRate, numChannels, option, 1.0f, 0.5f)

    {

        sampleRate = sourceSampleRate;
        TempBuffer.setSize(numChannels, sourceSampleRate);

        if (&thisRubberBandStretcher == nullptr)
        {
            jassertfalse;
            TRACKTION_LOG_ERROR("Cannot create Rubber Band Stretcher");
        }
        else
        {
            // This must be called first, before any other functions and can not
            // be called again
            maxFramesNeeded = thisRubberBandStretcher.available();
        }

        sampleRate = sourceSampleRate;

        thisRubberBandStretcher.reset();
    }

    bool isOk() const override { return &thisRubberBandStretcher != nullptr; }

    void reset() override { thisRubberBandStretcher.reset(); }

    bool setSpeedAndPitch(float speedRatio, float semitonesUp) override;
    int getFramesNeeded() const override;
    int getMaxFramesNeeded() const override;

    void processData(const float* const* inChannels, int numSamples, float* const* outChannels) override;

    int getSamplesRequired()
    {
        return thisRubberBandStretcher.getSamplesRequired();
    }

    void flush(float* const* outChannels) override
    {
    }



private:

    AudioBuffer<float> TempBuffer;

    int maxFramesNeeded = 0;

    RubberBand::RubberBandStretcher thisRubberBandStretcher;

    float sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RubberStretcher)

};
