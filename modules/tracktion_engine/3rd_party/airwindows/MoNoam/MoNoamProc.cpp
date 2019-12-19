/* ========================================
 *  MoNoam - MoNoam.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __MoNoam_H
#include "MoNoam.h"
#endif

void MoNoam::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    int processing = (VstInt32)( A * 7.999 );

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;

        long double mid; mid = inputSampleL + inputSampleR;
        long double side; side = inputSampleL - inputSampleR;

        if (processing == kMONO || processing == kMONOR || processing == kMONOL) side = 0.0;
        if (processing == kSIDE || processing == kSIDEM || processing == kSIDER || processing == kSIDEL) mid = 0.0;

        inputSampleL = (mid+side)/2.0;
        inputSampleR = (mid-side)/2.0;

        if (processing == kSIDEM || processing == kSIDER || processing == kSIDEL) inputSampleL = -inputSampleL;

        if (processing == kMONOR || processing == kSIDER) inputSampleL = 0.0;
        if (processing == kMONOL || processing == kSIDEL) inputSampleR = 0.0;

        if (processing == kBYPASS) {inputSampleL = *in1; inputSampleR = *in2;}

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

void MoNoam::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    int processing = (VstInt32)( A * 7.999 );

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;

        long double mid; mid = inputSampleL + inputSampleR;
        long double side; side = inputSampleL - inputSampleR;

        if (processing == kMONO || processing == kMONOR || processing == kMONOL) side = 0.0;
        if (processing == kSIDE || processing == kSIDEM || processing == kSIDER || processing == kSIDEL) mid = 0.0;

        inputSampleL = (mid+side)/2.0;
        inputSampleR = (mid-side)/2.0;

        if (processing == kSIDEM || processing == kSIDER || processing == kSIDEL) inputSampleL = -inputSampleL;

        if (processing == kMONOR || processing == kSIDER) inputSampleL = 0.0;
        if (processing == kMONOL || processing == kSIDEL) inputSampleR = 0.0;

        if (processing == kBYPASS) {inputSampleL = *in1; inputSampleR = *in2;}

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
