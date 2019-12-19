/* ========================================
 *  SoftGate - SoftGate.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SoftGate_H
#include "SoftGate.h"
#endif

void SoftGate::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double threshold = pow(A,6);
    double recovery = pow((B*0.5),6);
    recovery /= overallscale;
    double baseline = pow(C,6);
    double invrec = 1.0 - recovery;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;

        storedL[1] = storedL[0];
        storedL[0] = inputSampleL;
        diffL = storedL[0] - storedL[1];

        storedR[1] = storedR[0];
        storedR[0] = inputSampleR;
        diffR = storedR[0] - storedR[1];

        if (gate > 0) {gate = ((gate-baseline) * invrec) + baseline;}

        if ((fabs(diffR) > threshold) || (fabs(diffL) > threshold)) {gate = 1.1;}
        else {gate = (gate * invrec); if (threshold > 0) {gate += ((fabs(inputSampleL)/threshold) * recovery);gate += ((fabs(inputSampleR)/threshold) * recovery);}}

        if (gate < 0) gate = 0;

        if (gate < 1.0)
        {
            storedL[0] = storedL[1] + (diffL * gate);
            storedR[0] = storedR[1] + (diffR * gate);
        }

        if (gate < 1) {
            inputSampleL = (inputSampleL * gate) + (storedL[0] * (1.0-gate));
            inputSampleR = (inputSampleR * gate) + (storedR[0] * (1.0-gate));
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

void SoftGate::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double threshold = pow(A,6);
    double recovery = pow((B*0.5),6);
    recovery /= overallscale;
    double baseline = pow(C,6);
    double invrec = 1.0 - recovery;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;

        storedL[1] = storedL[0];
        storedL[0] = inputSampleL;
        diffL = storedL[0] - storedL[1];

        storedR[1] = storedR[0];
        storedR[0] = inputSampleR;
        diffR = storedR[0] - storedR[1];

        if (gate > 0) {gate = ((gate-baseline) * invrec) + baseline;}

        if ((fabs(diffR) > threshold) || (fabs(diffL) > threshold)) {gate = 1.1;}
        else {gate = (gate * invrec); if (threshold > 0) {gate += ((fabs(inputSampleL)/threshold) * recovery);gate += ((fabs(inputSampleR)/threshold) * recovery);}}

        if (gate < 0) gate = 0;

        if (gate < 1.0)
        {
            storedL[0] = storedL[1] + (diffL * gate);
            storedR[0] = storedR[1] + (diffR * gate);
        }

        if (gate < 1) {
            inputSampleL = (inputSampleL * gate) + (storedL[0] * (1.0-gate));
            inputSampleR = (inputSampleR * gate) + (storedR[0] * (1.0-gate));
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
