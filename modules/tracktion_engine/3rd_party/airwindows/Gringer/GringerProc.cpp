/* ========================================
 *  Gringer - Gringer.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Gringer_H
#include "Gringer.h"
#endif

void Gringer::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    inbandL[0] = 0.025/overallscale;
    outbandL[0] = 0.025/overallscale;
    inbandL[1] = 0.001;
    outbandL[1] = 0.001;
    inbandR[0] = 0.025/overallscale;
    outbandR[0] = 0.025/overallscale;
    inbandR[1] = 0.001;
    outbandR[1] = 0.001;
    //hardwired for wide bandpass around the rectification

    double K = tan(M_PI * inbandL[0]);
    double norm = 1.0 / (1.0 + K / inbandL[1] + K * K);
    inbandL[2] = K / inbandL[1] * norm;
    inbandL[4] = -inbandL[2];
    inbandL[5] = 2.0 * (K * K - 1.0) * norm;
    inbandL[6] = (1.0 - K / inbandL[1] + K * K) * norm;

    K = tan(M_PI * outbandL[0]);
    norm = 1.0 / (1.0 + K / outbandL[1] + K * K);
    outbandL[2] = K / outbandL[1] * norm;
    outbandL[4] = -outbandL[2];
    outbandL[5] = 2.0 * (K * K - 1.0) * norm;
    outbandL[6] = (1.0 - K / outbandL[1] + K * K) * norm;

    K = tan(M_PI * inbandR[0]);
    norm = 1.0 / (1.0 + K / inbandR[1] + K * K);
    inbandR[2] = K / inbandR[1] * norm;
    inbandR[4] = -inbandR[2];
    inbandR[5] = 2.0 * (K * K - 1.0) * norm;
    inbandR[6] = (1.0 - K / inbandR[1] + K * K) * norm;

    K = tan(M_PI * outbandR[0]);
    norm = 1.0 / (1.0 + K / outbandR[1] + K * K);
    outbandR[2] = K / outbandR[1] * norm;
    outbandR[4] = -outbandR[2];
    outbandR[5] = 2.0 * (K * K - 1.0) * norm;
    outbandR[6] = (1.0 - K / outbandR[1] + K * K) * norm;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;

        inputSampleL = sin(inputSampleL);
        inputSampleR = sin(inputSampleR);
        //encode Console5: good cleanness

        long double tempSample = (inputSampleL * inbandL[2]) + inbandL[7];
        inbandL[7] = -(tempSample * inbandL[5]) + inbandL[8];
        inbandL[8] = (inputSampleL * inbandL[4]) - (tempSample * inbandL[6]);
        inputSampleL = fabs(tempSample);
        //this is all you gotta do to make the Green Ringer fullwave rectification effect
        //the rest is about making it work within a DAW context w. filtering and such

        tempSample = (inputSampleR * inbandR[2]) + inbandR[7];
        inbandR[7] = -(tempSample * inbandR[5]) + inbandR[8];
        inbandR[8] = (inputSampleR * inbandR[4]) - (tempSample * inbandR[6]);
        inputSampleR = fabs(tempSample);
        //this is all you gotta do to make the Green Ringer fullwave rectification effect
        //the rest is about making it work within a DAW context w. filtering and such

        tempSample = (inputSampleL * outbandL[2]) + outbandL[7];
        outbandL[7] = -(tempSample * outbandL[5]) + outbandL[8];
        outbandL[8] = (inputSampleL * outbandL[4]) - (tempSample * outbandL[6]);
        inputSampleL = tempSample;

        tempSample = (inputSampleR * outbandR[2]) + outbandR[7];
        outbandR[7] = -(tempSample * outbandR[5]) + outbandR[8];
        outbandR[8] = (inputSampleR * outbandR[4]) - (tempSample * outbandR[6]);
        inputSampleR = tempSample;

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        //amplitude aspect

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

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

void Gringer::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    inbandL[0] = 0.025/overallscale;
    outbandL[0] = 0.025/overallscale;
    inbandL[1] = 0.001;
    outbandL[1] = 0.001;
    inbandR[0] = 0.025/overallscale;
    outbandR[0] = 0.025/overallscale;
    inbandR[1] = 0.001;
    outbandR[1] = 0.001;
    //hardwired for wide bandpass around the rectification

    double K = tan(M_PI * inbandL[0]);
    double norm = 1.0 / (1.0 + K / inbandL[1] + K * K);
    inbandL[2] = K / inbandL[1] * norm;
    inbandL[4] = -inbandL[2];
    inbandL[5] = 2.0 * (K * K - 1.0) * norm;
    inbandL[6] = (1.0 - K / inbandL[1] + K * K) * norm;

    K = tan(M_PI * outbandL[0]);
    norm = 1.0 / (1.0 + K / outbandL[1] + K * K);
    outbandL[2] = K / outbandL[1] * norm;
    outbandL[4] = -outbandL[2];
    outbandL[5] = 2.0 * (K * K - 1.0) * norm;
    outbandL[6] = (1.0 - K / outbandL[1] + K * K) * norm;

    K = tan(M_PI * inbandR[0]);
    norm = 1.0 / (1.0 + K / inbandR[1] + K * K);
    inbandR[2] = K / inbandR[1] * norm;
    inbandR[4] = -inbandR[2];
    inbandR[5] = 2.0 * (K * K - 1.0) * norm;
    inbandR[6] = (1.0 - K / inbandR[1] + K * K) * norm;

    K = tan(M_PI * outbandR[0]);
    norm = 1.0 / (1.0 + K / outbandR[1] + K * K);
    outbandR[2] = K / outbandR[1] * norm;
    outbandR[4] = -outbandR[2];
    outbandR[5] = 2.0 * (K * K - 1.0) * norm;
    outbandR[6] = (1.0 - K / outbandR[1] + K * K) * norm;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;

        inputSampleL = sin(inputSampleL);
        inputSampleR = sin(inputSampleR);
        //encode Console5: good cleanness

        long double tempSample = (inputSampleL * inbandL[2]) + inbandL[7];
        inbandL[7] = -(tempSample * inbandL[5]) + inbandL[8];
        inbandL[8] = (inputSampleL * inbandL[4]) - (tempSample * inbandL[6]);
        inputSampleL = fabs(tempSample);
        //this is all you gotta do to make the Green Ringer fullwave rectification effect
        //the rest is about making it work within a DAW context w. filtering and such

        tempSample = (inputSampleR * inbandR[2]) + inbandR[7];
        inbandR[7] = -(tempSample * inbandR[5]) + inbandR[8];
        inbandR[8] = (inputSampleR * inbandR[4]) - (tempSample * inbandR[6]);
        inputSampleR = fabs(tempSample);
        //this is all you gotta do to make the Green Ringer fullwave rectification effect
        //the rest is about making it work within a DAW context w. filtering and such

        tempSample = (inputSampleL * outbandL[2]) + outbandL[7];
        outbandL[7] = -(tempSample * outbandL[5]) + outbandL[8];
        outbandL[8] = (inputSampleL * outbandL[4]) - (tempSample * outbandL[6]);
        inputSampleL = tempSample;

        tempSample = (inputSampleR * outbandR[2]) + outbandR[7];
        outbandR[7] = -(tempSample * outbandR[5]) + outbandR[8];
        outbandR[8] = (inputSampleR * outbandR[4]) - (tempSample * outbandR[6]);
        inputSampleR = tempSample;

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        //amplitude aspect

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

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
