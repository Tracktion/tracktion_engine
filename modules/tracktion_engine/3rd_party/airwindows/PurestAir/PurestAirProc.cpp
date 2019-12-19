/* ========================================
 *  PurestAir - PurestAir.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PurestAir_H
#include "PurestAir.h"
#endif

void PurestAir::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double applyTarget = (A*2.0)-1.0;
    double threshold = pow((1-fabs(applyTarget)),3);
    if (applyTarget > 0) applyTarget *= 3;

    double intensity = pow(B,2)*5.0;
    double wet = C;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        halfDrySampleL = halfwaySampleL = (inputSampleL + last1SampleL) / 2.0;
        last1SampleL = inputSampleL;
        s3L = s2L;
        s2L = s1L;
        s1L = inputSampleL;
        double m1 = (s1L-s2L)*((s1L-s2L)/1.3);
        double m2 = (s2L-s3L)*((s1L-s2L)/1.3);
        double sense = fabs((m1-m2)*((m1-m2)/1.3))*intensity;
        //this will be 0 for smooth, high for SSS
        applyL += applyTarget - sense;
        applyL *= 0.5;
        if (applyL < -1.0) applyL = -1.0;

        double clamp = halfwaySampleL - halfDrySampleL;
        if (clamp > threshold) halfwaySampleL = lastSampleL + threshold;
        if (-clamp > threshold) halfwaySampleL = lastSampleL - threshold;
        lastSampleL = halfwaySampleL;

        clamp = inputSampleL - lastSampleL;
        if (clamp > threshold) inputSampleL = lastSampleL + threshold;
        if (-clamp > threshold) inputSampleL = lastSampleL - threshold;
        lastSampleL = inputSampleL;

        diffSampleL = *in1 - inputSampleL;
        halfDiffSampleL = halfDrySampleL - halfwaySampleL;

        inputSampleL = *in1 + ((diffSampleL + halfDiffSampleL)*applyL);
        //done with left channel

        halfDrySampleR = halfwaySampleR = (inputSampleR + last1SampleR) / 2.0;
        last1SampleR = inputSampleR;
        s3R = s2R;
        s2R = s1R;
        s1R = inputSampleR;
        m1 = (s1R-s2R)*((s1R-s2R)/1.3);
        m2 = (s2R-s3R)*((s1R-s2R)/1.3);
        sense = fabs((m1-m2)*((m1-m2)/1.3))*intensity;
        //this will be 0 for smooth, high for SSS
        applyR += applyTarget - sense;
        applyR *= 0.5;
        if (applyR < -1.0) applyR = -1.0;

        clamp = halfwaySampleR - halfDrySampleR;
        if (clamp > threshold) halfwaySampleR = lastSampleR + threshold;
        if (-clamp > threshold) halfwaySampleR = lastSampleR - threshold;
        lastSampleR = halfwaySampleR;

        clamp = inputSampleR - lastSampleR;
        if (clamp > threshold) inputSampleR = lastSampleR + threshold;
        if (-clamp > threshold) inputSampleR = lastSampleR - threshold;
        lastSampleR = inputSampleR;

        diffSampleR = *in2 - inputSampleR;
        halfDiffSampleR = halfDrySampleR - halfwaySampleR;

        inputSampleR = *in2 + ((diffSampleR + halfDiffSampleR)*applyR);
        //done with right channel

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
        }

        //begin 32 bit stereo floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        frexpf((float)inputSampleR, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        //end 32 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void PurestAir::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double applyTarget = (A*2.0)-1.0;
    double threshold = pow((1-fabs(applyTarget)),3);
    if (applyTarget > 0) applyTarget *= 3;

    double intensity = pow(B,2)*5.0;
    double wet = C;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        halfDrySampleL = halfwaySampleL = (inputSampleL + last1SampleL) / 2.0;
        last1SampleL = inputSampleL;
        s3L = s2L;
        s2L = s1L;
        s1L = inputSampleL;
        double m1 = (s1L-s2L)*((s1L-s2L)/1.3);
        double m2 = (s2L-s3L)*((s1L-s2L)/1.3);
        double sense = fabs((m1-m2)*((m1-m2)/1.3))*intensity;
        //this will be 0 for smooth, high for SSS
        applyL += applyTarget - sense;
        applyL *= 0.5;
        if (applyL < -1.0) applyL = -1.0;

        double clamp = halfwaySampleL - halfDrySampleL;
        if (clamp > threshold) halfwaySampleL = lastSampleL + threshold;
        if (-clamp > threshold) halfwaySampleL = lastSampleL - threshold;
        lastSampleL = halfwaySampleL;

        clamp = inputSampleL - lastSampleL;
        if (clamp > threshold) inputSampleL = lastSampleL + threshold;
        if (-clamp > threshold) inputSampleL = lastSampleL - threshold;
        lastSampleL = inputSampleL;

        diffSampleL = *in1 - inputSampleL;
        halfDiffSampleL = halfDrySampleL - halfwaySampleL;

        inputSampleL = *in1 + ((diffSampleL + halfDiffSampleL)*applyL);
        //done with left channel

        halfDrySampleR = halfwaySampleR = (inputSampleR + last1SampleR) / 2.0;
        last1SampleR = inputSampleR;
        s3R = s2R;
        s2R = s1R;
        s1R = inputSampleR;
        m1 = (s1R-s2R)*((s1R-s2R)/1.3);
        m2 = (s2R-s3R)*((s1R-s2R)/1.3);
        sense = fabs((m1-m2)*((m1-m2)/1.3))*intensity;
        //this will be 0 for smooth, high for SSS
        applyR += applyTarget - sense;
        applyR *= 0.5;
        if (applyR < -1.0) applyR = -1.0;

        clamp = halfwaySampleR - halfDrySampleR;
        if (clamp > threshold) halfwaySampleR = lastSampleR + threshold;
        if (-clamp > threshold) halfwaySampleR = lastSampleR - threshold;
        lastSampleR = halfwaySampleR;

        clamp = inputSampleR - lastSampleR;
        if (clamp > threshold) inputSampleR = lastSampleR + threshold;
        if (-clamp > threshold) inputSampleR = lastSampleR - threshold;
        lastSampleR = inputSampleR;

        diffSampleR = *in2 - inputSampleR;
        halfDiffSampleR = halfDrySampleR - halfwaySampleR;

        inputSampleR = *in2 + ((diffSampleR + halfDiffSampleR)*applyR);
        //done with right channel

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
        }

        //begin 64 bit stereo floating point dither
        int expon; frexp((double)inputSampleL, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        frexp((double)inputSampleR, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        //end 64 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
